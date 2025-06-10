/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2017  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 0

#define CMD_DISPATCH 0
#define CMD_START_DISPATCH 1
#define CMD_EXIT_SIGCALL 2
/* ================================ [ TYPES     ] ============================================== */

/* ================================ [ DECLARES  ] ============================================== */
extern void init_prot(void);
extern void serial_init(void);
extern void init_descriptor(mmu_descriptor_t *p_desc, uint32_t base, uint32_t limit,
                            uint16_t attribute);
extern uint32_t seg2phys(uint16_t seg);
extern void init_clock(void);
extern void restart(void);
extern void dispatch(int cmd, void *param);
static void sys_dispatch(int cmd, void *param);

/* ================================ [ DATAS     ] ============================================== */
extern mmu_descriptor_t g_Gdt[GDT_SIZE];
int k_reenter = 0;

void *sys_call_table[] = {
  sys_dispatch,
};

static boolean knl_started = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
static void sys_dispatch(int cmd, void *param) {
  EnterCritical();

  asAssert(RunningVar);
  asAssert(ReadyVar);

  if (CMD_EXIT_SIGCALL == cmd) {
    /* restore signal call */
    memcpy(&RunningVar->context.regs, param, sizeof(cpu_context_t));
  } else {
    if (CMD_START_DISPATCH == cmd) {
      /* reinitialize the context as the context modified
       * by "save" which is the first action of sys_call */
      Os_PortInitContext(RunningVar);
    }
  }

  RunningVar = ReadyVar;
#ifdef MULTIPLY_TASK_ACTIVATION
  asAssert(RunningVar->activation > 0);
#endif
  asAssert(0 == k_reenter);

  ExitCritical();
}

/* ================================ [ FUNCTIONS ] ============================================== */
void Os_PortActivate(void) {
  /* get internal resource or NON schedule */
  RunningVar->priority = RunningVar->pConst->runPriority;

  ASLOG(OS, ("%s(%d) is running\n", RunningVar->pConst->name, RunningVar->pConst->initPriority));

  OSPreTaskHook();

  CallLevel = TCL_TASK;
  EnableInterrupt();

  RunningVar->pConst->entry();

  /* Should not return here */
  TerminateTask();
}

void Os_PortInit(void) {
  int i;
  uint16_t selector_ldt = INDEX_LDT_FIRST << 3;
  for (i = 0; i < TASK_NUM + OS_PTHREAD_NUM; i++) {
    asAssert((selector_ldt >> 3) < GDT_SIZE);
    init_descriptor(&g_Gdt[selector_ldt >> 3],
                    vir2phys(seg2phys(SELECTOR_KERNEL_DS), TaskVarArray[i].context.ldts),
                    LDT_SIZE * sizeof(mmu_descriptor_t) - 1, DA_LDT);
    selector_ldt += 1 << 3;
  }
}

void Os_PortInitContext(TaskVarType *pTaskVar) {
  uint16_t selector_ldt = SELECTOR_LDT_FIRST + (pTaskVar - TaskVarArray) * (1 << 3);
  uint8_t privilege;
  uint8_t rpl;
  int eflags;
  privilege = PRIVILEGE_TASK;
  rpl = RPL_TASK;
  eflags = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */

  ASLOG(OS,
        ("InitContext %s(%d)\n", pTaskVar->pConst->name ?: "null", pTaskVar->pConst->initPriority));

  pTaskVar->context.ldt_sel = selector_ldt;
  memcpy(&pTaskVar->context.ldts[0], &g_Gdt[SELECTOR_KERNEL_CS >> 3], sizeof(mmu_descriptor_t));
  pTaskVar->context.ldts[0].attr1 = DA_C | privilege << 5; /* change the DPL */
  memcpy(&pTaskVar->context.ldts[1], &g_Gdt[SELECTOR_KERNEL_DS >> 3], sizeof(mmu_descriptor_t));
  pTaskVar->context.ldts[1].attr1 = DA_DRW | privilege << 5; /* change the DPL */
  pTaskVar->context.regs.cs = ((8 * 0) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
  pTaskVar->context.regs.ds = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
  pTaskVar->context.regs.es = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
  pTaskVar->context.regs.fs = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
  pTaskVar->context.regs.ss = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
  pTaskVar->context.regs.gs = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;
  pTaskVar->context.regs.eip = (uint32_t)Os_PortActivate;
  pTaskVar->context.regs.esp =
    (uint32_t)(pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize - 4);
  pTaskVar->context.regs.eflags = eflags;
}

void Os_PortSysTick(void) {
  EnterISR();

  OsTick();
#if COUNTER_NUM > 0
  SignalCounter(0);
#endif
  LeaveISR();
}

void Os_PortStartDispatch(void) {
  if (FALSE == knl_started) {
    knl_started = TRUE;
    RunningVar = ReadyVar;
    init_clock();
    restart();
  }

  dispatch(CMD_START_DISPATCH, NULL);
  /* should never return */
  asAssert(0);
}

void Os_PortDispatch(void) {
  dispatch(CMD_DISPATCH, NULL);
}

int ffs(int v) {
  int i;
  int r = 0;

  for (i = 0; i < 32; i++) {
    if (v & (1 << i)) {
      r = i + 1;
      break;
    }
  }

  return r;
}

#ifdef USE_PTHREAD_SIGNAL
void Os_PortCallSignal(int sig, void (*handler)(int), void *sp) {
  asAssert(NULL != handler);

  handler(sig);

  Sched_GetReady();
  /* restore its previous stack */
  dispatch(CMD_EXIT_SIGCALL, sp);
}

void Os_PortExitSignalCall(void) {
  asAssert(0);
}

int Os_PortInstallSignal(TaskVarType *pTaskVar, int sig, void *handler) {
  uint32_t *stk;
  cpu_context_t *regs;

  stk = (void *)pTaskVar->context.regs.esp;

  if ((((void *)stk) - pTaskVar->pConst->pStack) < (pTaskVar->pConst->stackSize * 3 / 4)) {
    /* stack 75% usage, ignore this signal call */
    ASLOG(OS, ("install signal %d failed\n", sig));
    return -1;
  }

  /* saving previous task context to stack */
  stk = ((void *)stk) - sizeof(cpu_context_t);
  memcpy(stk, &pTaskVar->context.regs, sizeof(cpu_context_t));

  *(stk - 1) = (uint32_t)stk;
  --stk;
  *(--stk) = (uint32_t)handler;
  *(--stk) = (uint32_t)sig;
  *(--stk) = (uint32_t)Os_PortExitSignalCall;

  pTaskVar->context.regs.eip = (uint32_t)Os_PortCallSignal;
  pTaskVar->context.regs.esp = (uint32_t)stk;

  return 0;
}

#endif /* USE_PTHREAD_SIGNAL */
