/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
#if defined(CHIP_STM32F10X)
#include "stm32f10x.h"
#elif defined(CHIP_AT91SAM3S)
#include "SAM3S.h"
#include "board.h"
#elif defined(CHIP_LM3S6965)
#include "hw_ints.h"
#elif defined(CHIP_STM32P107)
#include "stm32p107.h"
#else
#error "CHIP is not known, please select the CHIP_STM32F10X or CHIP_AT91SAM3S..."
#endif
#include <core_cm3.h>
#include "Mcu.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void knl_activate(void);
extern uint32 Mcu_GetSystemClock(void);
/* ================================ [ DATAS     ] ============================================== */
uint32 ISR2Counter;
extern const uint32 __vector_table[];
#if (ISR_NUM > 0)
extern const FP tisr_pc[ISR_NUM];
#endif
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
  const uint32 *pSrc;
  pSrc = __vector_table;

#if defined(CHIP_AT91SAM3S)
  SCB->VTOR = ((uint32)pSrc & SCB_VTOR_TBLOFF_Msk);
  if (((uint32)pSrc >= IRAM_ADDR) && ((uint32)pSrc < IRAM_ADDR + IRAM_SIZE)) {
    SCB->VTOR |= 1 << SCB_VTOR_TBLBASE_Pos;
  }
#else
  SCB->VTOR = (uint32)pSrc;
#endif

#if 0 /* better not enable this */
  SCB->CCR |= 0x18; /* enable div-by-0 and unaligned fault */
  SCB->SHCSR |= 0x00007000; /* enable Usage Fault, Bus Fault, and MMU Fault */
#endif
  ISR2Counter = 0;
  knl_dispatch_started = FALSE;

  if (SysTick_Config(Mcu_GetSystemClock() / 1000)) { /* Capture error */
    while (1)
      ;
  }
}

void Os_PortInitContext(TaskVarType *pTaskVar) {
  pTaskVar->context.sp = pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize - 4;
  pTaskVar->context.pc = knl_activate;
}

void EnterISR(void) {
  /* do nothing */
}

void LeaveISR(void) {
  /* do nothing */
}

void Os_PortDispatch(void) {
  SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
  __asm("cpsie   i");
  __asm("nop");
  __asm("cpsid   i");
}

void Os_PortStartDispatch(void) {
  knl_dispatch_started = TRUE; /* always set it to true here, ugly code */
  RunningVar = NULL;
  Os_PortDispatch();
  asAssert(0);
}

void knl_isr_handler(int intno) {
#if (ISR_NUM > 0)
  if ((intno > 15) && (intno < (16 + ISR_NUM)) && (tisr_pc[intno - 16] != NULL)) {
    tisr_pc[intno - 16]();
  } else
#endif
  {
    ShutdownOS(0xFF);
  }
}

void knl_system_tick_handler(void) {
  if (knl_dispatch_started) {
    OsTick();
    SignalCounter(0);
  }
#if defined(CHIP_AT91SAM3S)
  TimeTick_Increment();
#endif
}

int Os_PortInstallSignal(TaskVarType *pTaskVar, int sig, void *handler) {
  /* TODO: not implemented */
  asAssert(0);

  return 0;
}

void dump_fault_stack(const char *prefix, uint32 *sp) {
  DisableInterrupt();
  printf("%s, SP = %p :\n", prefix, sp);
  printf("R0 = 0x%X\n", sp[0]);
  printf("R1 = 0x%X\n", sp[1]);
  printf("R2 = 0x%X\n", sp[2]);
  printf("R3 = 0x%X\n", sp[3]);
  printf("R12= 0x%X\n", sp[4]);
  printf("LR = 0x%X\n", sp[5]);
  printf("PC = 0x%X\n", sp[6]);
  printf("PSR= 0x%X\n", sp[7]);
  asAssert(0);
}

void dump_nmi_fault_stack(uint32 *sp) {
  dump_fault_stack("nmi fault", sp);
}
void __naked nmi_handler(void) {
  __asm__ volatile("mov r0, sp");
  __asm__ volatile("b  dump_nmi_fault_stack");
}

void dump_hard_fault_stack(uint32 *sp) {
  dump_fault_stack("hart fault", sp);
}
void __naked hard_fault_handler(void) {
  __asm__ volatile("mov r0, sp");
  __asm__ volatile("b  dump_hard_fault_stack");
}

void dump_mpu_fault_stack(uint32 *sp) {
  dump_fault_stack("mpu fault", sp);
}
void mpu_fault_handler(void) {
  __asm__ volatile("mov r0, sp");
  __asm__ volatile("b  dump_mpu_fault_stack");
}

void dump_bus_fault_stack(uint32 *sp) {
  dump_fault_stack("bus fault", sp);
}
void bus_fault_handler(void) {
  __asm__ volatile("mov r0, sp");
  __asm__ volatile("b  dump_bus_fault_stack");
}

void dump_usage_fault_stack(uint32 *sp) {
  dump_fault_stack("usage fault", sp);
}
void usage_fault_handler(void) {
  __asm__ volatile("mov r0, sp");
  __asm__ volatile("b  dump_usage_fault_stack");
}

void dump_debug_monitor_fault_stack(uint32 *sp) {
  dump_fault_stack("debug monitor fault", sp);
}
void debug_monitor_handler(void) {
  __asm__ volatile("mov r0, sp");
  __asm__ volatile("b  dump_debug_monitor_fault_stack");
}
