/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if defined(USE_SCHED_FIFO) && defined(USE_SMP)
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define SCHED_FIFO_SIZE(fifo) ((fifo)->pFIFO)[0]
#define SCHED_FIFO_HEAD(fifo) ((fifo)->pFIFO)[1]
#define SCHED_FIFO_TAIL(fifo) ((fifo)->pFIFO)[2]

#define SCHED_FIFO_SLOT_OFFSET 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
extern const ReadyFIFOType ReadyFIFO[CPU_CORE_NUMBER + 1][PRIORITY_NUM + 1];

#if (PRIORITY_NUM > 63)
static uint8 ReadyGroup[CPU_CORE_NUMBER + 1];
#endif

#if (PRIORITY_NUM > 7)
static uint8 ReadyGroupTable[CPU_CORE_NUMBER + 1][(PRIORITY_NUM + 64) / 64];
#endif

static uint8 ReadyMapTable[CPU_CORE_NUMBER + 1][(PRIORITY_NUM + 8) / 8];
/**************************************************
#include <stdio.h>
int main(int argc, char *argv[])
{
    unsigned int i;
    printf("static uint8_t tableUnMap[256]=\n{");
    for (i=0; i <= 0xff; ++i)
    {
        if(i%16==0) printf("\n\t");
        if(i&(1u<<7))
            printf("7,");
        else if(i&(1u<<6))
            printf("6,");
        else if(i&(1u<<5))
            printf("5,");
        else if(i&(1u<<4))
            printf("4,");
        else if(i&(1u<<3))
            printf("3,");
        else if(i&(1u<<2))
            printf("2,");
        else if(i&(1u<<1))
            printf("1,");
        else if(i&(1u<<0))
            printf("0,");
        else printf("0,");
    }
    printf("\n}\n");
    return 0;
}
used to generate the map like ucos,but inverted from low to high
*******************************************************/
static uint8_t tableUnMap[256] = {
  0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
};
/* ================================ [ LOCALS    ] ============================================== */
static inline void Sched_SetReadyBit(uint8 oncpu, PriorityType priority) {
#if (PRIORITY_NUM > 63)
  ReadyGroup[oncpu] |= (1u << (priority >> 6));
#endif

#if (PRIORITY_NUM > 7)
  ReadyGroupTable[oncpu][priority >> 6] |= (1u << ((priority & 0x3Fu) >> 3));
#endif

  ReadyMapTable[oncpu][(priority >> 6) * 8 + ((priority & 0x3Fu) >> 3)] |=
    (1u << (priority & 0x7u));
}

static inline void Sched_ClearReadyBit(uint8 oncpu, PriorityType priority) {
  ReadyMapTable[oncpu][(priority >> 6) * 8 + ((priority & 0x3Fu) >> 3)] &=
    ~(1u << (priority & 0x7u));

#if (PRIORITY_NUM > 7)
  if (0u == ReadyMapTable[oncpu][(priority >> 6) * 8 + ((priority & 0x3Fu) >> 3)]) {
    ReadyGroupTable[oncpu][priority >> 6] &= ~(1u << ((priority & 0x3Fu) >> 3));
  }
#endif

#if (PRIORITY_NUM > 63)
  if (0u == ReadyGroupTable[oncpu][priority >> 6]) {
    ReadyGroup &= ~(1u << (priority >> 6));
  }
#endif
}

static inline PriorityType Sched_GetReadyBit(uint8 oncpu) {
#if (PRIORITY_NUM > 63)
  uint8 Z;
#endif
#if (PRIORITY_NUM > 7)
  uint8 X;
#endif
  uint8 Y;

#if (PRIORITY_NUM > 63)
  Z = tableUnMap[ReadyGroup[oncpu]];
#else
#define Z 0u
#endif

#if (PRIORITY_NUM > 7)
  X = tableUnMap[ReadyGroupTable[oncpu][Z]];
#else
#define X 0u
#endif

  Y = tableUnMap[ReadyMapTable[oncpu][(Z << 6) + X]];

  return ((Z << 6) + (X << 3) + Y);
}
static void RemoveFromFifo(TaskType TaskID, PriorityType priority) {
  const ReadyFIFOType *fifo;
  uint32 i, j, pos, posI, posJ;
  uint8 oncpu;

  asAssert(priority <= PRIORITY_NUM);

  oncpu = TaskVarArray[TaskID].oncpu;

  fifo = &(ReadyFIFO[oncpu][priority]);

  for (i = 0; i < SCHED_FIFO_SIZE(fifo); i++) {
    pos = SCHED_FIFO_HEAD(fifo) + i;
    if (pos >= fifo->max) {
      pos = SCHED_FIFO_SLOT_OFFSET;
    }
    while ((TaskID == fifo->pFIFO[pos]) && (SCHED_FIFO_SIZE(fifo) > 0)) {
      for (j = i + 1, posI = pos; j < SCHED_FIFO_SIZE(fifo); j++) {
        posJ = posI + 1;
        if (posJ >= fifo->max) {
          posJ = SCHED_FIFO_SLOT_OFFSET;
        }
        fifo->pFIFO[posI] = fifo->pFIFO[posJ];
        posI = posJ;
      }
      SCHED_FIFO_TAIL(fifo)--;
      if (SCHED_FIFO_TAIL(fifo) < SCHED_FIFO_SLOT_OFFSET) {
        SCHED_FIFO_TAIL(fifo) = fifo->max - 1;
      }
      SCHED_FIFO_SIZE(fifo)--;
    }
  }

  if (0u == SCHED_FIFO_SIZE(fifo)) {
    Sched_ClearReadyBit(oncpu, priority);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Sched_Init(void) {
  PriorityType prio;
  uint8 oncpu;
  const ReadyFIFOType *fifo;
  for (oncpu = 0; oncpu <= CPU_CORE_NUMBER; oncpu++) {
    for (prio = 0; prio <= PRIORITY_NUM; prio++) {
      fifo = &(ReadyFIFO[oncpu][prio]);
      if (fifo->pFIFO != NULL) {
        SCHED_FIFO_SIZE(fifo) = 0;
        SCHED_FIFO_HEAD(fifo) = SCHED_FIFO_SLOT_OFFSET;
        SCHED_FIFO_TAIL(fifo) = SCHED_FIFO_SLOT_OFFSET;
      }
    }
  }
#if (PRIORITY_NUM > 63)
  memset(ReadyGroup, 0, sizeof(ReadyGroup));
#endif

#if (PRIORITY_NUM > 7)
  memset(ReadyGroupTable, 0, sizeof(ReadyGroupTable));
#endif

  memset(ReadyMapTable, 0, sizeof(ReadyMapTable));
}

void Sched_AddReady(TaskType TaskID) {
  DECLARE_SMP_PROCESSOR_ID();
  uint8 oncpu;
  PriorityType priority;
  const ReadyFIFOType *fifo;

  priority = TaskVarArray[TaskID].pConst->initPriority;

  asAssert(priority <= PRIORITY_NUM);

  oncpu = TaskVarArray[TaskID].oncpu;

  fifo = &(ReadyFIFO[oncpu][priority]);
  asAssert(fifo->pFIFO);

  asAssert(SCHED_FIFO_SIZE(fifo) < (fifo->max - SCHED_FIFO_SLOT_OFFSET));

  SCHED_FIFO_SIZE(fifo)++;
  fifo->pFIFO[SCHED_FIFO_TAIL(fifo)] = TaskID;
  SCHED_FIFO_TAIL(fifo)++;
  if (SCHED_FIFO_TAIL(fifo) >= fifo->max) {
    SCHED_FIFO_TAIL(fifo) = SCHED_FIFO_SLOT_OFFSET;
  }

  Sched_SetReadyBit(oncpu, priority);

  if (priority > ReadyVar->priority) {
    ReadyVar = &TaskVarArray[fifo->pFIFO[SCHED_FIFO_HEAD(fifo)]];
  } else if (ReadyVar == RunningVar) {
    PriorityType priority1;
    PriorityType priority2;

    priority1 = Sched_GetReadyBit(cpuid);
    priority2 = Sched_GetReadyBit(OS_ON_ANY_CPU);

    if (priority1 >= priority2) {
      priority = priority1;
      oncpu = cpuid;
    } else {
      priority = priority2;
      oncpu = OS_ON_ANY_CPU;
    }

    fifo = &ReadyFIFO[oncpu][priority];
    asAssert(fifo->pFIFO);
    ReadyVar = &TaskVarArray[fifo->pFIFO[SCHED_FIFO_HEAD(fifo)]];
  } else {
    /* no update of ReadyVar */
  }

  if ((cpuid != oncpu) && (OS_ON_ANY_CPU != oncpu)) {
    if (priority > RunningVars[oncpu]->priority) {
      Os_PortRequestSchedule(oncpu);
    }
  }
}

void Sched_Preempt(void) {
  DECLARE_SMP_PROCESSOR_ID();
  uint8 oncpu;
  PriorityType priority;
  const ReadyFIFOType *fifo;

  /* remove the ReadyVar from the queue */
  priority = ReadyVar->priority;
  oncpu = ReadyVar->oncpu;
  fifo = &(ReadyFIFO[oncpu][priority]);
  asAssert(fifo->pFIFO);

  SCHED_FIFO_SIZE(fifo)--;
  if (0u == SCHED_FIFO_SIZE(fifo)) {
    Sched_ClearReadyBit(oncpu, priority);
  }

  SCHED_FIFO_HEAD(fifo)++;
  if (SCHED_FIFO_HEAD(fifo) >= fifo->max) {
    SCHED_FIFO_HEAD(fifo) = SCHED_FIFO_SLOT_OFFSET;
  }

  ReadyVar->oncpu = cpuid;

  /* put the RunningVar back to the head of queue */
  priority = RunningVar->priority;
  fifo = &(ReadyFIFO[cpuid][priority]);
  asAssert(fifo->pFIFO);

  SCHED_FIFO_SIZE(fifo)++;
  SCHED_FIFO_HEAD(fifo)--;
  if (SCHED_FIFO_HEAD(fifo) < SCHED_FIFO_SLOT_OFFSET) {
    SCHED_FIFO_HEAD(fifo) = fifo->max - 1;
  }
  fifo->pFIFO[SCHED_FIFO_HEAD(fifo)] = RunningVar - TaskVarArray;

  Sched_SetReadyBit(cpuid, priority);
}

void Sched_GetReady(void) {
  DECLARE_SMP_PROCESSOR_ID();
  const ReadyFIFOType *fifo;
  uint8 oncpu;

  PriorityType priority1;
  PriorityType priority2;
  PriorityType priority;

  priority1 = Sched_GetReadyBit(cpuid);
  priority2 = Sched_GetReadyBit(OS_ON_ANY_CPU);

  if (priority1 >= priority2) {
    priority = priority1;
    oncpu = cpuid;
  } else {
    priority = priority2;
    oncpu = OS_ON_ANY_CPU;
  }

  fifo = &ReadyFIFO[oncpu][priority];

  if (NULL != fifo->pFIFO) {
    if (SCHED_FIFO_SIZE(fifo) > 0) {
      /* remove the ReadyVar from the queue */
      ReadyVar = &TaskVarArray[fifo->pFIFO[SCHED_FIFO_HEAD(fifo)]];
      SCHED_FIFO_SIZE(fifo)--;
      if (0u == SCHED_FIFO_SIZE(fifo)) {
        Sched_ClearReadyBit(oncpu, priority);
      }

      SCHED_FIFO_HEAD(fifo)++;
      if (SCHED_FIFO_HEAD(fifo) >= fifo->max) {
        SCHED_FIFO_HEAD(fifo) = SCHED_FIFO_SLOT_OFFSET;
      }

      ReadyVar->oncpu = cpuid;
    } else {
      ReadyVar = NULL;
    }
  } else {
    ReadyVar = NULL;
  }
}

void Sched_RemoveReady(TaskType TaskID) {
  TaskVarType *pTaskVar = &TaskVarArray[TaskID];

  RemoveFromFifo(TaskID, pTaskVar->pConst->initPriority);
  RemoveFromFifo(TaskID, pTaskVar->priority);
}

boolean Sched_Schedule(void) {
  DECLARE_SMP_PROCESSOR_ID();
  boolean needSchedule = FALSE;
  const ReadyFIFOType *fifo;
  uint8 oncpu;

  PriorityType priority1;
  PriorityType priority2;
  PriorityType priority;

  priority1 = Sched_GetReadyBit(cpuid);
  priority2 = Sched_GetReadyBit(OS_ON_ANY_CPU);

  if (priority1 >= priority2) {
    priority = priority1;
    oncpu = cpuid;

  } else {
    priority = priority2;
    oncpu = OS_ON_ANY_CPU;
  }

  fifo = &ReadyFIFO[oncpu][priority];
  if (NULL != fifo->pFIFO) {
    if (SCHED_FIFO_SIZE(fifo) > 0) {
      ReadyVar = &TaskVarArray[fifo->pFIFO[SCHED_FIFO_HEAD(fifo)]];
#if (OS_PTHREAD_NUM > 0)
      if (ReadyVar->priority >= RunningVar->priority)
#else
      if (ReadyVar->priority > RunningVar->priority)
#endif
      {
        /* remove the ReadyVar from the queue */
        SCHED_FIFO_SIZE(fifo)--;
        if (0u == SCHED_FIFO_SIZE(fifo)) {
          Sched_ClearReadyBit(oncpu, priority);
        }

        SCHED_FIFO_HEAD(fifo)++;
        if (SCHED_FIFO_HEAD(fifo) >= fifo->max) {
          SCHED_FIFO_HEAD(fifo) = SCHED_FIFO_SLOT_OFFSET;
        }

        ReadyVar->oncpu = cpuid;

        /* put the RunningVar back to the head of queue */
        priority = RunningVar->priority;
        fifo = &ReadyFIFO[cpuid][priority];
        asAssert(fifo->pFIFO);

        SCHED_FIFO_SIZE(fifo)++;
        SCHED_FIFO_HEAD(fifo)--;
        if (SCHED_FIFO_HEAD(fifo) < SCHED_FIFO_SLOT_OFFSET) {
          SCHED_FIFO_HEAD(fifo) = fifo->max - 1;
        }
        fifo->pFIFO[SCHED_FIFO_HEAD(fifo)] = RunningVar - TaskVarArray;

        Sched_SetReadyBit(cpuid, priority);
        needSchedule = TRUE;
      }
    }
  }

  return needSchedule;
}
#endif /* USE_SCHED_FIFO */
