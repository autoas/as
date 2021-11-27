/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef STD_DEBUG_H
#define STD_DEBUG_H
/* ================================ [ INCLUDES  ] ============================================== */
#if !defined(USE_STD_PRINTF) || defined(linux) || defined(_WIN32)
#include <stdio.h>
#endif
#if defined(linux) || defined(_WIN32)
#include <stdarg.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DEFAULT 1
#define AS_LOG_DEBUG 1
#define AS_LOG_INFO 2
#define AS_LOG_WARN 3
#define AS_LOG_ERROR 4

#if defined(linux) || defined(_WIN32)
#ifndef USE_STD_DEBUG
#define USE_STD_DEBUG
#endif
#ifndef USE_STD_PRINTF
#define USE_STD_PRINTF
#endif
#if defined(linux)
#ifndef WEAK_ALIAS_PRINTF
#define WEAK_ALIAS_PRINTF __attribute__((weak, alias("asprintf")));
#endif
#else
#ifndef WEAK_ALIAS_PRINTF
#define WEAK_ALIAS_PRINTF __attribute__((weak, nonnull, alias("printf")));
#endif
#endif
#else
#define WEAK_ALIAS_PRINTF
#endif

#ifndef USE_STD_PRINTF
#define PRINTF printf
#else
#define PRINTF std_printf
#endif
#ifdef USE_STD_DEBUG
#define ASLOG(level, msg)                                                                          \
  do {                                                                                             \
    if ((AS_LOG_##level) >= AS_LOG_DEFAULT) {                                                      \
      PRINTF("%-8s:", #level);                                                                     \
      PRINTF msg;                                                                                  \
    }                                                                                              \
  } while (0)
#else
#define ASLOG(level, msg)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_STD_PRINTF
extern int std_printf(const char *fmt, ...) WEAK_ALIAS_PRINTF;
#endif
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(linux)
int __attribute__((weak)) asprintf(const char *fmt, ...) {
  va_list args;
  int length;

  va_start(args, fmt);
  length = vprintf(fmt, args);
  va_end(args);

  return length;
}
#endif
#ifdef __cplusplus
}
#endif
#endif /* STD_DEBUG_H */
