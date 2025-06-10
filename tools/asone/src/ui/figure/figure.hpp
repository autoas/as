/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __FIGURE_HPP__
#define __FIGURE_HPP__
/* ================================ [ INCLUDES  ] ============================================== */
#include <vector>
#include <string>
#include <stdint.h>
#include <iomanip>
#include <sstream>
#include <map>

#include <QChartView>
#include <QChart>
#include <QLineSeries>
#include <QSplineSeries>
#include <QScatterSeries>
#include <QValueAxis>
#include <QPushButton>

#include "Std_Timer.h"
#include "topic.hpp"
#include "signal.hpp"

namespace as {
namespace figure {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class UIChartView : public QChartView {
  Q_OBJECT
public:
  UIChartView();
  UIChartView(QChart *chart);
  ~UIChartView();

protected:
  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void mouseReleaseEvent(QMouseEvent *event) override;
  virtual void wheelEvent(QWheelEvent *event) override;

private:
  bool m_LeftButtonPressed = false;
  QPoint m_oPrePos = {0, 0};
};

class UIFigure : public QWidget {
  Q_OBJECT
public:
  enum FigureType {
    SIGNAL, /* legacy figure for COM signal */
    FIGURE, /* general figure */
  };

public:
  UIFigure(std::shared_ptr<Signal> sig, float scale = 1.0, int32_t offset = 0);
  UIFigure(topic::figure::FigureConfig &config);
  ~UIFigure();

public slots:
  void on_btnClear_clicked(void);

private:
  void updateSignal(void);
  void updateFigure(void);
  void timerEvent(QTimerEvent *event);

private:
  /* general stuff */
  UIChartView *m_ChartView;
  QChart *m_Chart;
  QValueAxis *m_axisX;
  QValueAxis *m_axisY;

  FigureType m_FigureType;

  QPushButton *m_btnClear;

  /* stuff for COM signal */
  std::shared_ptr<Signal> m_Signal = nullptr;
  float m_Scale;
  int32_t m_Offset;
  QLineSeries *m_Line = nullptr;

  /* stuff for general figure */
  topic::figure::FigureConfig m_FigureConfig;
  struct LineHolder {
    union {
      QLineSeries *line;
      QSplineSeries *spline;
      QScatterSeries *scatter;
      QAbstractSeries *abs = nullptr;
    };
    std::shared_ptr<MessageQueue<topic::figure::Point>> msgQue;
  };

  std::vector<LineHolder> m_Lines;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace figure */
} /* namespace as */
#endif /* __FIGURE_HPP__ */
