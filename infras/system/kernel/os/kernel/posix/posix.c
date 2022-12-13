/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if (OS_PTHREAD_NUM > 0)
#include "pthread.h"
#include <errno.h>
#include <stdlib.h>
#include "Std_Debug.h"
#ifdef USE_PTHREAD_SIGNAL
#include "signal.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
TASK(TaskPosix) {
  EventMaskType event;
  StatusType ercd;

  while (1) {
    /* -1: waiting all event */
    ercd = WaitEvent((EventMaskType)-1);
    if (E_OK == ercd) {
      (void)GetEvent(TASK_ID_TaskPosix, &event);
      (void)ClearEvent(event);

#ifdef USE_PTHREAD_SIGNAL
      if (EVENT_MASK_TaskPosix_EVENT_SIGALRM & event) {
        Os_SignalBroadCast(SIGALRM);
      }
#endif
    }
  }
  TerminateTask();
}
#endif /* OS_PTHREAD_NUM */
