/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Critical.h"
#include <pthread.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static pthread_mutex_t cirtical_mutex;
/* ================================ [ LOCALS    ] ============================================== */
static void __attribute__((constructor)) __CriticalInit(void) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&cirtical_mutex, &attr);
}
/* ================================ [ FUNCTIONS ] ============================================== */
imask_t Std_EnterCritical(void) {
  pthread_mutex_lock(&cirtical_mutex);
  return 0;
}

void Std_ExitCritical(imask_t mask) {
  pthread_mutex_unlock(&cirtical_mutex);
}