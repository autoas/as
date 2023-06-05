/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 - 2023 Parai Wang <parai@foxmail.com>
 */

/* https://static.docs.arm.com/100933/0100/aarch64_exception_and_interrupt_handling_100933_0100_en.pdf
 * https://www.arm.com/files/downloads/ARMv8_Architecture.pdf
 * http://infocenter.arm.com/help/topic/com.arm.doc.uan0015a/cortex_a57_software_optimisation_guide_external.pdf
 * https://developer.arm.com/docs/ddi0596/a/a64-base-instructions-alphabetic-order
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
 *  X30 -> LR, X29 -> BSP
 * https://git.kernel.org/pub/scm/virt/kvm/kvm-unit-tests.git/tree/arm
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#define MACROS_ONLY
#include "Os_Cfg.h"
/* ============================ [ MACROS    ] ====================================================== */
#if CPU_CORE_NUMBER > 1
#define RunningVar RunningVars
#define ReadyVar   ReadyVars
#define CallLevel  CallLevels
#endif

/* for i in range(16): print("stp x%d, x%d, [sp, #-16]!"%(2*i, 2*i+1)) */
.macro SaveContext
  stp x0, x1, [sp, #-16]!
  stp x2, x3, [sp, #-16]!
  stp x4, x5, [sp, #-16]!
  stp x6, x7, [sp, #-16]!
  stp x8, x9, [sp, #-16]!
  stp x10, x11, [sp, #-16]!
  stp x12, x13, [sp, #-16]!
  stp x14, x15, [sp, #-16]!
  stp x16, x17, [sp, #-16]!
  stp x18, x19, [sp, #-16]!
  stp x20, x21, [sp, #-16]!
  stp x22, x23, [sp, #-16]!
  stp x24, x25, [sp, #-16]!
  stp x26, x27, [sp, #-16]!
  stp x28, x29, [sp, #-16]!
  str x30, [sp, #-8]!

  mrs x0, spsr_el1
  mrs x1, elr_el1
  stp x0, x1, [sp, #-16]!
.endm

/* for i in range(16): print("ldp x%d, x%d, [sp], #16"%(31-(2*i+1), 31-2*i)) */
.macro RestoreContext
  ldp x0, x1, [sp], #16
  msr spsr_el1, x0
  msr elr_el1, x1

  ldr x30, [sp], #8
  ldp x28, x29, [sp], #16
  ldp x26, x27, [sp], #16
  ldp x24, x25, [sp], #16
  ldp x22, x23, [sp], #16
  ldp x20, x21, [sp], #16
  ldp x18, x19, [sp], #16
  ldp x16, x17, [sp], #16
  ldp x14, x15, [sp], #16
  ldp x12, x13, [sp], #16
  ldp x10, x11, [sp], #16
  ldp x8, x9, [sp], #16
  ldp x6, x7, [sp], #16
  ldp x4, x5, [sp], #16
  ldp x2, x3, [sp], #16
  ldp x0, x1, [sp], #16
.endm


/*
 * Vectors
 * Adapted from arch/arm64/kernel/entry.S
 */
.macro vector_stub, name, vec
\name:
  SaveContext

  mov  x0, \vec
  mov  x1, sp
  mrs  x2, esr_el1
  bl  Os_PortException

  RestoreContext
  eret
.endm
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
  .extern Os_PortIsrHandler
  .extern Os_PortSyncException
  .extern Sched_Preempt
  .extern Sched_GetReady
  .extern PreTaskHook
  .extern PostTaskHook
  .extern Os_PortActivateImpl
  .extern Os_RestoreKernelLock
/* ============================ [ DATAS     ] ====================================================== */
#if CPU_CORE_NUMBER > 1
  .extern RunningVars
  .extern ReadyVars
  .extern CallLevels
#else
  .extern RunningVar
  .extern ReadyVar
#endif
  .extern ISR2Counter
  .extern stack_top
  .extern Os_PortIdle
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
  .section .text

  .global Os_PortStartDispatchImpl
  .type   Os_PortStartDispatchImpl, %function
/* void Os_PortStartDispatchImpl( void ); */
Os_PortStartDispatchImpl:
  ldr     x0, =ReadyVar
  #if CPU_CORE_NUMBER > 1
  mrs     x6, mpidr_el1
  and     x6, x6, #3
  ldr     x0, [x0, x6, lsl #3]
  #else
  ldr     x0, [x0]
  #endif
  cmp     x0, #0
  beq     Os_PortIdle
  ldr     x1, =RunningVar
  #if CPU_CORE_NUMBER > 1
  str     x0, [x1, x6, lsl #3]
  #else
  str     x0, [x1]
  #endif
Os_PortDispatchFormISR:
  #if CPU_CORE_NUMBER > 1
  stp     x0,x1, [sp, #-16]!
  stp     x29,x30, [sp, #-16]!
  bl      Os_RestoreKernelLock
  ldp     x29,x30, [sp], #16
  ldp     x0,x1, [sp], #16
  #endif

  #ifdef OS_USE_PRETASK_HOOK
  ldr     x1, = CallLevel
  #if CPU_CORE_NUMBER > 1
  mrs     x6, mpidr_el1
  and     x6, x6, #3
  ldr     w3, [x1, x6, lsl #2]
  #else
  ldr     w3, [x1]
  #endif
  mov     w2, #8   /* CallLevel = TCL_PREPOST */
  #if CPU_CORE_NUMBER > 1
  str     w2, [x1, x6, lsl #2]
  #else
  str     w2,[x1]
  #endif

  stp     x0,x1, [sp, #-16]!
  stp     x2,x3, [sp, #-16]!
  stp     x29,x30, [sp, #-16]!
  bl      PreTaskHook
  ldp     x29,x30, [sp], #16
  ldp     x2,x3, [sp], #16
  ldp     x0,x1, [sp], #16
  #if CPU_CORE_NUMBER > 1
  str     w3,[x1, x6, lsl #2]
  #else
  str     w3,[x1]  /* restore CallLevel */
  #endif
  #endif

  ldr     x1, [x0, #0x0 ]
  mov     sp, x1
  ldr     x1, [x0,#0x08]
  br      x1

  .global Os_PortResume
  .type   Os_PortResume, %function
Os_PortResume:
  RestoreContext
  eret

  .global Os_PortActivate
  .type   Os_PortActivate, %function
Os_PortActivate:
  ldr x1, =Os_PortActivateImpl
  msr elr_el1, x1
  eret

  .global Os_PortDispatchImpl
  .type   Os_PortDispatchImpl, %function
Os_PortDispatchImpl:
  SaveContext

  ldr     x3, =RunningVar
  #if CPU_CORE_NUMBER > 1
  mrs     x6, mpidr_el1
  and     x6, x6, #3
  ldr     x4, [x3, x6, lsl #3]
  #else
  ldr     x4, [x3]
  #endif

  mov     x5, sp
  str     x5, [x4, #0x0 ]

  ldr     x12, =Os_PortResume
  str     x12, [x4, #0x08]

  /* loading system stack */
  ldr x0, =stack_top
  #if CPU_CORE_NUMBER > 1
  lsl x3, x6, #14    /*stack size is 16K(1<<14) */
  add x0, x0, x3
  #endif
  mov sp, x0

  #ifdef OS_USE_POSTTASK_HOOK
  ldr     x1, = CallLevel
  #if CPU_CORE_NUMBER > 1
  ldr     w3, [x1, x6, lsl #2]
  #else
  ldr     w3, [x1]
  #endif
  mov     w2, #8   /* CallLevel = TCL_PREPOST */
  #if CPU_CORE_NUMBER > 1
  str     w2, [x1, x6, lsl #2]
  #else
  str     w2,[x1]
  #endif
  stp     x0,x1, [sp, #-16]!
  stp     x2,x3, [sp, #-16]!
  stp     x29,x30, [sp, #-16]!
  bl      PostTaskHook
  ldp     x29,x30, [sp], #16
  ldp     x2,x3, [sp], #16
  ldp     x0,x1, [sp], #16
  #if CPU_CORE_NUMBER > 1
  str     w3,[x1, x6, lsl #2]
  #else
  str     w3,[x1]  /* restore CallLevel */
  #endif
  #endif

  b       Os_PortStartDispatchImpl

EnterISR:
  ldr     x3, =RunningVar
  #if CPU_CORE_NUMBER > 1
  mrs     x6, mpidr_el1
  and     x6, x6, #3
  ldr     x4, [x3, x6, lsl #3]
  #else
  ldr     x4, [x3]
  #endif
  cmp     x4, #0
  beq     l_nosave    /* no task is running */

  ldr     x1, =ISR2Counter
  #if CPU_CORE_NUMBER > 1
  ldr     w2, [x1, x6, lsl #2]
  #else
  ldr     w2, [x1]
  #endif
  add     w2, w2, #1  /* ISR2Counter++ */
  #if CPU_CORE_NUMBER > 1
  str     w2, [x1, x6, lsl #2]
  #else
  str     w2, [x1]
  #endif
  cmp     w2, #1      /* previous CirticalCounter==0 */
  bne     l_nosave

  mov     x5, sp
  str     x5, [x4, #0x0 ]

  ldr     x12, =Os_PortResume
  str     x12, [x4, #0x08]

  /* loading system stack */
  ldr x0, =stack_top
  #if CPU_CORE_NUMBER > 1
  lsl x3, x6, #14    /*stack size is 16K(1<<14) */
  add x0, x0, x3
  #endif
  mov sp, x0

  #ifdef OS_USE_POSTTASK_HOOK
  ldr     x1, = CallLevel
  #if CPU_CORE_NUMBER > 1
  ldr     w3, [x1, x6, lsl #2]
  #else
  ldr     w3, [x1]
  #endif
  mov     w2, #8   /* CallLevel = TCL_PREPOST */
  #if CPU_CORE_NUMBER > 1
  str     w2, [x1, x6, lsl #2]
  #else
  str     w2,[x1]
  #endif
  stp     x0,x1, [sp, #-16]!
  stp     x2,x3, [sp, #-16]!
  stp     x29,x30, [sp, #-16]!
  bl      PostTaskHook
  ldp     x29,x30, [sp], #16
  ldp     x2,x3, [sp], #16
  ldp     x0,x1, [sp], #16
  #if CPU_CORE_NUMBER > 1
  str     w3,[x1, x6, lsl #2]
  #else
  str     w3,[x1]  /* restore CallLevel */
  #endif
  #endif

l_nosave:
  ldr     x1, = CallLevel
  #if CPU_CORE_NUMBER > 1
  ldr     w0, [x1, x6, lsl #2]
  #else
  ldr     w0, [x1]
  #endif
  stp     x0,x1, [sp, #-16]!  /* previous CallLevel */
  mov     w0, #2   /* CallLevel = TCL_ISR2 */
  #if CPU_CORE_NUMBER > 1
  str     w0, [x1, x6, lsl #2]
  #else
  str     w0,[x1]
  #endif
  ret

ExitISR:
  #if CPU_CORE_NUMBER > 1
  mrs     x6, mpidr_el1
  and     x6, x6, #3
  #endif

  ldp     x0,x1, [sp], #16
  #if CPU_CORE_NUMBER > 1
  str     w0, [x1, x6, lsl #2]
  #else
  str     w0,[x1] /* restore CallLevel */
  #endif

  ldr     x0, = RunningVar
  #if CPU_CORE_NUMBER > 1
  ldr     x0, [x0, x6, lsl #3]
  #else
  ldr     x0, [x0]
  #endif
  cmp     x0, #0
  beq     l_nodispatch

  ldr     x3, =ISR2Counter
  #if CPU_CORE_NUMBER > 1
  ldr     w1, [x3, x6, lsl #2]
  #else
  ldr     w1, [x3]
  #endif

  sub     w1, w1, #1
  str     w1, [x3]
  cmp     w1, #0
  bne     l_nodispatch

  ldr     x1, = CallLevel
  #if CPU_CORE_NUMBER > 1
  ldr     w3, [x1, x6, lsl #2]
  #else
  ldr     w3, [x1]
  #endif
  cmp     w3, #1  /* TCL_TASK */
  bne     l_nopreempt

#if CPU_CORE_NUMBER > 1
  bl      Sched_Schedule
  cmp     x0, #0
  beq     mc_nopreempt

  bl      Os_PortSpinLock

  b       Os_PortStartDispatchImpl

mc_nopreempt:
  bl      Os_PortSpinLock
  ldr     x0, = RunningVar
  ldr     x0, [x0, x6, lsl #3]
#else
  ldr     x1, = ReadyVar
  #if CPU_CORE_NUMBER > 1
  ldr     x1, [x1, x6, lsl #3]
  #else
  ldr     x1, [x1]
  #endif

  cmp     x0, x1  /* ReadyVar == RunningVar */
  beq     l_nopreempt

  ldrb    w2, [x1, #16]  /* priority of ReadyVar */
  ldrb    w3, [x0, #16]  /* priority of RunningVar */
  cmp     w3, w2
  bge     l_nopreempt

  bl      Sched_Preempt

  b       Os_PortStartDispatchImpl
#endif

l_nopreempt:
  b       Os_PortDispatchFormISR

l_nodispatch:
  RestoreContext
  eret


l_sync:
  stp     x0,x1, [sp, #-16]!
  mrs     x0, esr_el1
  mov     x1, x0, lsr #26
  and     x0, x1, #0x3f
  cmp     x0, #0x15  /* SVC is 0x15 */
  bne      l_sync_exception
  ldr     x0, = RunningVar
  #if CPU_CORE_NUMBER > 1
  mrs     x1, mpidr_el1
  and     x1, x1, #3
  ldr     x0, [x0, x1, lsl #3]
  #else
  ldr     x0, [x0]
  #endif
  cmp     x0, #0
  beq     l_svc_start_disatch
  ldp     x0,x1, [sp], #16
  b       Os_PortDispatchImpl
l_svc_start_disatch:
  ldp     x0,x1, [sp], #16
  b     Os_PortStartDispatchImpl
l_sync_exception:
  ldp     x0,x1, [sp], #16
  SaveContext
  mov x0, sp
  mrs x1, esr_el1
  bl Os_PortSyncException




vector_stub el1t_sync,     0
vector_stub el1t_irq,      1
vector_stub el1t_fiq,      2
vector_stub el1t_error,    3

vector_stub el1h_sync,     4
vector_stub el1h_irq,      5
vector_stub el1h_fiq,      6
vector_stub el1h_error,    7

vector_stub el0_sync_64,   8
vector_stub el0_irq_64,    9
vector_stub el0_fiq_64,   10
vector_stub el0_error_64, 11

vector_stub el0_sync_32,  12
vector_stub el0_irq_32,   13
vector_stub el0_fiq_32,   14
vector_stub el0_error_32, 15


.balign 0x800
.global vector_table_el1
vector_table_el1:
curr_el_sp0_sync:
  b l_sync

.balign 0x80
curr_el_sp0_irq:
  b el1t_irq

.balign 0x80
curr_el_sp0_fiq:
  b el1t_fiq

.balign 0x80
curr_el_sp0_serror:
  b el1t_error

.balign 0x80
curr_el_spx_sync:
  b l_sync

.balign 0x80
curr_el_spx_irq:
  SaveContext
  bl EnterISR
  bl Os_PortIsrHandler
  b  ExitISR

.balign 0x80
curr_el_spx_fiq:
  b el1h_fiq

.balign 0x80
curr_el_spx_serror:
  b el1h_error

.balign 0x80
lower_el_aarch64_sync:
  b el0_sync_64

.balign 0x80
lower_el_aarch64_irq:
  b el0_irq_64

.balign 0x80
lower_el_aarch64_fiq:
  b el0_fiq_64

.balign 0x80
lower_el_aarch64_serror:
  b el0_error_64

.balign 0x80
lower_el_aarch32_sync:
  b el0_sync_32

.balign 0x80
lower_el_aarch32_irq:
  b el0_irq_32

.balign 0x80
lower_el_aarch32_fiq:
  b el0_fiq_32

.balign 0x80
lower_el_aarch32_serror:
  b el0_error_32
