/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __TOPIC_HPP__
#define __TOPIC_HPP__
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
  std::string name;
  std::string value;
};

struct Message {
  std::string name;
  std::string network;
  std::string dir;
  std::string id;
  std::string dlc;
  std::string data;
  std::string timestamp;
  std::vector<Signal> Signals;
};

class Publisher {
public:
  Publisher(std::string name);
  ~Publisher();

  void push(std::shared_ptr<Message> msg);

private:
  MessagePublisher<std::shared_ptr<Message>> m_Pub;
};

class Subscriber {
public:
  Subscriber(std::string name, uint32_t capability = 256);
  ~Subscriber();

  std::shared_ptr<Message> pop(void);

private:
  MessageSubscriber<std::shared_ptr<Message>> m_Sub;
};

std::vector<std::string> topics();
} /* namespace com */

namespace figure {
struct Point {
  float x;
  float y;
};

enum LineType {
  LINE,
  SPLINE,
  SCATTER,
  DOT
};

struct LineConfig {
  LineType type = LineType::LINE;
  std::string name;
};

struct FigureConfig {
  std::string name;
  std::string titleX = "x";
  std::string titleY = "y";
  float minX = 0;
  float maxX = 10;
  float minY = 0;
  float maxY = 10;
  std::vector<LineConfig> lines;
};

std::shared_ptr<MessageQueue<FigureConfig>> config(void);
std::shared_ptr<MessageQueue<Point>> line(std::string figure, std::string line);
std::shared_ptr<MessageQueue<Point>> find_line(std::string figure, std::string line);

} /* namespace figure */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace topic */
} /* namespace as */
#endif /* __TOPIC_HPP__ */
