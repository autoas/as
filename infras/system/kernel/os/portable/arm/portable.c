/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Os_PortResume(void);
/* ================================ [ DATAS     ] ============================================== */
uint32 ISR2Counter;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Os_PortActivate(void) {
  /* get internal resource or NON schedule */
  RunningVar->priority = RunningVar->pConst->runPriority;

  ASLOG(OS, ("%s(%d) is running\n", RunningVar->pConst->name, RunningVar->pConst->initPriority));

  CallLevel = TCL_TASK;
  EnableInterrupt();

  RunningVar->pConst->entry();

  /* Should not return here */
  TerminateTask();
}

void Os_PortInit(void) {
  ISR2Counter = 0;
}

void Os_PortInitContext(TaskVarType *pTaskVar) {
  pTaskVar->context.sp = pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize - 4;
  pTaskVar->context.pc = Os_PortActivate;
}

void EnterISR(void) {
  /* do nothing */
}

void LeaveISR(void) {
  /* do nothing */
}
#ifdef USE_PTHREAD_SIGNAL
void Os_PortCallSignal(int sig, void (*handler)(int), void *sp, void (*pc)(void)) {
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
  uint32_t *stk;

  sp = pTaskVar->context.sp;

  if ((sp - pTaskVar->pConst->pStack) < (pTaskVar->pConst->stackSize * 3 / 4)) {
    /* stack 75% usage, ignore this signal call */
    ASLOG(OS, ("install signal %d failed\n", sig));
    return -1;
  }

  stk = sp;

  *(--stk) = (uint32_t)Os_PortCallSignal;     /* entry point */
  *(--stk) = (uint32_t)Os_PortExitSignalCall; /* lr */
  *(--stk) = 0xdeadbeef;                      /* r12 */
  *(--stk) = 0xdeadbeef;                      /* r11 */
  *(--stk) = 0xdeadbeef;                      /* r10 */
  *(--stk) = 0xdeadbeef;                      /* r9 */
  *(--stk) = 0xdeadbeef;                      /* r8 */
  *(--stk) = 0xdeadbeef;                      /* r7 */
  *(--stk) = 0xdeadbeef;                      /* r6 */
  *(--stk) = 0xdeadbeef;                      /* r5 */
  *(--stk) = 0xdeadbeef;                      /* r4 */
  *(--stk) = (uint32_t)pTaskVar->context.pc;  /* r3 */
  *(--stk) = (uint32_t)sp;                    /* r2 */
  *(--stk) = (uint32_t)handler;               /* r1 */
  *(--stk) = (uint32_t)sig;                   /* r0 : argument */
  /* cpsr SYSMODE(0x1F) */
  if ((uint32_t)Os_PortCallSignal & 0x01)
    *(--stk) = 0x1F | 0x20; /* thumb mode */
  else
    *(--stk) = 0x1F; /* arm mode   */

  pTaskVar->context.sp = stk;
  pTaskVar->context.pc = Os_PortResume;

  return 0;
}
#endif