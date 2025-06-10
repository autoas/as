/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "figure.hpp"
#include "Log.hpp"
#include <assert.h>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace as {
namespace figure {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
UIChartView::UIChartView() : QChartView() {
}

UIChartView::UIChartView(QChart *chart) : QChartView(chart) {
}

UIChartView::~UIChartView() {
}

void UIChartView::mouseMoveEvent(QMouseEvent *event) {
  if (m_LeftButtonPressed) {
    QPoint oDeltaPos = event->pos() - m_oPrePos;
    chart()->scroll(-oDeltaPos.x(), oDeltaPos.y());
    m_oPrePos = event->pos();
  }
  QChartView::mouseMoveEvent(event);
}

void UIChartView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_LeftButtonPressed = true;
    m_oPrePos = event->pos();
    setCursor(Qt::OpenHandCursor);
  }
  QChartView::mousePressEvent(event);
}

void UIChartView::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_LeftButtonPressed = false;
    setCursor(Qt::ArrowCursor);
  }
  QChartView::mouseReleaseEvent(event);
}

void UIChartView::wheelEvent(QWheelEvent *event) {
  qreal rVal = std::pow(0.999, event->angleDelta().y());

  QRectF oPlotAreaRect = chart()->plotArea();
  QPointF oCenterPoint = oPlotAreaRect.center();

  oPlotAreaRect.setWidth(oPlotAreaRect.width() * rVal);
  oPlotAreaRect.setHeight(oPlotAreaRect.height() * rVal);

  QPointF oNewCenterPoint(2 * oCenterPoint - event->position() -
                          (oCenterPoint - event->position()) / rVal);

  oPlotAreaRect.moveCenter(oNewCenterPoint);

  chart()->zoomIn(oPlotAreaRect);
  QChartView::wheelEvent(event);
}

UIFigure::UIFigure(std::shared_ptr<Signal> sig, float scale, int32_t offset)
  : m_Signal(sig), m_Scale(scale), m_Offset(offset) {
  auto vbox = new QVBoxLayout();

  m_axisX = new QValueAxis();
  m_axisY = new QValueAxis();
  m_axisX->setTitleText("time");
  m_axisY->setTitleText(tr(m_Signal->name().c_str()));
  m_axisX->setMin(0);
  m_axisY->setMax(0);
  m_axisX->setMax(10);
  m_axisY->setMax(10);

  m_Chart = new QChart();
  m_Chart->setTitle(tr(m_Signal->name().c_str()));
  m_Chart->setAnimationOptions(QChart::AllAnimations);
  m_Chart->addAxis(m_axisY, Qt::AlignLeft);
  m_Chart->addAxis(m_axisX, Qt::AlignBottom);

  m_Line = new QLineSeries(m_Chart);
  m_Chart->addSeries(m_Line);
  m_Line->attachAxis(m_axisX);
  m_Line->attachAxis(m_axisY);

  auto hbox = new QHBoxLayout();
  m_btnClear = new QPushButton("clear");
  m_btnClear->setMaximumWidth(100);
  hbox->addWidget(m_btnClear);
  vbox->addLayout(hbox);

  m_ChartView = new UIChartView(m_Chart);
  vbox->addWidget(m_ChartView);
  setLayout(vbox);

  m_Signal->createViewMsgQueue();
  m_FigureType = FigureType::SIGNAL;
  startTimer(100);

  connect(m_btnClear, SIGNAL(clicked()), this, SLOT(on_btnClear_clicked()));
}

UIFigure::UIFigure(topic::figure::FigureConfig &config) : m_FigureConfig(config) {
  auto vbox = new QVBoxLayout();

  m_axisX = new QValueAxis();
  m_axisY = new QValueAxis();
  m_axisX->setTitleText(tr(m_FigureConfig.titleX.c_str()));
  m_axisY->setTitleText(tr(m_FigureConfig.titleY.c_str()));
  m_axisX->setMin(m_FigureConfig.minX);
  m_axisY->setMax(m_FigureConfig.minY);
  m_axisX->setMax(m_FigureConfig.maxX);
  m_axisY->setMax(m_FigureConfig.maxY);

  m_Chart = new QChart();
  m_Chart->setTitle(tr(m_FigureConfig.name.c_str()));
  m_Chart->setAnimationOptions(QChart::AllAnimations);
  m_Chart->addAxis(m_axisY, Qt::AlignLeft);
  m_Chart->addAxis(m_axisX, Qt::AlignBottom);

  for (auto line : m_FigureConfig.lines) {
    LineHolder lh;
    switch (line.type) {
    case topic::figure::LineType::LINE:
      lh.line = new QLineSeries(m_Chart);
      m_Chart->addSeries(lh.line);
      lh.line->attachAxis(m_axisX);
      lh.line->attachAxis(m_axisY);
      lh.line->setName(tr(line.name.c_str()));
      break;
    case topic::figure::LineType::SPLINE:
      lh.spline = new QSplineSeries(m_Chart);
      m_Chart->addSeries(lh.spline);
      lh.spline->attachAxis(m_axisX);
      lh.spline->attachAxis(m_axisY);
      lh.spline->setName(tr(line.name.c_str()));
      break;
    case topic::figure::LineType::SCATTER:
      lh.scatter = new QScatterSeries(m_Chart);
      m_Chart->addSeries(lh.scatter);
      lh.scatter->attachAxis(m_axisX);
      lh.scatter->attachAxis(m_axisY);
      lh.scatter->setName(tr(line.name.c_str()));
      lh.scatter->setMarkerSize(10);
      lh.scatter->setPointsVisible(true);
      break;
    case topic::figure::LineType::DOT:
      lh.line = new QLineSeries(m_Chart);
      m_Chart->addSeries(lh.line);
      lh.line->attachAxis(m_axisX);
      lh.line->attachAxis(m_axisY);
      lh.line->setName(tr(line.name.c_str()));
      lh.line->setMarkerSize(10);
      lh.line->setPointsVisible(true);
      break;
    default:
      assert(0);
      break;
    }

    lh.msgQue = topic::figure::line(m_FigureConfig.name, line.name);
    m_Lines.push_back(lh);
  }

  auto hbox = new QHBoxLayout();
  m_btnClear = new QPushButton("clear");
  m_btnClear->setMaximumWidth(100);
  hbox->addWidget(m_btnClear);
  vbox->addLayout(hbox);

  m_ChartView = new UIChartView(m_Chart);
  vbox->addWidget(m_ChartView);
  setLayout(vbox);

  setMinimumSize(500, 400);
  m_FigureType = FigureType::FIGURE;
  startTimer(100);

  connect(m_btnClear, SIGNAL(clicked()), this, SLOT(on_btnClear_clicked()));
}

void UIFigure::updateSignal(void) {
  auto vmsgs = m_Signal->view();
  for (auto &msg : vmsgs) {
    float value = m_Scale * (msg.value - m_Offset);
    m_Line->append(QPointF(msg.time, value));
    if (msg.time > m_axisX->max()) {
      m_axisX->setMax(msg.time + 10);
      if (msg.time > 90) {
        m_axisX->setMin(msg.time - 90);
      }
    }
    if (value > m_axisY->max()) {
      m_axisY->setMax(value);
    }
    if (value < m_axisY->min()) {
      m_axisY->setMin(value);
    }
  }
}

void UIFigure::updateFigure(void) {
  int idx = 0;
  for (auto line : m_FigureConfig.lines) {
    LineHolder lh = m_Lines[idx];
    topic::figure::Point p;
    auto msgQue = lh.msgQue;
    bool r = msgQue->get(p, true, 0);
    while (r) {
      switch (line.type) {
      case topic::figure::LineType::LINE:
        lh.line->append(QPointF(p.x, p.y));
        break;
      case topic::figure::LineType::SPLINE:
        lh.spline->append(QPointF(p.x, p.y));
        break;
      case topic::figure::LineType::SCATTER:
        lh.scatter->append(QPointF(p.x, p.y));
        break;
      case topic::figure::LineType::DOT:
        lh.line->append(QPointF(p.x, p.y));
        break;
      default:
        assert(0);
        break;
      }
      if (p.x > m_axisX->max()) {
        m_axisX->setMax(p.x + 1);
      }
      if (p.x < m_axisX->min()) {
        m_axisX->setMin(p.x - 1);
      }
      if (p.y > m_axisY->max()) {
        m_axisY->setMax(p.y + 1);
      }
      if (p.y < m_axisY->min()) {
        m_axisY->setMin(p.y - 1);
      }
      r = msgQue->get(p, true, 0);
    }
    idx++;
  }
}

void UIFigure::timerEvent(QTimerEvent *event) {
  (void)event;
  if (FigureType::SIGNAL == m_FigureType) {
    updateSignal();
  } else {
    updateFigure();
  }
}

void UIFigure::on_btnClear_clicked(void) {
  if (FigureType::SIGNAL == m_FigureType) {
    m_Line->clear();
  } else {
    int idx = 0;
    for (auto line : m_FigureConfig.lines) {
      LineHolder lh = m_Lines[idx];
      switch (line.type) {
      case topic::figure::LineType::LINE:
        lh.line->clear();
        break;
      case topic::figure::LineType::SPLINE:
        lh.spline->clear();
        break;
      case topic::figure::LineType::SCATTER:
        lh.scatter->clear();
        break;
      case topic::figure::LineType::DOT:
        lh.line->clear();
        break;
      default:
        assert(0);
        break;
      }

      idx++;
    }
  }
}

UIFigure::~UIFigure() {
  if (m_Signal) {
    m_Signal->destoryViewMsgQueue();
  }
}

} // namespace figure
} /* namespace as */
