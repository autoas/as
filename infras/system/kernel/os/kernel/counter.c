/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if (COUNTER_NUM > 0)
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
StatusType SignalCounter(CounterType CounterID) {
  StatusType ercd = E_OK;
  AlarmVarType *pVar;
  AlarmType AlarmID;
  unsigned int savedLevel;
  TickType curValue;
  DECLARE_SMP_PROCESSOR_ID();

  if (CounterID < COUNTER_NUM) {
    EnterCritical();
    savedLevel = CallLevel;
    CallLevel = TCL_LOCK;
    /* yes, only software counter supported */
    CounterVarArray[CounterID].value++;
    curValue = CounterVarArray[CounterID].value;
#if (ALARM_NUM > 0)
    while (NULL != (pVar = TAILQ_FIRST(&CounterVarArray[CounterID].head))) /* intended '=' */
    {
      if (pVar->value == curValue) {
        AlarmID = pVar - AlarmVarArray;
        TAILQ_REMOVE(&CounterVarArray[CounterID].head, &AlarmVarArray[AlarmID], entry);
        OS_STOP_ALARM(&AlarmVarArray[AlarmID]);
        if (AlarmVarArray[AlarmID].period != 0) {
          Os_StartAlarm(AlarmID, (TickType)(curValue + AlarmVarArray[AlarmID].period),
                        AlarmVarArray[AlarmID].period);
        }

        InterLeaveCritical();
        AlarmConstArray[AlarmID].Action();
        InterEnterCritical();
      } else {
        break;
      }
    }
#endif
    CallLevel = savedLevel;
    ExitCritical();
  } else {
    ercd = E_OS_ID;
  }

  return ercd;
}

void Os_CounterInit(void) {
  CounterType id;

  for (id = 0; id < COUNTER_NUM; id++) {
    CounterVarArray[id].value = 0;
    TAILQ_INIT(&CounterVarArray[id].head);
  }
}
#ifdef USE_SHELL
void statOsCounter(void) {
  CounterType id;
  AlarmVarType *pVar;

  EnterCritical();

  printf("\nName\n");
  for (id = 0; id < COUNTER_NUM; id++) {
    printf("%-16s ", CounterConstArray[id].name);
    TAILQ_FOREACH(pVar, &(CounterVarArray[id].head), entry) {
      printf("%s(%d) -> ", AlarmConstArray[pVar - AlarmVarArray].name, pVar->value);
    }
  }

  ExitCritical();
}
#endif
#else
#ifdef USE_SHELL
void statOsCounter(void) {
  printf("Counter is not configured!\n");
}
#endif
#endif /* #if (COUNTER_NUM > 0) */
