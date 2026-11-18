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
  m_IsoTp = IsoTp_Create(&config.IsotpParam);
  if (nullptr == m_IsoTp) {
    throw std::runtime_error(std::string("failed to create isotp with device ") +
                             config.IsotpParam.Device + " port " +
                             std::to_string(config.IsotpParam.Port));
  }

  m_AppSrec = Srec_Open(config.AppSrec.c_str());
  if (nullptr == m_AppSrec) {
    IsoTp_Destroy(m_IsoTp);
    throw std::runtime_error("failed to open app image " + config.AppSrec);
  }

  m_FlsSrec = Srec_Open(config.FlsDrvSrec.c_str());
  if (nullptr == m_FlsSrec) {
    IsoTp_Destroy(m_IsoTp);
    Srec_Close(m_AppSrec);
    throw std::runtime_error("failed to open flash driver image " + config.FlsDrvSrec);
  }

  if (config.Debug) {
    m_LogLevel = L_LOG_DEBUG;
  }

  m_Choice = config.Choice;
  m_SignType = config.SignType;
  m_FuncAddr = config.FuncAddr;
}

Loader::~Loader() {
  if (nullptr != m_IsoTp) {
    IsoTp_Destroy(m_IsoTp);
  }
  if (nullptr != m_AppSrec) {
    Srec_Close(m_AppSrec);
  }
  if (nullptr != m_FlsSrec) {
    Srec_Close(m_FlsSrec);
  }
  if (nullptr != m_Loader) {
    Loader_Destroy(m_Loader);
  }
}

void Loader::Start() {
  Loader_ArgsType args;
  args.AppSRec = m_AppSrec;
  args.FlsSRec = m_FlsSrec;
  args.Isotp = m_IsoTp;
  args.Choice = m_Choice.c_str();
  args.SignType = m_SignType;
  args.FuncAddr = m_FuncAddr;
  m_Loader = Loader_Create(&args);
  if (nullptr == m_Loader) {
    throw std::runtime_error("failed to start loader");
  } else {
    Loader_SetLogLevel(m_Loader, m_LogLevel);
  }
}

void Loader::Stop() {
  if (nullptr != m_Loader) {
    Loader_Destroy(m_Loader);
    m_Loader = nullptr;
  }
}

bool Loader::Poll(float &progress, char *&msg) {
  if (nullptr == m_Loader) {
    return false;
  }

  int progressI = 0;
  auto r = Loader_Poll(m_Loader, &progressI, &msg);
  progress = progressI / 100.f;
  return (0 == r);
}

UIFBL::UIFBL() : QWidget() {
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("protocol:"), 0, 0);
  m_CmbxProtocol = new QComboBox();
  m_CmbxProtocol->addItems({"CAN", "LIN"});
  m_CmbxProtocol->setEditable(true);
  grid->addWidget(m_CmbxProtocol, 0, 1);
  grid->addWidget(new QLabel("device:"), 0, 2);
  m_CmbxDevice = new QComboBox();
  m_CmbxDevice->addItems({"simulator_v2", "qemu", "peak", "vxl", "zlg", "simulator"});
  m_CmbxDevice->setEditable(true);
  grid->addWidget(m_CmbxDevice, 0, 3);
  grid->addWidget(new QLabel("port:"), 0, 4);
  m_CmbxDevicePort = new QComboBox();
  m_CmbxDevicePort->addItems({"0", "1", "2", "3"});
  m_CmbxDevicePort->setEditable(true);
  grid->addWidget(m_CmbxDevicePort, 0, 5);
  grid->addWidget(new QLabel("baurdare:"), 0, 6);
  m_CmbxDeviceBaudrate = new QComboBox();
  m_CmbxDeviceBaudrate->addItems({"125000", "250000", "500000", "1000000", "115200"});
  m_CmbxDeviceBaudrate->setCurrentIndex(2);
  m_CmbxDeviceBaudrate->setEditable(true);
  grid->addWidget(m_CmbxDeviceBaudrate, 0, 7);
  grid->addWidget(new QLabel("TxId:"), 1, 0);
  m_CmbxTxId = new QComboBox();
  m_CmbxTxId->addItems({"0x731", "0x3c"});
  m_CmbxTxId->setEditable(true);
  grid->addWidget(m_CmbxTxId, 1, 1);
  grid->addWidget(new QLabel("RxId:"), 1, 2);
  m_CmbxRxId = new QComboBox();
  m_CmbxRxId->addItems({"0x732", "0x3d"});
  m_CmbxRxId->setEditable(true);
  grid->addWidget(m_CmbxRxId, 1, 3);
  grid->addWidget(new QLabel("FuncAddr:"), 1, 4);
  m_CmbxFuncAddr = new QComboBox();
  m_CmbxFuncAddr->addItems({"0x7DF"});
  m_CmbxFuncAddr->setEditable(true);
  grid->addWidget(m_CmbxFuncAddr, 1, 5);
  m_CbxDebug = new QCheckBox("debug");
  grid->addWidget(m_CbxDebug, 1, 6);

  grid->addWidget(new QLabel("sign type:"), 2, 0);
  m_CmbxSignType = new QComboBox();
  m_CmbxSignType->addItems({"CRC16", "CRC32"});
  grid->addWidget(m_CmbxSignType, 2, 1);
  grid->addWidget(new QLabel("choice:"), 2, 2);
  m_CmbxChoice = new QComboBox();
  m_CmbxChoice->addItems({"FBL", "OTA"});
  m_CmbxChoice->setEditable(true);
  grid->addWidget(m_CmbxChoice, 2, 3);

  m_CmbxLinkLayerDataLength = new QComboBox();
  m_CmbxLinkLayerDataLength->addItems({"8", "64"});
  m_CmbxLinkLayerDataLength->setEditable(true);
  grid->addWidget(new QLabel("LinkLayerDataLength:"), 2, 4);
  grid->addWidget(m_CmbxLinkLayerDataLength, 2, 5);

  m_CmbxN_TA = new QComboBox();
  m_CmbxN_TA->addItems({"0xFFFF", "0x0000"});
  m_CmbxN_TA->setEditable(true);
  grid->addWidget(new QLabel("N_TA:"), 2, 6);
  grid->addWidget(m_CmbxN_TA, 2, 7);

  vbox->addLayout(grid);
  grid = new QGridLayout();
  grid->addWidget(new QLabel("Application"), 0, 0);
  m_LeApplication = new QLineEdit();
  grid->addWidget(m_LeApplication, 0, 1);
  m_BtnOpenApp = new QPushButton("...");
  grid->addWidget(m_BtnOpenApp, 0, 2);

  grid->addWidget(new QLabel("Flash Driver"), 1, 0);
  m_LeFlsDrv = new QLineEdit();
  grid->addWidget(m_LeFlsDrv, 1, 1);
  m_BtnOpenFlsDrv = new QPushButton("...");
  grid->addWidget(m_BtnOpenFlsDrv, 1, 2);

  grid->addWidget(new QLabel("Progress"), 2, 0);
  m_PgbProgress = new QProgressBar();
  m_PgbProgress->setRange(0, 100);
  grid->addWidget(m_PgbProgress, 2, 1);
  m_BtnStart = new QPushButton("Start");
  grid->addWidget(m_BtnStart, 2, 2);
  vbox->addLayout(grid);
  m_TeInfo = new QTextEdit();
  m_TeInfo->setReadOnly(true);
  vbox->addWidget(m_TeInfo);
  setLayout(vbox);

  std::string appSrc = "program/app.s19";
  std::string flsDrvSrc = "program/flsdrv.s19";

  if (0 == access(appSrc.c_str(), F_OK | R_OK)) {
    m_LeApplication->setText(tr(appSrc.c_str()));
  }

  if (0 == access(flsDrvSrc.c_str(), F_OK | R_OK)) {
    m_LeFlsDrv->setText(tr(flsDrvSrc.c_str()));
  }

  appSrc = "../../QemuVersatilepbGCC/VersatilepbCanApp/VersatilepbCanApp.s19.sign";
  flsDrvSrc = "../../QemuVersatilepbGCC/VersatilepbFlashDriver/VersatilepbFlashDriver.s19.sign";

  if (0 == access(appSrc.c_str(), F_OK | R_OK)) {
    m_LeApplication->setText(tr(appSrc.c_str()));
  }

  if (0 == access(flsDrvSrc.c_str(), F_OK | R_OK)) {
    m_LeFlsDrv->setText(tr(flsDrvSrc.c_str()));
  }

  connect(m_BtnOpenApp, SIGNAL(clicked()), this, SLOT(on_btnOpenApp_clicked()));
  connect(m_BtnOpenFlsDrv, SIGNAL(clicked()), this, SLOT(on_btnOpenFlsDrv_clicked()));
  connect(m_BtnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
}

UIFBL::~UIFBL() {
}

void UIFBL::on_btnOpenApp_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "application file", "",
                                               "application (*.s19 *.s19.sign *.bin *.mot)");
  m_LeApplication->setText(fileName);
}

void UIFBL::on_btnOpenFlsDrv_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "flash driver file", "",
                                               "flash driver (*.s19 *.s19.sign *.bin *.mot)");
  m_LeFlsDrv->setText(fileName);
}

void UIFBL::timerEvent(QTimerEvent *event) {
  if (nullptr != m_Loader) {
    float progress = 0;
    char *msg = nullptr;
    auto r = m_Loader->Poll(progress, msg);
    if (NULL != msg) {
      m_TeInfo->insertPlainText(tr(msg));
      m_TeInfo->moveCursor(QTextCursor::End);
      free(msg);
    }
    m_PgbProgress->setValue(progress);
    if (false == r) {
      delete m_Loader;
      m_Loader = nullptr;
      m_TeInfo->append(">>> FAIL <<< ");
      m_BtnStart->setDisabled(false);
      killTimer(event->timerId());
    }

    if (progress >= 100.0) {
      delete m_Loader;
      m_Loader = nullptr;
      m_TeInfo->append(">>> SUCCESS <<< ");
      m_BtnStart->setDisabled(false);
      killTimer(event->timerId());
    }
  } else {
    killTimer(event->timerId());
  }
}

void UIFBL::on_btnStart_clicked(void) {
  Loader::Config config;
  strcpy(config.IsotpParam.Device, m_CmbxDevice->currentText().toStdString().c_str());
  config.IsotpParam.Port = std::stoi(m_CmbxDevicePort->currentText().toStdString());
  config.IsotpParam.Baudrate = std::stoi(m_CmbxDeviceBaudrate->currentText().toStdString());
  config.IsotpParam.LinkLayerDataLength =
    std::stoi(m_CmbxLinkLayerDataLength->currentText().toStdString(), NULL, 10);
  config.IsotpParam.N_TA = std::stoi(m_CmbxN_TA->currentText().toStdString(), NULL, 16);
  auto protocol = m_CmbxProtocol->currentText().toStdString();
  if ((protocol == "CANFD") || (protocol == "CAN")) {
    config.IsotpParam.Protocol = ISOTP_OVER_CAN;
    config.IsotpParam.U.CAN.RxCanId =
      std::strtoul(m_CmbxRxId->currentText().toStdString().c_str(), NULL, 16);
    config.IsotpParam.U.CAN.TxCanId =
      std::strtoul(m_CmbxTxId->currentText().toStdString().c_str(), NULL, 16);
    config.IsotpParam.U.CAN.BlockSize = 8;
    config.IsotpParam.U.CAN.STmin = 0;
    config.IsotpParam.U.CAN.Version = ISOTP_CAN_V2;
    config.FuncAddr = std::strtoul(m_CmbxFuncAddr->currentText().toStdString().c_str(), NULL, 16);
    LOG(INFO,
        "loader over CAN device %s:%d, baudrate %u, LinkLayerDataLength %d, ID 0x%x:0x%x, FuncAddr "
        "0x%x\n",
        config.IsotpParam.Device, config.IsotpParam.Port, config.IsotpParam.Baudrate,
        config.IsotpParam.LinkLayerDataLength, config.IsotpParam.U.CAN.TxCanId,
        config.IsotpParam.U.CAN.RxCanId, config.FuncAddr);
  } else if (protocol == "LIN") {
    config.IsotpParam.Protocol = ISOTP_OVER_LIN;
    config.IsotpParam.U.LIN.RxId = std::stoi(m_CmbxRxId->currentText().toStdString(), NULL, 16);
    config.IsotpParam.U.LIN.TxId = std::stoi(m_CmbxTxId->currentText().toStdString(), NULL, 16);
    config.IsotpParam.U.LIN.Timeout = 100;
    config.IsotpParam.U.LIN.DelayUs = 20000;
    LOG(INFO, "loader over LIN device %s:%d, baudrate %u, LinkLayerDataLength %d, ID 0x%x:0x%x\n",
        config.IsotpParam.Device, config.IsotpParam.Port, config.IsotpParam.Baudrate,
        config.IsotpParam.LinkLayerDataLength, config.IsotpParam.U.LIN.TxId,
        config.IsotpParam.U.LIN.RxId);
  } else {
    std::string msg = "invalid protocol: " + protocol;
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  }

  if (m_CbxDebug->isChecked()) {
    config.Debug = true;
  } else {
    config.Debug = false;
  }

  config.AppSrec = m_LeApplication->text().toStdString();
  config.FlsDrvSrec = m_LeFlsDrv->text().toStdString();
  config.Choice = m_CmbxChoice->currentText().toStdString();
  config.SignType = (Srec_SignType)m_CmbxSignType->currentIndex();
  try {
    m_TeInfo->clear();
    m_Loader = new Loader(config);
    m_Loader->Start();
    m_BtnStart->setDisabled(true);
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

extern "C" std::string Name(void) {
  return "FBL";
}

extern "C" QWidget *Create(void) {
  return new UIFBL();
}
