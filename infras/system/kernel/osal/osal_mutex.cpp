/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "osal.h"
#include "Std_Types.h"
#include <stdlib.h>
#include <mutex>
#include <assert.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct OSAL_Mutex_s {
  std::recursive_mutex *rmutex = NULL;
  std::mutex *mutex = NULL;
} OSAL_Mutex_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int OSAL_MutexAttrInit(OSAL_MutexAttrType *attr) {
  attr->type = OSAL_MUTEX_NORMAL;
  return 0;
}

int OSAL_MutexAttrSetType(OSAL_MutexAttrType *attr, int type) {
  attr->type = type;
  return 0;
}

OSAL_MutexType OSAL_MutexCreate(OSAL_MutexAttrType *attr) {
  OSAL_Mutex_t *pMutex = new OSAL_Mutex_t;

  if ((NULL != attr) && (OSAL_MUTEX_RECURSIVE == attr->type)) {
    pMutex->rmutex = new std::recursive_mutex;
  } else {
    pMutex->mutex = new std::mutex;
  }

  return (OSAL_MutexType)pMutex;
}

int OSAL_MutexLock(OSAL_MutexType mutex) {
  OSAL_Mutex_t *pMutex = (OSAL_Mutex_t *)mutex;
  assert(NULL != mutex);
  if (NULL != pMutex->rmutex) {
    pMutex->rmutex->lock();
  } else {
    pMutex->mutex->lock();
  }
  return 0;
}

int OSAL_MutexUnlock(OSAL_MutexType mutex) {
  OSAL_Mutex_t *pMutex = (OSAL_Mutex_t *)mutex;
  if (NULL != pMutex->rmutex) {
    pMutex->rmutex->unlock();
  } else {
    pMutex->mutex->unlock();
  }
  return 0;
}

int OSAL_MutexDestory(OSAL_MutexType mutex) {
  OSAL_Mutex_t *pMutex = (OSAL_Mutex_t *)mutex;
  if (NULL != pMutex->rmutex) {
    delete pMutex->rmutex;
  } else {
    delete pMutex->mutex;
  }
  delete pMutex;
  return 0;
}
