/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef STD_TIMER_H
#define STD_TIMER_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#if defined(linux) || defined(_WIN32)
#define STD_TIME_MAX ((std_time_t)0xFFFFFFFFFFFFFFFF)
#else
#define STD_TIME_MAX ((std_time_t)0xFFFFFFFFUL)
#endif

#define PERF_BEGIN()                                                                               \
  do {                                                                                             \
    Std_TimerType _perfTimer;                                                                      \
  Std_TimerStart(&_perfTimer)

#define PERF_END(desc)                                                                             \
  printf("\n%s: cost %.2f ms\n", desc, (float)Std_GetTimerElapsedTime(&_perfTimer) / 1000.0);      \
  }                                                                                                \
  while (0)

#define STD_TIMER_ONE_SECOND ((std_time_t)1000000000)
#define STD_TIMER_ONE_MILISECOND ((std_time_t)1000000)
/* ================================ [ TYPES     ] ============================================== */
#if defined(linux) || defined(_WIN32) || defined(USE_STD_TIME_64)
typedef uint64_t std_time_t;
#else
typedef uint32_t std_time_t;
#endif
typedef struct {
  std_time_t time; /* time in us(microseconds), range 0~4294.9 seconds */
  uint8_t status;  /* 1: started, 0: stopped */
} Std_TimerType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
std_time_t Std_GetTime(void);
void Std_Sleep(std_time_t time);
void Std_TimerStart(Std_TimerType *timer);
void Std_TimerStop(Std_TimerType *timer);
bool Std_IsTimerStarted(Std_TimerType *timer);
std_time_t Std_GetTimerElapsedTime(Std_TimerType *timer);

void Std_TimerSet(Std_TimerType *timer, std_time_t timeout);
bool Std_IsTimerTimeout(Std_TimerType *timer);
/* for log purpose, return a time in string format: Year-Month-Day-Hour-Minute-Second-Milisecond */
void Std_GetDateTime(char *ts, size_t sz);
#ifdef __cplusplus
}
#endif
#endif /* STD_TIMER_H */
