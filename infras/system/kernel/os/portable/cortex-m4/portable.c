/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#define STD_NO_ERRNO_H
#include "kernel_internal.h"
#include "Std_Debug.h"
#if defined(CHIP_AC7840)
#include "ac7840x.h"
#else
#error "CHIP is not known, please select the CHIP_STM32F10X or CHIP_AT91SAM3S..."
#endif
#include <core_cm4.h>
#include "Mcu.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void knl_activate(void);
extern uint32 Mcu_GetSystemClock(void);
/* ================================ [ DATAS     ] ============================================== */
uint32 ISR2Counter;
uint32 knl_dispatch_started;
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
  knl_dispatch_started = FALSE;

  if (SysTick_Config(Mcu_GetSystemClock() / 1000)) { /* Capture error */
    while (1)
      ;
  }

  NVIC_SetPriority(PendSV_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
  NVIC_SetPriority(SVCall_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
}

void Os_PortInitContext(TaskVarType *pTaskVar) {
  pTaskVar->context.sp = pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize - 32;
  pTaskVar->context.pc = knl_activate;
}

void Os_PortDispatch(void) {
#if 0
  __asm("cpsie   i");
  __asm("svc 0");
  __asm("cpsid   i");
#else
  SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
  __asm("cpsie   i");
  __asm("nop");
  __asm("cpsid   i");
#endif
}

void Os_PortStartDispatch(void) {
  knl_dispatch_started = TRUE; /* always set it to true here, ugly code */
  RunningVar = NULL;
  Os_PortDispatch();
  asAssert(0);
}

void knl_system_tick_handler(void) {
  if (knl_dispatch_started) {
    OsTick();
#if (COUNTER_NUM > 0)
    SignalCounter(0);
#endif
  }
}

int Os_PortInstallSignal(TaskVarType *pTaskVar, int sig, void *handler) {
  /* TODO: not implemented */
  asAssert(0);

  return 0;
}
