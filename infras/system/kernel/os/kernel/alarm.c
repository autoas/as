/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
#if (ALARM_NUM > 0)
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* |------------------+------------------------------------------------------------------| */
/* | Syntax:          | StatusType GetAlarmBase (AlarmType <AlarmID>,                    | */
/* |                  | AlarmBaseRefType <Info> )                                        | */
/* |------------------+------------------------------------------------------------------| */
/* | Parameter (In):  | AlarmID: Reference to alarm                                      | */
/* |------------------+------------------------------------------------------------------| */
/* | Parameter (Out): | Info: Reference to structure with constants of the alarm base.   | */
/* |------------------+------------------------------------------------------------------| */
/* | Description:     | The system service GetAlarmBase reads the alarm base             | */
/* |                  | characteristics. The return value <Info> is a structure in which | */
/* |                  | the information of data type AlarmBaseType is stored.            | */
/* |------------------+------------------------------------------------------------------| */
/* | Particularities: | Allowed on task level, ISR, and in several hook routines (see    | */
/* |                  | Figure 12-1).                                                    | */
/* |------------------+------------------------------------------------------------------| */
/* | Status:          | Standard:No error, E_OK                                          | */
/* |                  | Extended:Alarm <AlarmID> is invalid, E_OS_ID                     | */
/* |------------------+------------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                           | */
/* |------------------+------------------------------------------------------------------| */
StatusType GetAlarmBase(AlarmType AlarmID, AlarmBaseRefType Info) {
  StatusType ercd = E_OK;

#if (OS_STATUS == EXTENDED)
  if (AlarmID < ALARM_NUM) {
#endif
    *Info = AlarmConstArray[AlarmID].pCounter->base;
#if (OS_STATUS == EXTENDED)
  } else {
    ercd = E_OS_ID;
  }
#endif

  OSErrorTwo(GetAlarmBase, AlarmID, Info);

  return ercd;
}

/* |------------------+------------------------------------------------------------------| */
/* | Syntax:          | StatusType GetAlarm ( AlarmType <AlarmID>,TickRefType <Tick>)    | */
/* |------------------+------------------------------------------------------------------| */
/* | Parameter (In):  | AlarmID:Reference to an alarm                                    | */
/* |------------------+------------------------------------------------------------------| */
/* | Parameter (Out): | Tick:Relative value in ticks before the alarm <AlarmID> expires. | */
/* |------------------+------------------------------------------------------------------| */
/* | Description:     | The system service GetAlarm returns the relative value in ticks  | */
/* |                  | before the alarm <AlarmID> expires.                              | */
/* |------------------+------------------------------------------------------------------| */
/* | Particularities: | 1.It is up to the application to decide whether for example a    | */
/* |                  | CancelAlarm may still be useful.                                 | */
/* |                  | 2.If <AlarmID> is not in use, <Tick> is not defined.             | */
/* |                  | 3.Allowed on task level, ISR, and in several hook routines (see  | */
/* |                  | Figure 12-1).                                                    | */
/* |------------------+------------------------------------------------------------------| */
/* | Status:          | Standard: No error, E_OK                                         | */
/* |                  | Alarm <AlarmID> is not used, E_OS_NOFUNC                         | */
/* |                  | Extended:  Alarm <AlarmID> is invalid, E_OS_ID                   | */
/* |------------------+------------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                           | */
/* |------------------+------------------------------------------------------------------| */
StatusType GetAlarm(AlarmType AlarmID, TickRefType Tick) {
  StatusType ercd = E_OK;

#if (OS_STATUS == EXTENDED)
  if (AlarmID < ALARM_NUM) {
#endif
    EnterCritical();
    if (OS_IS_ALARM_STARTED(&AlarmVarArray[AlarmID])) {
      /* rely on the trick of integer overflow */
      *Tick =
        (TickType)(AlarmVarArray[AlarmID].value - AlarmConstArray[AlarmID].pCounter->pVar->value);
    } else {
      ercd = E_OS_NOFUNC;
    }
    ExitCritical();
#if (OS_STATUS == EXTENDED)
  } else {
    ercd = E_OS_ID;
  }
#endif

  OSErrorTwo(GetAlarm, AlarmID, Tick);

  return ercd;
}

/* |------------------+-----------------------------------------------------------------| */
/* | Syntax:          | StatusType SetRelAlarm ( AlarmType <AlarmID>,                   | */
/* |                  | TickType <increment>,                                           | */
/* |                  | TickType <cycle> )                                              | */
/* |------------------+-----------------------------------------------------------------| */
/* | Parameter (In):  | AlarmID:Reference to the alarm element                          | */
/* |                  | increment:Relative value in ticks                               | */
/* |                  | cycle:Cycle value in case of cyclic alarm. In case of single    | */
/* |                  | alarms, cycle shall be zero.                                    | */
/* |------------------+-----------------------------------------------------------------| */
/* | Parameter (Out): | none                                                            | */
/* |------------------+-----------------------------------------------------------------| */
/* | Description:     | The system service occupies the alarm <AlarmID> element.        | */
/* |                  | After <increment> ticks have elapsed, the task assigned         | */
/* |                  | to the alarm <AlarmID> is activated or the assigned event       | */
/* |                  | (only for extended tasks) is set or the alarm-callback          | */
/* |                  | routine is called.                                              | */
/* |------------------+-----------------------------------------------------------------| */
/* | Particularities: | 1.The behaviour of <increment> equal to 0 is up to the          | */
/* |                  | implementation.                                                 | */
/* |                  | 2.If the relative value <increment> is very small, the alarm    | */
/* |                  | may expire, and the task may become ready or the alarm-callback | */
/* |                  | may be called before the system service returns to the user.    | */
/* |                  | 3.If <cycle> is unequal zero, the alarm element is logged on    | */
/* |                  | again immediately after expiry with the relative value <cycle>. | */
/* |                  | 4.The alarm <AlarmID> must not already be in use.               | */
/* |                  | 5.To change values of alarms already in use the alarm shall be  | */
/* |                  | cancelled first.                                                | */
/* |                  | 6.If the alarm is already in use, this call will be ignored and | */
/* |                  | the error E_OS_STATE is returned.                               | */
/* |                  | 7.Allowed on task level and in ISR, but not in hook routines.   | */
/* |------------------+-----------------------------------------------------------------| */
/* | Status:          | Standard:                                                       | */
/* |                  | 1.No error, E_OK                                                | */
/* |                  | 2.Alarm <AlarmID> is already in use, E_OS_STATE                 | */
/* |                  | Extended:                                                       | */
/* |                  | 1.Alarm <AlarmID> is invalid, E_OS_ID                           | */
/* |                  | 2.Value of <increment> outside of the admissible limits         | */
/* |                  | (lower than zero or greater than maxallowedvalue), E_OS_VALUE   | */
/* |                  | 3.Value of <cycle> unequal to 0 and outside of the admissible   | */
/* |                  | counter limits (less than mincycle or greater than              | */
/* |                  | maxallowedvalue), E_OS_VALUE                                    | */
/* |------------------+-----------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2; Events only ECC1, ECC2                  | */
/* |------------------+-----------------------------------------------------------------| */
StatusType SetRelAlarm(AlarmType AlarmID, TickType Increment, TickType Cycle) {
  StatusType ercd = E_OK;

#if (OS_STATUS == EXTENDED)
  if (AlarmID >= ALARM_NUM) {
    ercd = E_OS_ID;
  } else if (Increment > AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue) {
    ercd = E_OS_VALUE;
  } else if ((Cycle > AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue) ||
             ((Cycle > 0) && (Cycle < AlarmConstArray[AlarmID].pCounter->base.mincycle))) {
    ercd = E_OS_VALUE;
  }
#endif

  if (E_OK == ercd) {
    EnterCritical();
    if (FALSE == OS_IS_ALARM_STARTED(&AlarmVarArray[AlarmID])) {
      TickType Start = (TickType)(AlarmConstArray[AlarmID].pCounter->pVar->value + Increment);
      Os_StartAlarm(AlarmID, Start, Cycle);
    } else {
      ercd = E_OS_STATE;
    }
    ExitCritical();
  }

  OSErrorThree(SetRelAlarm, AlarmID, Increment, Cycle);

  return ercd;
}

/* |------------------+-----------------------------------------------------------------| */
/* | Syntax:          | StatusType SetAbsAlarm (AlarmType <AlarmID>,                    | */
/* |                  | TickType <start>,                                               | */
/* |                  | TickType <cycle> )                                              | */
/* |------------------+-----------------------------------------------------------------| */
/* | Parameter (In):  | AlarmID:Reference to the alarm element                          | */
/* |                  | start:Absolute value in ticks                                   | */
/* |                  | cycle:Cycle value in case of cyclic alarm. In case of           | */
/* |                  | single alarms, cycle shall be zero.                             | */
/* |------------------+-----------------------------------------------------------------| */
/* | Parameter (Out): | none                                                            | */
/* |------------------+-----------------------------------------------------------------| */
/* | Description:     | The system service occupies the alarm <AlarmID> element.        | */
/* |                  | When <start> ticks are reached, the task assigned to the alarm  | */
/* |                  | <AlarmID> is activated or the assigned event (only for extended | */
/* |                  | tasks) is set or the alarm-callback routine is called.          | */
/* |------------------+-----------------------------------------------------------------| */
/* | Particularities: | 1.If the absolute value <start> is very close to the current    | */
/* |                  | counter value, the alarm may expire, and the task may become    | */
/* |                  | ready or the alarm-callback may be called before the system     | */
/* |                  | service returns to the user.                                    | */
/* |                  | 2.If the absolute value <start> already was reached before      | */
/* |                  | the system call, the alarm shall only expire when the           | */
/* |                  | absolute value <start> is reached again, i.e. after the next    | */
/* |                  | overrun of the counter.                                         | */
/* |                  | 3.If <cycle> is unequal zero, the alarm element is logged on    | */
/* |                  | again immediately after expiry with the relative value <cycle>. | */
/* |                  | 4.The alarm <AlarmID> shall not already be in use.              | */
/* |                  | 5.To change values of alarms already in use the alarm shall be  | */
/* |                  | cancelled first.                                                | */
/* |                  | 6.If the alarm is already in use, this call will be ignored and | */
/* |                  | the error E_OS_STATE is returned.                               | */
/* |                  | 7.Allowed on task level and in ISR, but not in hook routines.   | */
/* |------------------+-----------------------------------------------------------------| */
/* | Status:          | Standard:                                                       | */
/* |                  | 1.No error, E_OK                                                | */
/* |                  | 2.Alarm <AlarmID> is already in use, E_OS_STATE                 | */
/* |                  | Extended:                                                       | */
/* |                  | 1.Alarm <AlarmID> is invalid, E_OS_ID                           | */
/* |                  | 2.Value of <start> outside of the admissible counter limit      | */
/* |                  | (less than zero or greater than maxallowedvalue), E_OS_VALUE    | */
/* |                  | 3.Value of <cycle> unequal to 0 and outside of the admissible   | */
/* |                  | counter limits (less than mincycle or greater than              | */
/* |                  | maxallowedvalue), E_OS_VALUE                                    | */
/* |------------------+-----------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2; Events only ECC1, ECC2                  | */
/* |------------------+-----------------------------------------------------------------| */
StatusType SetAbsAlarm(AlarmType AlarmID, TickType Start, TickType Cycle) {
  StatusType ercd = E_OK;

  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (AlarmID >= ALARM_NUM) {
    ercd = E_OS_ID;
  } else if (Start > AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue) {
    ercd = E_OS_VALUE;
  } else if ((Cycle > AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue) ||
             ((Cycle > 0) && (Cycle < AlarmConstArray[AlarmID].pCounter->base.mincycle))) {
    ercd = E_OS_VALUE;
  }
#endif

  if (E_OK == ercd) {
    EnterCritical();
    if (FALSE == OS_IS_ALARM_STARTED(&AlarmVarArray[AlarmID])) {
      TickType Increment = AlarmConstArray[AlarmID].pCounter->pVar->value %
                           AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue;

      if (Increment == Start) {
        if (Cycle > 0) {
          Start = AlarmConstArray[AlarmID].pCounter->pVar->value + Cycle;
          Os_StartAlarm(AlarmID, Start, Cycle);
        }
        AlarmConstArray[AlarmID].Action();
      } else {
        if (Increment < Start) {
          Start = (AlarmConstArray[AlarmID].pCounter->pVar->value /
                   AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue) *
                    AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue +
                  Start;
        } else {
          Start = (1 + (AlarmConstArray[AlarmID].pCounter->pVar->value /
                        AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue)) *
                    AlarmConstArray[AlarmID].pCounter->base.maxallowedvalue +
                  Start;
        }

        Os_StartAlarm(AlarmID, Start, Cycle);
      }
    } else {
      ercd = E_OS_STATE;
    }
    ExitCritical();
  }

  OSErrorThree(SetAbsAlarm, AlarmID, Start, Cycle);

  return ercd;
}

/* |------------------+-------------------------------------------------------------| */
/* | Syntax:          | StatusType CancelAlarm ( AlarmType <AlarmID> )              | */
/* |------------------+-------------------------------------------------------------| */
/* | Parameter (In):  | AlarmID:Reference to an alarm                               | */
/* |------------------+-------------------------------------------------------------| */
/* | Parameter (Out): | none                                                        | */
/* |------------------+-------------------------------------------------------------| */
/* | Description:     | The system service cancels the alarm <AlarmID>.             | */
/* |------------------+-------------------------------------------------------------| */
/* | Particularities: | Allowed on task level and in ISR, but not in hook routines. | */
/* |------------------+-------------------------------------------------------------| */
/* | Status:          | Standard:                                                   | */
/* |                  | 1.No error, E_OK                                            | */
/* |                  | 2.Alarm <AlarmID> not in use, E_OS_NOFUNC                   | */
/* |                  | Extended: 1.Alarm <AlarmID> is invalid, E_OS_ID             | */
/* |------------------+-------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                      | */
/* |------------------+-------------------------------------------------------------| */
StatusType CancelAlarm(AlarmType AlarmID) {
  StatusType ercd = E_OK;

#if (OS_STATUS == EXTENDED)
  if (AlarmID < ALARM_NUM) {
#endif
    EnterCritical();
    if (OS_IS_ALARM_STARTED(&AlarmVarArray[AlarmID])) {
      TAILQ_REMOVE(&(AlarmConstArray[AlarmID].pCounter->pVar->head), &AlarmVarArray[AlarmID],
                   entry);
      OS_STOP_ALARM(&AlarmVarArray[AlarmID]);
    } else {
      ercd = E_OS_NOFUNC;
    }
    ExitCritical();
#if (OS_STATUS == EXTENDED)
  } else {
    ercd = E_OS_ID;
  }
#endif

  OSErrorOne(CancelAlarm, AlarmID);

  return ercd;
}

void Os_AlarmInit(AppModeType appMode) {
  AlarmType id;

  asAssert(8 == ((TickType)(5 - (TICK_MAX - 2))));
  asAssert((567 + 9999 + 1) == ((TickType)(567 - (TICK_MAX - 9999))));
  for (id = 0; id < ALARM_NUM; id++) {
    AlarmVarArray[id].value = 0;
    AlarmVarArray[id].period = 0;

    OS_STOP_ALARM(&AlarmVarArray[id]);

    if (AlarmConstArray[id].appModeMask & appMode) {
      (void)SetAbsAlarm(id, AlarmConstArray[id].start, AlarmConstArray[id].period);
    }
  }
}

void Os_StartAlarm(AlarmType AlarmID, TickType Start, TickType Cycle) {
  AlarmVarType *pVar;
  AlarmVarType *pPosVar = NULL;
  TickType curValue = AlarmConstArray[AlarmID].pCounter->pVar->value;

  TickType left = (TickType)(Start - curValue);

  asAssert(FALSE == OS_IS_ALARM_STARTED(&AlarmVarArray[AlarmID]));

  AlarmVarArray[AlarmID].value = Start;
  AlarmVarArray[AlarmID].period = Cycle;

  TAILQ_FOREACH(pVar, &(AlarmConstArray[AlarmID].pCounter->pVar->head), entry) {
    if ((TickType)(pVar->value - curValue) > left) {
      pPosVar = pVar;
      break;
    } else if (((TickType)(pVar->value - curValue) == left) &&
               ((pVar - AlarmVarArray) > AlarmID)) { /*this is not necessary but for fix behavior,
                                                        lower AlarmID serve first */
      pPosVar = pVar;
      break;
    }
  }

  if (NULL != pPosVar) {
    TAILQ_INSERT_BEFORE(pPosVar, &AlarmVarArray[AlarmID], entry);
  } else {
    TAILQ_INSERT_TAIL(&(AlarmConstArray[AlarmID].pCounter->pVar->head), &AlarmVarArray[AlarmID],
                      entry);
  }
}

#ifdef USE_SHELL
void statOsAlarm(void) {
  AlarmType id;
  AlarmVarType *pVar;
  const AlarmConstType *pConst;

  EnterCritical();

  printf("\nName             Status Value      Period     Counter\n");
  for (id = 0; id < ALARM_NUM; id++) {
    pConst = &AlarmConstArray[id];
    pVar = &AlarmVarArray[id];

    printf("%-16s %-6s %-10d %-10d %s(%d)\n", pConst->name,
           OS_IS_ALARM_STARTED(pVar) ? "start" : "stop", pVar->value, pVar->period,
           pConst->pCounter->name, pConst->pCounter->pVar->value);
  }

  ExitCritical();
}
#endif
#else
#ifdef USE_SHELL
void statOsAlarm(void) {
  printf("Alarm is not configured!\n");
}
#endif
#endif /* #if (ALARM_NUM > 0) */
