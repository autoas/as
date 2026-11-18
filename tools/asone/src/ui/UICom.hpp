/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef UICOM_HPP
#define UICOM_HPP
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
#include <QScrollArea>
#include <QTreeWidget>
#include <QSplitter>
/* Build and install QtCharts:
 *  git clone https://gitcode.net/mirrors/qt/qtcharts.git -b 6.4.2
 *  mkdir qtchart/build
 *  cd qtchart/build
 *  set QT_PATH=c:\Qt\6.4.2\mingw_64
 *  set CMAKE_PREFIX_PATH=%QT_PATH%\lib\cmake
 *  set PATH=c:\Qt\Tools\CMake_64\bin;%PATH%
 *  cmake -G "MSYS Makefiles" ..
 *  make install
 */
#include <QChartView>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <vector>
#include <memory>
#include <map>
#include "figure.hpp"
#include "signal.hpp"
#include "aslua.hpp"

using namespace as;
using namespace as::figure;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class UIMsg : public QScrollArea {
  Q_OBJECT
public:
  UIMsg(std::shared_ptr<Message> msg);
  ~UIMsg();

public:
  void Run(void);

private:
  void UpdateMsg(void);
  int ToInteger(std::string s);
  void View(std::shared_ptr<Signal> sig);

private:
  void timerEvent(QTimerEvent *event);

public slots:
  void on_btnUpdate_clicked(void);
  void on_btnLoadScript_clicked(void);
  void on_btnExecScript_clicked(void);
  void on_cmbxSignals_currentIndexChanged(int index);
  void on_btnView_clicked(void);

private:
  std::shared_ptr<Message> m_Msg;
  std::vector<QLineEdit *> m_LeData;

  QLineEdit *m_LePeriod = nullptr;
  QPushButton *m_BtnUpdate;

  QComboBox *m_CmbxSignals;
  QLineEdit *m_LeScale;
  QLineEdit *m_LeOffset;
  QPushButton *m_BtnView;

  QLineEdit *m_LeScript;
  QPushButton *m_BtnLoadScript;
  QPushButton *m_BtnExecScript;
  int m_ExecTimerId = 0;

  QTabWidget *m_TabWidget;
  struct FigureInfo {
    int TabIndex;
    UIFigure *Figure;
  };
  std::map<std::string, FigureInfo> m_Figures;
};

class UICom : public QWidget {
  Q_OBJECT
public:
  UICom();
  ~UICom();

public slots:
  void on_btnOpenComJs_clicked(void);
  void on_btnStart_clicked(void);

  void on_btnLoadScript_clicked(void);
  void on_btnExecScript_clicked(void);

  void on_Tree_itemSelectionChanged(void);

private:
  void timerEvent(QTimerEvent *event);

private:
  bool Load(std::string cfgPath);
  bool LoadUi(void);

private:
  QLineEdit *m_LeComJs;
  QPushButton *m_BtnOpenComJs;
  QPushButton *m_BtnStart;
  QTabWidget *m_TabWidget;
  QTreeWidget *m_Tree;

  QLineEdit *m_LeScript;
  QPushButton *m_BtnLoadScript;
  QPushButton *m_BtnExecScript;
  int m_ExecTimerId = 0;

  std::vector<UIMsg *> m_UIMsgs;

  std::map<std::string, int> m_Msg2TabIndexMap;

  json m_Json;

  int m_TimerId = 0;

  std::vector<std::shared_ptr<Network>> m_Networks;

  std::shared_ptr<MessageQueue<topic::figure::FigureConfig>> m_FigCfgMsgQue;
  std::map<std::string, UIFigure *> m_Figures;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* UICOM_HPP */
