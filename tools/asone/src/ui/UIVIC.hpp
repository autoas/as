/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef __UIVIC_HPP__
#define __UIVIC_HPP__
/* ================================ [ INCLUDES  ] ============================================== */
#include <QWidget>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QImage>
#include <QLineEdit>
#include <QPushButton>
#include <stdint.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <map>
#include <chrono>
#include "usomeip/usomeip.hpp"
#include "MessageQueue.hpp"
#include "Std_Timer.h"

using json = nlohmann::json;
using namespace as;
using namespace as::usomeip;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class VICPointer : public QGraphicsItem {
public:
  struct Config {
    int32_t x;      /* center position x */
    int32_t y;      /* center position y */
    int32_t offset; /* offset length of tail */
    int32_t length; /* head and tail length */
    int32_t tail_width;
    int32_t head_width;
    uint32_t color;
    uint32_t start; /* software zero angle */
    uint32_t range;
    uint32_t is_clockwise; /* rotate directin */
  };

public:
  explicit VICPointer(const Config &config, QGraphicsItem *parent = 0);
  explicit VICPointer(json &config, QGraphicsItem *parent = 0);
  ~VICPointer();
  void setDegree(uint32_t degree);
  std::string name() {
    return m_Name;
  }

private:
  QRectF boundingRect() const;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
  uint32_t m_Degree = 0;
  Config m_Config;
  std::string m_Name;
};

class VICTelltale : public QGraphicsItem {
public:
  struct Config {
    int32_t x; /* top left position x */
    int32_t y; /* top left position y */
    std::string p;
  };

public:
  explicit VICTelltale(const Config &config, QGraphicsItem *parent = 0);
  explicit VICTelltale(json &config, QGraphicsItem *parent = 0);
  ~VICTelltale();
  void setOn(bool on);
  std::string name() {
    return m_Name;
  }

private:
  QRectF boundingRect() const;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
  bool m_On = false;
  Config m_Config;
  std::string m_Name;
  QImage m_Image;
};

class VICGraphicView : public QGraphicsView {
  Q_OBJECT
public:
  explicit VICGraphicView(QWidget *parent = 0);
  ~VICGraphicView();
  void loadJson(json &js);

private:
  void drawBackground(QPainter *painter, const QRectF &rect);

private slots:
  void timerEvent(QTimerEvent *e);

private:
  std::map<std::string, VICPointer *> m_MapPointers;
  std::map<std::string, VICTelltale *> m_MapTelltales;
  json m_Json;

  QGraphicsScene *m_Scene;
  QImage m_Background;
  bool m_Loaded = false;

  std::shared_ptr<MessageQueue<std::shared_ptr<Message>>> m_MsgQ = nullptr;
};

class UIVIC : public QWidget {
  Q_OBJECT
public:
  UIVIC();
  ~UIVIC();

public slots:
  void on_btnOpenJson_clicked(void);

private:
  void timerEvent(QTimerEvent *event);

private:
  bool load(std::string cfgPath);

private:
  VICGraphicView *m_VIC;
  QLineEdit *m_leJson;
  QPushButton *m_btnOpenJson;
  json m_Json;
  std::chrono::high_resolution_clock::time_point m_Start;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __UIVIC_HPP__ */
