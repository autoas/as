/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "osal.h"
#include "Std_Types.h"
#include <stdlib.h>
#include <thread>
#include <assert.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int OSAL_ThreadAttrInit(OSAL_ThreadAttrType *attr) {
  attr->stackPtr = NULL;
  attr->stackSize = 8096;
  attr->priority = 0;
  return 0;
}

int OSAL_ThreadAttrSetStack(OSAL_ThreadAttrType *attr, uint8_t *stackPtr, uint32_t stackSize) {
  attr->stackPtr = stackPtr;
  attr->stackSize = stackSize;
  return 0;
}

int OSAL_ThreadAttrSetStackSize(OSAL_ThreadAttrType *attr, uint32_t stackSize) {
  attr->stackPtr = NULL;
  attr->stackSize = stackSize;
  return 0;
}

int OSAL_ThreadAttrSetPriority(OSAL_ThreadAttrType *attr, int priority) {
  attr->priority = priority;
  return 0;
}

OSAL_ThreadType OSAL_ThreadCreateEx(const OSAL_ThreadAttrType *attr, OSAL_ThreadEntryType entry,
                                    void *args) {
  OSAL_ThreadType thread = NULL;

  thread = (OSAL_ThreadType) new std::thread(entry, args);

  return thread;
}

OSAL_ThreadType OSAL_ThreadCreate(OSAL_ThreadEntryType entry, void *args) {
  OSAL_ThreadType thread = NULL;

  thread = (OSAL_ThreadType) new std::thread(entry, args);

  return thread;
}

int OSAL_ThreadJoin(OSAL_ThreadType thread) {
  std::thread *th = (std::thread *)thread;

  if (th->joinable()) {
    th->join();
  }

  return 0;
}

int OSAL_ThreadDestory(OSAL_ThreadType thread) {
  std::thread *th = (std::thread *)thread;

  if (th->joinable()) {
    th->join();
  }

  delete th;
  return 0;
}