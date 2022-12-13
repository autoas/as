/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#define MACROS_ONLY
#include "Os_Cfg.h"
/* http://users.ece.utexas.edu/~valvano/EE345L/Labs/Fall2011/CortexM3_TRM_r2p1.pdf */
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
  .extern Irq_Enable
  .extern Irq_Disable
  .extern Os_PortIsrHandler
  .extern Sched_Preempt
  .extern Os_PortActivate
/* ============================ [ DATAS     ] ====================================================== */
  .extern RunningVar
  .extern ReadyVar
  .extern CallLevel
  .extern ISR2Counter
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
  .syntax unified
  .cpu cortex-m3
  .fpu softvfp
  .thumb
  .section .text

Os_PortIdle:
  /* set RunningVar to NULL */
  ldr     r1, =RunningVar
  str     r0, [r1]
  cpsie   i
l_idle:
  ldr     r0, =ReadyVar
  ldr     r0, [r0]
  cmp     r0, #0
  ble     l_idle
  cpsid   i
  bl      Sched_GetReady
  b       knl_start_dispatch

  .global knl_activate
  .type   knl_activate, %function
knl_activate:
  mov r3, #0x01000000 /* TS_PSR_T */
  ldr r2, =Os_PortActivate
  push {r2,r3}
  subs  sp,sp,#24
  bx lr

  .global  knl_resume
  .type   knl_resume, %function
knl_resume:
  /* start to restore task's context */
  pop     {r4-r11}
    cpsie   i
  bx      lr

  .global knl_start_dispatch
  .type   knl_start_dispatch, %function
knl_start_dispatch:
  cpsid   i
  ldr lr, =0xFFFFFFF9 /* force it to process mode and use MSP */
  ldr     r0, =ReadyVar
  ldr     r0, [r0]
  cmp     r0, #0
  beq     Os_PortIdle
  ldr     r1, =RunningVar
  str     r0, [r1]
knl_start_dispatch_from_isr:
  #ifdef OS_USE_PRETASK_HOOK
  ldr     r1, = CallLevel
  ldr     r3, [r1]
  mov     r2, #8   /* CallLevel = TCL_PREPOST */
  str     r2,[r1]
  push    {r0-r3,lr}
  bl      PreTaskHook
  pop     {r0-r3,lr}
  str     r3,[r1]  /* restore CallLevel */
  #endif
  ldr     sp, [r0, #0x0 ]
  ldr     r1,[r0,#0x04]
  bx      r1

  .global knl_dispatch_entry
  .type   knl_dispatch_entry, %function
knl_dispatch_entry:
  /* EXCEPTION stack:H->L: PSR PC LR R12 R3 R2 R1 R0 */
  ldr     r1, =RunningVar
  ldr     r2, [r1]
  cmp     r2, #0
  beq     knl_start_dispatch

  push    {r4-r11}

  str     sp, [r2, #0x0 ]

  ldr     r12, =knl_resume
  str     r12, [r2, #0x04]
  #ifdef OS_USE_POSTTASK_HOOK
  ldr     r1, = CallLevel
  ldr     r3, [r1]
  mov     r2, #8   /* CallLevel = TCL_PREPOST */
  str     r2,[r1]
  push    {r0-r3,lr}
  bl      PostTaskHook
  pop     {r0-r3,lr}
  str     r3,[r1]  /* restore CallLevel */
  #endif

  b       knl_start_dispatch

  .type   EnterISR, %function
EnterISR:
  cpsid   i
  ldr     r0, = RunningVar
  ldr     r0, [r0]
  cmp     r0, #0
  beq     l_nosave    /* no task is running */

  ldr     r1, =ISR2Counter
  ldr     r2, [r1]
  add     r2, r2, #1  /* ISR2Counter++ */
  str     r2, [r1]

  cmp     r2, #1      /* previous CirticalCounter==0 */
  bne     l_nosave

  /* save context on fisrt ISR enter */
  push    {r4-r11}

  str     sp, [r0, #0x0 ]

  ldr     r12, =knl_resume
  str     r12, [r0, #0x04]

    /* and then load isr system stack */
    ldr     sp, =knl_system_stack_top  /* Set system stack */

  #ifdef OS_USE_POSTTASK_HOOK
  ldr     r1, = CallLevel
  ldr     r4, [r1]
  mov     r2, #8   /* CallLevel = TCL_PREPOST */
  str     r2,[r1]
  push    {r0-r4,lr}
  bl      PostTaskHook
  pop     {r0-r4,lr}
  str     r4,[r1]  /* restore CallLevel */
  #endif
l_nosave:
  push    {r3} /* lr */
  ldr     r1, = CallLevel
  ldr     r3, [r1]
  push    {r3} /* previous CallLevel */
  mov     r3, #2   /* CallLevel = TCL_ISR2 */
  str     r3,[r1]
  cpsie   i
  bx      lr

  .type   EnterISR, %function
ExitISR:
  cpsid   i
  pop    {r3}
  ldr     r1, = CallLevel
  str     r3, [r1]

  pop     {lr}

  ldr     r0, = RunningVar
  ldr     r0, [r0]
  cmp     r0, #0
  beq     l_nodispatch

  ldr     r3, =ISR2Counter
  ldr     r1, [r3]
  sub     r1, r1, #1
  str     r1, [r3]
  cmp     r1, #0
  bne     l_nodispatch

  ldr     r1, = CallLevel
  ldr     r3, [r1]
  cmp     r3, #1  /* TCL_TASK */
  bne     l_nopreempt

  ldr     r1, = ReadyVar
  ldr     r1, [r1]

  cmp     r0, r1
  beq     l_nopreempt

  ldrb    r2, [r1, #8]  /* priority of ReadyVar */
  ldrb    r3, [r0, #8]  /* priority of RunningVar */
  cmp     r3, r2
  bge     l_nopreempt

  bl      Sched_Preempt

  b       knl_start_dispatch

l_nopreempt:
  b       knl_start_dispatch_from_isr

l_nodispatch:
  bx      lr

  .type   knl_system_tick_handler, %function
  .weak knl_system_tick_handler
knl_system_tick_handler:
  bx lr

  .global knl_system_tick
  .type   knl_system_tick, %function
knl_system_tick:
  mov r3,lr
  bl EnterISR
  bl knl_system_tick_handler
  b  ExitISR

  .global knl_isr_process
  .extern knl_isr_handler
  .type   knl_isr_process, %function
knl_isr_process:
  mov r3,lr
  bl EnterISR
  mrs     r0, ipsr                             /* r0 = dintno */
  bl knl_isr_handler
  b  ExitISR
