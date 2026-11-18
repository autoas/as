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
#include "PAL.h"
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
static std::vector<std::string> s_UIs;
/* ================================ [ FUNCTIONS ] ============================================== */
Action::Action(std::string name, QWidget *parent)
  : QAction(tr(name.c_str()), parent), m_Name(name) {
  connect(this, SIGNAL(triggered()), SLOT(OnTriggered()));
}

void Action::OnTriggered(void) {
  emit ActionRequested(m_Name);
}

Action::~Action() {
}

Window::Window() : QMainWindow() {
  setWindowTitle(tr("AsOne"));
  LoadUis();
  CreateGui();
}

void Window::LoadUis(void) {
  DIR *d = opendir(".");
  struct dirent *file;
  while ((file = readdir(d)) != NULL) {
    if (strstr(file->d_name, DLL)) {
      UIInfo uinfo;
      uinfo.Dll = PAL_DlOpen(file->d_name);
      if (nullptr != uinfo.Dll) {
        uinfo.CreateUiFnc = (CreateUIFncType)PAL_DlSym(uinfo.Dll, "Create");
        uinfo.GetNameFnc = (GetNameFncType)PAL_DlSym(uinfo.Dll, "Name");
        if ((nullptr != uinfo.CreateUiFnc) && (nullptr != uinfo.GetNameFnc)) {
          uinfo.Rpath = file->d_name;
          uinfo.Name = uinfo.GetNameFnc();
          if (m_UINum < MAX_UI) {
            m_UIs[m_UINum++] = uinfo;
          }
        }
      } else {
        LOG(ERROR, "fail to load UI %s: %s\n", file->d_name, PAL_DlErr());
      }
    }
  }
  closedir(d);
}

void Window::CreateGui(void) {
  auto wid = new QWidget();
  auto grid = new QVBoxLayout();
  m_Tab = new QTabWidget(this);
  grid->addWidget(m_Tab);
  wid->setLayout(grid);
  setCentralWidget(wid);
  CreateMenu();
  // setMinimumSize(1000, 800);
  if (s_UIs.size() > 0) {
    for (auto &name : s_UIs) {
      OnAction(name);
    }
  } else {
    OnAction("Trace");
    for (size_t i = 0; i < m_UINum; i++) {
      auto &uinfo = m_UIs[i];
      if (uinfo.Name != "VIC") {
        OnAction(uinfo.Name);
      }
    }
  }
}

void Window::CreateMenu(void) {
  m_MenuApp = menuBar()->addMenu(tr("App"));
  for (size_t i = 0; i < m_UINum; i++) {
    auto &uinfo = m_UIs[i];
    auto sItem = new Action(uinfo.Name, this);
    connect(sItem, SIGNAL(ActionRequested(std::string)), this, SLOT(OnAction(std::string)));
    m_MenuApp->addAction(sItem);
  }
}

void Window::OnAction(std::string name) {
  for (int i = 0; i < m_Tab->count(); i++) {
    if (name == m_Tab->tabText(i).toStdString()) {
      return;
    }
  }
  for (size_t i = 0; i < m_UINum; i++) {
    auto &uinfo = m_UIs[i];
    if (uinfo.Name == name) {
      m_Tab->addTab(uinfo.CreateUiFnc(), tr(name.c_str()));
    }
  }
}

Window::~Window() {
}

int main(int argc, char *argv[]) {
  int ch;
  opterr = 0;
  int d = 1;
  while ((ch = getopt(argc, argv, "dsu:v")) != -1) {
    switch (ch) {
    case 'd':
      d = 0;
      break;
    case 'v':
      Log::SetLogLevel(Logger::DEBUG);
      break;
    case 'u':
      s_UIs.push_back(optarg);
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
    Log::SetName("AsOne");
  }
  Window w;
  w.show();
  return a.exec();
}
