/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if (OS_PTHREAD_NUM > 0)
#include "pthread.h"
#include <unistd.h>
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static TAILQ_HEAD(sleep_list, TaskVar) OsSleepListHead;

static struct timeval timeofday;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Os_SleepInit(void) {
  /* TODO: should get timeofday from RTC*/
  timeofday.tv_sec = 0;
  timeofday.tv_usec = 0;
  TAILQ_INIT(&OsSleepListHead);
}

void Os_Sleep(TickType tick) {
  DECLARE_SMP_PROCESSOR_ID();

  EnterCritical();
  if (NULL != RunningVar) {
    Os_SleepAdd(RunningVar, tick);
    Sched_GetReady();
    Os_PortDispatch();
  }
  ExitCritical();
}

int usleep(useconds_t __useconds) {
  Os_Sleep((__useconds + USECONDS_PER_TICK - 1) / USECONDS_PER_TICK);
  return __useconds;
}
ELF_EXPORT(usleep);

long int sysconf(int parameter) {
  long int r;

  switch (parameter) {
  case _SC_CLK_TCK:
    r = OS_TICKS_PER_SECOND;
    break;
  default:
    r = 0;
    break;
  }

  return r;
}
ELF_EXPORT(sysconf);

unsigned int sleep(unsigned int __seconds) {
  Os_Sleep((__seconds * 1000000 + USECONDS_PER_TICK - 1) / USECONDS_PER_TICK);
  return 0;
}
ELF_EXPORT(sleep);

void Os_SleepTick(void) {
  TaskVarType *pTaskVar;
  TaskVarType *pNext;

  EnterCritical();
  timeofday.tv_usec += USECONDS_PER_TICK;

  if (timeofday.tv_usec > 1000000) {
    timeofday.tv_usec -= 1000000;
    timeofday.tv_sec += 1;
  }

  pTaskVar = TAILQ_FIRST(&OsSleepListHead);

  while (NULL != pTaskVar) {
    pNext = TAILQ_NEXT(pTaskVar, sentry);

    if (pTaskVar->sleep_tick > 0) {
      pTaskVar->sleep_tick--;
    }
    if (0u == pTaskVar->sleep_tick) {
      OS_TRACE_TASK_ACTIVATION(pTaskVar);
      Sched_AddReady(pTaskVar - TaskVarArray);
      Os_SleepRemove(pTaskVar);
    }

    pTaskVar = pNext;
    if ((NULL != pTaskVar) && (pTaskVar->sleep_tick > 0)) {
      break;
    }
  }
  ExitCritical();
}

void Os_SleepAdd(TaskVarType *pTaskVar, TickType ticks) {
  TaskVarType *pVar;
  TaskVarType *pPosVar = NULL;

  TAILQ_FOREACH(pVar, &OsSleepListHead, sentry) {
    if ((TickType)(pVar->sleep_tick) > ticks) {
      pPosVar = pVar;
      pVar->sleep_tick -= ticks;
      break;
    } else {
      ticks -= pVar->sleep_tick;
    }
  }

  pTaskVar->sleep_tick = ticks;
  asAssert(0u == (pTaskVar->state & PTHREAD_STATE_SLEEPING));
  pTaskVar->state |= PTHREAD_STATE_SLEEPING;

  if (NULL != pPosVar) {
    TAILQ_INSERT_BEFORE(pPosVar, pTaskVar, sentry);
  } else {
    TAILQ_INSERT_TAIL(&OsSleepListHead, pTaskVar, sentry);
  }
}

void Os_SleepRemove(TaskVarType *pTaskVar) {
  pTaskVar->state &= ~PTHREAD_STATE_SLEEPING;
  TAILQ_REMOVE(&OsSleepListHead, pTaskVar, sentry);
}
#if defined(__USE_BSD)
int gettimeofday(struct timeval *tp, struct timezone *tzp)
#else
int gettimeofday(struct timeval *tp, void *tzp)
#endif
{
  if (tp != NULL) {
    *tp = timeofday;
  }

  return 0;
}
ELF_EXPORT(gettimeofday);

TickType GetTimespecLeftTicks(const struct timespec *abstime) {
  TickType ticks;
  struct timeval tp;
  time_t sec;
  long usec;

  tp.tv_sec = abstime->tv_sec;
  tp.tv_usec = (abstime->tv_nsec + 999) / 1000;

  usec = tp.tv_usec - timeofday.tv_usec;

  if (tp.tv_sec > timeofday.tv_sec) {
    sec = tp.tv_sec - timeofday.tv_sec;
    if (usec < 0) {
      sec -= 1;
      usec = 1000000 + usec;
    }
  } else if ((tp.tv_sec == timeofday.tv_sec) && (usec > 0)) {
    sec = 0;
  } else {
    sec = 0;
    usec = 0;
  }

  ticks = sec * OS_TICKS_PER_SECOND + (usec / USECONDS_PER_TICK);

  return ticks;
}

int Os_ListWait(TaskListType *list, const struct timespec *abstime) {
  int ercd = 0;
  TickType ticks;
  DECLARE_SMP_PROCESSOR_ID();

  if (NULL != abstime) {
    ticks = GetTimespecLeftTicks(abstime);
    if (ticks > 0) {
      /* do wait event of list with timeout */
      asAssert(0u == (RunningVar->state & PTHREAD_STATE_WAITING));
      RunningVar->state |= PTHREAD_STATE_WAITING;
      RunningVar->list = list;
      TAILQ_INSERT_TAIL(list, RunningVar, entry);

      Os_SleepAdd(RunningVar, ticks);
    } else {
      /* no "-" for lwip ports/unix/sys_arch.c line 396 */
      ercd = ETIMEDOUT;
    }
  } else {
    /* do wait event of list forever*/
    asAssert(0u == (RunningVar->state & PTHREAD_STATE_WAITING));
    RunningVar->state |= PTHREAD_STATE_WAITING;
    RunningVar->list = list;
    TAILQ_INSERT_TAIL(list, RunningVar, entry);
  }

  if (0 == ercd) {
    Sched_GetReady();
    Os_PortDispatch();

    if (RunningVar->state & PTHREAD_STATE_SLEEPING) { /* event reached before timeout */
      Os_SleepRemove(RunningVar);
    }

    if (RunningVar->state & PTHREAD_STATE_WAITING) { /* this is timeout */
      RunningVar->state &= ~PTHREAD_STATE_WAITING;
      RunningVar->list = NULL;
      TAILQ_REMOVE(list, RunningVar, entry);
      /* no "-" for lwip ports/unix/sys_arch.c line 396 */
      ercd = ETIMEDOUT;
    }
  }

  return ercd;
}

int Os_ListPost(TaskListType *list, boolean schedule) {
  int ercd = 0;
  TaskVarType *pTaskVar;

  if (FALSE == TAILQ_EMPTY(list)) {
    pTaskVar = TAILQ_FIRST(list);
    TAILQ_REMOVE(list, pTaskVar, entry);
    pTaskVar->state &= ~PTHREAD_STATE_WAITING;
    pTaskVar->list = NULL;
    OS_TRACE_TASK_ACTIVATION(pTaskVar);
    Sched_AddReady(pTaskVar - TaskVarArray);
    if (schedule) {
      (void)Schedule();
    }
  } else { /* nobody is waiting in the list */
    ercd = -ENOENT;
  }

  return ercd;
}

/* make the task ready again if AddReady, else suspend it */
void Os_ListDetach(TaskVarType *pTaskVar, boolean AddReady) {
  if (NULL != pTaskVar->list) {
    asAssert(pTaskVar->state & PTHREAD_STATE_WAITING);
    TAILQ_REMOVE(pTaskVar->list, pTaskVar, entry);
  }

  if (pTaskVar->state & PTHREAD_STATE_SLEEPING) {
    Os_SleepRemove(pTaskVar);
  }

  if (AddReady) {
    pTaskVar->state = READY;
    OS_TRACE_TASK_ACTIVATION(pTaskVar);
    Sched_AddReady(pTaskVar - TaskVarArray);
  } else {
    pTaskVar->state = SUSPENDED;
  }
}
#endif
