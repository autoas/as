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
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void StartupHook(void);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
FUNC(void, __weak) StartupHook(void) {
}
FUNC(void, __weak) TaskIdleHook(void) {
}
/* ================================ [ FUNCTIONS ] ============================================== */
void vApplicationIdleHook(void) {
  TaskIdleHook();
}

OSAL_ThreadType OSAL_ThreadCreate(OSAL_ThreadEntryType entry, void *args) {
  OSAL_ThreadType thread = NULL;
  BaseType_t xReturn = xTaskCreate((TaskFunction_t)entry, NULL, configMINIMAL_SECURE_STACK_SIZE,
                                   args, tskIDLE_PRIORITY + 1, (TaskHandle_t *)&thread);
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