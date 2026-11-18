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
    QPoint oDeltaPos = event->pos() - m_OPrePos;
    chart()->scroll(-oDeltaPos.x(), oDeltaPos.y());
    m_OPrePos = event->pos();
  }
  QChartView::mouseMoveEvent(event);
}

void UIChartView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_LeftButtonPressed = true;
    m_OPrePos = event->pos();
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

  m_AxisX = new QValueAxis();
  m_AxisY = new QValueAxis();
  m_AxisX->setTitleText("time");
  m_AxisY->setTitleText(tr(m_Signal->Name().c_str()));
  m_AxisX->setMin(0);
  m_AxisY->setMax(0);
  m_AxisX->setMax(10);
  m_AxisY->setMax(10);

  m_Chart = new QChart();
  m_Chart->setTitle(tr(m_Signal->Name().c_str()));
  m_Chart->setAnimationOptions(QChart::AllAnimations);
  m_Chart->addAxis(m_AxisY, Qt::AlignLeft);
  m_Chart->addAxis(m_AxisX, Qt::AlignBottom);

  m_Line = new QLineSeries(m_Chart);
  m_Chart->addSeries(m_Line);
  m_Line->attachAxis(m_AxisX);
  m_Line->attachAxis(m_AxisY);

  auto hbox = new QHBoxLayout();
  m_BtnClear = new QPushButton("clear");
  m_BtnClear->setMaximumWidth(100);
  hbox->addWidget(m_BtnClear);
  vbox->addLayout(hbox);

  m_ChartView = new UIChartView(m_Chart);
  vbox->addWidget(m_ChartView);
  setLayout(vbox);

  m_Signal->CreateViewMsgQueue();
  m_FigureType = FigureType::SIGNAL;
  startTimer(100);

  connect(m_BtnClear, SIGNAL(clicked()), this, SLOT(on_btnClear_clicked()));
}

UIFigure::UIFigure(topic::figure::FigureConfig &config) : m_FigureConfig(config) {
  auto vbox = new QVBoxLayout();

  m_AxisX = new QValueAxis();
  m_AxisY = new QValueAxis();
  m_AxisX->setTitleText(tr(m_FigureConfig.TitleX.c_str()));
  m_AxisY->setTitleText(tr(m_FigureConfig.TitleY.c_str()));
  m_AxisX->setMin(m_FigureConfig.MinX);
  m_AxisY->setMax(m_FigureConfig.MinY);
  m_AxisX->setMax(m_FigureConfig.MaxX);
  m_AxisY->setMax(m_FigureConfig.MaxY);

  m_Chart = new QChart();
  m_Chart->setTitle(tr(m_FigureConfig.Name.c_str()));
  m_Chart->setAnimationOptions(QChart::AllAnimations);
  m_Chart->addAxis(m_AxisY, Qt::AlignLeft);
  m_Chart->addAxis(m_AxisX, Qt::AlignBottom);

  for (auto line : m_FigureConfig.Lines) {
    LineHolder lh;
    switch (line.Type) {
    case topic::figure::LineType::LINE:
      lh.line = new QLineSeries(m_Chart);
      m_Chart->addSeries(lh.line);
      lh.line->attachAxis(m_AxisX);
      lh.line->attachAxis(m_AxisY);
      lh.line->setName(tr(line.Name.c_str()));
      break;
    case topic::figure::LineType::SPLINE:
      lh.spline = new QSplineSeries(m_Chart);
      m_Chart->addSeries(lh.spline);
      lh.spline->attachAxis(m_AxisX);
      lh.spline->attachAxis(m_AxisY);
      lh.spline->setName(tr(line.Name.c_str()));
      break;
    case topic::figure::LineType::SCATTER:
      lh.scatter = new QScatterSeries(m_Chart);
      m_Chart->addSeries(lh.scatter);
      lh.scatter->attachAxis(m_AxisX);
      lh.scatter->attachAxis(m_AxisY);
      lh.scatter->setName(tr(line.Name.c_str()));
      lh.scatter->setMarkerSize(10);
      lh.scatter->setPointsVisible(true);
      break;
    case topic::figure::LineType::DOT:
      lh.line = new QLineSeries(m_Chart);
      m_Chart->addSeries(lh.line);
      lh.line->attachAxis(m_AxisX);
      lh.line->attachAxis(m_AxisY);
      lh.line->setName(tr(line.Name.c_str()));
      lh.line->setMarkerSize(10);
      lh.line->setPointsVisible(true);
      break;
    default:
      assert(0);
      break;
    }

    lh.msgQue = topic::figure::Line(m_FigureConfig.Name, line.Name);
    m_Lines.push_back(lh);
  }

  auto hbox = new QHBoxLayout();
  m_BtnClear = new QPushButton("clear");
  m_BtnClear->setMaximumWidth(100);
  hbox->addWidget(m_BtnClear);
  vbox->addLayout(hbox);

  m_ChartView = new UIChartView(m_Chart);
  vbox->addWidget(m_ChartView);
  setLayout(vbox);

  setMinimumSize(500, 400);
  m_FigureType = FigureType::FIGURE;
  startTimer(100);

  connect(m_BtnClear, SIGNAL(clicked()), this, SLOT(on_btnClear_clicked()));
}

void UIFigure::UpdateSignal(void) {
  auto vmsgs = m_Signal->View();
  for (auto &msg : vmsgs) {
    float value = m_Scale * (msg.Value - m_Offset);
    m_Line->append(QPointF(msg.Time, value));
    if (msg.Time > m_AxisX->max()) {
      m_AxisX->setMax(msg.Time + 10);
      if (msg.Time > 90) {
        m_AxisX->setMin(msg.Time - 90);
      }
    }
    if (value > m_AxisY->max()) {
      m_AxisY->setMax(value);
    }
    if (value < m_AxisY->min()) {
      m_AxisY->setMin(value);
    }
  }
}

void UIFigure::UpdateFigure(void) {
  int idx = 0;
  for (auto line : m_FigureConfig.Lines) {
    LineHolder lh = m_Lines[idx];
    topic::figure::Point p;
    auto msgQue = lh.msgQue;
    bool r = msgQue->Get(p, true, 0);
    while (r) {
      switch (line.Type) {
      case topic::figure::LineType::LINE:
        lh.line->append(QPointF(p.X, p.Y));
        break;
      case topic::figure::LineType::SPLINE:
        lh.spline->append(QPointF(p.X, p.Y));
        break;
      case topic::figure::LineType::SCATTER:
        lh.scatter->append(QPointF(p.X, p.Y));
        break;
      case topic::figure::LineType::DOT:
        lh.line->append(QPointF(p.X, p.Y));
        break;
      default:
        assert(0);
        break;
      }
      if (p.X > m_AxisX->max()) {
        m_AxisX->setMax(p.X + 1);
      }
      if (p.X < m_AxisX->min()) {
        m_AxisX->setMin(p.X - 1);
      }
      if (p.Y > m_AxisY->max()) {
        m_AxisY->setMax(p.Y + 1);
      }
      if (p.Y < m_AxisY->min()) {
        m_AxisY->setMin(p.Y - 1);
      }
      r = msgQue->Get(p, true, 0);
    }
    idx++;
  }
}

void UIFigure::timerEvent(QTimerEvent *event) {
  (void)event;
  if (FigureType::SIGNAL == m_FigureType) {
    UpdateSignal();
  } else {
    UpdateFigure();
  }
}

void UIFigure::on_btnClear_clicked(void) {
  if (FigureType::SIGNAL == m_FigureType) {
    m_Line->clear();
  } else {
    int idx = 0;
    for (auto line : m_FigureConfig.Lines) {
      LineHolder lh = m_Lines[idx];
      switch (line.Type) {
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
    m_Signal->DestroyViewMsgQueue();
  }
}

} // namespace figure
} /* namespace as */
