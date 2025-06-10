/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef __UIFBL_HPP__
#define __UIFBL_HPP__
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
    isotp_parameter_t isotpParam;
    std::string appSrec;
    std::string flsDrvSrec;
    std::string choice;
    srec_sign_type_t signType = SREC_SIGN_CRC16;
    bool debug = false;
    uint32_t funcAddr = 0;
  };

public:
  Loader(Config &config);
  ~Loader();
  void start();
  void stop();
  bool poll(float &progress, char *&msg);

private:
  srec_t *m_AppSrec = nullptr;
  srec_t *m_FlsSrec = nullptr;
  isotp_t *m_IsoTp = nullptr;
  loader_t *m_Loader = nullptr;
  std::string m_Choice = "FBL";
  int m_LogLevel = L_LOG_INFO;
  srec_sign_type_t m_SignType = SREC_SIGN_CRC16;
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
  QTextEdit *m_teInfo;
  QComboBox *m_cmbxProtocol;
  QComboBox *m_cmbxDevice;
  QComboBox *m_cmbxChoice;
  QComboBox *m_cmbxSignType;
  QComboBox *m_cmbxDevicePort;
  QComboBox *m_cmbxDeviceBaudrate;
  QComboBox *m_cmbxTxId;
  QComboBox *m_cmbxRxId;
  QComboBox *m_cmbxFuncAddr;
  QComboBox *m_cmbxLL_DL;
  QComboBox *m_cmbxN_TA;
  QLineEdit *m_leApplication;
  QLineEdit *m_leFlsDrv;
  QCheckBox *m_cbxDebug;
  QProgressBar *m_pgbProgress;
  QPushButton *m_btnOpenApp;
  QPushButton *m_btnOpenFlsDrv;
  QPushButton *m_btnStart;

  Loader *m_Loader = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __UIFBL_HPP__ */
