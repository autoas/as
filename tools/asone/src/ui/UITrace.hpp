/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __UITRACE_HPP__
#define __UITRACE_HPP__
/* ================================ [ INCLUDES  ] ============================================== */
#include <QWidget>
#include <QTimerEvent>
#include <QLineEdit>
#include <QTabWidget>
#include <QPushButton>
#include <QComboBox>
#include <QTreeWidget>
#include <QCheckBox>

#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStandardItem>

#include <map>

#include "topic.hpp"
#include "Log.hpp"
using namespace as;
using namespace as::topic;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class TraceFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT

public:
  TraceFilterProxyModel(QObject *parent = nullptr);
  ~TraceFilterProxyModel();
  bool setFilterPatten(QString patten);

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
  QRegularExpression m_rePatten;
  QRegularExpression m_IdPatten;
  QRegularExpression *m_NamePatten = nullptr;
  QRegularExpression *m_NetworkPatten = nullptr;
  QRegularExpression *m_DirPatten = nullptr;
};

class UITrace : public QWidget {
  Q_OBJECT
public:
  UITrace();
  ~UITrace();

public slots:
  void on_btnStart_clicked(void);
  void on_btnClear_clicked(void);
  void on_cmbxTopic_currentIndexChanged(int index);
  void on_cbxLog_stateChanged(int state);
  void on_btnApplyFilter_clicked(void);

private:
  void timerEvent(QTimerEvent *event);

private:
  QComboBox *m_cmbxTopic;
  QPushButton *m_btnStart;
  QPushButton *m_btnClear;
  QCheckBox *m_cbxLog;
  QComboBox *m_cmbxFilter;
  QPushButton *m_btnApplyFilter;
  QStandardItemModel *m_Model;
  TraceFilterProxyModel *m_ProxyModel;
  QTreeView *m_TreeView;
  uint64_t m_Number = 0;
  struct TraceInfo {
    std::shared_ptr<com::Subscriber> sub;
  };
  std::map<std::string, TraceInfo> m_Traces;

  std::shared_ptr<Logger> m_Logger = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __UITRACE_HPP__ */
