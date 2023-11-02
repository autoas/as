/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <QApplication>
#include "window.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <dlfcn.h>
#include "Log.hpp"
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#ifdef _WIN32
#define DLL ".dll"
#else
#define DLL ".so"
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Action::Action(std::string name, QWidget *parent)
  : QAction(tr(name.c_str()), parent), m_Name(name) {
  connect(this, SIGNAL(triggered()), SLOT(onAction()));
}

void Action::onAction(void) {
  emit action(m_Name);
}

Action::~Action() {
}

Window::Window() : QMainWindow() {
  setWindowTitle(tr("AsOne"));
  loadUIs();
  creGui();
}

void Window::loadUIs(void) {
  DIR *d = opendir(".");
  struct dirent *file;
  while ((file = readdir(d)) != NULL) {
    if (strstr(file->d_name, DLL)) {
      UIInfo uinfo;
      uinfo.dll = dlopen(file->d_name, RTLD_NOW);
      if (nullptr != uinfo.dll) {
        uinfo.createUIFnc = (CreateUIFncType)dlsym(uinfo.dll, "create");
        uinfo.getNameFnc = (GetNameFncType)dlsym(uinfo.dll, "name");
        if ((nullptr != uinfo.createUIFnc) && (nullptr != uinfo.getNameFnc)) {
          uinfo.rpath = file->d_name;
          uinfo.name = uinfo.getNameFnc();
          if (m_UINum < MAX_UI) {
            m_UIs[m_UINum++] = uinfo;
          }
        }
      } else {
        LOG(ERROR, "fail to load UI %s: %s\n", file->d_name, dlerror());
      }
    }
  }
  closedir(d);
}

void Window::creGui(void) {
  auto wid = new QWidget();
  auto grid = new QVBoxLayout();
  m_Tab = new QTabWidget(this);
  grid->addWidget(m_Tab);
  wid->setLayout(grid);
  setCentralWidget(wid);
  creMenu();
  setMinimumSize(1000, 800);

  for (size_t i = 0; i < m_UINum; i++) {
    auto &uinfo = m_UIs[i];
    onAction(uinfo.name);
  }
}

void Window::creMenu(void) {
  m_MenuApp = menuBar()->addMenu(tr("App"));
  for (size_t i = 0; i < m_UINum; i++) {
    auto &uinfo = m_UIs[i];
    auto sItem = new Action(uinfo.name, this);
    connect(sItem, SIGNAL(action(std::string)), this, SLOT(onAction(std::string)));
    m_MenuApp->addAction(sItem);
  }
}

void Window::onAction(std::string name) {
  for (int i = 0; i < m_Tab->count(); i++) {
    if (name == m_Tab->tabText(i).toStdString()) {
      return;
    }
  }
  for (size_t i = 0; i < m_UINum; i++) {
    auto &uinfo = m_UIs[i];
    if (uinfo.name == name) {
      m_Tab->addTab(uinfo.createUIFnc(), tr(name.c_str()));
    }
  }
}

Window::~Window() {
}

int main(int argc, char *argv[]) {
  int ch;
  opterr = 0;
  int d = 1;
  while ((ch = getopt(argc, argv, "dsv")) != -1) {
    switch (ch) {
    case 'd':
      d = 0;
      break;
    case 'v':
      Log::setLogLevel(Logger::DEBUG);
      break;
    case 's':
      /* https://doc.qt.io/archives/qt-5.6/highdpi.html */
      qputenv("QT_SCALE_FACTOR", "0.75");
      break;
    default:
      break;
    }
  }
  QApplication a(argc, argv);
  if (d) {
    Log::setName("AsOne");
  }
  Window w;
  w.show();
  return a.exec();
}
