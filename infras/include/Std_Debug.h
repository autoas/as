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
#ifndef AS_LOG_DEFAULT
#define AS_LOG_DEFAULT 1
#endif
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
#ifndef WEAK_ALIAS_PRINTF
#define WEAK_ALIAS_PRINTF __attribute__((weak, alias("_asprintf")));
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

#define ASHEXDUMP(level, msg, data, size)                                                          \
  do {                                                                                             \
    if ((AS_LOG_##level) >= AS_LOG_DEFAULT) {                                                      \
      uint8_t *pData = (uint8_t *)(data);                                                          \
      uint32_t __index;                                                                            \
      PRINTF("%-8s:", #level);                                                                     \
      PRINTF msg;                                                                                  \
      for (__index = 0; __index < (size); __index++) {                                             \
        if (0 == (__index & 0x1F)) {                                                               \
          PRINTF("\n  %08X ", __index);                                                            \
        }                                                                                          \
        PRINTF("%02X ", pData[__index]);                                                           \
      }                                                                                            \
      if (0 != (__index & 0x1F)) {                                                                 \
        PRINTF("\n");                                                                              \
      }                                                                                            \
    }                                                                                              \
  } while (0)
#else
#define ASLOG(level, msg)
#define ASHEXDUMP(level, msg, data, size)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_STD_PRINTF
extern int std_printf(const char *fmt, ...) WEAK_ALIAS_PRINTF;
#endif
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(linux) || defined(_WIN32)
int __attribute__((weak)) _asprintf(const char *fmt, ...) {
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
