/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef WINDOW_HPP
#define WINDOW_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <QMainWindow>
#include <QAction>
#include <QMenuBar>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <vector>
#include <string>
/* ================================ [ MACROS    ] ============================================== */
#define MAX_UI 32
/* ================================ [ TYPES     ] ============================================== */
typedef QWidget *(*CreateUIFncType)(void);
typedef std::string (*GetNameFncType)(void);

class Action : public QAction {
  Q_OBJECT
public:
  Action(std::string name, QWidget *parent);
  ~Action();

signals:
  void ActionRequested(std::string);

public slots:
  void OnTriggered(void);

private:
  std::string m_Name;
};

class Window : public QMainWindow {
  Q_OBJECT
public:
  Window();
  ~Window();

public slots:
  void OnAction(std::string name);

private:
  void CreateGui(void);
  void CreateMenu(void);
  void LoadUis(void);

private:
  QMenu *m_MenuApp;
  QTabWidget *m_Tab;
  struct UIInfo {
    std::string Rpath;
    std::string Name;
    CreateUIFncType CreateUiFnc;
    GetNameFncType GetNameFnc;
    void *Dll;
  };
  UIInfo m_UIs[MAX_UI];
  size_t m_UINum = 0;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* WINDOW_HPP */
