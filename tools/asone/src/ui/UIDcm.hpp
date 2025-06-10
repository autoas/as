/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __UIDCM_HPP__
#define __UIDCM_HPP__
/* ================================ [ INCLUDES  ] ============================================== */
#include "UICommon.hpp"
using namespace as::ui ::common;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class UIDcm;

class UISessionControl : public QGroupBox {
  Q_OBJECT
public:
  UISessionControl(json &js, UIDcm *dcm);
  ~UISessionControl();

public slots:
  void on_btnEnter_clicked(void);

private:
  QComboBox *m_cmbxSessions;
  QPushButton *m_btnEnter;
  std::map<std::string, uint8_t> m_SessionMap;
  UIDcm *m_Dcm;
};

typedef std::vector<uint8_t> (*calculate_key_t)(std::vector<uint8_t> res);

class UISecurityAccess : public QGroupBox {
  Q_OBJECT
public:
  UISecurityAccess(json &js, UIDcm *dcm);
  ~UISecurityAccess();

public slots:
  void on_btnUnlock_clicked(void);

private:
  struct Info {
    uint8_t level;
    std::string m_Scripts;
    void *dll = nullptr;
    calculate_key_t calculate_key = nullptr;
  };
  QComboBox *m_cmbxSecurityLevels;
  QPushButton *m_btnUnlock;
  std::map<std::string, Info> m_SecurityMap;
  UIDcm *m_Dcm;

private:
  std::vector<uint8_t> CalculateKey(Info &info, std::vector<uint8_t> res);
};

class UIService : public UIServiceBase {
  Q_OBJECT
public:
  UIService(json &js, UIDcm *dcm);
  ~UIService();

public slots:
  void on_btnEnter_clicked(void);

private:
  UIDcm *m_Dcm;
};

class Dcm;

class UIGroup : public QScrollArea {
  Q_OBJECT
public:
  UIGroup(json &js, UIDcm *dcm, UITreeItem<UIDcm> *topItem);
  ~UIGroup();
};

class UIDcm : public QWidget {
  Q_OBJECT
public:
  UIDcm();
  ~UIDcm();

public:
  int transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res);
  std::shared_ptr<AsLuaScript> luaScript() {
    return m_Lua;
  }
  QTreeWidget *tree(void);

public slots:
  void on_btnOpenDcmJs_clicked(void);
  void on_btnStart_clicked(void);
  void on_cbxTesterPresent_stateChanged(int state);
  void on_Tree_itemSelectionChanged(void);

private:
  void timerEvent(QTimerEvent *event);

private:
  bool load(std::string cfgPath);
  bool loadUI(void);
  bool loadScript(void);

private:
  QLineEdit *m_leDcmJs;
  QPushButton *m_btnOpenDcmJs;
  QPushButton *m_btnStart;
  QTabWidget *m_tabWidget;
  QTreeWidget *m_Tree;
  QComboBox *m_cmbxTarget;
  QCheckBox *m_cbxTesterPresent;

  int m_TPtimer = 0;

  json m_Json;

  std::string m_Dir;
  std::shared_ptr<AsLuaScript> m_Lua = nullptr;
  std::shared_ptr<Dcm> m_Dcm = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __UIDCM_HPP__ */
