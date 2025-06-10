/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail->com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "UIFBL.hpp"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QMainwindow>
#include <QTextCursor>
#include "Log.hpp"
#include <unistd.h>
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Loader::Loader(Config &config) {
  m_IsoTp = isotp_create(&config.isotpParam);
  if (nullptr == m_IsoTp) {
    throw std::runtime_error(std::string("failed to create isotp with device ") +
                             config.isotpParam.device + " port " +
                             std::to_string(config.isotpParam.port));
  }

  m_AppSrec = srec_open(config.appSrec.c_str());
  if (nullptr == m_AppSrec) {
    isotp_destory(m_IsoTp);
    throw std::runtime_error("failed to open app image " + config.appSrec);
  }

  m_FlsSrec = srec_open(config.flsDrvSrec.c_str());
  if (nullptr == m_FlsSrec) {
    isotp_destory(m_IsoTp);
    srec_close(m_AppSrec);
    throw std::runtime_error("failed to open flash driver image " + config.flsDrvSrec);
  }

  if (config.debug) {
    m_LogLevel = L_LOG_DEBUG;
  }

  m_Choice = config.choice;
  m_SignType = config.signType;
  m_FuncAddr = config.funcAddr;
}

Loader::~Loader() {
  if (nullptr != m_IsoTp) {
    isotp_destory(m_IsoTp);
  }
  if (nullptr != m_AppSrec) {
    srec_close(m_AppSrec);
  }
  if (nullptr != m_FlsSrec) {
    srec_close(m_FlsSrec);
  }
  if (nullptr != m_Loader) {
    loader_destory(m_Loader);
  }
}

void Loader::start() {
  loader_args_t args;
  args.appSRec = m_AppSrec;
  args.flsSRec = m_FlsSrec;
  args.isotp = m_IsoTp;
  args.choice = m_Choice.c_str();
  args.signType = m_SignType;
  args.funcAddr = m_FuncAddr;
  m_Loader = loader_create(&args);
  if (nullptr == m_Loader) {
    throw std::runtime_error("failed to start loader");
  } else {
    loader_set_log_level(m_Loader, m_LogLevel);
  }
}

void Loader::stop() {
  if (nullptr != m_Loader) {
    loader_destory(m_Loader);
    m_Loader = nullptr;
  }
}

bool Loader::poll(float &progress, char *&msg) {
  if (nullptr == m_Loader) {
    return false;
  }

  int progressI = 0;
  auto r = loader_poll(m_Loader, &progressI, &msg);
  progress = progressI / 100.f;
  return (0 == r);
}

UIFBL::UIFBL() : QWidget() {
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("protocol:"), 0, 0);
  m_cmbxProtocol = new QComboBox();
  m_cmbxProtocol->addItems({"CAN", "LIN"});
  m_cmbxProtocol->setEditable(true);
  grid->addWidget(m_cmbxProtocol, 0, 1);
  grid->addWidget(new QLabel("device:"), 0, 2);
  m_cmbxDevice = new QComboBox();
  m_cmbxDevice->addItems({"simulator_v2", "qemu", "peak", "vxl", "zlg", "simulator"});
  m_cmbxDevice->setEditable(true);
  grid->addWidget(m_cmbxDevice, 0, 3);
  grid->addWidget(new QLabel("port:"), 0, 4);
  m_cmbxDevicePort = new QComboBox();
  m_cmbxDevicePort->addItems({"0", "1", "2", "3"});
  m_cmbxDevicePort->setEditable(true);
  grid->addWidget(m_cmbxDevicePort, 0, 5);
  grid->addWidget(new QLabel("baurdare:"), 0, 6);
  m_cmbxDeviceBaudrate = new QComboBox();
  m_cmbxDeviceBaudrate->addItems({"125000", "250000", "500000", "1000000", "115200"});
  m_cmbxDeviceBaudrate->setCurrentIndex(2);
  m_cmbxDeviceBaudrate->setEditable(true);
  grid->addWidget(m_cmbxDeviceBaudrate, 0, 7);
  grid->addWidget(new QLabel("TxId:"), 1, 0);
  m_cmbxTxId = new QComboBox();
  m_cmbxTxId->addItems({"0x731", "0x3c"});
  m_cmbxTxId->setEditable(true);
  grid->addWidget(m_cmbxTxId, 1, 1);
  grid->addWidget(new QLabel("RxId:"), 1, 2);
  m_cmbxRxId = new QComboBox();
  m_cmbxRxId->addItems({"0x732", "0x3d"});
  m_cmbxRxId->setEditable(true);
  grid->addWidget(m_cmbxRxId, 1, 3);
  grid->addWidget(new QLabel("FuncAddr:"), 1, 4);
  m_cmbxFuncAddr = new QComboBox();
  m_cmbxFuncAddr->addItems({"0x7DF"});
  m_cmbxFuncAddr->setEditable(true);
  grid->addWidget(m_cmbxFuncAddr, 1, 5);
  m_cbxDebug = new QCheckBox("debug");
  grid->addWidget(m_cbxDebug, 1, 6);

  grid->addWidget(new QLabel("sign type:"), 2, 0);
  m_cmbxSignType = new QComboBox();
  m_cmbxSignType->addItems({"CRC16", "CRC32"});
  grid->addWidget(m_cmbxSignType, 2, 1);
  grid->addWidget(new QLabel("choice:"), 2, 2);
  m_cmbxChoice = new QComboBox();
  m_cmbxChoice->addItems({"FBL", "OTA"});
  m_cmbxChoice->setEditable(true);
  grid->addWidget(m_cmbxChoice, 2, 3);

  m_cmbxLL_DL = new QComboBox();
  m_cmbxLL_DL->addItems({"8", "64"});
  m_cmbxLL_DL->setEditable(true);
  grid->addWidget(new QLabel("LL_DL:"), 2, 4);
  grid->addWidget(m_cmbxLL_DL, 2, 5);

  m_cmbxN_TA = new QComboBox();
  m_cmbxN_TA->addItems({"0xFFFF", "0x0000"});
  m_cmbxN_TA->setEditable(true);
  grid->addWidget(new QLabel("N_TA:"), 2, 6);
  grid->addWidget(m_cmbxN_TA, 2, 7);

  vbox->addLayout(grid);
  grid = new QGridLayout();
  grid->addWidget(new QLabel("Application"), 0, 0);
  m_leApplication = new QLineEdit();
  grid->addWidget(m_leApplication, 0, 1);
  m_btnOpenApp = new QPushButton("...");
  grid->addWidget(m_btnOpenApp, 0, 2);

  grid->addWidget(new QLabel("Flash Driver"), 1, 0);
  m_leFlsDrv = new QLineEdit();
  grid->addWidget(m_leFlsDrv, 1, 1);
  m_btnOpenFlsDrv = new QPushButton("...");
  grid->addWidget(m_btnOpenFlsDrv, 1, 2);

  grid->addWidget(new QLabel("Progress"), 2, 0);
  m_pgbProgress = new QProgressBar();
  m_pgbProgress->setRange(0, 100);
  grid->addWidget(m_pgbProgress, 2, 1);
  m_btnStart = new QPushButton("start");
  grid->addWidget(m_btnStart, 2, 2);
  vbox->addLayout(grid);
  m_teInfo = new QTextEdit();
  m_teInfo->setReadOnly(true);
  vbox->addWidget(m_teInfo);
  setLayout(vbox);

  std::string appSrc = "program/app.s19";
  std::string flsDrvSrc = "program/flsdrv.s19";

  if (0 == access(appSrc.c_str(), F_OK | R_OK)) {
    m_leApplication->setText(tr(appSrc.c_str()));
  }

  if (0 == access(flsDrvSrc.c_str(), F_OK | R_OK)) {
    m_leFlsDrv->setText(tr(flsDrvSrc.c_str()));
  }

  appSrc = "../../QemuVersatilepbGCC/VersatilepbCanApp/VersatilepbCanApp.s19.sign";
  flsDrvSrc = "../../QemuVersatilepbGCC/VersatilepbFlashDriver/VersatilepbFlashDriver.s19.sign";

  if (0 == access(appSrc.c_str(), F_OK | R_OK)) {
    m_leApplication->setText(tr(appSrc.c_str()));
  }

  if (0 == access(flsDrvSrc.c_str(), F_OK | R_OK)) {
    m_leFlsDrv->setText(tr(flsDrvSrc.c_str()));
  }

  connect(m_btnOpenApp, SIGNAL(clicked()), this, SLOT(on_btnOpenApp_clicked()));
  connect(m_btnOpenFlsDrv, SIGNAL(clicked()), this, SLOT(on_btnOpenFlsDrv_clicked()));
  connect(m_btnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
}

UIFBL::~UIFBL() {
}

void UIFBL::on_btnOpenApp_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "application file", "",
                                               "application (*.s19 *.s19.sign *.bin *.mot)");
  m_leApplication->setText(fileName);
}

void UIFBL::on_btnOpenFlsDrv_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "flash driver file", "",
                                               "flash driver (*.s19 *.s19.sign *.bin *.mot)");
  m_leFlsDrv->setText(fileName);
}

void UIFBL::timerEvent(QTimerEvent *event) {
  if (nullptr != m_Loader) {
    float progress = 0;
    char *msg = nullptr;
    auto r = m_Loader->poll(progress, msg);
    if (NULL != msg) {
      m_teInfo->insertPlainText(tr(msg));
      m_teInfo->moveCursor(QTextCursor::End);
      free(msg);
    }
    m_pgbProgress->setValue(progress);
    if (false == r) {
      delete m_Loader;
      m_Loader = nullptr;
      m_teInfo->append(">>> FAIL <<< ");
      m_btnStart->setDisabled(false);
      killTimer(event->timerId());
    }

    if (progress >= 100.0) {
      delete m_Loader;
      m_Loader = nullptr;
      m_teInfo->append(">>> SUCCESS <<< ");
      m_btnStart->setDisabled(false);
      killTimer(event->timerId());
    }
  } else {
    killTimer(event->timerId());
  }
}

void UIFBL::on_btnStart_clicked(void) {
  Loader::Config config;
  strcpy(config.isotpParam.device, m_cmbxDevice->currentText().toStdString().c_str());
  config.isotpParam.port = std::stoi(m_cmbxDevicePort->currentText().toStdString());
  config.isotpParam.baudrate = std::stoi(m_cmbxDeviceBaudrate->currentText().toStdString());
  config.isotpParam.ll_dl = std::stoi(m_cmbxLL_DL->currentText().toStdString(), NULL, 10);
  config.isotpParam.N_TA = std::stoi(m_cmbxN_TA->currentText().toStdString(), NULL, 16);
  auto protocol = m_cmbxProtocol->currentText().toStdString();
  if ((protocol == "CANFD") || (protocol == "CAN")) {
    config.isotpParam.protocol = ISOTP_OVER_CAN;
    config.isotpParam.U.CAN.RxCanId =
      std::strtoul(m_cmbxRxId->currentText().toStdString().c_str(), NULL, 16);
    config.isotpParam.U.CAN.TxCanId =
      std::strtoul(m_cmbxTxId->currentText().toStdString().c_str(), NULL, 16);
    config.isotpParam.U.CAN.BlockSize = 8;
    config.isotpParam.U.CAN.STmin = 0;
    config.funcAddr = std::strtoul(m_cmbxFuncAddr->currentText().toStdString().c_str(), NULL, 16);
    LOG(INFO, "loader over CAN device %s:%d, baudrate %u, LL_DL %d, ID 0x%x:0x%x, FuncAddr 0x%x\n",
        config.isotpParam.device, config.isotpParam.port, config.isotpParam.baudrate,
        config.isotpParam.ll_dl, config.isotpParam.U.CAN.TxCanId, config.isotpParam.U.CAN.RxCanId,
        config.funcAddr);
  } else if (protocol == "LIN") {
    config.isotpParam.protocol = ISOTP_OVER_LIN;
    config.isotpParam.U.LIN.RxId = std::stoi(m_cmbxRxId->currentText().toStdString(), NULL, 16);
    config.isotpParam.U.LIN.TxId = std::stoi(m_cmbxTxId->currentText().toStdString(), NULL, 16);
    config.isotpParam.U.LIN.timeout = 100;
    config.isotpParam.U.LIN.delayUs = 20000;
    LOG(INFO, "loader over LIN device %s:%d, baudrate %u, LL_DL %d, ID 0x%x:0x%x\n",
        config.isotpParam.device, config.isotpParam.port, config.isotpParam.baudrate,
        config.isotpParam.ll_dl, config.isotpParam.U.LIN.TxId, config.isotpParam.U.LIN.RxId);
  } else {
    std::string msg = "invalid protocol: " + protocol;
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  }

  if (m_cbxDebug->isChecked()) {
    config.debug = true;
  } else {
    config.debug = false;
  }

  config.appSrec = m_leApplication->text().toStdString();
  config.flsDrvSrec = m_leFlsDrv->text().toStdString();
  config.choice = m_cmbxChoice->currentText().toStdString();
  config.signType = (srec_sign_type_t)m_cmbxSignType->currentIndex();
  try {
    m_teInfo->clear();
    m_Loader = new Loader(config);
    m_Loader->start();
    m_btnStart->setDisabled(true);
    startTimer(1);
  } catch (std::exception &e) {
    QString text = "Exception: ";
    text += e.what();
    QMessageBox::critical(this, "Error", text);
    if (m_Loader) {
      delete m_Loader;
      m_Loader = nullptr;
    }
  }
}

extern "C" std::string name(void) {
  return "FBL";
}

extern "C" QWidget *create(void) {
  return new UIFBL();
}
