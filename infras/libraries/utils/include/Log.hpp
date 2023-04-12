/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _LOG_HPP_
#define _LOG_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <inttypes.h>
#include <stdio.h>
#include <mutex>
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
  static int getLogLevel();
  static void print(int level, const char *fmt, ...);
  static void hexdump(int level, const char *prefix, const void *data, size_t size,
                      size_t len = 16);
  static void vprint(const char *fmt, va_list args);
  static void putc(char chr);
  static void setLogFile(const char *path);
  static void setLogFile(FILE *fp);
  static void close(void);

private:
  static int s_Level;
  static int s_Ended;
  static FILE *s_File;
  static std::mutex s_Lock;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* _LOG_HPP_ */
