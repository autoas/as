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
/* ================================ [ MACROS    ] ============================================== */
#define STD_TIME_MAX 0xFFFFFFFFUL

#define PERF_BEGIN()                                                                               \
  do {                                                                                             \
    Std_TimerType _perfTimer;                                                                      \
    Std_TimerStart(&_perfTimer)

#define PERF_END(desc)                                                                             \
    printf("\n%s: cost %.2f ms\n", desc, (float)Std_GetTimerElapsedTime(&_perfTimer) / 1000.0);    \
  } while (0)
/* ================================ [ TYPES     ] ============================================== */
typedef uint32_t std_time_t;
typedef struct {
  std_time_t time; /* time in us(microseconds), range 0~4294.9 seconds */
  uint8_t status;  /* 1: started, 0: stopped */
} Std_TimerType;
/* ================================ [ DECLARES  ] ============================================== */
extern std_time_t Std_GetTime(void);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Std_TimerStart(Std_TimerType *timer);
void Std_TimerStop(Std_TimerType *timer);
bool Std_IsTimerStarted(Std_TimerType *timer);
std_time_t Std_GetTimerElapsedTime(Std_TimerType *timer);

void Std_TimerSet(Std_TimerType *timer, std_time_t timeout);
bool Std_IsTimerTimeout(Std_TimerType *timer);
#endif /* STD_TIMER_H */