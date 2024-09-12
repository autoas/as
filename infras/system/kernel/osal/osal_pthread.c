/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/queue.h>
#include "osal.h"
#include "Std_Debug.h"
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include "Std_Compiler.h"

#ifdef USE_OS
#include "kernel.h"
#endif

#if !defined(USE_OS) && (defined(_WIN32) || defined(linux))
#include <dlfcn.h>
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
FUNC(void, __weak) StartupHook(void) {
}
/* ================================ [ FUNCTIONS ] ============================================== */
OSAL_ThreadType OSAL_ThreadCreate(OSAL_ThreadEntryType entry, void *args) {
  int ret;
  OSAL_ThreadType thread = Os_MemAlloc(sizeof(pthread_t));

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

int OSAL_ThreadJoin(OSAL_ThreadType thread) {
  return pthread_join(*(pthread_t *)thread, NULL);
}

int OSAL_ThreadDestory(OSAL_ThreadType thread) {
  Os_MemFree(thread);
  return 0;
}

void OSAL_SleepUs(uint32_t us) {
  usleep(us);
}

void OSAL_Start(void) {
#ifdef USE_OS
  StartOS(OSDEFAULTAPPMODE);
#else
  StartupHook();
  while (1) {
    usleep(1000000);
  }
#endif
}

int OSAL_MutexAttrInit(OSAL_MutexAttrType *attr) {
  attr->type = OSAL_MUTEX_NORMAL;
  return 0;
}

int OSAL_MutexAttrSetType(OSAL_MutexAttrType *attr, int type) {
  attr->type = OSAL_MUTEX_RECURSIVE;
  return 0;
}

OSAL_MutexType OSAL_MutexCreate(OSAL_MutexAttrType *attr) {
  pthread_mutexattr_t attr2;
  pthread_mutex_t *mutex = NULL;
  int ret = 0;

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(linux)
  pthread_mutexattr_init(&attr2);
  if (NULL != attr) {
    if (OSAL_MUTEX_RECURSIVE == attr->type) {
      pthread_mutexattr_settype(&attr2, PTHREAD_MUTEX_RECURSIVE);
    }
  }
#else
#warning Recursive mutex is not supported!
#endif

  mutex = (pthread_mutex_t *)Os_MemAlloc(sizeof(pthread_mutex_t));
  if (NULL != mutex) {
    ret = pthread_mutex_init(mutex, &attr2);
  } else {
    ret = ENOMEM;
  }

  if (0 != ret) {
    if (NULL != mutex) {
      Os_MemFree((uint8_t *)mutex);
    }
  }

  return mutex;
}

int OSAL_MutexLock(OSAL_MutexType mutex) {
  return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

int OSAL_MutexUnlock(OSAL_MutexType mutex) {
  return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

int OSAL_MutexDestory(OSAL_MutexType mutex) {
  Os_MemFree(mutex);
  return 0;
}

OSAL_SemType OSAL_SemaphoreCreate(int value) {
  sem_t *sem = NULL;
  int ret = 0;

  sem = (sem_t *)Os_MemAlloc(sizeof(sem_t));
  if (NULL != sem) {
    ret = sem_init(sem, 0, value);
  } else {
    ret = ENOMEM;
  }

  if (0 != ret) {
    if (NULL != sem) {
      Os_MemFree((uint8_t *)sem);
    }
  }

  return sem;
}

int OSAL_SemaphoreWait(OSAL_SemType sem) {
  return sem_wait((sem_t *)sem);
}

int OSAL_SemaphoreTryWait(OSAL_SemType sem) {
  return sem_trywait((sem_t *)sem);
}

int OSAL_SemaphoreTimedWait(OSAL_SemType sem, uint32_t timeoutMs) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeoutMs / 1000;
  ts.tv_nsec += (timeoutMs % 1000) * 1000000;

  return sem_timedwait((sem_t *)sem, &ts);
}

int OSAL_SemaphorePost(OSAL_SemType sem) {
  return sem_post((sem_t *)sem);
}

int OSAL_SemaphoreDestory(OSAL_SemType sem) {
  Os_MemFree(sem);
  return 0;
}

bool OSAL_FileExists(const char *file) {
  bool ret = false;
  if (0 != access(file, F_OK | R_OK)) {
    ret = true;
  }
  return ret;
}

#if !defined(USE_OS) && (defined(_WIN32) || defined(linux))
void *OSAL_DlOpen(const char *path) {
  return dlopen(path, RTLD_NOW);
}

void *OSAL_DlSym(void *dll, const char *symbol) {
  return dlsym(dll, symbol);
}

void OSAL_DlClose(void *dll) {
  dlclose(dll);
}
#endif
