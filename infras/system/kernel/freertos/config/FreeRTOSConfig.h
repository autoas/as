#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <assert.h>

#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0
#define configTICK_RATE_HZ (1000)
#define configMINIMAL_STACK_SIZE ((unsigned short)4096)
#define configTOTAL_HEAP_SIZE ((size_t)(65 * 1024))
#define configMAX_TASK_NAME_LEN (12)
#define configUSE_TRACE_FACILITY 0
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configCHECK_FOR_STACK_OVERFLOW 0
#define configUSE_RECURSIVE_MUTEXES 1
#define configQUEUE_REGISTRY_SIZE 20
#define configUSE_APPLICATION_TASK_TAG 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_ALTERNATIVE_API 0
#define configUSE_QUEUE_SETS 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configSUPPORT_STATIC_ALLOCATION 0

#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH 20
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2)

#define configMAX_PRIORITIES (31)

#define configGENERATE_RUN_TIME_STATS 0

#define configUSE_CO_ROUTINES 0
#define configMAX_CO_ROUTINE_PRIORITIES (2)

#define configUSE_STATS_FORMATTING_FUNCTIONS 0

#define configSTACK_DEPTH_TYPE uint32_t

#define configASSERT(x) assert(x)

#define configUSE_MALLOC_FAILED_HOOK 0

#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskCleanUpResources 0
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_uxTaskGetStackHighWaterMark2 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 1
#define INCLUDE_xTaskGetIdleTaskHandle 1
#define INCLUDE_xTaskGetHandle 1
#define INCLUDE_eTaskGetState 1
#define INCLUDE_xSemaphoreGetMutexHolder 1
#define INCLUDE_xTimerPendFunctionCall 1
#define INCLUDE_xTaskAbortDelay 1

#endif /* FREERTOS_CONFIG_H */
