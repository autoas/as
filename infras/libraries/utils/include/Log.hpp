/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _LOG_HPP_
#define _LOG_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <inttypes.h>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
#define LOG(level, ...) Log::print(Log::level, #level ": " __VA_ARGS__)
/* ================================ [ TYPES     ] ============================================== */
class Log {
public:
  enum
  {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR
  };

public:
  static void setLogLevel(int level);
  static void print(int level, const char *fmt, ...);

private:
  static int s_Level;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* _LOG_HPP_ */
