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
#include <ctype.h>
#include "Log.hpp"
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
int Log::s_Level = Log::INFO;
FILE *Log::s_File = stdout;
int Log::s_Ended = true;
std::mutex Log::s_Lock;
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
  LOG(INFO, "setting log level: %d\n", level);
}

int Log::getLogLevel() {
  return s_Level;
}

void Log::setLogFile(const char *path) {
  FILE *fp = fopen(path, "wb");
  if (nullptr != fp) {
    if (s_File != stdout) {
      fclose(s_File);
    } else {
      atexit(Log::close);
    }
    s_File = fp;
  }
}

void Log::setLogFile(FILE *fp) {
  s_File = fp;
}

extern "C" void std_set_log_file(const char *path) {
  Log::setLogFile(path);
}

void Log::print(int level, const char *fmt, ...) {
  va_list args;
  std::unique_lock<std::mutex> lck(s_Lock);
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

void Log::hexdump(int level, const char *prefix, const void *data, size_t size, size_t len) {
  size_t i, j;
  uint8_t *src = (uint8_t *)data;
  uint32_t offset = 0;

  std::unique_lock<std::mutex> lck(s_Lock);

  if (size <= len) {
    len = size;
    fprintf(s_File, "%s:", prefix);
  } else {
    fprintf(s_File, "%8s:", prefix);
    for (i = 0; i < len; i++) {
      fprintf(s_File, " %02X", (uint32_t)i);
    }
    fprintf(s_File, "\n");
  }

  for (i = 0; i < (size + len - 1) / len; i++) {
    if (size > len) {
      fprintf(s_File, "%08X:", (uint32_t)offset);
    }
    for (j = 0; j < len; j++) {
      if ((i * len + j) < size) {
        fprintf(s_File, " %02X", (uint32_t)src[i * len + j]);
      } else {
        fprintf(s_File, "   ");
      }
    }
    fprintf(s_File, "\t");
    for (j = 0; j < len; j++) {
      if (((i * len + j) < size) && isprint(src[i * len + j])) {
        fprintf(s_File, "%c", src[i * len + j]);
      } else {
        fprintf(s_File, ".");
      }
    }
    fprintf(s_File, "\n");
    offset += len;
  }
}

void Log::vprint(const char *fmt, va_list args) {
  std::unique_lock<std::mutex> lck(s_Lock);
  (void)vfprintf(s_File, fmt, args);
}

void Log::putc(char chr) {
  if (s_Ended) {
    float rtime = get_rel_time();
    fprintf(s_File, "%.4f ", rtime);
    s_Ended = false;
  }
  fputc(chr, s_File);
  if (chr == '\n') {
    s_Ended = true;
  }
}

void Log::close(void) {
  if (nullptr != s_File) {
    fclose(s_File);
  }
}

extern "C" int std_get_log_level(void) {
  return Log::getLogLevel();
}

extern "C" void std_set_log_level(int level) {
  Log::setLogLevel(level);
}

extern "C" void __putchar(char chr) {
  Log::putc(chr);
}

#ifndef USE_STDIO_CAN
extern "C" int std_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  (void)Log::vprint(fmt, args);
  va_end(args);
  return 0;
}
#endif
} /* namespace as */
