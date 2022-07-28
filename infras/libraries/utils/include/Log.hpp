/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _LOG_HPP_
#define _LOG_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <inttypes.h>
#include <stdio.h>
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
  static void vprint(const char *fmt, va_list args);
  static void setLogFile(const char *path);
  static void setLogFile(FILE *fp);
  static void close(void);

private:
  static int s_Level;
  static FILE *s_File;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* _LOG_HPP_ */
