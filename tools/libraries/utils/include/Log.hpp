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
#include <memory>
#include <string>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
#define LOG(level, ...) Log::print(Logger::level, #level ": " __VA_ARGS__)
/* ================================ [ TYPES     ] ============================================== */
class Logger {
public:
  enum {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR
  };

public:
  Logger();
  Logger(std::string name, std::string format = "txt");

  void setName(std::string name, std::string format = "txt");
  void setMaxSize(int sz);
  void setMaxNum(int num);
  void setLogLevel(int level);
  int getLogLevel();

  void write(const char *fmt, ...);
  void check(void);

  void print(const char *fmt, ...);
  void print(int level, const char *fmt, ...);
  void vprint(int level, const char *fmt, va_list args);

  void hexdump(int level, const char *prefix, const void *data, size_t size, size_t len = 16);
  void vprint(const char *fmt, va_list args);
  void putc(char chr);

  FILE *getFile();

  ~Logger();

private:
  void open(void);
  int getFileIndex(void);
  void setFileIndex(int index);

private:
  int m_Level = INFO;
  int m_Ended = true;
  FILE *m_File = nullptr;
  std::mutex m_Lock;
  std::string m_Name;
  std::string m_Format;
  int m_FileIndex = 0;
  int m_FileMaxSize = 32 * 1024 * 1024;
  int m_FileMaxNum = 10;
};

class Log {
public:
  static void setLogLevel(int level);
  static int getLogLevel();
  static void setName(std::string name);
  static void print(int level, const char *fmt, ...);
  static void hexdump(int level, const char *prefix, const void *data, size_t size,
                      size_t len = 16);
  static void vprint(const char *fmt, va_list args);
  static void putc(char chr);
  static FILE *getFile();

private:
  static std::shared_ptr<Logger> s_Logger;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* _LOG_HPP_ */
