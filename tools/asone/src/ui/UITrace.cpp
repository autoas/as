/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "UITrace.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include "Log.hpp"
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
TraceFilterProxyModel::TraceFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
  m_IdPatten = QRegularExpression(".*", QRegularExpression::CaseInsensitiveOption);

  m_RePatten = QRegularExpression("(name|network|dir|id)=([^\\s]+)",
                                  QRegularExpression::CaseInsensitiveOption);
}

TraceFilterProxyModel::~TraceFilterProxyModel() {
}

bool TraceFilterProxyModel::SetFilterPatten(QString patten) {
  bool ret = true;
  QString pId = ".*";
  QString pName = "";
  QString pNetwork = "";
  QString pDir = "";
  auto L = patten.split(",");

  for (auto l : L) {
    auto match = m_RePatten.match(l);
    if (match.hasMatch()) {
      auto id = match.captured(1);
      auto vp = match.captured(2);
      if (id == "Id") {
        pId = vp;
        LOG(INFO, "Trace: id filter = '%s'\n", vp.toStdString().c_str());
      } else if (id == "Name") {
        LOG(INFO, "Trace: name filter = '%s'\n", vp.toStdString().c_str());
        auto re = new QRegularExpression(vp, QRegularExpression::CaseInsensitiveOption);
        if (re->isValid()) {
          if (nullptr != m_NamePatten) {
            delete m_NamePatten;
          }
          m_NamePatten = re;
        } else {
          ret = false;
        }
      } else if (id == "Network") {
        LOG(INFO, "Trace: network filter = '%s'\n", vp.toStdString().c_str());
        auto re = new QRegularExpression(vp, QRegularExpression::CaseInsensitiveOption);
        if (re->isValid()) {
          if (nullptr != m_NetworkPatten) {
            delete m_NetworkPatten;
          }
          m_NetworkPatten = re;
        } else {
          ret = false;
        }
      } else if (id == "dir") {
        LOG(INFO, "Trace: dir filter = '%s'\n", vp.toStdString().c_str());
        auto re = new QRegularExpression(vp, QRegularExpression::CaseInsensitiveOption);
        if (re->isValid()) {
          if (nullptr != m_DirPatten) {
            delete m_DirPatten;
          }
          m_DirPatten = re;
        } else {
          ret = false;
        }
      }
    }
  }

  auto re = QRegularExpression(pId, QRegularExpression::CaseInsensitiveOption);
  if (re.isValid()) {
    m_IdPatten = re;
  } else {
    ret = false;
  }
  setFilterRegularExpression(m_IdPatten);
  setFilterKeyColumn(4);

  return ret;
}

bool TraceFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
  bool accept;
  QModelIndex indId = sourceModel()->index(sourceRow, 4, sourceParent);
  auto idString = sourceModel()->data(indId).toString();
  if (idString == "") {
    accept = true;
  } else {
    accept = QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    if (accept) {
      if (m_NamePatten) {
        QModelIndex ind = sourceModel()->index(sourceRow, 1, sourceParent);
        auto str = sourceModel()->data(ind).toString();
        accept = str.contains(*m_NamePatten);
        LOG(DEBUG, "Trace: name filter '%s' = %s\n", str.toStdString().c_str(),
            accept ? "true" : "false");
      }

      if ((true == accept) && m_NetworkPatten) {
        QModelIndex ind = sourceModel()->index(sourceRow, 2, sourceParent);
        auto str = sourceModel()->data(ind).toString();
        accept = str.contains(*m_NetworkPatten);
        LOG(DEBUG, "Trace: network filter '%s' = %s\n", str.toStdString().c_str(),
            accept ? "true" : "false");
      }

      if ((true == accept) && m_DirPatten) {
        QModelIndex ind = sourceModel()->index(sourceRow, 3, sourceParent);
        auto str = sourceModel()->data(ind).toString();
        accept = str.contains(*m_DirPatten);
        LOG(DEBUG, "Trace: dir filter '%s' = %s\n", str.toStdString().c_str(),
            accept ? "true" : "false");
      }
    }
  }

  return accept;
}

UITrace::UITrace() {
  auto vbox = new QVBoxLayout();
  auto hbox = new QHBoxLayout();

  hbox->addWidget(new QLabel("topic:"));
  m_CmbxTopic = new QComboBox();
  m_CmbxTopic->addItems({"everything", "isotp", "uds"});
  m_CmbxTopic->setEditable(true);
  hbox->addWidget(m_CmbxTopic);
  m_BtnStart = new QPushButton("Start");
  m_BtnClear = new QPushButton("clear");
  m_CbxLog = new QCheckBox("Log");
  hbox->addWidget(m_BtnStart);
  hbox->addWidget(m_BtnClear);
  hbox->addWidget(m_CbxLog);
  vbox->addLayout(hbox);

  hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("filter:"));
  m_CmbxFilter = new QComboBox();
  m_CmbxFilter->addItems({"name=.*, network=.*, dir=.*, id=.*"});
  m_CmbxFilter->setEditable(true);
  m_BtnApplyFilter = new QPushButton("apply");
  hbox->addWidget(m_CmbxFilter);
  hbox->addWidget(m_BtnApplyFilter);
  vbox->addLayout(hbox);

  m_Model = new QStandardItemModel();
  m_Model->setHorizontalHeaderLabels(
    {"Number", "Name", "Network", "dir", "Id", "Dlc", "Data", "timestamp"});
  m_ProxyModel = new TraceFilterProxyModel();
  m_TreeView = new QTreeView();
  m_TreeView->setSortingEnabled(false);
  m_ProxyModel->setSourceModel(m_Model);
  m_TreeView->setModel(m_ProxyModel);
  vbox->addWidget(m_TreeView);

  setLayout(vbox);

  connect(m_BtnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
  connect(m_BtnClear, SIGNAL(clicked()), this, SLOT(on_btnClear_clicked()));
  connect(m_BtnApplyFilter, SIGNAL(clicked()), this, SLOT(on_btnApplyFilter_clicked()));

  connect(m_CmbxTopic, SIGNAL(currentIndexChanged(int)), this,
          SLOT(on_cmbxTopic_currentIndexChanged(int)));

  connect(m_CbxLog, SIGNAL(stateChanged(int)), this, SLOT(on_cbxLog_stateChanged(int)));
  startTimer(100);
}

UITrace::~UITrace() {
}

void UITrace::on_btnStart_clicked(void) {
  auto topicName = m_CmbxTopic->currentText().toStdString();
  if (false == topicName.empty()) {
    auto it = m_Traces.find(topicName);
    if (it == m_Traces.end()) {
      if ("everything" == topicName) {
        m_CmbxTopic->setDisabled(true);
        m_Traces.clear();
      }
      auto sub = std::make_shared<com::Subscriber>(topicName);
      m_Traces[topicName] = {sub};
      m_BtnStart->setText("Stop");
    } else {
      m_Traces.erase(topicName);
      m_BtnStart->setText("Start");
      if ("everything" == topicName) {
        m_CmbxTopic->setDisabled(false);
      }
    }
  }
}

void UITrace::on_btnClear_clicked(void) {
  m_Model->clear();
  m_Model->setHorizontalHeaderLabels(
    {"Number", "Name", "Network", "dir", "Id", "Dlc", "Data", "timestamp"});
  m_Number = 0;
}

void UITrace::on_btnApplyFilter_clicked(void) {
  auto filter = m_CmbxFilter->currentText();
  auto ret = m_ProxyModel->SetFilterPatten(filter);
  if (false == ret) {
    QMessageBox::critical(this, "Error", "Invalid filter patten");
  }
}

void UITrace::on_cmbxTopic_currentIndexChanged(int index) {
  auto topicName = m_CmbxTopic->currentText().toStdString();
  if (false == topicName.empty()) {
    auto it = m_Traces.find(topicName);
    if (it == m_Traces.end()) {
      m_BtnStart->setText("Start");
    } else {
      m_BtnStart->setText("Stop");
    }
  } else {
    m_BtnStart->setText("Start");
  }
}

void UITrace::on_cbxLog_stateChanged(int state) {
  if (state) {
    m_Logger = std::make_shared<Logger>("Trace", "csv");
  } else {
    m_Logger = nullptr;
  }
}

void UITrace::timerEvent(QTimerEvent *event) {
  auto topics = com::Topics();
  for (auto tp : topics) {
    bool hasIt = false;
    for (int i = 0; i < m_CmbxTopic->count(); i++) {
      auto it = m_CmbxTopic->itemText(i).toStdString();
      if (it == tp) {
        hasIt = true;
        break;
      }
    }
    if (false == hasIt) {
      m_CmbxTopic->addItem(tr(tp.c_str()));
    }
  }

  for (auto it : m_Traces) {
    auto sub = it.second.Sub;
    std::shared_ptr<com::Message> msg;
    do {
      msg = sub->Pop();
      if (nullptr != msg) {
        auto number = std::to_string(m_Number);
        QStandardItem *itemRoot = m_Model->invisibleRootItem();
        QList<QStandardItem *> msgItem = {new QStandardItem(tr(number.c_str())),
                                          new QStandardItem(tr(msg->Name.c_str())),
                                          new QStandardItem(tr(msg->Network.c_str())),
                                          new QStandardItem(tr(msg->Dir.c_str())),
                                          new QStandardItem(tr(msg->Id.c_str())),
                                          new QStandardItem(tr(msg->Dlc.c_str())),
                                          new QStandardItem(tr(msg->Data.c_str())),
                                          new QStandardItem(tr(msg->Timestamp.c_str()))};
        for (auto it : msgItem) {
          it->setEditable(false);
        }
        if (nullptr != m_Logger) {
          m_Logger->Write("%s,%s,%s,%s,%s,%s,%s,%s\n", number.c_str(), msg->Name.c_str(),
                          msg->Network.c_str(), msg->Dir.c_str(), msg->Id.c_str(), msg->Dlc.c_str(),
                          msg->Data.c_str(), msg->Timestamp.c_str());
        }
        for (auto &sig : msg->Signals) {
          QList<QStandardItem *> child = {
            nullptr, new QStandardItem(tr(sig.Name.c_str())), nullptr, nullptr, nullptr,
            nullptr, new QStandardItem(tr(sig.Value.c_str()))};
          child[1]->setEditable(false);
          child[6]->setEditable(false);
          msgItem[0]->appendRow(child);
          if (nullptr != m_Logger) {
            m_Logger->Write(",%s,%s,%s,%s,%s,%s\n", sig.Name.c_str(), "", "", "", "",
                            sig.Value.c_str());
          }
        }
        if (nullptr != m_Logger) {
          m_Logger->Check();
        }
        itemRoot->appendRow(msgItem);
        m_Number++;
      }
    } while (nullptr != msg);
  }
}

extern "C" std::string Name(void) {
  return "Trace";
}

extern "C" QWidget *Create(void) {
  return new UITrace();
}
