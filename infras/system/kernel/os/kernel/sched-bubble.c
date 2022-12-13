/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if defined(USE_SCHED_BUBBLE) && !defined(USE_SMP)
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
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
} ReadyQueueType;
/* ================================ [ DECLARES  ] ============================================== */
#ifdef MULTIPLY_TASK_PER_PRIORITY
#define NEW_PRIORITY(prio)                                                                         \
  (((uint16)(prio) << SEQUENCE_SHIFT) | ((--PrioSeqVal[prio]) & SEQUENCE_MASK))
#define NEW_PRIOHIGHEST(prio) (((uint16)(prio) << SEQUENCE_SHIFT) | (SEQUENCE_MASK))
#define REAL_PRIORITY(prio) Sched_RealPriority(prio)
#else
#define NEW_PRIORITY(prio) (prio)
#define NEW_PRIOHIGHEST(prio) (prio)
#define REAL_PRIORITY(prio) (prio)
#endif
/* ================================ [ DATAS     ] ============================================== */
static ReadyQueueType ReadyQueue;
#ifdef MULTIPLY_TASK_PER_PRIORITY
static uint8 PrioSeqVal[PRIORITY_NUM + 1];
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifdef MULTIPLY_TASK_PER_PRIORITY
static inline uint16 Sched_RealPriority(uint16 priority) {
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
  tmp = (tmp - PrioSeqVal[real]) & SEQUENCE_MASK;

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

/* ================================ [ FUNCTIONS ] ============================================== */
void Sched_Init(void) {
  ReadyQueue.size = 0;
}
void Sched_ShowRdyQ(void) {
  uint32 i;
  printf("\nRDYQ:");
  for (i = 0; i < ReadyQueue.size; i++) {
    printf("%d(%d/%d)->", ReadyQueue.heap[i].taskID, REAL_PRIORITY(ReadyQueue.heap[i].priority),
           TaskVarArray[ReadyQueue.heap[i].taskID].priority);
  }
  printf("\n");
}
void Sched_AddReady(TaskType TaskID) {
  DECLARE_SMP_PROCESSOR_ID();

  asAssert(ReadyQueue.size < ACTIVATION_SUM);

  ReadyQueue.heap[ReadyQueue.size].taskID = TaskID;
  ReadyQueue.heap[ReadyQueue.size].priority =
    NEW_PRIORITY(TaskVarArray[TaskID].pConst->initPriority);
  Sched_BubbleUp(&ReadyQueue, ReadyQueue.size);
  ReadyQueue.size++;
  ReadyVar = &TaskVarArray[ReadyQueue.heap[0].taskID];
}

void Sched_RemoveReady(TaskType TaskID) {
  uint32 i;

  for (i = 0; i < ReadyQueue.size; i++) {
    while ((TaskID == ReadyQueue.heap[i].taskID) && (ReadyQueue.size > 0)) {
      ReadyQueue.heap[i] = ReadyQueue.heap[--ReadyQueue.size];
      Sched_BubbleDown(&ReadyQueue, i);
    }
  }
}

void Sched_Preempt(void) {
  DECLARE_SMP_PROCESSOR_ID();

  asAssert(ReadyVar == &TaskVarArray[ReadyQueue.heap[0].taskID]);
  ReadyQueue.heap[0].taskID = RunningVar - TaskVarArray;
  ReadyQueue.heap[0].priority = NEW_PRIOHIGHEST(RunningVar->priority);
  Sched_BubbleDown(&ReadyQueue, 0);
}

void Sched_GetReady(void) {
  DECLARE_SMP_PROCESSOR_ID();

  if (ReadyQueue.size > 0) {
    ReadyVar = &TaskVarArray[ReadyQueue.heap[0].taskID];
    ReadyQueue.size--;
    ReadyQueue.heap[0] = ReadyQueue.heap[ReadyQueue.size];

    Sched_BubbleDown(&ReadyQueue, 0);
  } else {
    ReadyVar = NULL;
  }
}

boolean Sched_Schedule(void) {
  boolean needSchedule = FALSE;
  DECLARE_SMP_PROCESSOR_ID();

  if (ReadyQueue.size > 0) {
    ReadyVar = &TaskVarArray[ReadyQueue.heap[0].taskID];
#if (OS_PTHREAD_NUM > 0)
    if (ReadyVar->priority >= RunningVar->priority)
#else
    if (ReadyVar->priority > RunningVar->priority)
#endif
    {
      ReadyQueue.heap[0].taskID = RunningVar - TaskVarArray;
      ReadyQueue.heap[0].priority = NEW_PRIOHIGHEST(RunningVar->priority);
      Sched_BubbleDown(&ReadyQueue, 0);

      needSchedule = TRUE;
    }
  }
  return needSchedule;
}

#endif /* USE_SCHED_BUBBLE */
