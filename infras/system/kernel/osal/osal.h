/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _OS_AL_H_
#define _OS_AL_H_
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
/* ================================ [ MACROS    ] ============================================== */
#define OSAL_MUTEX_NORMAL 0
#define OSAL_MUTEX_RECURSIVE 1
/* ================================ [ TYPES     ] ============================================== */
typedef void (*OSAL_ThreadEntryType)(void *args);
typedef void *OSAL_ThreadType;

typedef struct {
  int type;
} OSAL_MutexAttrType;

typedef void *OSAL_MutexType;

typedef void *OSAL_SemType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
OSAL_ThreadType OSAL_ThreadCreate(OSAL_ThreadEntryType entry, void *args);
int OSAL_ThreadJoin(OSAL_ThreadType thread);
int OSAL_ThreadDestory(OSAL_ThreadType thread);

void OSAL_SleepUs(uint32_t us);
void OSAL_Start(void);

int OSAL_MutexAttrInit(OSAL_MutexAttrType *attr);
int OSAL_MutexAttrSetType(OSAL_MutexAttrType *attr, int type);

OSAL_MutexType OSAL_MutexCreate(OSAL_MutexAttrType *attr);
int OSAL_MutexLock(OSAL_MutexType mutex);
int OSAL_MutexUnlock(OSAL_MutexType mutex);
int OSAL_MutexDestory(OSAL_MutexType mutex);

OSAL_SemType OSAL_SemaphoreCreate(int value);
int OSAL_SemaphoreWait(OSAL_SemType sem);
int OSAL_SemaphoreTryWait(OSAL_SemType sem);
int OSAL_SemaphoreTimedWait(OSAL_SemType sem, uint32_t timeoutMs);
int OSAL_SemaphorePost(OSAL_SemType sem);
int OSAL_SemaphoreDestory(OSAL_SemType sem);

void *OSAL_DlOpen(const char *path);
void *OSAL_DlSym(void *dll, const char *symbol);
void OSAL_DlClose(void *dll);

bool OSAL_FileExists(const char *file);
#ifdef __cplusplus
}
#endif
#endif /* _OS_AL_H_ */
