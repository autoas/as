/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* NOTES: please make sure codes of this file all in the same PPAGE */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
#include "derivative.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 0

#define disable_interrupt() asm sei
#define enable_interrupt() asm cli
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void StartOsTick(void);
/* ================================ [ DATAS     ] ============================================== */
static uint16 ISR2Counter;
static unsigned int ISR2SavedCallLevel;
static uint8_t *ISR2SavedSP;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Os_PortInit(void) {
  ISR2Counter = 0;
  StartOsTick();
}

void Os_PortActivate(void) {
  /* get internal resource or NON schedule */
  RunningVar->priority = RunningVar->pConst->runPriority;

  ASLOG(OS, ("%s(%d) is running\n", RunningVar->pConst->name,
             (uint32)RunningVar->pConst->initPriority));

  CallLevel = TCL_TASK;
  enable_interrupt();

  RunningVar->pConst->entry();

  /* Should not return here */
  TerminateTask();
}

#pragma CODE_SEG __NEAR_SEG NON_BANKED
void Os_PortResume(void) {
  asm {
  pula
  staa $15 /* PPAGE */
  pula
  staa $16 /* RPAGE */
  pula
  staa $17 /* EPAGE */
  pula
  staa $10 /* GPAGE */
  rti
  }
}
#pragma CODE_SEG DEFAULT
void Os_PortInitContext(TaskVarType *pTaskVar) {
  pTaskVar->context.sp =
    (void *)((uint32_t)pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize - 4);
  pTaskVar->context.pc = Os_PortActivate;
}

void Os_PortIdle(void) {
  RunningVar = NULL;
  CallLevel = TCL_ISR2;
  INIT_SP_FROM_STARTUP_DESC();
  do {
    enable_interrupt();
    /* wait for a while */
    asm nop;
    asm nop;
    asm nop;
    asm nop;
    disable_interrupt();
  } while (NULL == ReadyVar);

  Os_PortStartDispatch();
}
#ifdef OS_USE_PRETASK_HOOK
void Os_PortCallPreTaskHook(void) {
  unsigned int salvedLevel = CallLevel;
  CallLevel = TCL_PREPOST;
  PreTaskHook();
  CallLevel = salvedLevel;
}
#else
#define Os_PortCallPreTaskHook()
#endif

#ifdef OS_USE_POSTTASK_HOOK
void Os_PortCallPostTaskHook(void) {
  unsigned int salvedLevel = CallLevel;
  CallLevel = TCL_PREPOST;
  PostTaskHook();
  CallLevel = salvedLevel;
}
#else
#define Os_PortCallPostTaskHook()
#endif

void Os_PortStartDispatch(void) {
  disable_interrupt();
  if (NULL == ReadyVar) {
    Os_PortIdle();
  }

  RunningVar = ReadyVar;

  Os_PortCallPreTaskHook();

  asm {
  ldx RunningVar
  lds 0, x
  jmp [0x2, x]
  }
}

void Os_PortDispatch(void) {
  asm swi
}

void EnterISR(void) {
  ISR2Counter++;
  if ((1 == ISR2Counter) && (RunningVar != NULL)) {
    ISR2SavedCallLevel = CallLevel;
    CallLevel = TCL_ISR2;
    asm sts ISR2SavedSP

    INIT_SP_FROM_STARTUP_DESC();

    Os_PortCallPostTaskHook();

    asm ldx ISR2SavedSP asm ldd 1, x asm pshd asm ldaa 0, x asm psha asm staa - 1,
      x /* save PPAGE */

        ISR2SavedSP = ISR2SavedSP + 3;
    *(--ISR2SavedSP) = GPAGE;
    *(--ISR2SavedSP) = EPAGE;
    *(--ISR2SavedSP) = RPAGE;
    --ISR2SavedSP; /* PPAGE already saved */
    RunningVar->context.pc = (FP)(((uint32)Os_PortResume) << 8);
    RunningVar->context.sp = ISR2SavedSP;
  }

  enable_interrupt();
}

void LeaveISR(void) {
  disable_interrupt();

  ISR2Counter--;
  if ((0 == ISR2Counter) && (RunningVar != NULL)) {
    CallLevel = ISR2SavedCallLevel;

    if (TCL_TASK == CallLevel) {
      if ((ReadyVar != NULL) && (ReadyVar->priority > RunningVar->priority)) {
        Sched_Preempt();
        Os_PortStartDispatch();
      } else {
        Os_PortCallPreTaskHook();
        asm {
        ldx RunningVar
        lds 0, x
        jmp [0x2, x]
        }
      }
    }
  }
}
#pragma CODE_SEG __NEAR_SEG NON_BANKED
/* Dispatch Implementation
 * For an interrupt or exception, the stack looks like below
 *   SP+7: PC
 *   SP+5: Y
 *   SP+3: X
 *   SP+1: D
 *   SP  : CCR
 */
interrupt void Isr_SoftwareInterrupt(void) {
  asm {
  ldaa $10 /* GPAGE */
  psha
  ldaa $17 /* EPAGE */
  psha
  ldaa $16 /* RPAGE */
  psha
  ldaa $15 /* PPAGE */
  psha
  ldx RunningVar
  sts 0, x
  }

  RunningVar->context.pc = (FP)(((uint32)Os_PortResume) << 8);

  Os_PortCallPostTaskHook();

  Os_PortStartDispatch();
}
#pragma CODE_SEG DEFAULT
