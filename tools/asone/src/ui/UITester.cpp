/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "UITester.hpp"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <unistd.h>
#include <string>
#include <errno.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
UITester::UITester() {
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("Test Case Specification Excel:"), 0, 0);
  m_LeExcel = new QLineEdit();
  grid->addWidget(m_LeExcel, 0, 1);
  m_BtnOpenExcel = new QPushButton("...");
  grid->addWidget(m_BtnOpenExcel, 0, 2);

  grid->addWidget(new QLabel("Progress"), 2, 0);
  m_PgbProgress = new QProgressBar();
  m_PgbProgress->setRange(0, 100);
  grid->addWidget(m_PgbProgress, 2, 1);
  m_BtnStart = new QPushButton("Start");
  grid->addWidget(m_BtnStart, 2, 2);
  vbox->addLayout(grid);
  m_TeInfo = new QTextEdit();
  m_TeInfo->setReadOnly(true);
  vbox->addWidget(m_TeInfo);
  setLayout(vbox);

  std::string dftExcel = "tools/asone/examples/TestCaseSpecification.xlsx";
  if (0 == access(dftExcel.c_str(), F_OK | R_OK)) {
    m_LeExcel->setText(tr(dftExcel.c_str()));
  }

  dftExcel = "../../../../tools/asone/examples/TestCaseSpecification.xlsx";
  if (0 == access(dftExcel.c_str(), F_OK | R_OK)) {
    m_LeExcel->setText(tr(dftExcel.c_str()));
  }

  connect(m_BtnOpenExcel, SIGNAL(clicked()), this, SLOT(on_btnOpenExcel_clicked()));
  connect(m_BtnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
}

UITester::~UITester() {
}

void UITester::on_btnOpenExcel_clicked(void) {
  auto fileName =
    QFileDialog::getOpenFileName(this, "Test Case Specification Excel", "", "Excel (*.xlsx)");
  m_LeExcel->setText(fileName);
}

void UITester::on_btnStart_clicked(void) {
  std::string filePath = m_LeExcel->text().toStdString();
  int ret = m_XLTester.Start(filePath);
  if (0 == ret) {
    m_TeInfo->append(tr(m_XLTester.GetMsg().c_str()));
    m_PgbProgress->setValue(0);
    startTimer(1);
    m_BtnStart->setDisabled(true);
  } else {
    m_TeInfo->append(tr(m_XLTester.GetMsg().c_str()));
  }
}

void UITester::timerEvent(QTimerEvent *event) {
  int progress = m_XLTester.GetProgress();
  auto msg = m_XLTester.GetMsg();

  if (msg.size() > 0) {
    m_TeInfo->append(tr(msg.c_str()));
  }

  if (progress >= 0) {
    m_PgbProgress->setValue(progress / (XLTESTER_PROGRESS_DONE / 100.f));
  }

  if ((progress < 0) || (progress >= XLTESTER_PROGRESS_DONE)) {
    killTimer(event->timerId());
    m_BtnStart->setDisabled(false);
    m_TeInfo->append(tr("-------------------- END --------------------\n"));
  }
}

extern "C" std::string Name(void) {
  return "Tester";
}

extern "C" QWidget *Create(void) {
  return new UITester();
}
