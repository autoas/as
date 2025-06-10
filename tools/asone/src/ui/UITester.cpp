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
  m_leExcel = new QLineEdit();
  grid->addWidget(m_leExcel, 0, 1);
  m_btnOpenExcel = new QPushButton("...");
  grid->addWidget(m_btnOpenExcel, 0, 2);

  grid->addWidget(new QLabel("Progress"), 2, 0);
  m_pgbProgress = new QProgressBar();
  m_pgbProgress->setRange(0, 100);
  grid->addWidget(m_pgbProgress, 2, 1);
  m_btnStart = new QPushButton("start");
  grid->addWidget(m_btnStart, 2, 2);
  vbox->addLayout(grid);
  m_teInfo = new QTextEdit();
  m_teInfo->setReadOnly(true);
  vbox->addWidget(m_teInfo);
  setLayout(vbox);

  std::string dftExcel = "tools/asone/examples/TestCaseSpecification.xlsx";
  if (0 == access(dftExcel.c_str(), F_OK | R_OK)) {
    m_leExcel->setText(tr(dftExcel.c_str()));
  }

  dftExcel = "../../../../tools/asone/examples/TestCaseSpecification.xlsx";
  if (0 == access(dftExcel.c_str(), F_OK | R_OK)) {
    m_leExcel->setText(tr(dftExcel.c_str()));
  }

  connect(m_btnOpenExcel, SIGNAL(clicked()), this, SLOT(on_btnOpenExcel_clicked()));
  connect(m_btnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
}

UITester::~UITester() {
}

void UITester::on_btnOpenExcel_clicked(void) {
  auto fileName =
    QFileDialog::getOpenFileName(this, "Test Case Specification Excel", "", "Excel (*.xlsx)");
  m_leExcel->setText(fileName);
}

void UITester::on_btnStart_clicked(void) {
  std::string filePath = m_leExcel->text().toStdString();
  int ret = m_XLTester.start(filePath);
  if (0 == ret) {
    m_teInfo->append(tr(m_XLTester.getMsg().c_str()));
    m_pgbProgress->setValue(0);
    startTimer(1);
    m_btnStart->setDisabled(true);
  } else {
    m_teInfo->append(tr(m_XLTester.getMsg().c_str()));
  }
}

void UITester::timerEvent(QTimerEvent *event) {
  int progress = m_XLTester.getProgress();
  auto msg = m_XLTester.getMsg();

  if (msg.size() > 0) {
    m_teInfo->append(tr(msg.c_str()));
  }

  if (progress >= 0) {
    m_pgbProgress->setValue(progress / (XLTESTER_PROGRESS_DONE / 100.f));
  }

  if ((progress < 0) || (progress >= XLTESTER_PROGRESS_DONE)) {
    killTimer(event->timerId());
    m_btnStart->setDisabled(false);
    m_teInfo->append(tr("-------------------- END --------------------\n"));
  }
}

extern "C" std::string name(void) {
  return "Tester";
}

extern "C" QWidget *create(void) {
  return new UITester();
}
