/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail->com>
 */
/* ================================ { INCLUDES  ] ============================================== */
#include "UICom.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QMainwindow>
#include <QTextCursor>
#include <sstream>
#include <iomanip>
#include "Log.hpp"
#include <unistd.h>

using namespace as;
/* ================================ { MACROS    ] ============================================== */
/* ================================ { TYPES     ] ============================================== */
/* ================================ { DECLARES  ] ============================================== */
/* ================================ { DATAS     ] ============================================== */
/* ================================ { LOCALS    ] ============================================== */
/* ================================ { FUNCTIONS ] ============================================== */
UIMsg::UIMsg(std::shared_ptr<Message> msg) {
  m_Msg = msg;

  auto wd = new QWidget();
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  int row = 0, col = 0;
  for (auto sig : msg->Signals()) {
    grid->addWidget(new QLabel(tr(sig->name().c_str())), row, col);
    auto le = new QLineEdit("0");
    grid->addWidget(le, row, col + 1);
    if (false == msg->IsTransmit()) {
      le->setReadOnly(true);
      le->setStyleSheet("background-color:rgba(128,128,128,255)");
    }
    m_leData.push_back(le);

    col += 2;
    if (col > 3) {
      col = 0;
      row += 1;
    }
  }
  row += 1;

  if (msg->IsTransmit() || (Network::Type::LIN == msg->network()->type())) {
    grid->addWidget(new QLabel("period(ms):"), row, 0);
    m_lePeriod = new QLineEdit(tr(std::to_string(msg->period()).c_str()));
    grid->addWidget(m_lePeriod, row, 1);
    m_btnUpdate = new QPushButton("update");
    m_btnUpdate->setToolTip("if period is 0, will update message and send or read it once\n"
                            "if period is not 0, will only update the period and message value");
    grid->addWidget(m_btnUpdate, row, 2);
    connect(m_btnUpdate, SIGNAL(clicked()), this, SLOT(on_btnUpdate_clicked()));
  }
  vbox->addLayout(grid);

  auto hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("View:"));
  m_cmbxSignals = new QComboBox();
  for (auto sig : m_Msg->Signals()) {
    m_cmbxSignals->addItem(tr(sig->name().c_str()));
  }
  m_cmbxSignals->setMinimumWidth(300);
  hbox->addWidget(m_cmbxSignals);
  hbox->addWidget(new QLabel("scale:"));
  m_leScale = new QLineEdit("1");
  hbox->addWidget(m_leScale);
  hbox->addWidget(new QLabel("offset:"));
  m_leOffset = new QLineEdit("0");
  hbox->addWidget(m_leOffset);
  m_btnView = new QPushButton("View");
  hbox->addWidget(m_btnView);
  vbox->addLayout(hbox);

  hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("Lua script:"));
  auto dft = m_Msg->name() + ".lua";
  m_leScript = new QLineEdit(tr(dft.c_str()));
  hbox->addWidget(m_leScript);
  m_btnLoadScript = new QPushButton("...");
  hbox->addWidget(m_btnLoadScript);
  m_btnExecScript = new QPushButton("Exec");
  hbox->addWidget(m_btnExecScript);
  vbox->addLayout(hbox);

  connect(m_btnLoadScript, SIGNAL(clicked()), this, SLOT(on_btnLoadScript_clicked()));
  connect(m_btnExecScript, SIGNAL(clicked()), this, SLOT(on_btnExecScript_clicked()));

  m_tabWidget = new QTabWidget();
  m_tabWidget->setMinimumHeight(500);
  vbox->addWidget(m_tabWidget);

  wd->setLayout(vbox);
  setWidget(wd);
  setWidgetResizable(true);

  connect(m_cmbxSignals, SIGNAL(currentIndexChanged(int)), this,
          SLOT(on_cmbxSignals_currentIndexChanged(int)));
  connect(m_btnView, SIGNAL(clicked()), this, SLOT(on_btnView_clicked()));
}

int UIMsg::toInteger(std::string s) {
  int r;
  if ((0 == strncmp(s.c_str(), "0x", 2)) || (0 == strncmp(s.c_str(), "0X", 2))) {
    r = strtoul(s.c_str(), NULL, 16);
  } else {
    r = atoi(s.c_str());
  }
  return r;
}

void UIMsg::updateMsg(void) {
  int value;
  int index = 0;
  for (auto sig : m_Msg->Signals()) {
    auto le = m_leData[index];
    if (m_Msg->IsTransmit()) {
      if (sig->bylua()) {
        /* signal is controlled by lua */
        value = (int)sig->read();
        std::stringstream ss;
        ss << value;
        le->setText(tr(ss.str().c_str()));
      } else {
        value = toInteger(le->text().toStdString());
        sig->write((uint32_t)value);
      }
    } else {
      value = (int)sig->read();
      std::stringstream ss;
      ss << value << "(0x" << std::hex << value << ")";
      le->setText(tr(ss.str().c_str()));
    }
    index++;
  }

  if (nullptr != m_lePeriod) {
    value = toInteger(m_lePeriod->text().toStdString());
    m_Msg->set_period((uint32_t)value);
  }
}

void UIMsg::run(void) {
  updateMsg();
}

void UIMsg::on_btnUpdate_clicked(void) {
  updateMsg();
  if (m_Msg->period() == 0) {
    m_Msg->trigger();
  }
}

void UIMsg::on_cmbxSignals_currentIndexChanged(int index) {
  (void)index;
  m_leScale->setText("1");
  m_leOffset->setText("0");
  auto sigName = m_cmbxSignals->currentText().toStdString();
  auto it = m_Figures.find(sigName);
  if (it == m_Figures.end()) {
    m_btnView->setText("View");
  } else {
    m_btnView->setText("Stop");
  }
}

void UIMsg::on_btnView_clicked(void) {
  auto sigName = m_cmbxSignals->currentText().toStdString();
  std::shared_ptr<Signal> signal = nullptr;

  for (auto sig : m_Msg->Signals()) {
    if (sig->name() == sigName) {
      signal = sig;
      break;
    }
  }

  if (signal) {
    auto it = m_Figures.find(signal->name());
    if (it == m_Figures.end()) {
      auto figure = new UIFigure(signal);
      int index = m_tabWidget->addTab(figure, tr(signal->name().c_str()));
      m_Figures[signal->name()] = {index, figure};
      m_btnView->setText("Stop");
      m_tabWidget->setCurrentIndex(index);
    } else {
      auto info = m_Figures[signal->name()];
      m_tabWidget->removeTab(info.tabIndex);
      delete info.figure;
      m_Figures.erase(it);
      m_btnView->setText("View");
    }
  }
}

void UIMsg::on_btnLoadScript_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "Lua script", "", "Lua script (*.lua)");
  if ("" != fileName) {
    m_leScript->setText(fileName);
  }
}

void UIMsg::on_btnExecScript_clicked(void) {
  auto txt = m_btnExecScript->text().toStdString();
  if (txt == "Exec") {
    auto luaScript = m_leScript->text().toStdString();
    m_Msg->startLua(luaScript);
    auto err = m_Msg->luaError();
    if (err.empty() == false) {
      QMessageBox::critical(this, "Error", tr(err.c_str()));
      return;
    }
    if (m_Msg->isLuaRunning()) {
      m_btnExecScript->setText("Stop");
      m_ExecTimerId = startTimer(100); /* lua monitor */
    }
  } else {
    m_btnExecScript->setText("Exec");
    m_Msg->stopLua();
    if (0 != m_ExecTimerId) {
      killTimer(m_ExecTimerId);
      m_ExecTimerId = 0;
    }
  }
}

void UIMsg::timerEvent(QTimerEvent *event) {
  auto err = m_Msg->luaError();
  if (err.empty() == false) {
    QMessageBox::critical(this, "Error", tr(err.c_str()));
  }

  if (false == m_Msg->isLuaRunning()) {
    m_btnExecScript->setText("Exec");
    killTimer(m_ExecTimerId);
    m_ExecTimerId = 0;
  }

  (void)event;
}

UIMsg::~UIMsg() {
}

UICom::UICom() : QWidget() {
  auto vbox = new QVBoxLayout();
  auto hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("load COM json:"));
  m_leComJs = new QLineEdit();
  m_leComJs->setReadOnly(true);
  hbox->addWidget(m_leComJs);
  m_btnOpenComJs = new QPushButton("...");
  hbox->addWidget(m_btnOpenComJs);
  m_btnStart = new QPushButton("start");
  hbox->addWidget(m_btnStart);
  vbox->addLayout(hbox);

  hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("Lua script:"));
  m_leScript = new QLineEdit("com.lua");
  hbox->addWidget(m_leScript);
  m_btnLoadScript = new QPushButton("...");
  hbox->addWidget(m_btnLoadScript);
  m_btnExecScript = new QPushButton("Exec");
  hbox->addWidget(m_btnExecScript);
  vbox->addLayout(hbox);

  auto qSplitter = new QSplitter(Qt::Horizontal);
  m_Tree = new QTreeWidget();
  m_Tree->setMaximumWidth(300);
  m_Tree->setHeaderLabel("Messages");
  m_tabWidget = new QTabWidget();
  qSplitter->insertWidget(0, m_Tree);
  qSplitter->insertWidget(1, m_tabWidget);

  vbox->addWidget(qSplitter);

  setLayout(vbox);
  connect(m_btnOpenComJs, SIGNAL(clicked()), this, SLOT(on_btnOpenComJs_clicked()));
  connect(m_btnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));

  connect(m_btnLoadScript, SIGNAL(clicked()), this, SLOT(on_btnLoadScript_clicked()));
  connect(m_btnExecScript, SIGNAL(clicked()), this, SLOT(on_btnExecScript_clicked()));

  connect(m_Tree, SIGNAL(itemSelectionChanged()), this, SLOT(on_Tree_itemSelectionChanged()));

  std::string defJs = "./Com.json";
  if (0 != access(defJs.c_str(), F_OK | R_OK)) {
    defJs = "../../../../app/app/config/Com/GEN/Com.json";
  }
  auto r = load(defJs);
  if (r) {
    m_leComJs->setText(tr(defJs.c_str()));
  }

  m_FigCfgMsgQue = topic::figure::config();
}

void UICom::on_Tree_itemSelectionChanged(void) {
  auto item = m_Tree->currentItem();
  auto dirItem = item->parent();
  if (nullptr != dirItem) {
    auto netItem = dirItem->parent();
    if (nullptr != netItem) {
      auto index = m_Tree->indexOfTopLevelItem(netItem);
      if (-1 != index) {
        auto ns = netItem->text(0).toStdString() + ":" + item->text(0).toStdString();
        auto it = m_msg2TabIndexMap.find(ns);
        if (it != m_msg2TabIndexMap.end()) {
          m_tabWidget->setCurrentIndex(it->second);
        }
      }
    }
  }
}

bool UICom::loadUI(void) {
  bool r = true;

  if (0 != m_TimerId) {
    killTimer(m_TimerId);
  }

  m_tabWidget->clear();
  m_Tree->clear();
  m_Networks.clear();

  if (false == m_Json.contains("networks")) {
    QMessageBox::critical(this, "Error", tr("json was not loaded"));
    return false;
  }

  for (auto &cfg : m_Json["networks"]) {
    auto network = std::make_shared<Network>(cfg);
    m_Networks.push_back(network);
  }

  int tabIndex = 0;
  for (auto &network : m_Networks) {
    auto itemNet = new QTreeWidgetItem({QString(network->name().c_str())});
    auto itemRx = new QTreeWidgetItem({"RX"});
    auto itemTx = new QTreeWidgetItem({"TX"});
    itemNet->addChild(itemRx);
    itemNet->addChild(itemTx);
    for (auto &msg : network->messages()) {
      auto uiMsg = new UIMsg(msg);
      m_tabWidget->addTab(uiMsg, tr(msg->name().c_str()));
      m_UIMsgs.push_back(uiMsg);
      auto item = new QTreeWidgetItem({QString(msg->name().c_str())});
      if (msg->IsTransmit()) {
        itemTx->addChild(item);
      } else {
        itemRx->addChild(item);
      }
      auto ns = network->name() + ":" + msg->name();
      m_msg2TabIndexMap[ns] = tabIndex;
      tabIndex++;
    }
    m_Tree->addTopLevelItem(itemNet);
  }

  for (auto &network : m_Networks) {
    r = network->start();
    if (r != true) {
      std::string msg("failed to start network " + network->name());
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
      break;
    }
  }

  if (r) {
    m_TimerId = startTimer(1);
  } else {
    for (auto &network : m_Networks) {
      network->stop();
    }
  }

  return r;
}

bool UICom::load(std::string cfgPath) {
  bool ret = true;

  FILE *fp = fopen(cfgPath.c_str(), "rb");
  if (nullptr == fp) {
    std::string msg("config file <" + cfgPath + "> not exists");
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
    ret = false;
  }

  if (true == ret) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::unique_ptr<char[]> text(new char[size + 1]);
    fread(text.get(), size, 1, fp);
    char *eol = (char *)text.get();
    eol[size] = 0;
    try {
      m_Json = json::parse(text.get());
    } catch (std::exception &e) {
      ret = false;
      std::string msg("config file " + cfgPath + " error: " + e.what());
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
    }
    fclose(fp);
  }

  return ret;
}

void UICom::on_btnOpenComJs_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "json config", "", "json config (*.json)");

  auto r = load(fileName.toStdString());
  if (r) {
    m_leComJs->setText(fileName);
  }
}

void UICom::on_btnStart_clicked(void) {
  if (m_btnStart->text().toStdString() == "start") {
    auto r = loadUI();
    if (r) {
      m_btnStart->setText("stop");
    }
  } else {
    killTimer(m_TimerId);
    m_TimerId = 0;

    m_tabWidget->clear();
    m_Networks.clear();
    for (auto uiMsg : m_UIMsgs) {
      delete uiMsg;
    }
    m_UIMsgs.clear();
    m_btnStart->setText("start");
  }
}

UICom::~UICom() {
  for (auto uiMsg : m_UIMsgs) {
    delete uiMsg;
  }
  m_UIMsgs.clear();
}

void UICom::on_btnLoadScript_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "Lua script", "", "Lua script (*.lua)");
  if ("" != fileName) {
    m_leScript->setText(fileName);
  }
}

void UICom::on_btnExecScript_clicked(void) {
  auto txt = m_btnExecScript->text().toStdString();
  if (txt == "Exec") {
    auto luaScript = m_leScript->text().toStdString();
    Network::startLua(luaScript);
    auto err = Network::luaError();
    if (err.empty() == false) {
      QMessageBox::critical(this, "Error", tr(err.c_str()));
      return;
    }
    if (Network::isLuaRunning()) {
      m_btnExecScript->setText("Stop");
      m_ExecTimerId = startTimer(100); /* lua monitor */
    }
  } else {
    m_btnExecScript->setText("Exec");
    Network::stopLua();
    if (0 != m_ExecTimerId) {
      killTimer(m_ExecTimerId);
      m_ExecTimerId = 0;
    }
  }
}

void UICom::timerEvent(QTimerEvent *event) {
  if (event->timerId() == m_ExecTimerId) {
    auto err = Network::luaError();
    if (err.empty() == false) {
      QMessageBox::critical(this, "Error", tr(err.c_str()));
    }

    if (false == Network::isLuaRunning()) {
      m_btnExecScript->setText("Exec");
      killTimer(m_ExecTimerId);
      m_ExecTimerId = 0;
    }
  } else {
    for (auto uiMsg : m_UIMsgs) {
      uiMsg->run();
    }
    topic::figure::FigureConfig figCfg;
    auto r = m_FigCfgMsgQue->get(figCfg, true, 0);
    if (r) {
      auto it = m_Figures.find(figCfg.name);
      if (it == m_Figures.end()) {
        auto figure = new UIFigure(figCfg);
        figure->show();
        m_Figures[figCfg.name] = figure;
      } else {
        auto figure = it->second;
        if (figure->isVisible()) {
          std::string err = "already has figure " + figCfg.name;
          QMessageBox::critical(this, "Error", tr(err.c_str()));
        } else {
          delete figure;
          auto figure = new UIFigure(figCfg);
          figure->show();
          m_Figures[figCfg.name] = figure;
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  QMainWindow w;
  w.setCentralWidget(new UICom());
  w.show();
  return a.exec();
}

extern "C" std::string name(void) {
  return "Com";
}

extern "C" QWidget *create(void) {
  return new UICom();
}
