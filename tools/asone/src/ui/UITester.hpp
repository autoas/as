/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef UITESTER_HPP
#define UITESTER_HPP
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
  QTextEdit *m_TeInfo;
  QLineEdit *m_LeExcel;
  QPushButton *m_BtnOpenExcel;
  QPushButton *m_BtnStart;
  QProgressBar *m_PgbProgress;

  XLTester m_XLTester;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* UITESTER_HPP */
