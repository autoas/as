/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
#ifdef USE_SMP
#include "spinlock.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 0
#define AS_LOG_OSE 1
#define AS_LOG_SMP 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Os_PortResume(void);
extern void Os_PortActivate(void);
extern void Os_PortStartSysTick(void);
#ifdef USE_SMP
extern void secondary_start(void);
extern void Ipc_KickTo(int cpu, int irqno);
extern void Irq_Install(int irqno, void (*handler)(void), int oncpu);
#endif
/* ================================ [ DATAS     ] ============================================== */
#ifdef USE_SMP
boolean smpStarted = FALSE;
static spinlock_t knlSpinlock;
uint32 ISR2Counter[CPU_CORE_NUMBER];
#else
uint32 ISR2Counter;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifdef USE_SMP
static void Os_PortSchedule(void) {
  DECLARE_SMP_PROCESSOR_ID();
  ASLOG(SMP, ("Os_PortSchedule on CPU%d!\n", cpuid));
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void Os_PortActivateImpl(void) {
  DECLARE_SMP_PROCESSOR_ID();

  /* get internal resource or NON schedule */
  RunningVar->priority = RunningVar->pConst->runPriority;

#ifdef USE_SMP
  ASLOG(OS, ("%s(%d) is running on CPU %d\n", RunningVar->pConst->name,
             RunningVar->pConst->initPriority, cpuid));
#else
  ASLOG(OS, ("%s(%d) is running\n", RunningVar->pConst->name, RunningVar->pConst->initPriority));
#endif

  OSPreTaskHook();

  CallLevel = TCL_TASK;
  EnableInterrupt();

  RunningVar->pConst->entry();

  /* Should not return here */
  TerminateTask();
}

void Os_PortInit(void) {
#ifdef USE_SMP
  memset(ISR2Counter, 0, sizeof(ISR2Counter));
  Irq_Install(0, Os_PortSchedule, 0);
  Irq_Install(1, Os_PortSchedule, 1);
#else
  ISR2Counter = 0;
#endif
}

void Os_PortInitContext(TaskVarType *pTaskVar) {
  /* 8 byte aligned */
  pTaskVar->context.sp =
    (void *)((uint64_t)(pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize - 8) &
             (~(uint64_t)0x7UL));
  pTaskVar->context.pc = Os_PortActivate;
}

void Os_PortDispatch(void) {
  __asm("svc 0");
}

void Os_PortSyncException(void *sp, uint32_t esr) {
  uint8_t ec;
  uint64_t *stk = (uint64_t *)sp;

  ec = (uint8_t)((esr >> 26) & 0x3fU);
  if (ec != 0x15) { /* only valid sync exception is SVC call for os task dispatch */
    printf("!!!Invalid Sync Exception 0x%x, sp=0x%x!!!\n", ec, (uint32_t)(uint64_t)sp);
    printf("elr = 0x%x\n", (uint32_t)stk[0]);
    printf("spsr = 0x%x\n", (uint32_t)stk[1]);
    printf("lr = 0x%x\n", (uint32_t)stk[2]);
    printf("bsp = 0x%x\n", (uint32_t)stk[3]);
    while (1)
      DisableInterrupt();
  }
}

void Os_PortStartDispatch(void) {
  DECLARE_SMP_PROCESSOR_ID();

  RunningVar = NULL;
  Os_PortDispatch();
  asAssert(0);
}
void Os_PortIdle(void) {
  DECLARE_SMP_PROCESSOR_ID();
#ifdef USE_SMP
  ASLOG(OSE, ("!!!CPU%d enter PortIdle!!!\n", SMP_PROCESSOR_ID()));
#else
  ASLOG(OSE, ("!!!enter PortIdle!!!\n"));
#endif

  asAssert(0);
}
#ifdef USE_SMP
void Os_PortSpinLock(void) {
  spin_lock(&knlSpinlock);
}

void Os_PortSpinUnLock(void) {
  spin_unlock(&knlSpinlock);
}

void secondary_main(void) {
  DECLARE_SMP_PROCESSOR_ID();

  ASLOG(SMP, ("!!!CPU%d is up!!!\n", SMP_PROCESSOR_ID()));
  smpStarted = TRUE;
  Os_PortSpinLock();
  Sched_GetReady();
  Os_PortStartDispatch();
  while (1)
    ;
}

void Os_PortRequestSchedule(uint8 cpu) {
  if (smpStarted) {
    Ipc_KickTo((int)cpu, (int)cpu);
  }
}
#endif

void Os_PortStartFirstDispatch(void) {
#ifdef USE_SMP
  ASLOG(SMP, ("!!!CPU%d is up!!!\n", smp_processor_id()));
  smp_boot_secondary(1, secondary_start);
#endif
#ifndef USE_LATE_MCU_INIT
  Os_PortStartSysTick();
#endif
  Os_PortStartDispatch();
}

void Os_PortException(long exception, void *sp, long esr) {
  ASLOG(OSE, ("Exception %d happened!\n", (int)exception));
  asAssert(0);
}

TASK(TaskIdle1) {
  while (1) {
    STD_TRACE_OS_MAIN();
  }
}

#ifdef USE_PTHREAD_SIGNAL
void Os_PortCallSignal(int sig, void (*handler)(int), void *sp, void (*pc)(void)) {
  DECLARE_SMP_PROCESSOR_ID();

  asAssert(NULL != handler);

  handler(sig);

  /* restore its previous stack */
  RunningVar->context.sp = sp;
  RunningVar->context.pc = pc;
}

void Os_PortExitSignalCall(void) {
  Sched_GetReady();
  Os_PortStartDispatch();
}

int Os_PortInstallSignal(TaskVarType *pTaskVar, int sig, void *handler) {
  void *sp;
  uint64_t *stk;

  sp = pTaskVar->context.sp;

  if ((sp - pTaskVar->pConst->pStack) < (pTaskVar->pConst->stackSize * 3 / 4)) {
    /* stack 75% usage, ignore this signal call */
    ASLOG(OS, ("install signal %d failed\n", sig));
    return -1;
  }

  stk = sp;

  *(--stk) = (uint64_t)handler;              /* x1 */
  *(--stk) = (uint64_t)sig;                  /* x0 */
  *(--stk) = (uint64_t)pTaskVar->context.pc; /* x3 */
  *(--stk) = (uint64_t)sp;                   /* x2 */

  *(--stk) = 5;  /* x5  */
  *(--stk) = 4;  /* x4  */
  *(--stk) = 7;  /* x7  */
  *(--stk) = 6;  /* x6  */
  *(--stk) = 9;  /* x9  */
  *(--stk) = 8;  /* x8  */
  *(--stk) = 11; /* x11 */
  *(--stk) = 10; /* x10 */
  *(--stk) = 13; /* x13 */
  *(--stk) = 12; /* x12 */
  *(--stk) = 15; /* x15 */
  *(--stk) = 14; /* x14 */
  *(--stk) = 17; /* x17 */
  *(--stk) = 16; /* x16 */
  *(--stk) = 19; /* x19 */
  *(--stk) = 18; /* x18 */
  *(--stk) = 21; /* x21 */
  *(--stk) = 20; /* x20 */
  *(--stk) = 23; /* x23 */
  *(--stk) = 22; /* x22 */
  *(--stk) = 25; /* x25 */
  *(--stk) = 24; /* x24 */
  *(--stk) = 27; /* x27 */
  *(--stk) = 26; /* x26 */
  *(--stk) = 29; /* x29 */
  *(--stk) = 28; /* x28 */

  *(--stk) = (uint64_t)Os_PortExitSignalCall; /* x30 */
  *(--stk) = (uint64_t)Os_PortCallSignal;     /* elr_el1 */
  *(--stk) = 0x20000305;                      /* spsr_el1 */

  pTaskVar->context.sp = stk;
  pTaskVar->context.pc = Os_PortResume;

  return 0;
}
#endif
