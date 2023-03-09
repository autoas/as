/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/queue.h>
#include "osal.h"
#include "Std_Debug.h"
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef USE_OS
#include "kernel.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef USE_OS
#define Os_MemAlloc malloc
#define Os_MemFree free
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_OS
uint8_t *Os_MemAlloc(uint32_t size);
void Os_MemFree(uint8_t *buffer);
#else
void StartupHook(void);
#endif
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
__attribute__((weak)) void StartupHook(void) {
}
/* ================================ [ FUNCTIONS ] ============================================== */
osal_thread_t osal_thread_create(osal_thread_entry_t entry, void *args) {
  int ret;
  osal_thread_t thread = Os_MemAlloc(sizeof(pthread_t));

  if (NULL != thread) {
    ret = pthread_create((pthread_t *)thread, NULL, (void *(*)(void *))entry, args);
    if (0 != ret) {
      ASLOG(ERROR, ("create thread over pthread failed: %d\n", ret));
      Os_MemFree(thread);
      thread = NULL;
    }
  }

  return thread;
}

int osal_thread_join(osal_thread_t thread) {
  return pthread_join(*(pthread_t *)thread, NULL);
}

void osal_usleep(uint32_t us) {
  usleep(us);
}

void osal_start(void) {
#ifdef USE_OS
  StartOS(OSDEFAULTAPPMODE);
#else
  StartupHook();
  while (1) {
    usleep(1000000);
  }
#endif
}