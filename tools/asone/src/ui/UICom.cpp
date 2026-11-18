/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail->com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
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
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
UIMsg::UIMsg(std::shared_ptr<Message> msg) {
  m_Msg = msg;

  auto wd = new QWidget();
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  int row = 0, col = 0;
  for (auto sig : msg->Signals()) {
    grid->addWidget(new QLabel(tr(sig->Name().c_str())), row, col);
    auto le = new QLineEdit("0");
    grid->addWidget(le, row, col + 1);
    if (false == msg->IsTransmit()) {
      le->setReadOnly(true);
      le->setStyleSheet("background-color:rgba(128,128,128,255)");
    }
    m_LeData.push_back(le);

    col += 2;
    if (col > 3) {
      col = 0;
      row += 1;
    }
  }
  row += 1;

  if (msg->IsTransmit() || (Network::Type::LIN == msg->GetNetwork()->GetType())) {
    grid->addWidget(new QLabel("period(ms):"), row, 0);
    m_LePeriod = new QLineEdit(tr(std::to_string(msg->Period()).c_str()));
    grid->addWidget(m_LePeriod, row, 1);
    m_BtnUpdate = new QPushButton("update");
    m_BtnUpdate->setToolTip("if period is 0, will update message and send or read it once\n"
                            "if period is not 0, will only update the period and message value");
    grid->addWidget(m_BtnUpdate, row, 2);
    connect(m_BtnUpdate, SIGNAL(clicked()), this, SLOT(on_btnUpdate_clicked()));
  }
  vbox->addLayout(grid);

  auto hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("View:"));
  m_CmbxSignals = new QComboBox();
  for (auto sig : m_Msg->Signals()) {
    m_CmbxSignals->addItem(tr(sig->Name().c_str()));
  }
  m_CmbxSignals->setMinimumWidth(300);
  hbox->addWidget(m_CmbxSignals);
  hbox->addWidget(new QLabel("scale:"));
  m_LeScale = new QLineEdit("1");
  hbox->addWidget(m_LeScale);
  hbox->addWidget(new QLabel("offset:"));
  m_LeOffset = new QLineEdit("0");
  hbox->addWidget(m_LeOffset);
  m_BtnView = new QPushButton("View");
  hbox->addWidget(m_BtnView);
  vbox->addLayout(hbox);

  hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("Lua script:"));
  auto dft = m_Msg->Name() + ".lua";
  m_LeScript = new QLineEdit(tr(dft.c_str()));
  hbox->addWidget(m_LeScript);
  m_BtnLoadScript = new QPushButton("...");
  hbox->addWidget(m_BtnLoadScript);
  m_BtnExecScript = new QPushButton("Exec");
  hbox->addWidget(m_BtnExecScript);
  vbox->addLayout(hbox);

  connect(m_BtnLoadScript, SIGNAL(clicked()), this, SLOT(on_btnLoadScript_clicked()));
  connect(m_BtnExecScript, SIGNAL(clicked()), this, SLOT(on_btnExecScript_clicked()));

  m_TabWidget = new QTabWidget();
  m_TabWidget->setMinimumHeight(500);
  vbox->addWidget(m_TabWidget);

  wd->setLayout(vbox);
  setWidget(wd);
  setWidgetResizable(true);

  connect(m_CmbxSignals, SIGNAL(currentIndexChanged(int)), this,
          SLOT(on_cmbxSignals_currentIndexChanged(int)));
  connect(m_BtnView, SIGNAL(clicked()), this, SLOT(on_btnView_clicked()));
}

int UIMsg::ToInteger(std::string s) {
  int r;
  if ((0 == strncmp(s.c_str(), "0x", 2)) || (0 == strncmp(s.c_str(), "0X", 2))) {
    r = strtoul(s.c_str(), NULL, 16);
  } else {
    r = atoi(s.c_str());
  }
  return r;
}

void UIMsg::UpdateMsg(void) {
  int value;
  int index = 0;
  for (auto sig : m_Msg->Signals()) {
    auto le = m_LeData[index];
    if (m_Msg->IsTransmit()) {
      if (sig->IsByLua()) {
        /* signal is controlled by lua */
        value = (int)sig->Read();
        std::stringstream ss;
        ss << value;
        le->setText(tr(ss.str().c_str()));
      } else {
        value = ToInteger(le->text().toStdString());
        sig->Write((uint32_t)value);
      }
    } else {
      value = (int)sig->Read();
      std::stringstream ss;
      ss << value << "(0x" << std::hex << value << ")";
      le->setText(tr(ss.str().c_str()));
    }
    index++;
  }

  if (nullptr != m_LePeriod) {
    value = ToInteger(m_LePeriod->text().toStdString());
    m_Msg->SetPeriod((uint32_t)value);
  }
}

void UIMsg::Run(void) {
  UpdateMsg();
}

void UIMsg::on_btnUpdate_clicked(void) {
  UpdateMsg();
  if (m_Msg->Period() == 0) {
    m_Msg->Trigger();
  }
}

void UIMsg::on_cmbxSignals_currentIndexChanged(int index) {
  (void)index;
  m_LeScale->setText("1");
  m_LeOffset->setText("0");
  auto sigName = m_CmbxSignals->currentText().toStdString();
  auto it = m_Figures.find(sigName);
  if (it == m_Figures.end()) {
    m_BtnView->setText("View");
  } else {
    m_BtnView->setText("Stop");
  }
}

void UIMsg::on_btnView_clicked(void) {
  auto sigName = m_CmbxSignals->currentText().toStdString();
  std::shared_ptr<Signal> signal = nullptr;

  for (auto sig : m_Msg->Signals()) {
    if (sig->Name() == sigName) {
      signal = sig;
      break;
    }
  }

  if (signal) {
    auto it = m_Figures.find(signal->Name());
    if (it == m_Figures.end()) {
      auto figure = new UIFigure(signal);
      int index = m_TabWidget->addTab(figure, tr(signal->Name().c_str()));
      m_Figures[signal->Name()] = {index, figure};
      m_BtnView->setText("Stop");
      m_TabWidget->setCurrentIndex(index);
    } else {
      auto info = m_Figures[signal->Name()];
      m_TabWidget->removeTab(info.TabIndex);
      delete info.Figure;
      m_Figures.erase(it);
      m_BtnView->setText("View");
    }
  }
}

void UIMsg::on_btnLoadScript_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "Lua script", "", "Lua script (*.lua)");
  if ("" != fileName) {
    m_LeScript->setText(fileName);
  }
}

void UIMsg::on_btnExecScript_clicked(void) {
  auto txt = m_BtnExecScript->text().toStdString();
  if (txt == "Exec") {
    auto LuaScript = m_LeScript->text().toStdString();
    m_Msg->StartLua(LuaScript);
    auto err = m_Msg->LuaError();
    if (err.empty() == false) {
      QMessageBox::critical(this, "Error", tr(err.c_str()));
      return;
    }
    if (m_Msg->IsLuaRunning()) {
      m_BtnExecScript->setText("Stop");
      m_ExecTimerId = startTimer(100); /* lua monitor */
    }
  } else {
    m_BtnExecScript->setText("Exec");
    m_Msg->StopLua();
    if (0 != m_ExecTimerId) {
      killTimer(m_ExecTimerId);
      m_ExecTimerId = 0;
    }
  }
}

void UIMsg::timerEvent(QTimerEvent *event) {
  auto err = m_Msg->LuaError();
  if (err.empty() == false) {
    QMessageBox::critical(this, "Error", tr(err.c_str()));
  }

  if (false == m_Msg->IsLuaRunning()) {
    m_BtnExecScript->setText("Exec");
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
  m_LeComJs = new QLineEdit();
  m_LeComJs->setReadOnly(true);
  hbox->addWidget(m_LeComJs);
  m_BtnOpenComJs = new QPushButton("...");
  hbox->addWidget(m_BtnOpenComJs);
  m_BtnStart = new QPushButton("Start");
  hbox->addWidget(m_BtnStart);
  vbox->addLayout(hbox);

  hbox = new QHBoxLayout();
  hbox->addWidget(new QLabel("Lua script:"));
  m_LeScript = new QLineEdit("com.lua");
  hbox->addWidget(m_LeScript);
  m_BtnLoadScript = new QPushButton("...");
  hbox->addWidget(m_BtnLoadScript);
  m_BtnExecScript = new QPushButton("Exec");
  hbox->addWidget(m_BtnExecScript);
  vbox->addLayout(hbox);

  auto qSplitter = new QSplitter(Qt::Horizontal);
  m_Tree = new QTreeWidget();
  m_Tree->setMaximumWidth(300);
  m_Tree->setHeaderLabel("Messages");
  m_TabWidget = new QTabWidget();
  qSplitter->insertWidget(0, m_Tree);
  qSplitter->insertWidget(1, m_TabWidget);

  vbox->addWidget(qSplitter);

  setLayout(vbox);
  connect(m_BtnOpenComJs, SIGNAL(clicked()), this, SLOT(on_btnOpenComJs_clicked()));
  connect(m_BtnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));

  connect(m_BtnLoadScript, SIGNAL(clicked()), this, SLOT(on_btnLoadScript_clicked()));
  connect(m_BtnExecScript, SIGNAL(clicked()), this, SLOT(on_btnExecScript_clicked()));

  connect(m_Tree, SIGNAL(itemSelectionChanged()), this, SLOT(on_Tree_itemSelectionChanged()));

  std::string defJs = "./Com.json";
  if (0 != access(defJs.c_str(), F_OK | R_OK)) {
    defJs = "../../../../app/app/config/Com/GEN/Com.json";
  }
  auto r = Load(defJs);
  if (r) {
    m_LeComJs->setText(tr(defJs.c_str()));
  }

  m_FigCfgMsgQue = topic::figure::Config();
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
        auto it = m_Msg2TabIndexMap.find(ns);
        if (it != m_Msg2TabIndexMap.end()) {
          m_TabWidget->setCurrentIndex(it->second);
        }
      }
    }
  }
}

bool UICom::LoadUi(void) {
  bool r = true;

  if (0 != m_TimerId) {
    killTimer(m_TimerId);
  }

  m_TabWidget->clear();
  m_Tree->clear();
  m_Networks.clear();

  if (false == m_Json.contains("Networks")) {
    QMessageBox::critical(this, "Error", tr("json was not loaded"));
    return false;
  }

  for (auto &cfg : m_Json["Networks"]) {
    auto network = std::make_shared<Network>(cfg);
    m_Networks.push_back(network);
  }

  int tabIndex = 0;
  for (auto &network : m_Networks) {
    auto itemNet = new QTreeWidgetItem({QString(network->Name().c_str())});
    auto itemRx = new QTreeWidgetItem({"RX"});
    auto itemTx = new QTreeWidgetItem({"TX"});
    itemNet->addChild(itemRx);
    itemNet->addChild(itemTx);
    for (auto &msg : network->Messages()) {
      auto uiMsg = new UIMsg(msg);
      m_TabWidget->addTab(uiMsg, tr(msg->Name().c_str()));
      m_UIMsgs.push_back(uiMsg);
      auto item = new QTreeWidgetItem({QString(msg->Name().c_str())});
      if (msg->IsTransmit()) {
        itemTx->addChild(item);
      } else {
        itemRx->addChild(item);
      }
      auto ns = network->Name() + ":" + msg->Name();
      m_Msg2TabIndexMap[ns] = tabIndex;
      tabIndex++;
    }
    m_Tree->addTopLevelItem(itemNet);
  }

  for (auto &network : m_Networks) {
    r = network->Start();
    if (r != true) {
      std::string msg("failed to start network " + network->Name());
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
      break;
    }
  }

  if (r) {
    m_TimerId = startTimer(1);
  } else {
    for (auto &network : m_Networks) {
      network->Stop();
    }
  }

  return r;
}

bool UICom::Load(std::string cfgPath) {
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

  auto r = Load(fileName.toStdString());
  if (r) {
    m_LeComJs->setText(fileName);
  }
}

void UICom::on_btnStart_clicked(void) {
  if (m_BtnStart->text().toStdString() == "Start") {
    auto r = LoadUi();
    if (r) {
      m_BtnStart->setText("Stop");
    }
  } else {
    killTimer(m_TimerId);
    m_TimerId = 0;

    m_TabWidget->clear();
    m_Networks.clear();
    for (auto uiMsg : m_UIMsgs) {
      delete uiMsg;
    }
    m_UIMsgs.clear();
    m_BtnStart->setText("Start");
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
    m_LeScript->setText(fileName);
  }
}

void UICom::on_btnExecScript_clicked(void) {
  auto txt = m_BtnExecScript->text().toStdString();
  if (txt == "Exec") {
    auto LuaScript = m_LeScript->text().toStdString();
    Network::StartLua(LuaScript);
    auto err = Network::LuaError();
    if (err.empty() == false) {
      QMessageBox::critical(this, "Error", tr(err.c_str()));
      return;
    }
    if (Network::IsLuaRunning()) {
      m_BtnExecScript->setText("Stop");
      m_ExecTimerId = startTimer(100); /* lua monitor */
    }
  } else {
    m_BtnExecScript->setText("Exec");
    Network::StopLua();
    if (0 != m_ExecTimerId) {
      killTimer(m_ExecTimerId);
      m_ExecTimerId = 0;
    }
  }
}

void UICom::timerEvent(QTimerEvent *event) {
  if (event->timerId() == m_ExecTimerId) {
    auto err = Network::LuaError();
    if (err.empty() == false) {
      QMessageBox::critical(this, "Error", tr(err.c_str()));
    }

    if (false == Network::IsLuaRunning()) {
      m_BtnExecScript->setText("Exec");
      killTimer(m_ExecTimerId);
      m_ExecTimerId = 0;
    }
  } else {
    for (auto uiMsg : m_UIMsgs) {
      uiMsg->Run();
    }
    topic::figure::FigureConfig figCfg;
    auto r = m_FigCfgMsgQue->Get(figCfg, true, 0);
    if (r) {
      auto it = m_Figures.find(figCfg.Name);
      if (it == m_Figures.end()) {
        auto figure = new UIFigure(figCfg);
        figure->show();
        m_Figures[figCfg.Name] = figure;
      } else {
        auto figure = it->second;
        if (figure->isVisible()) {
          std::string err = "already has figure " + figCfg.Name;
          QMessageBox::critical(this, "Error", tr(err.c_str()));
        } else {
          delete figure;
          auto figure = new UIFigure(figCfg);
          figure->show();
          m_Figures[figCfg.Name] = figure;
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

extern "C" std::string Name(void) {
  return "Com";
}

extern "C" QWidget *Create(void) {
  return new UICom();
}
