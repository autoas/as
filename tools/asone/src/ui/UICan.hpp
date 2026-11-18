/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef UICAN_HPP
#define UICAN_HPP
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
  void OnBtnOpen(int id);
  void OnCanSend(void);
  void OnCanRead(void);
  void OnCanStdoutRead(void);

private:
  static constexpr int MAX_BUS = 4;
  int m_BusID[MAX_BUS];
  std::vector<QComboBox *> m_CmbxCanDevice;
  std::vector<QComboBox *> m_CmbxCanPort;
  std::vector<QComboBox *> m_CmbxCanBaud;
  std::vector<QPushButton *> m_BtnOpen;
  QComboBox *m_CmbxBusID;
  QLineEdit *m_LeCanID;
  QLineEdit *m_LeCanData;
  QLineEdit *m_LeCanPeriod;
  QPushButton *m_BtnSend;
  QTabWidget *m_Tab;
  QTextEdit *m_TeCanTrace;
  QCheckBox *m_CbxCanTraceEnable;
  QPushButton *m_BtnCanTraceClear;
  QTextEdit *m_TeCanStdout;
  QLineEdit *m_LeCanStdin;
  QCheckBox *m_CbxCanStdoutEnable;

  int m_TraceTimer = -1;
  int m_CanSendTimer = -1;
  int m_CanStdoutTimer = -1;

  Std_TimerType m_Timer;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* UICAN_HPP */
