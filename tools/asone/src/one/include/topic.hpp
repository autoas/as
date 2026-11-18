/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef TOPIC_HPP
#define TOPIC_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <vector>
#include <string>
#include <stdint.h>
#include <iomanip>
#include <sstream>
#include "Std_Timer.h"
#include "MessageQueue.hpp"
namespace as {
namespace topic {

/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
namespace com {
struct Signal {
  std::string Name;
  std::string Value;
};

struct Message {
  std::string Name;
  std::string Network;
  std::string Dir;
  std::string Id;
  std::string Dlc;
  std::string Data;
  std::string Timestamp;
  std::vector<Signal> Signals;
};

class Publisher {
public:
  Publisher(std::string name);
  ~Publisher();

  void Push(std::shared_ptr<Message> msg);

private:
  MessagePublisher<std::shared_ptr<Message>> m_Pub;
};

class Subscriber {
public:
  Subscriber(std::string name, uint32_t capability = 256);
  ~Subscriber();

  std::shared_ptr<Message> Pop(void);

private:
  MessageSubscriber<std::shared_ptr<Message>> m_Sub;
};

std::vector<std::string> Topics();
} /* namespace com */

namespace figure {
struct Point {
  float X;
  float Y;
};

enum LineType {
  LINE,
  SPLINE,
  SCATTER,
  DOT
};

struct LineConfig {
  LineType Type = LineType::LINE;
  std::string Name;
};

struct FigureConfig {
  std::string Name;
  std::string TitleX = "X";
  std::string TitleY = "Y";
  float MinX = 0;
  float MaxX = 10;
  float MinY = 0;
  float MaxY = 10;
  std::vector<LineConfig> Lines;
};

std::shared_ptr<MessageQueue<FigureConfig>> Config(void);
std::shared_ptr<MessageQueue<Point>> Line(std::string figure, std::string line);
std::shared_ptr<MessageQueue<Point>> FindLine(std::string figure, std::string line);

} /* namespace figure */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace topic */
} /* namespace as */
#endif /* TOPIC_HPP */
