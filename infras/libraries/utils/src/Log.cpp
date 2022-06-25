/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include "Log.hpp"
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
int Log::s_Level = Log::INFO;
FILE *Log::s_File = stdout;
/* ================================ [ LOCALS    ] ============================================== */
static float get_rel_time(void) {
  static struct timeval m0 = {-1, -1};
  struct timeval m1;
  float rtim;

  if ((-1 == m0.tv_sec) && (-1 == m0.tv_usec)) {
    gettimeofday(&m0, NULL);
  }
  gettimeofday(&m1, NULL);
  rtim = m1.tv_sec - m0.tv_sec;
  if (m1.tv_usec > m0.tv_usec) {
    rtim += (float)(m1.tv_usec - m0.tv_usec) / 1000000.0;
  } else {
    rtim = rtim - 1 + (float)(1000000.0 + m1.tv_usec - m0.tv_usec) / 1000000.0;
  }

  return rtim;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Log::setLogLevel(int level) {
  s_Level = level;
}

void Log::setLogFile(const char *path) {
  FILE *fp = fopen(path, "wb");
  if (nullptr != fp) {
    s_File = fp;
  }
}

void Log::setLogFile(FILE *fp) {
  s_File = fp;
}

void Log::print(int level, const char *fmt, ...) {
  va_list args;

  if (level >= s_Level) {
    if ((0 == memcmp(fmt, "ERROR", 5)) || (0 == memcmp(fmt, "WARN", 4)) ||
        (0 == memcmp(fmt, "INFO", 4)) || (0 == memcmp(fmt, "DEBUG", 5))) {
      float rtime = get_rel_time();
      fprintf(s_File, "%.4f ", rtime);
    }
    va_start(args, fmt);
    (void)vfprintf(s_File, fmt, args);
    va_end(args);
  }
}

void Log::vprint(const char *fmt, va_list args) {
  (void)vfprintf(s_File, fmt, args);
}

} /* namespace as */
