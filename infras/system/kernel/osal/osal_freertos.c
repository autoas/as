/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/queue.h>
#include "osal.h"
#include "FreeRTOS.h"
#include "Std_Debug.h"
#include "task.h"
#include "Std_Compiler.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef configAS_OSAL_THREAD_SIZE
#define configAS_OSAL_THREAD_SIZE 2048
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void StartupHook(void);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
FUNC(void, __weak) StartupHook(void) {
}
FUNC(void, __weak) TaskIdleHook(void) {
}
FUNC(void, __weak) vApplicationIdleHook(void) {
  TaskIdleHook();
}
/* ================================ [ FUNCTIONS ] ============================================== */
int OSAL_ThreadAttrInit(OSAL_ThreadAttrType *attr) {
  attr->stackPtr = NULL;
  attr->stackSize = configAS_OSAL_THREAD_SIZE;
  attr->priority = tskIDLE_PRIORITY + 1;
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
  BaseType_t xReturn = xTaskCreate((TaskFunction_t)entry, attr->stackPtr, attr->stackSize, args,
                                   (UBaseType_t)attr->priority, (TaskHandle_t *)&thread);
  if (pdPASS != xReturn) {
    ASLOG(ERROR, ("create thread over freertos failed: %d\n", xReturn));
  }

  return thread;
}

OSAL_ThreadType OSAL_ThreadCreate(OSAL_ThreadEntryType entry, void *args) {
  OSAL_ThreadType thread = NULL;
  BaseType_t xReturn = xTaskCreate((TaskFunction_t)entry, NULL, configAS_OSAL_THREAD_SIZE, args,
                                   tskIDLE_PRIORITY + 1, (TaskHandle_t *)&thread);
  if (pdPASS != xReturn) {
    ASLOG(ERROR, ("create thread over freertos failed: %d\n", xReturn));
  }

  return thread;
}

int OSAL_ThreadJoin(OSAL_ThreadType thread) {
  eTaskState state;

  do {
    state = eTaskGetState((TaskHandle_t)thread);
    vTaskDelay(1);
    /* When task exited, the state is ready */
  } while (eReady != state);

  return 0;
}

int OSAL_ThreadDestory(OSAL_ThreadType thread) {
  vTaskDelete((TaskHandle_t)thread);
  return 0;
}

void OSAL_SleepUs(uint32_t us) {
  TickType_t ticks = (us + (1000000 / configTICK_RATE_HZ / 2)) / (1000000 / configTICK_RATE_HZ);
  vTaskDelay(ticks);
}

void OSAL_Start(void) {
  StartupHook();
  vTaskStartScheduler();
}
