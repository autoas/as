/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef __WINDOW_HPP__
#define __WINDOW_HPP__
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
  void action(std::string);

public slots:
  void onAction(void);

private:
  std::string m_Name;
};

class Window : public QMainWindow {
  Q_OBJECT
public:
  Window();
  ~Window();

public slots:
  void onAction(std::string name);

private:
  void creGui(void);
  void creMenu(void);
  void loadUIs(void);

private:
  QMenu *m_MenuApp;
  QTabWidget *m_Tab;
  struct UIInfo {
    std::string rpath;
    std::string name;
    CreateUIFncType createUIFnc;
    GetNameFncType getNameFnc;
    void *dll;
  };
  UIInfo m_UIs[MAX_UI];
  size_t m_UINum = 0;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __WINDOW_HPP__ */
