/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if (OS_PTHREAD_NUM > 0)
#include "semaphore.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "Std_Debug.h"
#include "OsMem.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* this type is for named semaphore */
struct semaphore {
  sem_t sem;
  unsigned int refcount;
  unsigned int unlinked;
  char *name;
  TAILQ_ENTRY(semaphore) entry;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static TAILQ_HEAD(semaphore_list,
                  semaphore) OsSemaphoreList = TAILQ_HEAD_INITIALIZER(OsSemaphoreList);
/* ================================ [ LOCALS    ] ============================================== */
static struct semaphore *sem_find(const char *name) {
  struct semaphore *sem = NULL;

  TAILQ_FOREACH(sem, &OsSemaphoreList, entry) {
    if (0u == strcmp(sem->name, name)) {
      break;
    }
  }

  return sem;
}

static struct semaphore *sem_find2(struct semaphore *sem2) {
  struct semaphore *sem = NULL;

  TAILQ_FOREACH(sem, &OsSemaphoreList, entry) {
    if (sem == sem2) {
      break;
    }
  }

  return sem;
}
/* ================================ [ FUNCTIONS ] ============================================== */
int sem_init(sem_t *sem, int pshared, unsigned int value) {
  (void)pshared;

  TAILQ_INIT(&(sem->head));

  sem->value = value;

  return 0;
}
ELF_EXPORT(sem_init);

int sem_getvalue(sem_t *sem, int *sval) {
  EnterCritical();
  *sval = sem->value;
  ExitCritical();

  return 0;
}
ELF_EXPORT(sem_getvalue);

int sem_destroy(sem_t *sem) {
  (void)sem;

  return 0;
}
ELF_EXPORT(sem_destroy);

int sem_timedwait(sem_t *sem, const struct timespec *abstime) {
  int ercd = 0;

  EnterCritical();
  if (sem->value > 0) {
    sem->value--;
  } else {
    ercd = Os_ListWait(&(sem->head), abstime);
  }
  ExitCritical();

  return ercd;
}
ELF_EXPORT(sem_timedwait);

int sem_trywait(sem_t *sem) {
  struct timespec tm = {0, 0};
  return sem_timedwait(sem, &tm);
}
ELF_EXPORT(sem_trywait);

int sem_wait(sem_t *sem) {
  return sem_timedwait(sem, NULL);
}
ELF_EXPORT(sem_wait);

int sem_post(sem_t *sem) {
  int ercd = 0;

  EnterCritical();
  if (0 != Os_ListPost(&(sem->head), TRUE)) {
    sem->value++;
  }
  ExitCritical();

  return ercd;
}
ELF_EXPORT(sem_post);

sem_t *sem_open(const char *name, int oflag, ...) {
  struct semaphore *sem = NULL;
  va_list arg;
  mode_t mode;
  unsigned int value;

  asAssert(name != NULL);

  EnterCritical();
  sem = sem_find(name);
  ExitCritical();

  if ((NULL == sem) && (0 != (oflag & O_CREAT))) {
    va_start(arg, oflag);
    mode = (mode_t)va_arg(arg, mode_t);
    mode = mode;
    value = (unsigned int)va_arg(arg, unsigned int);
    va_end(arg);

    sem = (struct semaphore *)Os_MemAlloc(sizeof(struct semaphore) + strlen(name) + 1);
    sem->name = (char *)&sem[1];
    strcpy(sem->name, name);
    sem->refcount = 0;
    sem->unlinked = 0;
    sem->sem.value = value;
    EnterCritical();
    /* no consideration of the sem_create race condition,
     * so just assert if such condition */
    asAssert(NULL == sem_find(name));
    TAILQ_INSERT_TAIL(&OsSemaphoreList, sem, entry);
    ExitCritical();
  }

  if (NULL != sem) {
    EnterCritical();
    sem->refcount++;
    ExitCritical();
  }

  return &(sem->sem);
}
ELF_EXPORT(sem_open);

int sem_close(sem_t *sem2) {
  int ercd = 0;
  struct semaphore *sem;

  EnterCritical();
  sem = sem_find2((struct semaphore *)sem2);
  if ((NULL != sem) && (sem->refcount > 0)) {
    sem->refcount--;
    if ((0 == sem->refcount) && (sem->unlinked)) {
      TAILQ_REMOVE(&OsSemaphoreList, sem, entry);
      Os_MemFree((uint8_t *)sem);
    }
  } else {
    ercd = -EACCES;
  }
  ExitCritical();

  return ercd;
}
ELF_EXPORT(sem_close);

int sem_unlink(const char *name) {
  int ercd = 0;
  struct semaphore *sem;

  EnterCritical();
  sem = sem_find(name);
  if (NULL != sem) {
    sem->unlinked = 1;
    if (0 == sem->refcount) {
      TAILQ_REMOVE(&OsSemaphoreList, sem, entry);
      Os_MemFree((uint8_t *)sem);
    }
  }
  ExitCritical();

  return ercd;
}
ELF_EXPORT(sem_unlink);

#endif /* OS_PTHREAD_NUM > 0 */
