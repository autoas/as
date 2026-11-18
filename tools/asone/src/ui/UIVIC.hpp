/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef UIVIC_HPP
#define UIVIC_HPP
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
#include "VICProxy.h"
#include "MessageQueue.hpp"
#include "Std_Timer.h"

using json = nlohmann::json;
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class VICPointer : public QGraphicsItem {
public:
  struct Config {
    int32_t X;      /* center position x */
    int32_t Y;      /* center position y */
    int32_t Offset; /* offset length of tail */
    int32_t Length; /* head and tail length */
    int32_t TailWidth;
    int32_t HeadWidth;
    uint32_t Color;
    uint32_t Start; /* software zero angle */
    uint32_t Range;
    uint32_t IsClockwise; /* rotate directin */
  };

public:
  explicit VICPointer(const Config &config, QGraphicsItem *parent = 0);
  explicit VICPointer(json &config, QGraphicsItem *parent = 0);
  ~VICPointer();
  void SetDegree(uint32_t degree);
  std::string Name() {
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
    int32_t X; /* top left position x */
    int32_t Y; /* top left position y */
    std::string P;
  };

public:
  explicit VICTelltale(const Config &config, QGraphicsItem *parent = 0);
  explicit VICTelltale(json &config, QGraphicsItem *parent = 0);
  ~VICTelltale();
  void SetOn(bool on);
  std::string Name() {
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
  void LoadJson(json &js);

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

  std::shared_ptr<MessageQueue<std::shared_ptr<Display_Type>>> m_MsgQ = nullptr;
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
  bool Load(std::string cfgPath);

private:
  VICGraphicView *m_VIC;
  QLineEdit *m_LeJson;
  QPushButton *m_BtnOpenJson;
  json m_Json;
  std::chrono::high_resolution_clock::time_point m_Start;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* UIVIC_HPP */
