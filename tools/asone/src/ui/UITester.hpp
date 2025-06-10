/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef __UI_TESTER_HPP__
#define __UI_TESTER_HPP__
/* ================================ [ INCLUDES  ] ============================================== */
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTimerEvent>
#include <QProgressBar>

#include "XLTester.hpp"

using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class UITester : public QWidget {
  Q_OBJECT
public:
  UITester();
  ~UITester();

public slots:
  void on_btnOpenExcel_clicked(void);
  void on_btnStart_clicked(void);

private:
  void timerEvent(QTimerEvent *event);
  std::string GetCellValue(XLWorksheet &sheet, int row, int col);

private:
  QTextEdit *m_teInfo;
  QLineEdit *m_leExcel;
  QPushButton *m_btnOpenExcel;
  QPushButton *m_btnStart;
  QProgressBar *m_pgbProgress;

  XLTester m_XLTester;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __UI_TESTER_HPP__ */
