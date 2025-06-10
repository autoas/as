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

  m_rePatten = QRegularExpression("(name|network|dir|id)=([^\\s]+)",
                                  QRegularExpression::CaseInsensitiveOption);
}

TraceFilterProxyModel::~TraceFilterProxyModel() {
}

bool TraceFilterProxyModel::setFilterPatten(QString patten) {
  bool ret = true;
  QString pId = ".*";
  QString pName = "";
  QString pNetwork = "";
  QString pDir = "";
  auto L = patten.split(",");

  for (auto l : L) {
    auto match = m_rePatten.match(l);
    if (match.hasMatch()) {
      auto id = match.captured(1);
      auto vp = match.captured(2);
      if (id == "id") {
        pId = vp;
        LOG(INFO, "Trace: id filter = '%s'\n", vp.toStdString().c_str());
      } else if (id == "name") {
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
      } else if (id == "network") {
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
  m_cmbxTopic = new QComboBox();
  m_cmbxTopic->addItems({"everything", "isotp", "uds"});
  m_cmbxTopic->setEditable(true);
  hbox->addWidget(m_cmbxTopic);
  m_btnStart = new QPushButton("start");
  m_btnClear = new QPushButton("clear");
  m_cbxLog = new QCheckBox("Log");
  hbox->addWidget(m_btnStart);
  hbox->addWidget(m_btnClear);
  hbox->addWidget(m_cbxLog);
  vbox->addLayout(hbox);

  hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("filter:"));
  m_cmbxFilter = new QComboBox();
  m_cmbxFilter->addItems({"name=.*, network=.*, dir=.*, id=.*"});
  m_cmbxFilter->setEditable(true);
  m_btnApplyFilter = new QPushButton("apply");
  hbox->addWidget(m_cmbxFilter);
  hbox->addWidget(m_btnApplyFilter);
  vbox->addLayout(hbox);

  m_Model = new QStandardItemModel();
  m_Model->setHorizontalHeaderLabels(
    {"number", "name", "network", "dir", "id", "dlc", "data", "timestamp"});
  m_ProxyModel = new TraceFilterProxyModel();
  m_TreeView = new QTreeView();
  m_TreeView->setSortingEnabled(false);
  m_ProxyModel->setSourceModel(m_Model);
  m_TreeView->setModel(m_ProxyModel);
  vbox->addWidget(m_TreeView);

  setLayout(vbox);

  connect(m_btnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
  connect(m_btnClear, SIGNAL(clicked()), this, SLOT(on_btnClear_clicked()));
  connect(m_btnApplyFilter, SIGNAL(clicked()), this, SLOT(on_btnApplyFilter_clicked()));

  connect(m_cmbxTopic, SIGNAL(currentIndexChanged(int)), this,
          SLOT(on_cmbxTopic_currentIndexChanged(int)));

  connect(m_cbxLog, SIGNAL(stateChanged(int)), this, SLOT(on_cbxLog_stateChanged(int)));
  startTimer(100);
}

UITrace::~UITrace() {
}

void UITrace::on_btnStart_clicked(void) {
  auto topicName = m_cmbxTopic->currentText().toStdString();
  if (false == topicName.empty()) {
    auto it = m_Traces.find(topicName);
    if (it == m_Traces.end()) {
      if ("everything" == topicName) {
        m_cmbxTopic->setDisabled(true);
        m_Traces.clear();
      }
      auto sub = std::make_shared<com::Subscriber>(topicName);
      m_Traces[topicName] = {sub};
      m_btnStart->setText("stop");
    } else {
      m_Traces.erase(topicName);
      m_btnStart->setText("start");
      if ("everything" == topicName) {
        m_cmbxTopic->setDisabled(false);
      }
    }
  }
}

void UITrace::on_btnClear_clicked(void) {
  m_Model->clear();
  m_Model->setHorizontalHeaderLabels(
    {"number", "name", "network", "dir", "id", "dlc", "data", "timestamp"});
  m_Number = 0;
}

void UITrace::on_btnApplyFilter_clicked(void) {
  auto filter = m_cmbxFilter->currentText();
  auto ret = m_ProxyModel->setFilterPatten(filter);
  if (false == ret) {
    QMessageBox::critical(this, "Error", "Invalid filter patten");
  }
}

void UITrace::on_cmbxTopic_currentIndexChanged(int index) {
  auto topicName = m_cmbxTopic->currentText().toStdString();
  if (false == topicName.empty()) {
    auto it = m_Traces.find(topicName);
    if (it == m_Traces.end()) {
      m_btnStart->setText("start");
    } else {
      m_btnStart->setText("stop");
    }
  } else {
    m_btnStart->setText("start");
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
  auto topics = com::topics();
  for (auto tp : topics) {
    bool hasIt = false;
    for (int i = 0; i < m_cmbxTopic->count(); i++) {
      auto it = m_cmbxTopic->itemText(i).toStdString();
      if (it == tp) {
        hasIt = true;
        break;
      }
    }
    if (false == hasIt) {
      m_cmbxTopic->addItem(tr(tp.c_str()));
    }
  }

  for (auto it : m_Traces) {
    auto sub = it.second.sub;
    std::shared_ptr<com::Message> msg;
    do {
      msg = sub->pop();
      if (nullptr != msg) {
        auto number = std::to_string(m_Number);
        QStandardItem *itemRoot = m_Model->invisibleRootItem();
        QList<QStandardItem *> msgItem = {new QStandardItem(tr(number.c_str())),
                                          new QStandardItem(tr(msg->name.c_str())),
                                          new QStandardItem(tr(msg->network.c_str())),
                                          new QStandardItem(tr(msg->dir.c_str())),
                                          new QStandardItem(tr(msg->id.c_str())),
                                          new QStandardItem(tr(msg->dlc.c_str())),
                                          new QStandardItem(tr(msg->data.c_str())),
                                          new QStandardItem(tr(msg->timestamp.c_str()))};
        for (auto it : msgItem) {
          it->setEditable(false);
        }
        if (nullptr != m_Logger) {
          m_Logger->write("%s,%s,%s,%s,%s,%s,%s,%s\n", number.c_str(), msg->name.c_str(),
                          msg->network.c_str(), msg->dir.c_str(), msg->id.c_str(), msg->dlc.c_str(),
                          msg->data.c_str(), msg->timestamp.c_str());
        }
        for (auto &sig : msg->Signals) {
          QList<QStandardItem *> child = {
            nullptr, new QStandardItem(tr(sig.name.c_str())), nullptr, nullptr, nullptr,
            nullptr, new QStandardItem(tr(sig.value.c_str()))};
          child[1]->setEditable(false);
          child[6]->setEditable(false);
          msgItem[0]->appendRow(child);
          if (nullptr != m_Logger) {
            m_Logger->write(",%s,%s,%s,%s,%s,%s\n", sig.name.c_str(), "", "", "", "",
                            sig.value.c_str());
          }
        }
        if (nullptr != m_Logger) {
          m_Logger->check();
        }
        itemRoot->appendRow(msgItem);
        m_Number++;
      }
    } while (nullptr != msg);
  }
}

extern "C" std::string name(void) {
  return "Trace";
}

extern "C" QWidget *create(void) {
  return new UITrace();
}
