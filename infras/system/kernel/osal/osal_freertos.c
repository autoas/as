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
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
osal_thread_t osal_thread_create(osal_thread_entry_t entry, void *args) {
  osal_thread_t thread = NULL;
  BaseType_t xReturn = xTaskCreate((TaskFunction_t)entry, NULL, configMINIMAL_STACK_SIZE, args,
                                   tskIDLE_PRIORITY + 1, (TaskHandle_t *)&thread);
  if (pdPASS != xReturn) {
    ASLOG(ERROR, ("create thread over freertos failed: %d\n", xReturn));
  }

  return thread;
}

int osal_thread_join(osal_thread_t thread) {
  eTaskState state;

  do {
    state = eTaskGetState((TaskHandle_t)thread);
    vTaskDelay(1);
    /* When task exited, the state is ready */
  } while (eReady != state);

  vTaskDelete((TaskHandle_t)thread);
  return 0;
}

void osal_usleep(uint32_t us) {
  TickType_t ticks = (us + (1000000 / configTICK_RATE_HZ / 2)) / (1000000 / configTICK_RATE_HZ);
  vTaskDelay(ticks);
}

void osal_start(void) {
  vTaskStartScheduler();
}