/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef LOG_HPP
#define LOG_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <inttypes.h>
#include <stdio.h>
#include <mutex>
#include <memory>
#include <string>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
#define LOG(level, ...) Log::Print(Logger::level, #level ": " __VA_ARGS__)
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

  void SetName(std::string name, std::string format = "txt");
  void SetMaxSize(int sz);
  void SetMaxNum(int num);
  void SetLogLevel(int level);
  int GetLogLevel();

  void Write(const char *fmt, ...);
  void Check(void);

  void Print(const char *fmt, ...);
  void Print(int level, const char *fmt, ...);
  void VPrint(int level, const char *fmt, va_list args);

  void HexDump(int level, const char *prefix, const void *data, size_t size, size_t len = 16);
  void VPrint(const char *fmt, va_list args);
  void PutChar(char chr);

  FILE *GetFile();

  ~Logger();

private:
  void Open(void);
  int GetFileIndex(void);
  void SetFileIndex(int index);

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
  static void SetLogLevel(int level);
  static int GetLogLevel();
  static void SetName(std::string name);
  static void Print(int level, const char *fmt, ...);
  static void HexDump(int level, const char *prefix, const void *data, size_t size,
                      size_t len = 16);
  static void VPrint(const char *fmt, va_list args);
  static void PutChar(char chr);
  static FILE *GetFile();

private:
  static std::shared_ptr<Logger> s_Logger;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* LOG_HPP */
