/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023-2024 Parai Wang <parai@foxmail.com>
 */
#ifndef __UIXCP_HPP__
#define __UIXCP_HPP__
/* ================================ [ INCLUDES  ] ============================================== */
#include "UICommon.hpp"
using namespace as::ui ::common;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class UIXcp;

class UIService : public UIServiceBase {
  Q_OBJECT
public:
  UIService(json &js, UIXcp *xcp);
  ~UIService();

public slots:
  void on_btnEnter_clicked(void);

private:
  UIXcp *m_Xcp;
};

class UIMeasure : public UIServiceBase {
  Q_OBJECT
public:
  UIMeasure(json &js, UIXcp *xcp, BitArray::Endian endian);
  ~UIMeasure();

public slots:
  void on_btnRead_clicked(void);
  void on_btnWrite_clicked(void);

private:
  uint32_t m_Addr;
  size_t m_BitSize = 0;
  std::vector<std::shared_ptr<UIData::Data>> m_Datas;
  UIXcp *m_Xcp;
  BitArray::Endian m_Endian;

  QPushButton *m_btnRead;
  QPushButton *m_btnWrite;
};

class Xcp;

class UIGroup : public QScrollArea {
  Q_OBJECT
public:
  UIGroup(json &js, UIXcp *xcp, UITreeItem<UIXcp> *topItem, int tabIndex);
  UIGroup(json &js, UIXcp *xcp, UITreeItem<UIXcp> *topItem, int tabIndex, BitArray::Endian endian);
  ~UIGroup();
};

class UIXcp : public QWidget {
  Q_OBJECT
public:
  UIXcp();
  ~UIXcp();

public:
  void empty_rx_queue();
  int transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res);
  int receive(std::vector<uint8_t> &res);
  std::shared_ptr<AsLuaScript> luaScript() {
    return m_Lua;
  }
  QTreeWidget *tree(void);

public slots:
  void on_btnOpenXcpJs_clicked(void);
  void on_btnOpenEcuJs_clicked(void);
  void on_btnStart_clicked(void);
  void on_Tree_itemSelectionChanged(void);

private:
  void timerEvent(QTimerEvent *event);

private:
  bool load(std::string cfgPath, json &js);
  bool loadUI(void);
  bool loadScript(void);

private:
  QLineEdit *m_leXcpJs;
  QPushButton *m_btnOpenXcpJs;
  QLineEdit *m_leEcuJs;
  QPushButton *m_btnOpenEcuJs;
  QPushButton *m_btnStart;
  QTabWidget *m_tabWidget;
  QTreeWidget *m_Tree;
  QComboBox *m_cmbxTarget;

  json m_Json;

  json m_EcuJson;

  std::string m_Dir;
  std::shared_ptr<AsLuaScript> m_Lua = nullptr;
  std::shared_ptr<Xcp> m_Xcp = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __UIXCP_HPP__ */
