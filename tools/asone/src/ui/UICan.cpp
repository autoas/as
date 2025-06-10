/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail->com>
 */
/* ================================ { INCLUDES  ] ============================================== */
#include "UICan.hpp"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QMainwindow>
#include <QTextCursor>
#include <sstream>
#include <iomanip>
#include "canlib.h"
#include "Log.hpp"
using namespace as;
/* ================================ { MACROS    ] ============================================== */
/* ================================ { TYPES     ] ============================================== */
/* ================================ { DECLARES  ] ============================================== */
/* ================================ { DATAS     ] ============================================== */
/* ================================ { LOCALS    ] ============================================== */
/* ================================ { FUNCTIONS ] ============================================== */
UICan::UICan() : QWidget() {
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  for (int i = 0; i < MAX_BUS; i++) {
    m_cmbxCanDevice.push_back(new QComboBox());
    m_cmbxCanPort.push_back(new QComboBox());
    m_cmbxCanBaud.push_back(new QComboBox());
    m_btnOpen.push_back(new QPushButton("Open"));
    m_cmbxCanDevice[i]->addItems({"simulator_v2", "qemu", "peak", "vxl", "zlg"});
    m_cmbxCanPort[i]->addItems({"0", "1", "2", "3", "4", "5", "6", "7"});
    m_cmbxCanPort[i]->setStatusTip("port");
    m_cmbxCanBaud[i]->addItems({"125000", "250000", "500000", "1000000"});
    m_cmbxCanBaud[i]->setStatusTip("baudrate");
    m_cmbxCanBaud[i]->setCurrentIndex(2);
    m_cmbxCanDevice[i]->setEditable(true);
    m_cmbxCanPort[i]->setEditable(true);
    m_cmbxCanBaud[i]->setEditable(true);

    std::string ln = "CAN" + std::to_string(i) + ":";
    grid->addWidget(new QLabel(tr(ln.c_str())), i, 0);
    grid->addWidget(m_cmbxCanDevice[i], i, 1);
    grid->addWidget(m_cmbxCanPort[i], i, 2);
    grid->addWidget(m_cmbxCanBaud[i], i, 3);
    grid->addWidget(m_btnOpen[i], i, 4);
    m_BusID[i] = -1;
  }
  vbox->addLayout(grid);

  auto hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("BUS ID:"));
  m_cmbxBusID = new QComboBox();
  m_cmbxBusID->addItems({"CAN0", "CAN1", "CAN2", "CAN3"});
  hbox->addWidget(m_cmbxBusID);
  hbox->addWidget(new QLabel("CAN ID:"));
  m_leCanID = new QLineEdit();
  m_leCanID->setMaximumWidth(120);
  m_leCanID->setText("0");
  m_leCanID->setStatusTip("hex value");
  hbox->addWidget(m_leCanID);
  hbox->addWidget(new QLabel("DATA:"));
  m_leCanData = new QLineEdit();
  m_leCanData->setText("1122334455667788");
  hbox->addWidget(m_leCanData);
  hbox->addWidget(new QLabel("PERIOD:"));
  m_leCanPeriod = new QLineEdit();
  m_leCanPeriod->setText("0");
  hbox->addWidget(m_leCanPeriod);
  m_leCanPeriod->setMaximumWidth(120);
  m_btnSend = new QPushButton("Send");
  hbox->addWidget(m_btnSend);
  vbox->addLayout(hbox);

  auto hbox2 = new QHBoxLayout();
  m_cbxCanTraceEnable = new QCheckBox("CAN trace log enable");
  m_cbxCanTraceEnable->setChecked(true);
  m_cbxCanStdoutEnable = new QCheckBox("CAN stdout enable");
  m_cbxCanStdoutEnable->setChecked(true);
  m_btnCanTraceClear = new QPushButton("Clear");
  hbox2->addWidget(m_cbxCanTraceEnable);
  hbox2->addWidget(m_cbxCanStdoutEnable);
  hbox2->addWidget(m_btnCanTraceClear);
  vbox->addLayout(hbox2);

  m_Tab = new QTabWidget(this);

  m_teCanTrace = new QTextEdit();
  m_teCanTrace->setReadOnly(true);
  m_Tab->addTab(m_teCanTrace, tr("trace"));

  auto qWdt = new QWidget();
  auto vbox2 = new QVBoxLayout();
  m_teCanStdout = new QTextEdit();
  m_teCanStdout->setReadOnly(true);
  vbox2->addWidget(m_teCanStdout);
  m_leCanStdin = new QLineEdit();
  vbox2->addWidget(m_leCanStdin);
  m_leCanStdin->setStyleSheet("background-color: rgb(36, 36, 36); color: rgb(12, 190, 255);");
  qWdt->setLayout(vbox2);
  m_Tab->addTab(qWdt, tr("stdout"));

  vbox->addWidget(m_Tab);
  setLayout(vbox);

  connect(m_btnOpen[0], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_0()));
  connect(m_btnOpen[1], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_1()));
  connect(m_btnOpen[2], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_2()));
  connect(m_btnOpen[3], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_3()));

  connect(m_btnSend, SIGNAL(clicked()), this, SLOT(on_btnSend_clicked()));
  connect(m_btnCanTraceClear, SIGNAL(clicked()), this, SLOT(on_btnCanTraceClear_clicked()));
  connect(m_cbxCanTraceEnable, SIGNAL(stateChanged(int)), this,
          SLOT(on_cbxCanTraceEnable_stateChanged(int)));
  connect(m_cbxCanStdoutEnable, SIGNAL(stateChanged(int)), this,
          SLOT(on_cbxCanStdoutEnable_stateChanged(int)));

  connect(m_leCanStdin, SIGNAL(returnPressed()), this, SLOT(on_leCanStdin_returnPressed()));
  Std_TimerStop(&m_Timer);

  m_canStdoutTimer = startTimer(1);
  m_traceTimer = startTimer(1);
  Std_TimerStart(&m_Timer);
}

void UICan::on_btnOpen(int id) {
  if (m_btnOpen[id]->text() == "Open") {
    const char *device = m_cmbxCanDevice[id]->currentText().toStdString().c_str();
    uint32_t port = std::stoi(m_cmbxCanPort[id]->currentText().toStdString());
    uint32_t baud = std::stoi(m_cmbxCanBaud[id]->currentText().toStdString());
    std::string msg = "open CAN device " + std::string(device) + " port " + std::to_string(port) +
                      " baudrate " + std::to_string(baud);
    int busid = can_open(device, port, baud);
    if (busid >= 0) {
      m_BusID[id] = busid;
      m_btnOpen[id]->setText("Close");
      QMessageBox::information(this, "Info", tr(msg.c_str()) + " OK");
    } else {
      QMessageBox::critical(this, "Error", tr(msg.c_str()) + " FAIL");
    }
  } else {
    m_btnOpen[id]->setText("Open");
    can_close(m_BusID[id]);
    m_BusID[id] = -1;
  }
}

void UICan::on_btnOpenClicked_0(void) {
  on_btnOpen(0);
}

void UICan::on_btnOpenClicked_1(void) {
  on_btnOpen(1);
}

void UICan::on_btnOpenClicked_2(void) {
  on_btnOpen(2);
}

void UICan::on_btnOpenClicked_3(void) {
  on_btnOpen(3);
}

void UICan::on_btnSend_clicked(void) {
  onCanSend();
  auto period = std::stoi(m_leCanPeriod->text().toStdString());
  if (period > 0) {
    if (m_canSendTimer != -1) {
      killTimer(m_canSendTimer);
    }
    m_canSendTimer = startTimer(period);
  }
}

void UICan::on_btnCanTraceClear_clicked(void) {
  m_teCanTrace->clear();
  m_teCanStdout->clear();
}

void UICan::on_cbxCanStdoutEnable_stateChanged(int state) {
  if (state) {
    LOG(INFO, "CAN: stdout enabled\n");
    m_teCanStdout->clear();
    m_canStdoutTimer = startTimer(1);
  } else {
    killTimer(m_canStdoutTimer);
    m_canStdoutTimer = -1;
    LOG(INFO, "CAN: stdout disabled\n");
  }
}

void UICan::on_cbxCanTraceEnable_stateChanged(int state) {
  if (state) {
    LOG(INFO, "CAN: trace enabled\n");
    m_traceTimer = startTimer(1);
    if (false == Std_IsTimerStarted(&m_Timer)) {
      Std_TimerStart(&m_Timer);
    }
  } else {
    LOG(INFO, "CAN: trace disabled\n");
    killTimer(m_traceTimer);
  }
}

void UICan::on_leCanStdin_returnPressed(void) {
  int id = m_cmbxBusID->currentIndex();
  if (m_BusID[id] >= 0) {
    auto ds = m_leCanStdin->text().toStdString() + "\n";
    for (size_t i = 0; i < ds.size(); i += 8) {
      uint8_t dlc = ds.size() - i;
      if (dlc > 8) {
        dlc = 8;
      }
      can_write(m_BusID[id], 0x7FE, dlc, (uint8_t *)&(ds.data()[i]));
    }
    m_leCanStdin->clear();
  }
}

UICan::~UICan() {
}

void UICan::onCanSend(void) {
  int id = m_cmbxBusID->currentIndex();
  if (m_BusID[id] >= 0) {
    uint32_t canid = strtoul(m_leCanID->text().toStdString().c_str(), nullptr, 16);
    auto ds = m_leCanData->text().toStdString();
    std::vector<uint8_t> data;
    uint8_t dlc = ds.size() / 2;
    data.reserve(dlc);
    char ss[3] = {0, 0, 0};
    for (int i = 0; i < dlc; i++) {
      ss[0] = ds.c_str()[2 * i];
      ss[0] = ds.c_str()[2 * i + 1];
      data.push_back(strtoul(ss, nullptr, 16));
    }
    can_write(m_BusID[id], canid, dlc, data.data());
  }
}

void UICan::onCanRead(void) {
  for (int i = 0; i < MAX_BUS; i++) {
    if (m_BusID[i] >= 0) {
      bool r = true;
      while (r) {
        uint32_t canid = (uint32_t)-2;
        uint8_t data[64];
        uint8_t dlc = sizeof(data);
        r = can_read(m_BusID[i], &canid, &dlc, data);
        if (r) {
          std::stringstream ss;
          double elapsed = (double)Std_GetTimerElapsedTime(&m_Timer) / 1000000.0;
          ss << "bus=" << m_BusID[i];
          ss << " canid=" << std::setw(3) << std::setfill('0') << std::hex
             << (canid & (~CAN_ID_EXTENDED));
          ss << " data=[";
          for (int j = 0; j < dlc; j++) {
            ss << " " << std::setw(2) << std::setfill('0') << std::hex << (int)data[j];
          }
          ss << " ] @ " << std::setprecision(4) << elapsed << "s";
          m_teCanTrace->append(tr(ss.str().c_str()));
        }
      }
    }
  }
}

void UICan::onCanStdoutRead(void) {
  int busId = m_cmbxBusID->currentIndex();
  if (m_BusID[busId] >= 0) {
    bool r = true;
    while (r) {
      uint32_t canid = (uint32_t)0x7FF;
      uint8_t data[64];
      uint8_t dlc = sizeof(data);
      r = can_read(m_BusID[busId], &canid, &dlc, data);
      if (r) {
        std::stringstream ss;
        for (int i = 0; i < dlc; i++) {
          ss << (char)data[i];
        }
        m_teCanStdout->insertPlainText(tr(ss.str().c_str()));
        m_teCanStdout->moveCursor(QTextCursor::End);
      }
    }
  }
}

void UICan::timerEvent(QTimerEvent *event) {
  if (event->timerId() == m_traceTimer) {
    onCanRead();
  } else if (event->timerId() == m_canSendTimer) {
    onCanSend();
    auto period = std::stoi(m_leCanPeriod->text().toStdString());
    if (period == 0) {
      killTimer(m_canSendTimer);
      m_canSendTimer = -1;
    }
  } else if (event->timerId() == m_canStdoutTimer) {
    onCanStdoutRead();
  }
}

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  QMainWindow w;
  w.setCentralWidget(new UICan());
  w.show();
  return a.exec();
}

extern "C" std::string name(void) {
  return "CAN";
}

extern "C" QWidget *create(void) {
  return new UICan();
}
