/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#ifdef USE_SCHED_LIST
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#ifndef RDY_BITS
/* default 32 bit CPU */
#define RDY_BITS 32
#endif
#if (RDY_BITS == 32)
#define RDY_SHIFT 5
#define RDY_MASK 0x1F
#elif (RDY_BITS == 16)
#define RDY_SHIFT 4
#define RDY_MASK 0xF
#else
#error Not supported RDY_BITS, set it according to CPU bits.
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
static TAILQ_HEAD(ready_list, TaskVar) ReadyList[PRIORITY_NUM + 1];
static int ReadyGroup;
static int ReadyMapTable[(PRIORITY_NUM + RDY_BITS) / RDY_BITS];
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static inline void Sched_SetReadyBit(PriorityType priority) {
  asAssert(priority <= PRIORITY_NUM);

  priority = PRIORITY_NUM - priority;

  ReadyGroup |= (1u << (priority >> RDY_SHIFT));

  ReadyMapTable[priority >> RDY_SHIFT] |= (1u << (priority & RDY_MASK));
}

static inline void Sched_ClearReadyBit(PriorityType priority) {
  asAssert(priority <= PRIORITY_NUM);

  priority = PRIORITY_NUM - priority;

  ReadyMapTable[priority >> RDY_SHIFT] &= ~(1u << (priority & RDY_MASK));

  if (0u == ReadyMapTable[priority >> RDY_SHIFT]) {
    ReadyGroup &= ~(1u << (priority >> RDY_SHIFT));
  }
}

static inline PriorityType Sched_GetReadyBit(void) {
  int X, Y;
  PriorityType priority;

  Y = ffs(ReadyGroup);
  if (Y > 0) {
    asAssert(Y <= (sizeof(ReadyMapTable) / sizeof(int)));
    X = ffs(ReadyMapTable[Y - 1]);
    asAssert((X > 0) && (X < RDY_BITS));
    priority = ((Y - 1) << RDY_SHIFT) + (X - 1);
    priority = PRIORITY_NUM - priority;
  } else {
    priority = 0;
  }

  return priority;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Sched_Init(void) {
  PriorityType prio;

  ReadyGroup = 0;

  for (prio = 0; prio < (sizeof(ReadyMapTable) / sizeof(ReadyMapTable[0])); prio++) {
    ReadyMapTable[prio] = 0;
  }

  for (prio = 0; prio < (PRIORITY_NUM + 1); prio++) {
    TAILQ_INIT(&(ReadyList[prio]));
  }
}

void Sched_AddReady(TaskType TaskID) {
  PriorityType priority;
  TaskVarType *pTaskVar = &TaskVarArray[TaskID];

  priority = pTaskVar->pConst->initPriority;
  asAssert(priority <= PRIORITY_NUM);

  TAILQ_INSERT_TAIL(&(ReadyList[priority]), pTaskVar, rentry);

  Sched_SetReadyBit(priority);

  if (priority > ReadyVar->priority) {
    ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));
  } else if (ReadyVar == RunningVar) {
    priority = Sched_GetReadyBit();
    ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));
  } else {
    /* no update of ReadyVar */
  }
}

void Sched_RemoveReady(TaskType TaskID) {
  PriorityType priority;
  TaskVarType *pVar;
  TaskVarType *pTaskVar = &TaskVarArray[TaskID];

  priority = pTaskVar->pConst->initPriority;
  asAssert(priority <= PRIORITY_NUM);

  TAILQ_FOREACH(pVar, &(ReadyList[priority]), rentry) {
    if (pVar == pTaskVar) {
      TAILQ_REMOVE(&(ReadyList[priority]), pVar, rentry);
      if (TAILQ_EMPTY(&(ReadyList[priority]))) {
        Sched_ClearReadyBit(priority);
      }
      break;
    }
  }

  priority = pTaskVar->priority;
  asAssert(priority <= PRIORITY_NUM);

  TAILQ_FOREACH(pVar, &(ReadyList[priority]), rentry) {
    if (pVar == pTaskVar) {
      TAILQ_REMOVE(&(ReadyList[priority]), pVar, rentry);
      if (TAILQ_EMPTY(&(ReadyList[priority]))) {
        Sched_ClearReadyBit(priority);
      }
      break;
    }
  }
}

void Sched_Preempt(void) {
  PriorityType priority;

  /* remove the ReadyVar from the queue */
  priority = ReadyVar->priority;
  TAILQ_REMOVE(&(ReadyList[priority]), ReadyVar, rentry);
  if (TAILQ_EMPTY(&(ReadyList[priority]))) {
    Sched_ClearReadyBit(priority);
  }

  /* put the RunningVar back to the head of queue */
  priority = RunningVar->priority;
  TAILQ_INSERT_HEAD(&(ReadyList[priority]), RunningVar, rentry);
  Sched_SetReadyBit(priority);
}

void Sched_GetReady(void) {
  PriorityType priority = Sched_GetReadyBit();

  ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));

  if (NULL != ReadyVar) {
    TAILQ_REMOVE(&(ReadyList[priority]), ReadyVar, rentry);
    if (TAILQ_EMPTY(&(ReadyList[priority]))) {
      Sched_ClearReadyBit(priority);
    }
  }
}

boolean Sched_Schedule(void) {
  boolean needSchedule = FALSE;

  PriorityType priority = Sched_GetReadyBit();

  ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));

#if (OS_PTHREAD_NUM > 0)
  if ((NULL != ReadyVar) && (ReadyVar->priority >= RunningVar->priority))
#else
  if ((NULL != ReadyVar) && (ReadyVar->priority > RunningVar->priority))
#endif
  {
    /* remove the ReadyVar from the queue */
    TAILQ_REMOVE(&(ReadyList[priority]), ReadyVar, rentry);
    if (TAILQ_EMPTY(&(ReadyList[priority]))) {
      Sched_ClearReadyBit(priority);
    }

    /* put the RunningVar back to the head of queue */
    priority = RunningVar->priority;
    TAILQ_INSERT_HEAD(&(ReadyList[priority]), RunningVar, rentry);
    Sched_SetReadyBit(priority);

    needSchedule = TRUE;
  }
  return needSchedule;
}

#endif /* USE_SCHED_LIST */
