/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if defined(USE_SCHED_BUBBLE) && defined(USE_SMP)
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SCHED 1
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  TaskType taskID;
#ifdef MULTIPLY_TASK_PER_PRIORITY
  uint16 priority;
#else
  PriorityType priority;
#endif
} ReadyEntryType;

typedef struct {
  uint32 size;
  ReadyEntryType heap[ACTIVATION_SUM];
#ifdef MULTIPLY_TASK_PER_PRIORITY
  uint8 PrioSeqVal[PRIORITY_NUM + 1];
#endif
} ReadyQueueType;
/* ================================ [ DECLARES  ] ============================================== */
#ifdef MULTIPLY_TASK_PER_PRIORITY
#define NEW_PRIORITY(prio)                                                                         \
  (((uint16)(prio) << SEQUENCE_SHIFT) | ((--pReadyQueue->PrioSeqVal[prio]) & SEQUENCE_MASK))
#define NEW_PRIOHIGHEST(prio) (((uint16)(prio) << SEQUENCE_SHIFT) | (SEQUENCE_MASK))
#define REAL_PRIORITY(prio) Sched_RealPriority(pReadyQueue, prio)
#else
#define NEW_PRIORITY(prio) (prio)
#define NEW_PRIOHIGHEST(prio) (prio)
#define REAL_PRIORITY(prio) (prio)
#endif
/* ================================ [ DATAS     ] ============================================== */
/* The last one is used to sort the task that can run on ANY CPU core */
static ReadyQueueType ReadyQueue[CPU_CORE_NUMBER + 1];
/* ================================ [ LOCALS    ] ============================================== */
#ifdef MULTIPLY_TASK_PER_PRIORITY
static inline uint16 Sched_RealPriority(ReadyQueueType *pReadyQueue, PriorityType priority) {
  uint16 real, tmp;

  real = priority >> SEQUENCE_SHIFT;
  tmp = priority & SEQUENCE_MASK;
  /* equals to
   * if(tmp > PrioSeqVal[real])
   *   tmp=tmp - PrioSeqVal[real];
   * else
   *   tmp=SEQUENCE_MASK+tmp+1-PrioSeqVal[real];
   * so it was the sequence decreased value after the activation of <real>,
   * bigger value means higher priority.
   */
  tmp = (tmp - pReadyQueue->PrioSeqVal[real]) & SEQUENCE_MASK;

  real = (real << SEQUENCE_SHIFT) | tmp;

  return real;
}
#endif

static void Sched_BubbleUp(ReadyQueueType *pReadyQueue, uint32 index) {
  uint32 father = index >> 1;
  while (REAL_PRIORITY(pReadyQueue->heap[father].priority) <
         REAL_PRIORITY(pReadyQueue->heap[index].priority)) {
    /*
     * if the father priority is lower then the index priority, swap them
     */
    ReadyEntryType tmpVar = pReadyQueue->heap[index];
    pReadyQueue->heap[index] = pReadyQueue->heap[father];
    pReadyQueue->heap[father] = tmpVar;
    index = father;
    father >>= 1;
  }
}

static void Sched_BubbleDown(ReadyQueueType *pReadyQueue, uint32 index) {
  uint32 size = pReadyQueue->size;
  uint32 child;
  while ((child = index << 1) < size) /* child = left */
  {
    uint32 right = child + 1;
    if ((right < size) && (REAL_PRIORITY(pReadyQueue->heap[child].priority) <
                           REAL_PRIORITY(pReadyQueue->heap[right].priority))) {
      /* the right child exists and is greater */
      child = right;
    }
    if (REAL_PRIORITY(pReadyQueue->heap[index].priority) <
        REAL_PRIORITY(pReadyQueue->heap[child].priority)) {
      /* the child has a higher priority, swap */
      ReadyEntryType tmpVar = pReadyQueue->heap[index];
      pReadyQueue->heap[index] = pReadyQueue->heap[child];
      pReadyQueue->heap[child] = tmpVar;
      /* go down */
      index = child;
    } else {
      /* went down to its place, stop the loop */
      break;
    }
  }
}

static void Sched_FindReady(ReadyQueueType **pReadyQueue) {
  DECLARE_SMP_PROCESSOR_ID();
  ReadyQueueType *pReadyQueue1 = &ReadyQueue[cpuid];
  ReadyQueueType *pReadyQueue2 = &ReadyQueue[OS_ON_ANY_CPU];
  TaskVarType *pTaskVar1;
  TaskVarType *pTaskVar2;

  if ((pReadyQueue1->size > 0) && (pReadyQueue2->size > 0)) {
    pTaskVar1 = &TaskVarArray[pReadyQueue1->heap[0].taskID];
    pTaskVar2 = &TaskVarArray[pReadyQueue2->heap[0].taskID];
    if (pTaskVar1->priority >= pTaskVar2->priority) {
      ReadyVar = pTaskVar1;
      *pReadyQueue = pReadyQueue1;
    } else {
      ReadyVar = pTaskVar2;
      *pReadyQueue = pReadyQueue2;
    }
  } else if (pReadyQueue1->size > 0) {
    ReadyVar = &TaskVarArray[pReadyQueue1->heap[0].taskID];
    *pReadyQueue = pReadyQueue1;
  } else if (pReadyQueue2->size > 0) {
    ReadyVar = &TaskVarArray[pReadyQueue2->heap[0].taskID];
    *pReadyQueue = pReadyQueue2;
  } else {
    ReadyVar = NULL;
    *pReadyQueue = NULL;
  }
}

static void Sched_AddReadyInternal(ReadyQueueType *pReadyQueue, TaskType TaskID,
                                   PriorityType priority) {
  asAssert(pReadyQueue->size < ACTIVATION_SUM);

  pReadyQueue->heap[pReadyQueue->size].taskID = TaskID;
  pReadyQueue->heap[pReadyQueue->size].priority = priority;
  Sched_BubbleUp(pReadyQueue, pReadyQueue->size);
  pReadyQueue->size++;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Sched_Init(void) {
  uint8 oncpu;

  for (oncpu = 0; oncpu <= CPU_CORE_NUMBER; oncpu++) {
    ReadyQueue[oncpu].size = 0;
  }
}

void Sched_ShowRdyQ(void) {
  uint32 i;
  uint8 oncpu;
  ReadyQueueType *pReadyQueue;
  for (oncpu = 0; oncpu <= CPU_CORE_NUMBER; oncpu++) {
    printf("\nRDYQ%d:", oncpu);
    pReadyQueue = &ReadyQueue[oncpu];
    for (i = 0; (i < pReadyQueue->size) && (i < ACTIVATION_SUM); i++) {
      printf("%d(%d/%d)->", pReadyQueue->heap[i].taskID,
             REAL_PRIORITY(pReadyQueue->heap[i].priority),
             TaskVarArray[pReadyQueue->heap[i].taskID].priority);
    }
    printf("\n");
  }
}
void Sched_AddReady(TaskType TaskID) {
  DECLARE_SMP_PROCESSOR_ID();
  uint8 oncpu;
  PriorityType priority;
  ReadyQueueType *pReadyQueue;

  oncpu = TaskVarArray[TaskID].oncpu;
  pReadyQueue = &ReadyQueue[oncpu];
#ifdef MULTIPLY_TASK_ACTIVATION
  asAssert((OS_ON_ANY_CPU != oncpu) || (1 == TaskVarArray[TaskID].pConst->maxActivation));
#endif
  priority = TaskVarArray[TaskID].pConst->initPriority;
  Sched_AddReadyInternal(pReadyQueue, TaskID, NEW_PRIORITY(priority));

  Sched_FindReady(&pReadyQueue);

  if ((cpuid != oncpu) && (OS_ON_ANY_CPU != oncpu)) {
    if (priority > RunningVars[oncpu]->priority) {
      Os_PortRequestSchedule(oncpu);
    }
  }
}

void Sched_RemoveReady(TaskType TaskID) {
  uint32 i;
  uint8 oncpu;
  ReadyQueueType *pReadyQueue;

  oncpu = TaskVarArray[TaskID].oncpu;
  pReadyQueue = &ReadyQueue[oncpu];
  for (i = 0; i < pReadyQueue->size; i++) {
    while ((TaskID == pReadyQueue->heap[i].taskID) && (pReadyQueue->size > 0)) {
      pReadyQueue->heap[i] = pReadyQueue->heap[--pReadyQueue->size];
      Sched_BubbleDown(pReadyQueue, i);
    }
  }

  if (TaskVarArray[TaskID].oncpu != OS_ON_ANY_CPU) {
    pReadyQueue = &ReadyQueue[OS_ON_ANY_CPU];
    for (i = 0; i < pReadyQueue->size; i++) {
      while ((TaskID == pReadyQueue->heap[i].taskID) && (pReadyQueue->size > 0)) {
        pReadyQueue->heap[i] = pReadyQueue->heap[--pReadyQueue->size];
        Sched_BubbleDown(pReadyQueue, i);
      }
    }
  }
}

void Sched_Preempt(void) {
  DECLARE_SMP_PROCESSOR_ID();
  ReadyQueueType *pReadyQueue;

  asAssert(ReadyVar);

  pReadyQueue = &ReadyQueue[ReadyVar->oncpu];

  asAssert(ReadyVar == &TaskVarArray[pReadyQueue->heap[0].taskID]);
  pReadyQueue->size--;
  pReadyQueue->heap[0] = pReadyQueue->heap[pReadyQueue->size];
  Sched_BubbleDown(pReadyQueue, 0);

  ReadyVar->oncpu = cpuid;

  Sched_AddReadyInternal(&ReadyQueue[cpuid], RunningVar - TaskVarArray,
                         NEW_PRIOHIGHEST(RunningVar->priority));
}

void Sched_GetReady(void) {
  DECLARE_SMP_PROCESSOR_ID();
  ReadyQueueType *pReadyQueue;

  Sched_FindReady(&pReadyQueue);
  if (NULL != pReadyQueue) {
    asAssert((ReadyVar - TaskVarArray) == pReadyQueue->heap[0].taskID);
    pReadyQueue->size--;
    pReadyQueue->heap[0] = pReadyQueue->heap[pReadyQueue->size];

    Sched_BubbleDown(pReadyQueue, 0);

    ReadyVar->oncpu = cpuid;
  }
}

boolean Sched_Schedule(void) {
  boolean needSchedule = FALSE;
  DECLARE_SMP_PROCESSOR_ID();
  ReadyQueueType *pReadyQueue;

  EnterCritical();
  Sched_FindReady(&pReadyQueue);
  if (NULL != pReadyQueue) {
#if (OS_PTHREAD_NUM > 0)
    if (ReadyVar->priority >= RunningVar->priority)
#else
    if (ReadyVar->priority > RunningVar->priority)
#endif
    {
      asAssert((ReadyVar - TaskVarArray) == pReadyQueue->heap[0].taskID);
      pReadyQueue->size--;
      pReadyQueue->heap[0] = pReadyQueue->heap[pReadyQueue->size];

      Sched_BubbleDown(pReadyQueue, 0);

      ReadyVar->oncpu = cpuid;

      Sched_AddReadyInternal(&ReadyQueue[cpuid], RunningVar - TaskVarArray,
                             NEW_PRIOHIGHEST(RunningVar->priority));

      needSchedule = TRUE;
    }
  }
  ExitCritical();
  return needSchedule;
}

#endif /* USE_SCHED_BUBBLE */
