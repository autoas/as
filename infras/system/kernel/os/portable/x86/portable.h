/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2018  AS <parai@foxmail.com>
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
#ifndef PORTABLE_H_
#define PORTABLE_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "mmu.h"
#include "x86.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
/* ================================ [ MACROS    ] ============================================== */
/* 每个任务有一个单独的 LDT, 每个 LDT 中的描述符个数: */
#define LDT_SIZE 2
/* GDT and IDT size */
#define GDT_SIZE (INDEX_LDT_FIRST + TASK_NUM + OS_PTHREAD_NUM)
#define IDT_SIZE 256

#define EnterISR()                                                                                 \
  unsigned int savedLevel;                                                                         \
  EnterCritical();                                                                                 \
  savedLevel = CallLevel;                                                                          \
  CallLevel = TCL_ISR2;                                                                            \
  ExitCritical()

#define LeaveISR()                                                                                 \
  EnterCritical();                                                                                 \
  CallLevel = savedLevel;                                                                          \
  asAssert(RunningVar);                                                                            \
  asAssert(ReadyVar);                                                                              \
  if ((RunningVar != ReadyVar) && (RunningVar->priority < ReadyVar->priority)) {                   \
    Sched_Preempt();                                                                               \
    RunningVar = ReadyVar;                                                                         \
  }                                                                                                \
  ExitCritical()

/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  cpu_context_t regs; /* process' registers saved in stack frame */

  uint16_t ldt_sel;                /* selector in gdt giving ldt base and limit*/
  mmu_descriptor_t ldts[LDT_SIZE]; /* local descriptors for code and data */
                                   /* 2 is LDT_SIZE - avoid include protect.h */
} TaskContextType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* PORTABLE_H_ */
