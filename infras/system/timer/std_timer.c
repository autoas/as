/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Timer.h"
#include "Std_Debug.h"
#if defined(linux) || defined(_WIN32)
#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#include <synchapi.h>
#endif
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#define STD_TIMER_STARTED 1
#define STD_TIMER_SET_NO_OVERFLOW 2
#define STD_TIMER_SET_OVERFLOW 3

#define STD_TIMER_SET_MAX (STD_TIME_MAX / 2)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#if defined(_WIN32)
static void __std_timer_deinit(void) {
  TIMECAPS xTimeCaps;

  if (timeGetDevCaps(&xTimeCaps, sizeof(xTimeCaps)) == MMSYSERR_NOERROR) {
    /* Match the call to timeBeginPeriod( xTimeCaps.wPeriodMin ) made when
    the process started with a timeEndPeriod() as the process exits. */
    timeEndPeriod(xTimeCaps.wPeriodMin);
  }
}

static void __attribute__((constructor)) __std_timer_init(void) {
  TIMECAPS xTimeCaps;
  if (timeGetDevCaps(&xTimeCaps, sizeof(xTimeCaps)) == MMSYSERR_NOERROR) {
    timeBeginPeriod(xTimeCaps.wPeriodMin);
    atexit(__std_timer_deinit);
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(linux) || defined(_WIN32)
std_time_t Std_GetTime(void) {
  struct timeval now;
  std_time_t tm;

  (void)gettimeofday(&now, NULL);
  tm = (std_time_t)(now.tv_sec * 1000000 + now.tv_usec);

  return tm;
}

void Std_GetDateTime(char *ts, size_t sz) {
  uint32_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint32_t miniseconds = 0;

#if defined(_WIN32)
  SYSTEMTIME st = {0};
  GetLocalTime(&st);
  year = st.wYear;
  month = st.wMonth;
  day = st.wDay;
  hour = st.wHour;
  minute = st.wMinute;
  second = st.wSecond;
  miniseconds = st.wMilliseconds;
#else
  time_t t = time(0);
  struct tm *lt = localtime(&t);
  year = (1900 + lt->tm_year);
  month = lt->tm_mon + 1;
  day = lt->tm_mday;
  hour = lt->tm_hour;
  minute = lt->tm_min;
  second = lt->tm_sec;
#endif

  snprintf(ts, sz, "%d-%02d-%02d %02d:%02d:%02d:%d", year, month, day, hour, minute, second,
           miniseconds);
}

void Std_Sleep(std_time_t time) {
#if defined(_WIN32)
  Sleep(time / 1000);
#else
  usleep(time);
#endif
}
#endif

void Std_TimerStart(Std_TimerType *timer) {
  timer->status = STD_TIMER_STARTED;
  timer->time = Std_GetTime();
}

void Std_TimerStop(Std_TimerType *timer) {
  timer->status = 0;
  /* timer->time = 0; */
}

bool Std_IsTimerStarted(Std_TimerType *timer) {
  return (timer->status != 0);
}

std_time_t Std_GetTimerElapsedTime(Std_TimerType *timer) {
  std_time_t curTime;
  std_time_t elapsed = 0;

  if (timer->status) {
    curTime = Std_GetTime();
    if (curTime > timer->time) {
      elapsed = curTime - timer->time;
    } else {
      elapsed = (STD_TIME_MAX - timer->time) + 1 + curTime;
    }
  }

  return elapsed;
}

void Std_TimerSet(Std_TimerType *timer, std_time_t timeout) {
  std_time_t curTime = Std_GetTime();

  asAssert(timeout <= STD_TIMER_SET_MAX);

  if (timeout <= (STD_TIME_MAX - curTime)) {
    timer->time = curTime + timeout;
    timer->status = STD_TIMER_SET_NO_OVERFLOW;
  } else {
    timer->time = timeout - 1 - (STD_TIME_MAX - curTime);
    timer->status = STD_TIMER_SET_OVERFLOW;
  }
}

bool Std_IsTimerTimeout(Std_TimerType *timer) {
  bool r = false;
  std_time_t curTime = Std_GetTime();
  /* 0 -------- 1/4 -------- 1/2 -------- 3/4 -------- MAX */
  switch (timer->status) {
  case STD_TIMER_SET_NO_OVERFLOW:
    if (curTime >= timer->time) {
      r = true;
    } else if ((timer->time > STD_TIMER_SET_MAX) && (curTime < STD_TIMER_SET_MAX)) {
      r = true;
    }
    break;
  case STD_TIMER_SET_OVERFLOW:
    if ((curTime <= STD_TIMER_SET_MAX) && (curTime >= timer->time)) {
      r = true;
    } else if ((timer->time > (STD_TIME_MAX / 4)) && (curTime > STD_TIMER_SET_MAX) &&
               (curTime < ((STD_TIME_MAX / 4 * 3)))) {
      r = true;
    }
    break;
  }

  return r;
}
