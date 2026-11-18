/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail->com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
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
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
UICan::UICan() : QWidget() {
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  for (int i = 0; i < MAX_BUS; i++) {
    m_CmbxCanDevice.push_back(new QComboBox());
    m_CmbxCanPort.push_back(new QComboBox());
    m_CmbxCanBaud.push_back(new QComboBox());
    m_BtnOpen.push_back(new QPushButton("Open"));
    m_CmbxCanDevice[i]->addItems({"simulator_v2", "qemu", "peak", "vxl", "zlg"});
    m_CmbxCanPort[i]->addItems({"0", "1", "2", "3", "4", "5", "6", "7"});
    m_CmbxCanPort[i]->setStatusTip("Port");
    m_CmbxCanBaud[i]->addItems({"125000", "250000", "500000", "1000000"});
    m_CmbxCanBaud[i]->setStatusTip("Baudrate");
    m_CmbxCanBaud[i]->setCurrentIndex(2);
    m_CmbxCanDevice[i]->setEditable(true);
    m_CmbxCanPort[i]->setEditable(true);
    m_CmbxCanBaud[i]->setEditable(true);

    std::string ln = "CAN" + std::to_string(i) + ":";
    grid->addWidget(new QLabel(tr(ln.c_str())), i, 0);
    grid->addWidget(m_CmbxCanDevice[i], i, 1);
    grid->addWidget(m_CmbxCanPort[i], i, 2);
    grid->addWidget(m_CmbxCanBaud[i], i, 3);
    grid->addWidget(m_BtnOpen[i], i, 4);
    m_BusID[i] = -1;
  }
  vbox->addLayout(grid);

  auto hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("BUS ID:"));
  m_CmbxBusID = new QComboBox();
  m_CmbxBusID->addItems({"CAN0", "CAN1", "CAN2", "CAN3"});
  hbox->addWidget(m_CmbxBusID);
  hbox->addWidget(new QLabel("CAN ID:"));
  m_LeCanID = new QLineEdit();
  m_LeCanID->setMaximumWidth(120);
  m_LeCanID->setText("0");
  m_LeCanID->setStatusTip("hex value");
  hbox->addWidget(m_LeCanID);
  hbox->addWidget(new QLabel("DATA:"));
  m_LeCanData = new QLineEdit();
  m_LeCanData->setText("1122334455667788");
  hbox->addWidget(m_LeCanData);
  hbox->addWidget(new QLabel("PERIOD:"));
  m_LeCanPeriod = new QLineEdit();
  m_LeCanPeriod->setText("0");
  hbox->addWidget(m_LeCanPeriod);
  m_LeCanPeriod->setMaximumWidth(120);
  m_BtnSend = new QPushButton("Send");
  hbox->addWidget(m_BtnSend);
  vbox->addLayout(hbox);

  auto hbox2 = new QHBoxLayout();
  m_CbxCanTraceEnable = new QCheckBox("CAN trace log enable");
  m_CbxCanTraceEnable->setChecked(true);
  m_CbxCanStdoutEnable = new QCheckBox("CAN stdout enable");
  m_CbxCanStdoutEnable->setChecked(true);
  m_BtnCanTraceClear = new QPushButton("Clear");
  hbox2->addWidget(m_CbxCanTraceEnable);
  hbox2->addWidget(m_CbxCanStdoutEnable);
  hbox2->addWidget(m_BtnCanTraceClear);
  vbox->addLayout(hbox2);

  m_Tab = new QTabWidget(this);

  m_TeCanTrace = new QTextEdit();
  m_TeCanTrace->setReadOnly(true);
  m_Tab->addTab(m_TeCanTrace, tr("trace"));

  auto qWdt = new QWidget();
  auto vbox2 = new QVBoxLayout();
  m_TeCanStdout = new QTextEdit();
  m_TeCanStdout->setReadOnly(true);
  vbox2->addWidget(m_TeCanStdout);
  m_LeCanStdin = new QLineEdit();
  vbox2->addWidget(m_LeCanStdin);
  m_LeCanStdin->setStyleSheet("background-color: rgb(36, 36, 36); color: rgb(12, 190, 255);");
  qWdt->setLayout(vbox2);
  m_Tab->addTab(qWdt, tr("stdout"));

  vbox->addWidget(m_Tab);
  setLayout(vbox);

  connect(m_BtnOpen[0], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_0()));
  connect(m_BtnOpen[1], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_1()));
  connect(m_BtnOpen[2], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_2()));
  connect(m_BtnOpen[3], SIGNAL(clicked()), this, SLOT(on_btnOpenClicked_3()));

  connect(m_BtnSend, SIGNAL(clicked()), this, SLOT(on_btnSend_clicked()));
  connect(m_BtnCanTraceClear, SIGNAL(clicked()), this, SLOT(on_btnCanTraceClear_clicked()));
  connect(m_CbxCanTraceEnable, SIGNAL(stateChanged(int)), this,
          SLOT(on_cbxCanTraceEnable_stateChanged(int)));
  connect(m_CbxCanStdoutEnable, SIGNAL(stateChanged(int)), this,
          SLOT(on_cbxCanStdoutEnable_stateChanged(int)));

  connect(m_LeCanStdin, SIGNAL(returnPressed()), this, SLOT(on_leCanStdin_returnPressed()));
  Std_TimerStop(&m_Timer);

  m_CanStdoutTimer = startTimer(1);
  m_TraceTimer = startTimer(1);
  Std_TimerStart(&m_Timer);
}

void UICan::OnBtnOpen(int id) {
  if (m_BtnOpen[id]->text() == "Open") {
    const char *device = m_CmbxCanDevice[id]->currentText().toStdString().c_str();
    uint32_t port = std::stoi(m_CmbxCanPort[id]->currentText().toStdString());
    uint32_t baud = std::stoi(m_CmbxCanBaud[id]->currentText().toStdString());
    std::string msg = "open CAN device " + std::string(device) + " port " + std::to_string(port) +
                      " baudrate " + std::to_string(baud);
    int busid = CanL_Open(device, port, baud);
    if (busid >= 0) {
      m_BusID[id] = busid;
      m_BtnOpen[id]->setText("Close");
      QMessageBox::information(this, "Info", tr(msg.c_str()) + " OK");
    } else {
      QMessageBox::critical(this, "Error", tr(msg.c_str()) + " FAIL");
    }
  } else {
    m_BtnOpen[id]->setText("Open");
    CanL_Close(m_BusID[id]);
    m_BusID[id] = -1;
  }
}

void UICan::on_btnOpenClicked_0(void) {
  OnBtnOpen(0);
}

void UICan::on_btnOpenClicked_1(void) {
  OnBtnOpen(1);
}

void UICan::on_btnOpenClicked_2(void) {
  OnBtnOpen(2);
}

void UICan::on_btnOpenClicked_3(void) {
  OnBtnOpen(3);
}

void UICan::on_btnSend_clicked(void) {
  OnCanSend();
  auto period = std::stoi(m_LeCanPeriod->text().toStdString());
  if (period > 0) {
    if (m_CanSendTimer != -1) {
      killTimer(m_CanSendTimer);
    }
    m_CanSendTimer = startTimer(period);
  }
}

void UICan::on_btnCanTraceClear_clicked(void) {
  m_TeCanTrace->clear();
  m_TeCanStdout->clear();
}

void UICan::on_cbxCanStdoutEnable_stateChanged(int state) {
  if (state) {
    LOG(INFO, "CAN: stdout enabled\n");
    m_TeCanStdout->clear();
    m_CanStdoutTimer = startTimer(1);
  } else {
    killTimer(m_CanStdoutTimer);
    m_CanStdoutTimer = -1;
    LOG(INFO, "CAN: stdout disabled\n");
  }
}

void UICan::on_cbxCanTraceEnable_stateChanged(int state) {
  if (state) {
    LOG(INFO, "CAN: trace enabled\n");
    m_TraceTimer = startTimer(1);
    if (false == Std_IsTimerStarted(&m_Timer)) {
      Std_TimerStart(&m_Timer);
    }
  } else {
    LOG(INFO, "CAN: trace disabled\n");
    killTimer(m_TraceTimer);
  }
}

void UICan::on_leCanStdin_returnPressed(void) {
  int id = m_CmbxBusID->currentIndex();
  if (m_BusID[id] >= 0) {
    auto ds = m_LeCanStdin->text().toStdString() + "\n";
    for (size_t i = 0; i < ds.size(); i += 8) {
      uint8_t dlc = ds.size() - i;
      if (dlc > 8) {
        dlc = 8;
      }
      CanL_Write(m_BusID[id], 0x7FE, dlc, (uint8_t *)&(ds.data()[i]));
    }
    m_LeCanStdin->clear();
  }
}

UICan::~UICan() {
}

void UICan::OnCanSend(void) {
  int id = m_CmbxBusID->currentIndex();
  if (m_BusID[id] >= 0) {
    uint32_t canid = strtoul(m_LeCanID->text().toStdString().c_str(), nullptr, 16);
    auto ds = m_LeCanData->text().toStdString();
    std::vector<uint8_t> data;
    uint8_t dlc = ds.size() / 2;
    data.reserve(dlc);
    char ss[3] = {0, 0, 0};
    for (int i = 0; i < dlc; i++) {
      ss[0] = ds.c_str()[2 * i];
      ss[0] = ds.c_str()[2 * i + 1];
      data.push_back(strtoul(ss, nullptr, 16));
    }
    CanL_Write(m_BusID[id], canid, dlc, data.data());
  }
}

void UICan::OnCanRead(void) {
  for (int i = 0; i < MAX_BUS; i++) {
    if (m_BusID[i] >= 0) {
      bool r = true;
      while (r) {
        uint32_t canid = (uint32_t)-2;
        uint8_t data[64];
        uint8_t dlc = sizeof(data);
        r = CanL_Read(m_BusID[i], &canid, &dlc, data);
        if (r) {
          std::stringstream ss;
          double elapsed = (double)Std_GetTimerElapsedTime(&m_Timer) / 1000000.0;
          ss << "bus=" << m_BusID[i];
          ss << " canid=" << std::setw(3) << std::setfill('0') << std::hex
             << (canid & (~CANL_ID_EXTENDED));
          ss << " data=[";
          for (int j = 0; j < dlc; j++) {
            ss << " " << std::setw(2) << std::setfill('0') << std::hex << (int)data[j];
          }
          ss << " ] @ " << std::setprecision(4) << elapsed << "s";
          m_TeCanTrace->append(tr(ss.str().c_str()));
        }
      }
    }
  }
}

void UICan::OnCanStdoutRead(void) {
  int busId = m_CmbxBusID->currentIndex();
  if (m_BusID[busId] >= 0) {
    bool r = true;
    while (r) {
      uint32_t canid = (uint32_t)0x7FF;
      uint8_t data[64];
      uint8_t dlc = sizeof(data);
      r = CanL_Read(m_BusID[busId], &canid, &dlc, data);
      if (r) {
        std::stringstream ss;
        for (int i = 0; i < dlc; i++) {
          ss << (char)data[i];
        }
        m_TeCanStdout->insertPlainText(tr(ss.str().c_str()));
        m_TeCanStdout->moveCursor(QTextCursor::End);
      }
    }
  }
}

void UICan::timerEvent(QTimerEvent *event) {
  if (event->timerId() == m_TraceTimer) {
    OnCanRead();
  } else if (event->timerId() == m_CanSendTimer) {
    OnCanSend();
    auto period = std::stoi(m_LeCanPeriod->text().toStdString());
    if (period == 0) {
      killTimer(m_CanSendTimer);
      m_CanSendTimer = -1;
    }
  } else if (event->timerId() == m_CanStdoutTimer) {
    OnCanStdoutRead();
  }
}

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  QMainWindow w;
  w.setCentralWidget(new UICan());
  w.show();
  return a.exec();
}

extern "C" std::string Name(void) {
  return "CAN";
}

extern "C" QWidget *Create(void) {
  return new UICan();
}
