/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const ResourceConstType ResourceConstArray[RESOURCE_NUM];
/* ================================ [ DATAS     ] ============================================== */
static ResourceVarType ResourceVarArray[RESOURCE_NUM];
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* |------------------+-----------------------------------------------------------| */
/* | Syntax:          | StatusType GetResource ( ResourceType <ResID> )           | */
/* |------------------+-----------------------------------------------------------| */
/* | Parameter (In):  | ResID:Reference to resource                               | */
/* |------------------+-----------------------------------------------------------| */
/* | Parameter (Out): | none                                                      | */
/* |------------------+-----------------------------------------------------------| */
/* | Description:     | This call serves to enter critical sections in the code   | */
/* |                  | that are assigned to the resource referenced by <ResID>.  | */
/* |                  | A critical section shall always be left using             | */
/* |                  | ReleaseResource.                                          | */
/* |------------------+-----------------------------------------------------------| */
/* | Particularities: | 1.The OSEK priority ceiling protocol for resource         | */
/* |                  | management is described in chapter 8.5.                   | */
/* |                  | 2.Nested resource occupation is only allowed if the       | */
/* |                  | inner critical sections are completely executed within    | */
/* |                  | the surrounding critical section (strictly stacked,       | */
/* |                  | see chapter 8.2, Restrictions when using resources).      | */
/* |                  | Nested occupation of one and the same resource is         | */
/* |                  | also forbidden!                                           | */
/* |                  | 3.It is recommended that corresponding calls to           | */
/* |                  | GetResource and ReleaseResource appear within the         | */
/* |                  | same function.                                            | */
/* |                  | 4.It is not allowed to use services which are points      | */
/* |                  | of rescheduling for non preemptable tasks (TerminateTask, | */
/* |                  | ChainTask, Schedule and WaitEvent, see chapter 4.6.2)     | */
/* |                  | in critical sections. Additionally, critical sections     | */
/* |                  | are to be left before completion of an interrupt service  | */
/* |                  | routine.                                                  | */
/* |                  | 5.Generally speaking, critical sections should be short.  | */
/* |                  | 6.The service may be called from an ISR and from task     | */
/* |                  | level (see Figure 12-1).                                  | */
/* |------------------+-----------------------------------------------------------| */
/* | Status:          | Standard:1.No error, E_OK                                 | */
/* |                  | Extended:1.Resource <ResID> is invalid, E_OS_ID           | */
/* |                  | 2.Attempt to get a resource which is already occupied     | */
/* |                  | by any task or ISR, or the statically assigned priority   | */
/* |                  | of the calling task or interrupt routine is higher than   | */
/* |                  | the calculated ceiling priority, E_OS_ACCESS              | */
/* |------------------+-----------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                    | */
/* |------------------+-----------------------------------------------------------| */
StatusType GetResource(ResourceType ResID) {
  StatusType ercd = E_OK;
  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (ResID >= RESOURCE_NUM) {
    ercd = E_OS_ID;
  } else if (ResourceVarArray[ResID].prevPrio != INVALID_PRIORITY) {
    ercd = E_OS_ACCESS;
  } else if ((TCL_TASK == CallLevel) &&
             ((ResourceConstArray[ResID].ceilPrio < RunningVar->pConst->initPriority) ||
              (FALSE == RunningVar->pConst->CheckAccess(ResID)))) {
    ercd = E_OS_ACCESS;
  }
#endif

  if (E_OK == ercd) {
    if (TCL_TASK == CallLevel) {
      EnterCritical();
      ResourceVarArray[ResID].prevRes = RunningVar->currentResource;
      ResourceVarArray[ResID].prevPrio = RunningVar->priority;
      RunningVar->currentResource = ResID;
      if (RunningVar->priority < ResourceConstArray[ResID].ceilPrio) {
        RunningVar->priority = ResourceConstArray[ResID].ceilPrio;
      }
      ExitCritical();
    } else if (TCL_ISR2 == CallLevel) {
      ercd = E_OS_CALLEVEL;
      asAssert(0); /* TODO */
    } else {
      ercd = E_OS_CALLEVEL;
    }
  }

  OSErrorOne(GetResource, ResID);
  return ercd;
}

/* |------------------+------------------------------------------------------------| */
/* | Syntax:          | StatusType ReleaseResource ( ResourceType <ResID> )        | */
/* |------------------+------------------------------------------------------------| */
/* | Parameter (In):  | ResID:Reference to resource                                | */
/* |------------------+------------------------------------------------------------| */
/* | Parameter (Out): | none                                                       | */
/* |------------------+------------------------------------------------------------| */
/* | Description:     | ReleaseResource is the counterpart of GetResource and      | */
/* |                  | serves to leave critical sections in the code that are     | */
/* |                  | assigned to the resource referenced by <ResID>.            | */
/* |------------------+------------------------------------------------------------| */
/* | Particularities: | For information on nesting conditions, see particularities | */
/* |                  | of GetResource.                                            | */
/* |                  | The service may be called from an ISR and from task level  | */
/* |                  | (see Figure 12-1).                                         | */
/* |------------------+------------------------------------------------------------| */
/* | Status:          | Standard: No error, E_OK                                   | */
/* |                  | Extended: Resource <ResID> is invalid, E_OS_ID             | */
/* |                  | Attempt to release a resource which is not occupied by     | */
/* |                  | any task or ISR, or another resource shall be released     | */
/* |                  | before, E_OS_NOFUNC                                        | */
/* |                  | Attempt to release a resource which has a lower ceiling    | */
/* |                  | priority than the statically assigned priority of the      | */
/* |                  | calling task or interrupt routine, E_OS_ACCESS             | */
/* |------------------+------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                     | */
/* |------------------+------------------------------------------------------------| */
StatusType ReleaseResource(ResourceType ResID) {
  StatusType ercd = E_OK;
  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (ResID >= RESOURCE_NUM) {
    ercd = E_OS_ID;
  } else if (ResourceVarArray[ResID].prevPrio == INVALID_PRIORITY) {
    ercd = E_OS_NOFUNC;
  } else if ((TCL_TASK == CallLevel) &&
             ((ResourceConstArray[ResID].ceilPrio < RunningVar->pConst->initPriority) ||
              (FALSE == RunningVar->pConst->CheckAccess(ResID)))) {
    ercd = E_OS_ACCESS;
  } else if ((TCL_TASK == CallLevel) && (RunningVar->currentResource != ResID)) {
    ercd = E_OS_NOFUNC;
  }
#endif

  if (E_OK == ercd) {
    if (TCL_TASK == CallLevel) {
      EnterCritical();
      RunningVar->currentResource = ResourceVarArray[ResID].prevRes;
      RunningVar->priority = ResourceVarArray[ResID].prevPrio;
      ResourceVarArray[ResID].prevPrio = INVALID_PRIORITY;
      if (PRIORITY_NUM != RunningVar->priority) { /* if PRIORITY_NUM, then not preempt-able */
        if (Sched_Schedule()) {
          Os_PortDispatch();
        }
      }
      ExitCritical();
    } else if (TCL_ISR2 == CallLevel) {
      ercd = E_OS_CALLEVEL;
      asAssert(0); /* TODO */
    } else {
      ercd = E_OS_CALLEVEL;
    }
  }
  OSErrorOne(ReleaseResource, ResID);
  return ercd;
}

void Os_ResourceInit(void) {
  ResourceType id;

  for (id = 0; id < RESOURCE_NUM; id++) {
    ResourceVarArray[id].prevPrio = INVALID_PRIORITY;
  }
}
