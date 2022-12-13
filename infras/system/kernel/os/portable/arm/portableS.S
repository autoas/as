/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#define MACROS_ONLY
#include "Os_Cfg.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
  .extern EnableInterrupt
  .extern DisableInterrupt
  .extern Os_PortIsrHandler
  .extern Sched_Preempt
/* ============================ [ DATAS     ] ====================================================== */
  .extern RunningVar
  .extern ReadyVar
  .extern CallLevel
  .extern ISR2Counter
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
  .section .text

Os_PortIdle:
  /* set RunningVar to NULL */
  ldr     r1, =RunningVar
  str     r0, [r1]
  bl      EnableInterrupt
l_idle:
  ldr     r0, =ReadyVar
  ldr     r0, [r0]
  cmp     r0, #0
  ble     l_idle
  bl      DisableInterrupt
  bl      Sched_GetReady
  b       Os_PortStartDispatch

  .global Os_PortStartDispatch
  .type   Os_PortStartDispatch, %function
/* void Os_PortStartDispatch( void ); */
Os_PortStartDispatch:
  ldr     r0, =ReadyVar
  ldr     r0, [r0]
  cmp     r0, #0
  beq     Os_PortIdle
  ldr     r1, =RunningVar
  str     r0, [r1]
Os_PortDispatchFormISR:
  #ifdef OS_USE_PRETASK_HOOK
  ldr     r1, = CallLevel
  ldr     r3, [r1]
  mov     r2, #8   /* CallLevel = TCL_PREPOST */
  str     r2,[r1]
  stmfd   sp!, {r0-r3}
  bl      PreTaskHook
  ldmfd   sp!, {r0-r3}
  str     r3,[r1]  /* restore CallLevel */
  #endif
  ldr     sp, [r0, #0x0 ]
  ldr     r1,[r0,#0x04]
  mov     pc,r1

  .global Os_PortResume
  .type   Os_PortResume, %function
Os_PortResume:
  ldmfd   sp!, {r0}
  msr     spsr, r0
  ldmfd   sp!, {r0-r12, lr, pc}^

  .global Os_PortDispatch
  .type   Os_PortDispatch, %function
Os_PortDispatch:
  stmfd   sp, {r0-r2}
  sub     r0, sp, #12
  sub     r1, lr, #0              /* Adjust PC for return address to task */
  mrs     r2, cpsr
  stmfd   sp!, {r1}               /* Push task''s PC */
  stmfd   sp!, {r3-r12, lr}       /* Push task''s LR,R12-R4 */
  ldmfd   r0!, {r3-r5}            /* Load Task''s R0-R2 from stack */
  stmfd   sp!, {r3-r5}            /* Push Task''s R0-R2 to SVC stack */
  stmfd   sp!, {r2}               /* Push task''s SPSR */

  ldr     r3, =RunningVar
  ldr     r4, [r3]

  str     sp, [r4, #0x0 ]

  ldr     r12, =Os_PortResume
  str     r12, [r4, #0x04]

  #ifdef OS_USE_POSTTASK_HOOK
  ldr     r1, = CallLevel
  ldr     r3, [r1]
  mov     r2, #8   /* CallLevel = TCL_PREPOST */
  str     r2,[r1]
  stmfd   sp!, {r0-r3}
  bl      PostTaskHook
  ldmfd   sp!, {r0-r3}
  str     r3,[r1]  /* restore CallLevel */
  #endif
  b       Os_PortStartDispatch                 /* To dispatch processing */

  .global vector_irq
  .type   vector_irq, %function
vector_irq:
  /* Save working register to IRQ stack */
  stmfd   sp, {r0-r2}
EnterISR:
  ldr     r0, = RunningVar
  ldr     r0, [r0]
  cmp     r0, #0
  beq     l_nosave    /* no task is running */

  ldr     r0, =ISR2Counter
  ldr     r2, [r0]
  add     r2, r2, #1  /* ISR2Counter++ */
  str     r2, [r0]

  cmp     r2, #1      /* previous ISR2Counter==0 */
  bne     l_nosave

  /* save context on fisrt ISR enter                                          */
  /* now R0 R1 R2 have all saved to IRQ's stack before,but no adjust to SP    */
  /*------------------------------------------------------------------------  */
  /*   R0--SP                                                                 */
  /*   R1--PC                                                                 */
  /*   R2--SPSR                                                               */
  /*------------------------------------------------------------------------  */

  sub     r0, sp, #12
  sub     r1, lr, #4              /* Adjust PC for return address to task */
  mrs     r2, spsr                /* Copy SPSR (Task CPSR) */
  ;@ SYSMODE
  msr     cpsr_c, #0xDF           /* Change to SYS mode,that is task's mode */
                  /* SAVE TASK''S CONTEXT ONTO OLD TASK''S STACK */
  stmfd   sp!, {r1}               /* Push task''s PC */
  stmfd   sp!, {r3-r12, lr}       /* Push task''s LR,R12-R4 */
  ldmfd   r0!, {r3-r5}            /* Load Task''s R0-R2 from IRQ stack */
  stmfd   sp!, {r3-r5}            /* Push Task''s R0-R2 to SVC stack */
  stmfd   sp!, {r2}               /* Push task''s SPSR */

  ldr     r3, =RunningVar
  ldr     r4, [r3]

  str     sp, [r4, #0x0 ]

  ldr     r12, =Os_PortResume
  str     r12, [r4, #0x04]

  #ifdef OS_USE_POSTTASK_HOOK
  ldr     r1, = CallLevel
  ldr     r3, [r1]
  mov     r2, #8   /* CallLevel = TCL_PREPOST */
  str     r2,[r1]
  stmfd   sp!, {r0-r3}
  bl      PostTaskHook
  ldmfd   sp!, {r0-r3}
  str     r3,[r1]  /* restore CallLevel */
  #endif
l_nosave:
  stmfd   sp!, {lr}     /* push {lr} */
  ldr     r1, = CallLevel
  ldr     r3, [r1]
  stmfd   sp!, {r3}
  mov     r3, #2   /* CallLevel = TCL_ISR2 */
  str     r3,[r1]

  bl Os_PortIsrHandler

ExitISR:
  ldmfd   sp!, {r3}
  ldr     r1, = CallLevel
  str     r3, [r1]

  ldmfd   sp!, {lr}

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

  b       Os_PortStartDispatch

l_nopreempt:
  b       Os_PortDispatchFormISR

l_nodispatch:
  subs pc, lr, #4
