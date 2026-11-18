/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "UIVIC.hpp"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <unistd.h>
#include "SomeIp.h"
#include "Sd.h"
#include "SoAd.h"
#include "TcpIp.h"
#include "plugin.h"
#include "Log.hpp"
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
VICPointer::VICPointer(const Config &config, QGraphicsItem *parent)
  : QGraphicsItem(parent), m_Config(config) {
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setCacheMode(QGraphicsItem::DeviceCoordinateCache);
  setZValue(1);
  SetDegree(0);
}

VICPointer::VICPointer(json &config, QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setCacheMode(QGraphicsItem::DeviceCoordinateCache);
  setZValue(1);
  m_Name = config["Name"].get<std::string>();
  m_Config.X = config["X"].get<int32_t>();
  m_Config.Y = config["Y"].get<int32_t>();
  m_Config.Offset = config["Offset"].get<int32_t>();
  m_Config.Length = config["Length"].get<int32_t>();
  m_Config.TailWidth = config["TailWidth"].get<int32_t>();
  m_Config.HeadWidth = config["HeadWidth"].get<int32_t>();
  m_Config.Color = strtoul(config["Color"].get<std::string>().c_str(), nullptr, 16);
  m_Config.Start = config["Start"].get<uint32_t>();
  m_Config.Range = config["Range"].get<uint32_t>();
  m_Config.IsClockwise = config["IsClockwise"].get<bool>();
  SetDegree(0);
}

QRectF VICPointer::boundingRect() const {
  return QRectF(-m_Config.Length - m_Config.Offset, -m_Config.Length - m_Config.Offset,
                (m_Config.Length + m_Config.Offset) * 2, (m_Config.Length + m_Config.Offset) * 2);
}

void VICPointer::SetDegree(uint32_t degree) {
  degree = degree / 100;
  if (degree > m_Config.Range) {
    degree = m_Config.Range;
  }

  if (m_Config.IsClockwise) {
    degree = degree + m_Config.Start;
  } else {
    if (m_Config.Start > degree) {
      degree = m_Config.Start - degree;
    } else {
      degree = 360 + degree - m_Config.Start;
    }
  }

  m_Degree = degree;
  setRotation(degree);
}

void VICPointer::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  (void)widget;
  (void)option;
  uint32_t radius, radius2;
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(QColor((m_Config.Color >> 16) & 0xFF, (m_Config.Color >> 8) & 0xFF,
                                  (m_Config.Color >> 0) & 0xFF)));
  QPolygon polygon(4);
  polygon.setPoint(0, QPoint(-m_Config.Offset, m_Config.TailWidth / 2));
  polygon.setPoint(1, QPoint(-m_Config.Offset - m_Config.Length, m_Config.HeadWidth / 2));
  polygon.setPoint(2, QPoint(-m_Config.Offset - m_Config.Length, -m_Config.HeadWidth / 2));
  polygon.setPoint(3, QPoint(-m_Config.Offset, -m_Config.TailWidth / 2));
  painter->drawConvexPolygon(polygon);
  if (m_Config.Offset < 0) {
    radius = -m_Config.Offset;
  } else if (m_Config.Offset > 0) {
    radius = m_Config.Offset;
  } else {
    radius = m_Config.TailWidth;
  }
  radius += 2;
  painter->drawEllipse(-radius, -radius, radius * 2, radius * 2);
  painter->setBrush(QBrush(QColor(0, 0, 0))); /* black */
  radius2 = radius * 2 / 3;
  if ((int32_t)radius2 > m_Config.TailWidth) {
    radius2 = radius - m_Config.TailWidth * 2 / 3;
  }
  painter->drawEllipse(-radius2, -radius2, radius2 * 2, radius2 * 2);
  setPos(m_Config.X, m_Config.Y);

  setRotation(m_Degree);
}

VICPointer::~VICPointer() {
}

VICTelltale::VICTelltale(const Config &config, QGraphicsItem *parent)
  : QGraphicsItem(parent), m_Config(config) {
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setCacheMode(QGraphicsItem::DeviceCoordinateCache);
  setZValue(1);
  m_Image = QImage(m_Config.P.c_str());
  SetOn(true);
}

VICTelltale::VICTelltale(json &config, QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setCacheMode(QGraphicsItem::DeviceCoordinateCache);
  setZValue(1);
  m_Name = config["Name"].get<std::string>();
  m_Config.X = config["X"].get<int32_t>();
  m_Config.Y = config["Y"].get<int32_t>();
  m_Config.P = config["P"].get<std::string>();
  m_Image = QImage(m_Config.P.c_str());
  SetOn(true);
}

QRectF VICTelltale::boundingRect() const {
  return QRectF(m_Config.X, m_Config.Y, m_Image.size().width(), m_Image.size().height());
}

void VICTelltale::SetOn(bool on) {
  m_On = on;
  setVisible(on);
}

void VICTelltale::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                        QWidget *widget) {
  (void)option;
  (void)widget;
  painter->drawImage(m_Config.X, m_Config.Y, m_Image);
  setVisible(m_On);
}

VICTelltale::~VICTelltale() {
}

VICGraphicView::VICGraphicView(QWidget *parent) : QGraphicsView(parent) {
  m_Scene = new QGraphicsScene(this);
  setScene(m_Scene);
  setCacheMode(QGraphicsView::CacheBackground);
  setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
  setRenderHint(QPainter::Antialiasing);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);

  m_MsgQ = MessageQueue<std::shared_ptr<Display_Type>>::Add("VIC");

  startTimer(1);
}

void VICGraphicView::LoadJson(json &js) {
  m_Json = js;
  if (m_Loaded) {
    m_Scene->clear();
  }
  m_Loaded = false;
  try {
    auto bgPath = m_Json["Background"].get<std::string>();
    m_Background = QImage(tr(bgPath.c_str()));
    for (auto &cfg : m_Json["Telltales"]) {
      auto tt = new VICTelltale(cfg);
      m_MapTelltales[tt->Name()] = tt;
      m_Scene->addItem(tt);
    }
    for (auto &cfg : m_Json["Pointers"]) {
      auto ptr = new VICPointer(cfg);
      m_MapPointers[ptr->Name()] = ptr;
      m_Scene->addItem(ptr);
    }
    m_Loaded = true;
  } catch (std::exception &e) {
    std::string msg(std::string("json error: ") + e.what());
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  }
}

void VICGraphicView::drawBackground(QPainter *painter, const QRectF &rect) {
  if (false == m_Loaded) {
    return;
  }
  auto w = m_Background.size().width();
  auto h = m_Background.size().height();
  resize(w, h);
  scene()->setSceneRect(0, 0, w, h);
  painter->drawImage(0, 0, m_Background);
  (void)rect;
}

void VICGraphicView::timerEvent(QTimerEvent *e) {
  (void)e;
  std::shared_ptr<Display_Type> msg;
  auto ret = m_MsgQ->Get(msg, false, 0);
  if (true == ret) {
    Display_Type &display = *msg;
    for (int i = 0; i < display.gaugesLen; i++) {
      auto &gauge = display.gauges[i];
      std::string name((char *)gauge.name);
      auto degree = gauge.degree;
      auto it = m_MapPointers.find(name);
      if (it != m_MapPointers.end()) {
        auto ptr = it->second;
        ptr->SetDegree(degree);
      } else {
        LOG(ERROR, "pointer %s is not found\n", name.c_str());
      }
    }

    for (int i = 0; i < display.telltalesLen; i++) {
      auto &telltale = display.telltales[i];
      std::string name((char *)telltale.name);
      auto on = telltale.on;
      auto it = m_MapTelltales.find(name);
      if (it != m_MapTelltales.end()) {
        auto tt = it->second;
        tt->SetOn(on);
      } else {
        LOG(ERROR, "telltale %s is not found\n", name.c_str());
      }
    }
  }
}

VICGraphicView::~VICGraphicView() {
}

UIVIC::UIVIC() : QWidget() {
  auto vbox = new QVBoxLayout();
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("load json config:"), 0, 0);
  m_LeJson = new QLineEdit();
  grid->addWidget(m_LeJson, 0, 1);
  m_BtnOpenJson = new QPushButton("...");
  grid->addWidget(m_BtnOpenJson, 0, 2);
  vbox->addLayout(grid);
  m_VIC = new VICGraphicView(this);
  vbox->addWidget(m_VIC);
  setLayout(vbox);

  connect(m_BtnOpenJson, SIGNAL(clicked()), this, SLOT(on_btnOpenJson_clicked()));

  std::string dft = "vic.json";
  auto r = Load(dft);
  if (r) {
    m_LeJson->setText(tr(dft.c_str()));
    m_VIC->LoadJson(m_Json);
  }

  LOG(INFO, "starting SOMEIP/SD\n");
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);
  plugin_init();
  startTimer(1);
  m_Start = std::chrono::high_resolution_clock::now();
}

void UIVIC::timerEvent(QTimerEvent *e) {
  (void)e;
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_Start);
  uint64_t nTicks = elapsed.count() / 10;
  for (uint64_t i = 0; i < nTicks; i++) {
    TcpIp_MainFunction();
    SoAd_MainFunction();
    Sd_MainFunction();
    SomeIp_MainFunction();
    plugin_main();
  }
  m_Start += nTicks * std::chrono::milliseconds(10);
}

bool UIVIC::Load(std::string cfgPath) {
  bool ret = true;

  FILE *fp = fopen(cfgPath.c_str(), "rb");
  if (nullptr == fp) {
    std::string msg("config file " + cfgPath + " not exists");
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
    ret = false;
  }

  if (true == ret) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::unique_ptr<char[]> text(new char[size + 1]);
    fread(text.get(), size, 1, fp);
    char *eol = (char *)text.get();
    eol[size] = 0;
    try {
      m_Json = json::parse(text.get());
    } catch (std::exception &e) {
      ret = false;
      std::string msg("config file " + cfgPath + " error: " + e.what());
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
    }
  }

  return ret;
}

void UIVIC::on_btnOpenJson_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "json config", "", "json config (*.json)");

  auto r = Load(fileName.toStdString());
  if (r) {
    m_LeJson->setText(fileName);
    m_VIC->LoadJson(m_Json);
  }
}

UIVIC::~UIVIC() {
}

extern "C" std::string Name(void) {
  return "VIC";
}

extern "C" QWidget *Create(void) {
  return new UIVIC();
}
