/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef UIFBL_HPP
#define UIFBL_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTimerEvent>
#include <memory>
#include "loader.h"
#include "srec.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class Loader {
public:
  struct Config {
    IsoTp_ParameterType IsotpParam;
    std::string AppSrec;
    std::string FlsDrvSrec;
    std::string Choice;
    Srec_SignType SignType = SREC_SIGN_CRC16;
    bool Debug = false;
    uint32_t FuncAddr = 0;
  };

public:
  Loader(Config &config);
  ~Loader();
  void Start();
  void Stop();
  bool Poll(float &progress, char *&msg);

private:
  Srec_FileType *m_AppSrec = nullptr;
  Srec_FileType *m_FlsSrec = nullptr;
  IsoTp_Type *m_IsoTp = nullptr;
  Loader_Type *m_Loader = nullptr;
  std::string m_Choice = "FBL";
  int m_LogLevel = L_LOG_INFO;
  Srec_SignType m_SignType = SREC_SIGN_CRC16;
  uint32_t m_FuncAddr = 0;
};

class UIFBL : public QWidget {
  Q_OBJECT
public:
  UIFBL();
  ~UIFBL();

public slots:
  void on_btnOpenApp_clicked(void);
  void on_btnOpenFlsDrv_clicked(void);
  void on_btnStart_clicked(void);

private:
  void timerEvent(QTimerEvent *event);

private:
  QTextEdit *m_TeInfo;
  QComboBox *m_CmbxProtocol;
  QComboBox *m_CmbxDevice;
  QComboBox *m_CmbxChoice;
  QComboBox *m_CmbxSignType;
  QComboBox *m_CmbxDevicePort;
  QComboBox *m_CmbxDeviceBaudrate;
  QComboBox *m_CmbxTxId;
  QComboBox *m_CmbxRxId;
  QComboBox *m_CmbxFuncAddr;
  QComboBox *m_CmbxLinkLayerDataLength;
  QComboBox *m_CmbxN_TA;
  QLineEdit *m_LeApplication;
  QLineEdit *m_LeFlsDrv;
  QCheckBox *m_CbxDebug;
  QProgressBar *m_PgbProgress;
  QPushButton *m_BtnOpenApp;
  QPushButton *m_BtnOpenFlsDrv;
  QPushButton *m_BtnStart;

  Loader *m_Loader = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* UIFBL_HPP */
