/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
.extern __bss_start
.extern __bss_end
.extern main
.extern vector_table_el1
.extern stack_top
.extern stack2_top
.extern secondary_main
.extern InitBssSection
.extern InitDataSection

  .section .init

  .globl _start
_start:
  /* set up stack */
  mov x4, #1
  msr spsel, x4
  isb
  ldr x4, =stack_top
  mov sp, x4

  /* enable FP/ASIMD */
  mov x4, #(3 << 20)
  msr cpacr_el1, x4

  /* set up exception handling */
  ldr x4, =vector_table_el1
  msr vbar_el1, x4
  isb

  ldr x0, =__bss_start
  ldr x1, =__bss_end
  bl  InitBssSection

  bl  DisableInterrupt

  ldr x0, =__etext
  ldr x1, =__data_start__
  ldr x2, =__data_end__
  bl  InitDataSection

  b main
  b .

  .section .text

  .global secondary_start
  .weak secondary_main
secondary_main:
  b .

  .global secondary_start
secondary_start:
  /* set up stack */
  mov x4, #1
  msr spsel, x4
  isb
  ldr x4, =stack2_top
  mov sp, x4

  /* enable FP/ASIMD */
  mov x4, #(3 << 20)
  msr cpacr_el1, x4

  /* set up exception handling */
  ldr x4, =vector_table_el1
  msr vbar_el1, x4
  isb

  b secondary_main
  b .

  .global smp_processor_id
smp_processor_id:
  mrs  x0, mpidr_el1
  and  x0, x0, #3
  ret
