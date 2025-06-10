/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef __UICAN_HPP__
#define __UICAN_HPP__
/* ================================ [ INCLUDES  ] ============================================== */
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QTimerEvent>
#include <vector>
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class UICan : public QWidget {
  Q_OBJECT
public:
  UICan();
  ~UICan();

public slots:
  void on_btnOpenClicked_0(void);
  void on_btnOpenClicked_1(void);
  void on_btnOpenClicked_2(void);
  void on_btnOpenClicked_3(void);
  void on_btnSend_clicked(void);
  void on_btnCanTraceClear_clicked(void);
  void on_cbxCanTraceEnable_stateChanged(int state);
  void on_cbxCanStdoutEnable_stateChanged(int state);
  void on_leCanStdin_returnPressed(void);

private:
  void timerEvent(QTimerEvent *event);
  void on_btnOpen(int id);
  void onCanSend(void);
  void onCanRead(void);
  void onCanStdoutRead(void);

private:
  static constexpr int MAX_BUS = 4;
  int m_BusID[MAX_BUS];
  std::vector<QComboBox *> m_cmbxCanDevice;
  std::vector<QComboBox *> m_cmbxCanPort;
  std::vector<QComboBox *> m_cmbxCanBaud;
  std::vector<QPushButton *> m_btnOpen;
  QComboBox *m_cmbxBusID;
  QLineEdit *m_leCanID;
  QLineEdit *m_leCanData;
  QLineEdit *m_leCanPeriod;
  QPushButton *m_btnSend;
  QTabWidget *m_Tab;
  QTextEdit *m_teCanTrace;
  QCheckBox *m_cbxCanTraceEnable;
  QPushButton *m_btnCanTraceClear;
  QTextEdit *m_teCanStdout;
  QLineEdit *m_leCanStdin;
  QCheckBox *m_cbxCanStdoutEnable;

  int m_traceTimer = -1;
  int m_canSendTimer = -1;
  int m_canStdoutTimer = -1;

  Std_TimerType m_Timer;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __UICAN_HPP__ */
