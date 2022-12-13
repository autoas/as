/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if (OS_PTHREAD_NUM > 0)
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#ifdef USE_PTHREAD_SIGNAL
#include "signal.h"
#endif
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_TIMER 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static inline TickType timeval2Ticks(const struct timeval *val) { /* no consideration overflow */
  return (OS_TICKS_PER_SECOND * val->tv_sec) +
         ((val->tv_usec + USECONDS_PER_TICK - 1) / USECONDS_PER_TICK);
}

static inline void ticks2Timeval(struct timeval *val, TickType v) {
  val->tv_sec = v / OS_TICKS_PER_SECOND;
  val->tv_usec = (v % OS_TICKS_PER_SECOND) * USECONDS_PER_TICK;
}
#ifdef USE_PTHREAD_SIGNAL
static int start_itimer(AlarmType AlarmId, const struct itimerval *new, struct itimerval *old) {
  int ercd;
  TickType start;
  TickType period;

  if (new != NULL) {
    if (old != NULL) {
      if (E_OK == GetAlarm(AlarmId, &start)) { /* alarm is running */
        period = AlarmVarArray[AlarmId].period;
        ticks2Timeval(&(old->it_value), start);
        ticks2Timeval(&(old->it_interval), start);
      } else {
        memset(old, 0, sizeof(struct itimerval));
      }
    }

    start = timeval2Ticks(&(new->it_value));
    period = timeval2Ticks(&(new->it_interval));

    ASLOG(TIMER, ("setitimer %d %u %u\n", AlarmId, start, period));
    (void)CancelAlarm(AlarmId);
    ercd = SetRelAlarm(AlarmId, start, period);
  } else {
    ercd = -EINVAL;
  }

  return ercd;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
int getitimer(int which, struct itimerval *old) {
  return 0;
}
ELF_EXPORT(getitimer);

int setitimer(int which, const struct itimerval *new, struct itimerval *old) {
  int ercd = 0;

  switch (which) {
#ifdef USE_PTHREAD_SIGNAL
  case ITIMER_REAL:
    ercd = start_itimer(ALARM_ID_Alarm_SIGALRM, new, old);
    break;
#endif
  default:
    ercd = -EINVAL;
    break;
  }

  return ercd;
}
ELF_EXPORT(setitimer);

clock_t times(struct tms *buffer) {
  buffer->tms_stime = OsTickCounter;
  buffer->tms_cstime = OsTickCounter;
  buffer->tms_stime = OsTickCounter;
  buffer->tms_utime = OsTickCounter;

  return OsTickCounter;
}
ELF_EXPORT(times);

#endif /* OS_PTHREAD_NUM */
