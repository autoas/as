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
/* ================================ [ DATAS     ] ============================================== */
TaskVarType TaskVarArray[TASK_NUM + OS_PTHREAD_NUM];
#ifdef USE_SHELL
static const char *statsNames[] = {"SUSPENDED", "RUNNING", "READY", "WAITING"};
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void InitContext(TaskVarType *pTaskVar) {
  pTaskVar->state = READY;
  pTaskVar->priority = pTaskVar->pConst->initPriority;
  pTaskVar->currentResource = INVALID_RESOURCE;

#ifdef USE_SMP
  pTaskVar->lock = 0;
  pTaskVar->oncpu = pTaskVar->pConst->cpu;
#endif

#ifdef EXTENDED_TASK
  if (NULL != pTaskVar->pConst->pEventVar) {
    pTaskVar->pConst->pEventVar->set = 0u;
    pTaskVar->pConst->pEventVar->wait = 0u;
  }
#endif

  Os_PortInitContext(pTaskVar);
}
#ifdef USE_SHELL
static const char *taskStateToString(TaskStateType state) {
  const char *p = "unknown";
  if ((state & OSEK_TASK_STATE_MASK) < sizeof(statsNames) / sizeof(char *)) {
    p = statsNames[state & OSEK_TASK_STATE_MASK];
  }

  if ((state & (PTHREAD_STATE_SLEEPING | PTHREAD_STATE_WAITING)) ==
      (PTHREAD_STATE_SLEEPING | PTHREAD_STATE_WAITING)) {
    p = "TIMEDWAIT";
  } else if (state & PTHREAD_STATE_WAITING) {
    p = "WAITLIST";
  } else if (state & PTHREAD_STATE_SLEEPING) {
    p = "SLEEPING";
  }

  return p;
}
static uint32_t checkStackUsage(const TaskConstType *pTaskConst, uint32_t *used) {
  uint32_t i;
  uint32_t *pStack = pTaskConst->pStack;

  for (i = 0; (i < pTaskConst->stackSize) && (0u == *pStack); i += sizeof(uint32_t), pStack++) {
    /* do nothing */
  }

  *used = pTaskConst->stackSize - i;
  /* round up */
  return (*used + (pTaskConst->stackSize / 100 - 1)) * 100 / pTaskConst->stackSize;
}

static boolean isRunning(TaskVarType *pTaskVar) {
#ifdef USE_SMP
  int cpuid;
  boolean rv = FALSE;
  for (cpuid = 0; cpuid < CPU_CORE_NUMBER; cpuid++) {
    if (pTaskVar == RunningVar) {
      rv = TRUE;
      break;
    }
  }

  return rv;
#else
  return (pTaskVar == RunningVar);
#endif
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
/* |------------------+------------------------------------------------------------| */
/* | Syntax:          | StatusType ActivateTask ( TaskType <TaskID> )              | */
/* |------------------+------------------------------------------------------------| */
/* | Parameter (In):  | TaskID: Task reference                                     | */
/* |------------------+------------------------------------------------------------| */
/* | Parameter (Out): | none                                                       | */
/* |------------------+------------------------------------------------------------| */
/* | Description:     | The task <TaskID> is transferred from the suspended state  | */
/* |                  | into the ready state. The operating system ensures that    | */
/* |                  | the task code is being executed from the first statement.  | */
/* |------------------+------------------------------------------------------------| */
/* | Particularities: | 1) The service may be called from interrupt level and from | */
/* |                  | task level (see Figure 12-1(os223.doc)).                   | */
/* |                  | 2) Rescheduling after the call to ActivateTask depends on  | */
/* |                  | the place it is called from (ISR, non preemptable task,    | */
/* |                  | preemptable task).                                         | */
/* |                  | 3)If E_OS_LIMIT is returned the activation is ignored.     | */
/* |                  | 4)When an extended task is transferred from suspended      | */
/* |                  | state into ready state all its events are cleared.         | */
/* |------------------+------------------------------------------------------------| */
/* | Status:          | Standard:                                                  | */
/* |                  | 1) No error, E_OK                                          | */
/* |                  | 2) Too many task activations of <TaskID>, E_OS_LIMIT       | */
/* |                  | Extended:                                                  | */
/* |                  | 1) Task <TaskID> is invalid, E_OS_ID                       | */
/* |------------------+------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                     | */
/* |------------------+------------------------------------------------------------| */
StatusType ActivateTask(TaskType TaskID) {
  StatusType ercd = E_OK;
  TaskVarType *pTaskVar;
  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (TaskID < TASK_NUM) {
#endif
    pTaskVar = &TaskVarArray[TaskID];
    EnterCritical();
    if (SUSPENDED == TASK_STATE(pTaskVar)) {
      InitContext(pTaskVar);

#ifdef MULTIPLY_TASK_ACTIVATION
      pTaskVar->activation = 1;
#endif

      OS_TRACE_TASK_ACTIVATION(pTaskVar);

      Sched_AddReady(TaskID);
    } else {
#ifdef MULTIPLY_TASK_ACTIVATION
      if (pTaskVar->activation < pTaskVar->pConst->maxActivation) {
        OS_TRACE_TASK_ACTIVATION(pTaskVar);
        pTaskVar->activation++;
#ifndef USE_SCHED_LIST
        Sched_AddReady(TaskID);
#endif
      } else
#endif
      {
        ercd = E_OS_LIMIT;
      }
    }

    if ((E_OK == ercd) && (TCL_TASK == CallLevel) && (ReadyVar->priority > RunningVar->priority)) {
      Sched_Preempt();
      Os_PortDispatch();
    }

    ExitCritical();
#if (OS_STATUS == EXTENDED)
  } else {
    ercd = E_OS_ID;
  }
#endif

  OSErrorOne(ActivateTask, TaskID);

  return ercd;
}

/* |------------------+--------------------------------------------------------------| */
/* | Syntax:          | StatusType TerminateTask ( void )                            | */
/* |------------------+--------------------------------------------------------------| */
/* | Parameter (In):  | none                                                         | */
/* |------------------+--------------------------------------------------------------| */
/* | Parameter (Out): | none                                                         | */
/* |------------------+--------------------------------------------------------------| */
/* | Description:     | This service causes the termination of the calling task. The | */
/* |                  | calling task is transferred from the running state into the  | */
/* |                  | suspended state.                                             | */
/* |------------------+--------------------------------------------------------------| */
/* | Particularities: | 1) An internal resource assigned to the calling task is      | */
/* |                  | automatically released. Other resources occupied by the task | */
/* |                  | shall have been released before the call to TerminateTask.   | */
/* |                  | If a resource is still occupied in standard status the       | */
/* |                  | behaviour is undefined.                                      | */
/* |                  | 2) If the call was successful, TerminateTask does not return | */
/* |                  | to the call level and the status can not be evaluated.       | */
/* |                  | 3) If the version with extended status is used, the service  | */
/* |                  | returns in case of error, and provides a status which can be | */
/* |                  | evaluated in the application.                                | */
/* |                  | 4) If the service TerminateTask is called successfully, it   | */
/* |                  | enforces a rescheduling.                                     | */
/* |                  | 5) Ending a task function without call to TerminateTask or   | */
/* |                  | ChainTask is strictly forbidden and may leave the system in  | */
/* |                  | an undefined state.                                          | */
/* |------------------+--------------------------------------------------------------| */
/* | Status:          | Standard:                                                    | */
/* |                  | 1)No return to call level                                    | */
/* |                  | Extended:                                                    | */
/* |                  | 1) Task still occupies resources, E_OS_RESOURCE              | */
/* |                  | 2) Call at interrupt level, E_OS_CALLEVEL                    | */
/* |------------------+--------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                       | */
/* |------------------+--------------------------------------------------------------| */
StatusType TerminateTask(void) {
  StatusType ercd = E_OK;
  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (CallLevel != TCL_TASK) {
    ercd = E_OS_CALLEVEL;
  } else if (RunningVar->currentResource != INVALID_RESOURCE) {
    ercd = E_OS_RESOURCE;
  }
#endif

  if (E_OK == ercd) {
    EnterCritical();
#ifdef MULTIPLY_TASK_ACTIVATION
    asAssert(RunningVar->activation > 0);
    RunningVar->activation--;
    if (RunningVar->activation > 0) {
      InitContext(RunningVar);
#ifdef USE_SCHED_LIST
      Sched_AddReady(RunningVar - TaskVarArray);
#endif
    } else
#endif
    {
      RunningVar->state = SUSPENDED;
    }

    Sched_GetReady();
    OSPostTaskHook();
    Os_PortStartDispatch();

    ExitCritical();
  }

  OSErrorNone(TerminateTask);
  return ercd;
}

/* |------------------+-------------------------------------------------------------| */
/* | Syntax:          | StatusType ChainTask ( TaskType <TaskID> )                  | */
/* |------------------+-------------------------------------------------------------| */
/* | Parameter (In):  | TaskID Reference to the sequential succeeding task to       | */
/* |                  | be activated.                                               | */
/* |------------------+-------------------------------------------------------------| */
/* | Parameter (Out): | none                                                        | */
/* |------------------+-------------------------------------------------------------| */
/* | Description:     | This service causes the termination of the calling task.    | */
/* |                  | After termination of the calling task a succeeding task     | */
/* |                  | <TaskID> is activated. Using this service, it ensures       | */
/* |                  | that the succeeding task starts to run at the earliest      | */
/* |                  | after the calling task has been terminated.                 | */
/* |------------------+-------------------------------------------------------------| */
/* | Particularities: | 1. If the succeeding task is identical with the current     | */
/* |                  | task, this does not result in multiple requests. The task   | */
/* |                  | is not transferred to the suspended state, but will         | */
/* |                  | immediately become ready again.                             | */
/* |                  | 2. An internal resource assigned to the calling task is     | */
/* |                  | automatically released, even if the succeeding task is      | */
/* |                  | identical with the current task. Other resources occupied   | */
/* |                  | by the calling shall have been released before ChainTask    | */
/* |                  | is called. If a resource is still occupied in standard      | */
/* |                  | status the behaviour is undefined.                          | */
/* |                  | 3. If called successfully, ChainTask does not return to     | */
/* |                  | the call level and the status can not be evaluated.         | */
/* |                  | 4. In case of error the service returns to the calling      | */
/* |                  | task and provides a status which can then be evaluated      | */
/* |                  | in the application.                                         | */
/* |                  | 5.If the service ChainTask is called successfully, this     | */
/* |                  | enforces a rescheduling.                                    | */
/* |                  | 6. Ending a task function without call to TerminateTask     | */
/* |                  | or ChainTask is strictly forbidden and may leave the system | */
/* |                  | in an undefined state.                                      | */
/* |                  | 7. If E_OS_LIMIT is returned the activation is ignored.     | */
/* |                  | 8. When an extended task is transferred from suspended      | */
/* |                  | state into ready state all its events are cleared.          | */
/* |------------------+-------------------------------------------------------------| */
/* | Status:          | Standard:                                                   | */
/* |                  | 1. No return to call level                                  | */
/* |                  | 2. Too many task activations of <TaskID>, E_OS_LIMIT        | */
/* |                  | Extended:                                                   | */
/* |                  | 1. Task <TaskID> is invalid, E_OS_ID                        | */
/* |                  | 2. Calling task still occupies resources, E_OS_RESOURCE     | */
/* |                  | 3. Call at interrupt level, E_OS_CALLEVEL                   | */
/* |------------------+-------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                      | */
/* |------------------+-------------------------------------------------------------| */
StatusType ChainTask(TaskType TaskID) {
  StatusType ercd = E_OK;
  TaskVarType *pTaskVar;
  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (TaskID >= TASK_NUM) {
    ercd = E_OS_ID;
  } else if (RunningVar->currentResource != INVALID_RESOURCE) {
    ercd = E_OS_RESOURCE;
  } else if (CallLevel != TCL_TASK) {
    ercd = E_OS_CALLEVEL;
  }
#endif

  if (E_OK == ercd) {
    pTaskVar = &TaskVarArray[TaskID];

    if (pTaskVar == RunningVar) {
      EnterCritical();
      InitContext(RunningVar);
      Sched_AddReady(TaskID);
      ExitCritical();
    } else {
      EnterCritical();
      if (SUSPENDED == TASK_STATE(pTaskVar)) {
        InitContext(pTaskVar);

#ifdef MULTIPLY_TASK_ACTIVATION
        pTaskVar->activation = 1;
#endif

        Sched_AddReady(TaskID);
      } else {
#ifdef MULTIPLY_TASK_ACTIVATION
        if (pTaskVar->activation < pTaskVar->pConst->maxActivation) {
          pTaskVar->activation++;
#ifndef USE_SCHED_LIST
          Sched_AddReady(TaskID);
#endif
        } else
#endif
        {
          ercd = E_OS_LIMIT;
        }
      }
      ExitCritical();

      if (ercd == E_OK) {
        EnterCritical();
#ifdef MULTIPLY_TASK_ACTIVATION
        asAssert(RunningVar->activation > 0);
        RunningVar->activation--;
        if (RunningVar->activation > 0) {
          InitContext(RunningVar);
#ifdef USE_SCHED_LIST
          Sched_AddReady(RunningVar - TaskVarArray);
#endif
        } else
#endif
        {
          RunningVar->state = SUSPENDED;
        }
        ExitCritical();
      }
    }

    if (ercd == E_OK) {
      OS_TRACE_TASK_ACTIVATION(pTaskVar);
      EnterCritical();
      Sched_GetReady();
      OSPostTaskHook();
      Os_PortStartDispatch();
      ExitCritical();
    }
  }

  OSErrorOne(ChainTask, TaskID);

  return ercd;
}
/* |------------------+-------------------------------------------------------------| */
/* | Syntax:          | StatusType Schedule ( void )                                | */
/* |------------------+-------------------------------------------------------------| */
/* | Parameter (In):  | none                                                        | */
/* |------------------+-------------------------------------------------------------| */
/* | Parameter (Out): | none                                                        | */
/* |------------------+-------------------------------------------------------------| */
/* | Description:     | If a higher-priority task is ready, the internal resource   | */
/* |                  | of the task is released, the current task is put into the   | */
/* |                  | ready state, its context is saved and the higher-priority   | */
/* |                  | task is executed. Otherwise the calling task is continued.  | */
/* |------------------+-------------------------------------------------------------| */
/* | Particularities: | Rescheduling only takes place if the task an internal       | */
/* |                  | resource is assigned to the calling task during             | */
/* |                  | system generation. For these tasks, Schedule enables a      | */
/* |                  | processor assignment to other tasks with lower or equal     | */
/* |                  | priority than the ceiling priority of the internal resource | */
/* |                  | and higher priority than the priority of the calling task   | */
/* |                  | in application-specific locations. When returning from      | */
/* |                  | Schedule, the internal resource has been taken again.       | */
/* |                  | This service has no influence on tasks with no internal     | */
/* |                  | resource assigned (preemptable tasks).                      | */
/* |------------------+-------------------------------------------------------------| */
/* | Status:          | Standard:                                                   | */
/* |                  | 1. No error, E_OK                                           | */
/* |                  | Extended:                                                   | */
/* |                  | 1. Call at interrupt level, E_OS_CALLEVEL                   | */
/* |                  | 2. Calling task occupies resources, E_OS_RESOURCE           | */
/* |------------------+-------------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                      | */
/* |------------------+-------------------------------------------------------------| */
StatusType Schedule(void) {
  StatusType ercd = E_OK;
  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (CallLevel != TCL_TASK) {
    ercd = E_OS_CALLEVEL;
  } else if (RunningVar->currentResource != INVALID_RESOURCE) {
    ercd = E_OS_RESOURCE;
  }
#endif

  if (E_OK == ercd) {
    EnterCritical();
    RunningVar->priority = RunningVar->pConst->initPriority;
    if (Sched_Schedule()) {
      Os_PortDispatch();
    }
    RunningVar->priority = RunningVar->pConst->runPriority;
    ExitCritical();
  }

  OSErrorNone(Schedule);

  return ercd;
}

/* |------------------+---------------------------------------------------------| */
/* | Syntax:          | StatusType GetTaskID ( TaskRefType <TaskID> )           | */
/* |------------------+---------------------------------------------------------| */
/* | Parameter (In):  | none                                                    | */
/* |------------------+---------------------------------------------------------| */
/* | Parameter (Out): | TaskID Reference to the task which is currently running | */
/* |------------------+---------------------------------------------------------| */
/* | Description:     | GetTaskID returns the information about the TaskID of   | */
/* |                  | the task which is currently running.                    | */
/* |------------------+---------------------------------------------------------| */
/* | Particularities: | 1. Allowed on task level, ISR level and in several hook | */
/* |                  | routines (see Figure 12-1(os223)).                      | */
/* |                  | 2. This service is intended to be used by library       | */
/* |                  | functions and hook routines.                            | */
/* |                  | 3. If <TaskID> can't be evaluated (no task currently    | */
/* |                  | running), the service returns INVALID_TASK as TaskType. | */
/* |------------------+---------------------------------------------------------| */
/* | Status:          | Standard:  No error, E_OK                               | */
/* |                  | Extended:  No error, E_OK                               | */
/* |------------------+---------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                  | */
/* |------------------+---------------------------------------------------------| */
StatusType GetTaskID(TaskRefType pTaskType) {
  DECLARE_SMP_PROCESSOR_ID();

  EnterCritical();
  if (RunningVar != NULL) {
    *pTaskType = RunningVar - TaskVarArray;
  } else {
    *pTaskType = INVALID_TASK;
  }
  ExitCritical();

  return E_OK;
}

/* |------------------+-------------------------------------------------------| */
/* | Syntax:          | StatusType GetTaskState ( TaskType <TaskID>,          | */
/* |                  | TaskStateRefType <State> )                            | */
/* |------------------+-------------------------------------------------------| */
/* | Parameter (In):  | TaskID: Task reference                                | */
/* |------------------+-------------------------------------------------------| */
/* | Parameter (Out): | State: Reference to the state of the task <TaskID>    | */
/* |------------------+-------------------------------------------------------| */
/* | Description:     | Returns the state of a task (running, ready, waiting, | */
/* |                  | suspended) at the time of calling GetTaskState.       | */
/* |------------------+-------------------------------------------------------| */
/* | Particularities: | The service may be called from interrupt service      | */
/* |                  | routines, task level, and some hook routines (see     | */
/* |                  | Figure 12-1(os223.doc)). When a call is made from a   | */
/* |                  | task in a full preemptive system, the result may      | */
/* |                  | already be incorrect at the time of evaluation.       | */
/* |                  | When the service is called for a task, which is       | */
/* |                  | activated more than once, the state is set to         | */
/* |                  | running if any instance of the task is running.       | */
/* |------------------+-------------------------------------------------------| */
/* | Status:          | Standard: No error, E_OK                              | */
/* |                  | Extended: Task <TaskID> is invalid, E_OS_ID           | */
/* |------------------+-------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                                | */
/* |------------------+-------------------------------------------------------| */
StatusType GetTaskState(TaskType TaskID, TaskStateRefType State) {
  StatusType ercd = E_OK;
  TaskVarType *pTaskVar;
  DECLARE_SMP_PROCESSOR_ID();

#if (OS_STATUS == EXTENDED)
  if (TaskID < TASK_NUM) {
#endif
    EnterCritical();
    pTaskVar = &TaskVarArray[TaskID];
    if (RunningVar == pTaskVar) {
      *State = RUNNING;
    } else {
      *State = pTaskVar->state;
    }
    ExitCritical();
#if (OS_STATUS == EXTENDED)
  } else {
    ercd = E_OS_ID;
  }
#endif

  OSErrorTwo(GetTaskState, TaskID, State);
  return ercd;
}

void Os_TaskInit(AppModeType appMode) {
  TaskType id;

  const TaskConstType *pTaskConst;
  TaskVarType *pTaskVar;

  for (id = 0; id < TASK_NUM; id++) {
    pTaskConst = &TaskConstArray[id];
    pTaskVar = &TaskVarArray[id];

    pTaskVar->state = SUSPENDED;
    pTaskVar->pConst = pTaskConst;
#ifdef USE_SHELL
    pTaskVar->actCnt = 0;
#endif

    if (pTaskConst->appModeMask & appMode) {
      (void)ActivateTask(id);
    }
  }
#if (OS_PTHREAD_NUM > 0)
  for (id = 0; id < OS_PTHREAD_NUM; id++) {
    pTaskVar = &TaskVarArray[TASK_NUM + id];
    pTaskVar->pConst = NULL;
  }
#endif
}

#ifdef USE_SHELL
void statOsTask(void) {
  TaskType id;
  const TaskConstType *pTaskConst;
  TaskVarType *pTaskVar;
  uint32_t pused;
  uint32_t used;
  printf("Name             State      Prio IPrio RPrio  StackBase  StackSize"
         "   Used       Event(set/wait)   Act/ActSum parent\n");
#if (OS_PTHREAD_NUM > 0)
  printf("                                                                  "
         "              list/entry\n");
#endif

  for (id = 0; id < TASK_NUM; id++) {
    pTaskConst = &TaskConstArray[id];
    pTaskVar = &TaskVarArray[id];
    pused = checkStackUsage(pTaskConst, &used);
    printf("%-16s %-9s %3u  %3u   %3u     0x%08X 0x%08X %2d%%(0x%04X) ", pTaskConst->name,
           taskStateToString(pTaskVar->state), (uint32_t)pTaskVar->priority,
           (uint32_t)pTaskConst->initPriority, (uint32_t)pTaskConst->runPriority,
           (uint32_t)pTaskConst->pStack, (uint32_t)pTaskConst->stackSize, (uint32_t)pused,
           (uint32_t)used);
    if (NULL != pTaskConst->pEventVar) {
      printf("%08X/%08X %3u/%-6u ", (uint32_t)pTaskConst->pEventVar->set,
             (uint32_t)pTaskConst->pEventVar->wait,
#ifdef MULTIPLY_TASK_ACTIVATION
             (uint32_t)pTaskVar->activation,
#else
             (uint32_t)1,
#endif
             (uint32_t)pTaskVar->actCnt);
    } else {
      printf("null              %3u/%-6u ",
#ifdef MULTIPLY_TASK_ACTIVATION
             (uint32_t)pTaskVar->activation,
#else
             (uint32_t)1,
#endif
             (uint32_t)pTaskVar->actCnt);
    }
#ifdef USE_SMP
    printf(" on CPU%d", (int)pTaskVar->oncpu);
#endif
    printf(" %s\n", isRunning(pTaskVar) ? "<-RunningVar" : "");
  }

#if (OS_PTHREAD_NUM > 0)
  for (id = 0; id < OS_PTHREAD_NUM; id++) {
    struct pthread *tid;
    pTaskVar = &TaskVarArray[TASK_NUM + id];
    pTaskConst = pTaskVar->pConst;
    tid = (struct pthread *)pTaskConst;
    if (tid > (struct pthread *)1) {
      pused = checkStackUsage(pTaskConst, &used);
      printf("pthread%-9u %-9s %3u  %3u   %3u     0x%08X 0x%08X %2d%%(0x%04X) %08X/%08X %3d/%-6d ",
             (uint32_t)id, taskStateToString(pTaskVar->state), (uint32_t)pTaskVar->priority,
             (uint32_t)pTaskConst->initPriority, (uint32_t)pTaskConst->runPriority,
             (uint32_t)pTaskConst->pStack, (uint32_t)pTaskConst->stackSize, (uint32_t)pused,
             (uint32_t)used, (uint32_t)(long)pTaskVar->list, (uint32_t)(long)tid->start,
#ifdef MULTIPLY_TASK_ACTIVATION
             (uint32_t)pTaskVar->activation,
#else
             (uint32_t)1,
#endif
             (uint32_t)pTaskVar->actCnt);
#ifdef USE_PTHREAD_PARENT
      if (NULL != tid->parent) {
        if ((tid->parent - TaskVarArray) < TASK_NUM) {
          printf(" %s", tid->parent->pConst->name);
        } else {
          printf(" pthread%u", (uint32)(tid->parent - TaskVarArray));
        }
      } else {
        printf(" none");
      }
#endif
#ifdef USE_SMP
      printf(" on CPU%d", (int)pTaskVar->oncpu);
#endif
      printf(" %s\n", isRunning(pTaskVar) ? "<-RunningVar" : "");
    }
  }
#endif
}
#endif
