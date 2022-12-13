/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
  .syntax unified
  .cpu cortex-m3
  .fpu softvfp
  .thumb

.macro DEFAULT_ISR_HANDLER name=
  .thumb_func
  .weak \name
\name:
1: b 1b /* endless loop */
.endm

  .global __vector_table

  .extern knl_start_dispatch
  .extern knl_dispatch_entry
  .extern knl_system_tick
  .extern knl_isr_process
  .extern nmi_handler
  .extern hard_fault_handler
  .extern mpu_fault_handler
  .extern bus_fault_handler
  .extern usage_fault_handler
  .extern debug_monitor_handler
  .extern knl_system_stack_top
  .extern main

/******************************************************************************
* Vector table for a Cortex M3. Vectors start at addr 0x0.
******************************************************************************/
  .section .isr_vector,"a",%progbits
  .type __vector_table, %object
__vector_table:
   /*    Internal Exceptions Vector Define                                          */
  .word     knl_system_stack_top            /* 00: Top of Main Stack           */
  .word     reset_handler                        /* 01: Reset Handler               */
  .word     nmi_handler                           /* 02: NMI Handler                 */
  .word     hard_fault_handler                   /* 03: Hard Fault Handler          */
  .word     mpu_fault_handler                    /* 04: MPU Fault Handler           */
  .word     bus_fault_handler                    /* 05: Bus Fault Handler           */
  .word     usage_fault_handler                  /* 06: Usage Fault Handler         */
  .word     0                                    /* 07: Reserved                    */
  .word     0                                    /* 08: Reserved                    */
  .word     0                                    /* 09: Reserved                    */
  .word     0                                    /* 10: Reserved                    */
  .word     knl_start_dispatch                   /* 11: SVCall Handler              */
  .word     debug_monitor_handler                /* 12: Debug Monitor Handler       */
  .word     0                                    /* 13: Reserved                    */
  .word     knl_dispatch_entry                   /* 14: PendSV Handler              */
  .word     knl_system_tick                      /* 15: Systick handler             */

  /*    External Interrupts Vector Define                                         */
  .word     knl_isr_process                      /* 16: */
  .word     knl_isr_process                      /* 17: */
  .word     knl_isr_process                      /* 18: */
  .word     knl_isr_process                      /* 19: */
  .word     knl_isr_process                      /* 20: */
  .word     knl_isr_process                      /* 21: */
  .word     knl_isr_process                      /* 22: */
  .word     knl_isr_process                      /* 23: */
  .word     knl_isr_process                      /* 24: */
  .word     knl_isr_process                      /* 25: */
  .word     knl_isr_process                      /* 26: */
  .word     knl_isr_process                      /* 27: */
  .word     knl_isr_process                      /* 28: */
  .word     knl_isr_process                      /* 29: */
  .word     knl_isr_process                      /* 30: */
  .word     knl_isr_process                      /* 31: */
  .word     knl_isr_process                      /* 32: */
  .word     knl_isr_process                      /* 33: */
  .word     knl_isr_process                      /* 34: */
  .word     knl_isr_process                      /* 35: */
  .word     knl_isr_process                      /* 36: */
  .word     knl_isr_process                      /* 37: */
  .word     knl_isr_process                      /* 38: */
  .word     knl_isr_process                      /* 39: */
  .word     knl_isr_process                      /* 40: */
  .word     knl_isr_process                      /* 41: */
  .word     knl_isr_process                      /* 42: */
  .word     knl_isr_process                      /* 43: */
  .word     knl_isr_process                      /* 44: */
  .word     knl_isr_process                      /* 45: */
  .word     knl_isr_process                      /* 46: */
  .word     knl_isr_process                      /* 47: */
  .word     knl_isr_process                      /* 48: */
  .word     knl_isr_process                      /* 49: */
  .word     knl_isr_process                      /* 50: */
  .word     knl_isr_process                      /* 51: */
  .word     knl_isr_process                      /* 52: */
  .word     knl_isr_process                      /* 53: */
  .word     knl_isr_process                      /* 54: */
  .word     knl_isr_process                      /* 55: */
  .word     knl_isr_process                      /* 56: */
  .word     knl_isr_process                      /* 57: */
  .word     knl_isr_process                      /* 58: */
  .word     knl_isr_process                      /* 59: */
  .word     knl_isr_process                      /* 60: */
  .word     knl_isr_process                      /* 61: */
  .word     knl_isr_process                      /* 62: */
  .word     knl_isr_process                      /* 63: */
  .word     knl_isr_process                      /* 64: */
  .word     knl_isr_process                      /* 65: */
  .word     knl_isr_process                      /* 66: */
  .word     knl_isr_process                      /* 67: */
  .word     knl_isr_process                      /* 68: */
  .word     knl_isr_process                      /* 69: */
  .word     knl_isr_process                      /* 70: */
  .word     knl_isr_process                      /* 71: */
  .word     knl_isr_process                      /* 72: */
  .word     knl_isr_process                      /* 73: */
  .word     knl_isr_process                      /* 74: */
  .word     knl_isr_process                      /* 75: */
  .word     knl_isr_process                      /* 76: */
  .word     knl_isr_process                      /* 77: */
  .word     knl_isr_process                      /* 78: */
  .word     knl_isr_process                      /* 79: */
  .word     knl_isr_process                      /* 80: */
  .word     knl_isr_process                      /* 81: */
  .word     knl_isr_process                      /* 82: */
  .word     knl_isr_process                      /* 83: */
  .word     knl_isr_process                      /* 84: */
  .word     knl_isr_process                      /* 85: */
  .word     knl_isr_process                      /* 86: */
  .word     knl_isr_process                      /* 87: */
  .word     knl_isr_process                      /* 88: */
  .word     knl_isr_process                      /* 89: */
  .word     knl_isr_process                      /* 90: */
  .word     knl_isr_process                      /* 91: */
  .word     knl_isr_process                      /* 92: */
  .word     knl_isr_process                      /* 93: */
  .word     knl_isr_process                      /* 94: */
  .word     knl_isr_process                      /* 95: */
  .word     knl_isr_process                      /* 96: */
  .word     knl_isr_process                      /* 97: */
  .word     knl_isr_process                      /* 98: */
  .word     knl_isr_process                      /* 99: */
  .word     knl_isr_process                      /* 100: */
  .word     knl_isr_process                      /* 101: */
  .word     knl_isr_process                      /* 102: */
  .word     knl_isr_process                      /* 103: */
  .word     knl_isr_process                      /* 104: */
  .word     knl_isr_process                      /* 105: */
  .word     knl_isr_process                      /* 106: */
  .word     knl_isr_process                      /* 107: */
  .word     knl_isr_process                      /* 108: */
  .word     knl_isr_process                      /* 109: */
  .word     knl_isr_process                      /* 110: */
  .word     knl_isr_process                      /* 111: */
  .word     knl_isr_process                      /* 112: */
  .word     knl_isr_process                      /* 113: */
  .word     knl_isr_process                      /* 114: */
  .word     knl_isr_process                      /* 115: */
  .word     knl_isr_process                      /* 116: */
  .word     knl_isr_process                      /* 117: */
  .word     knl_isr_process                      /* 118: */
  .word     knl_isr_process                      /* 119: */
  .word     knl_isr_process                      /* 120: */
  .word     knl_isr_process                      /* 121: */
  .word     knl_isr_process                      /* 122: */
  .word     knl_isr_process                      /* 123: */
  .word     knl_isr_process                      /* 124: */
  .word     knl_isr_process                      /* 125: */
  .word     knl_isr_process                      /* 126: */
  .word     knl_isr_process                      /* 127: */
  .word     knl_isr_process                      /* 128: */
  .word     knl_isr_process                      /* 129: */
  .word     knl_isr_process                      /* 130: */
  .word     knl_isr_process                      /* 131: */
  .word     knl_isr_process                      /* 132: */
  .word     knl_isr_process                      /* 133: */
  .word     knl_isr_process                      /* 134: */
  .word     knl_isr_process                      /* 135: */
  .word     knl_isr_process                      /* 136: */
  .word     knl_isr_process                      /* 137: */
  .word     knl_isr_process                      /* 138: */
  .word     knl_isr_process                      /* 139: */
  .word     knl_isr_process                      /* 140: */
  .word     knl_isr_process                      /* 141: */
  .word     knl_isr_process                      /* 142: */
  .word     knl_isr_process                      /* 143: */
  .word     knl_isr_process                      /* 144: */
  .word     knl_isr_process                      /* 145: */
  .word     knl_isr_process                      /* 146: */
  .word     knl_isr_process                      /* 147: */
  .word     knl_isr_process                      /* 148: */
  .word     knl_isr_process                      /* 149: */
  .word     knl_isr_process                      /* 150: */
  .word     knl_isr_process                      /* 151: */
  .word     knl_isr_process                      /* 152: */
  .word     knl_isr_process                      /* 153: */
  .word     knl_isr_process                      /* 154: */
  .word     knl_isr_process                      /* 155: */
  .word     knl_isr_process                      /* 156: */
  .word     knl_isr_process                      /* 157: */
  .word     knl_isr_process                      /* 158: */
  .word     knl_isr_process                      /* 159: */
  .word     knl_isr_process                      /* 160: */
  .word     knl_isr_process                      /* 161: */
  .word     knl_isr_process                      /* 162: */
  .word     knl_isr_process                      /* 163: */
  .word     knl_isr_process                      /* 164: */
  .word     knl_isr_process                      /* 165: */
  .word     knl_isr_process                      /* 166: */
  .word     knl_isr_process                      /* 167: */
  .word     knl_isr_process                      /* 168: */
  .word     knl_isr_process                      /* 169: */
  .word     knl_isr_process                      /* 170: */
  .word     knl_isr_process                      /* 171: */
  .word     knl_isr_process                      /* 172: */
  .word     knl_isr_process                      /* 173: */
  .word     knl_isr_process                      /* 174: */
  .word     knl_isr_process                      /* 175: */
  .word     knl_isr_process                      /* 176: */
  .word     knl_isr_process                      /* 177: */
  .word     knl_isr_process                      /* 178: */
  .word     knl_isr_process                      /* 179: */
  .word     knl_isr_process                      /* 180: */
  .word     knl_isr_process                      /* 181: */
  .word     knl_isr_process                      /* 182: */
  .word     knl_isr_process                      /* 183: */
  .word     knl_isr_process                      /* 184: */
  .word     knl_isr_process                      /* 185: */
  .word     knl_isr_process                      /* 186: */
  .word     knl_isr_process                      /* 187: */
  .word     knl_isr_process                      /* 188: */
  .word     knl_isr_process                      /* 189: */
  .word     knl_isr_process                      /* 190: */
  .word     knl_isr_process                      /* 191: */
  .word     knl_isr_process                      /* 192: */
  .word     knl_isr_process                      /* 193: */
  .word     knl_isr_process                      /* 194: */
  .word     knl_isr_process                      /* 195: */
  .word     knl_isr_process                      /* 196: */
  .word     knl_isr_process                      /* 197: */
  .word     knl_isr_process                      /* 198: */
  .word     knl_isr_process                      /* 199: */
  .word     knl_isr_process                      /* 200: */
  .word     knl_isr_process                      /* 201: */
  .word     knl_isr_process                      /* 202: */
  .word     knl_isr_process                      /* 203: */
  .word     knl_isr_process                      /* 204: */
  .word     knl_isr_process                      /* 205: */
  .word     knl_isr_process                      /* 206: */
  .word     knl_isr_process                      /* 207: */
  .word     knl_isr_process                      /* 208: */
  .word     knl_isr_process                      /* 209: */
  .word     knl_isr_process                      /* 210: */
  .word     knl_isr_process                      /* 211: */
  .word     knl_isr_process                      /* 212: */
  .word     knl_isr_process                      /* 213: */
  .word     knl_isr_process                      /* 214: */
  .word     knl_isr_process                      /* 215: */
  .word     knl_isr_process                      /* 216: */
  .word     knl_isr_process                      /* 217: */
  .word     knl_isr_process                      /* 218: */
  .word     knl_isr_process                      /* 219: */
  .word     knl_isr_process                      /* 220: */
  .word     knl_isr_process                      /* 221: */
  .word     knl_isr_process                      /* 222: */
  .word     knl_isr_process                      /* 223: */
  .word     knl_isr_process                      /* 224: */
  .word     knl_isr_process                      /* 225: */
  .word     knl_isr_process                      /* 226: */
  .word     knl_isr_process                      /* 227: */
  .word     knl_isr_process                      /* 228: */
  .word     knl_isr_process                      /* 229: */
  .word     knl_isr_process                      /* 230: */
  .word     knl_isr_process                      /* 231: */
  .word     knl_isr_process                      /* 232: */
  .word     knl_isr_process                      /* 233: */
  .word     knl_isr_process                      /* 234: */
  .word     knl_isr_process                      /* 235: */
  .word     knl_isr_process                      /* 236: */
  .word     knl_isr_process                      /* 237: */
  .word     knl_isr_process                      /* 238: */
  .word     knl_isr_process                      /* 239: */
  .word     knl_isr_process                      /* 240: */
  .word     knl_isr_process                      /* 241: */
  .word     knl_isr_process                      /* 242: */
  .word     knl_isr_process                      /* 243: */
  .word     knl_isr_process                      /* 244: */
  .word     knl_isr_process                      /* 245: */
  .word     knl_isr_process                      /* 246: */
  .word     knl_isr_process                      /* 247: */
  .word     knl_isr_process                      /* 248: */
  .word     knl_isr_process                      /* 249: */
  .word     knl_isr_process                      /* 250: */
  .word     knl_isr_process                      /* 251: */
  .word     knl_isr_process                      /* 252: */
  .word     knl_isr_process                      /* 253: */
  .word     knl_isr_process                      /* 254: */
  .word     knl_isr_process                      /* 255: */

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval : None
*/
  .section  .startup
  .weak  reset_handler
  .type  reset_handler, %function
reset_handler:
  ldr  sp, =knl_system_stack_top
  /* Copy the data segment initializers from flash to SRAM */
  ldr  r0, =__data_start__  /* r0 holds start of data in ram */
  ldr  r3, =__data_end__    /* r3 holds end of data in ram */
  ldr  r5, =__etext        /* r5 start of data in flash */
  movs r1, #0
  b    LoopCopyDataInit

CopyDataInit:
  ldr  r4, [r5, r1]          /* read current position in flash */
  str  r4, [r0, r1]          /* store current position in ram */
  adds r1, r1, #4        /* increment counter */

LoopCopyDataInit:
  adds r2, r0, r1        /* are we at the final position? */
  cmp  r2, r3                /* ... */
  bcc  CopyDataInit          /* nope, continue */
  ldr  r2, =__bss_start__
  b    LoopFillZerobss

/* Zero fill the bss segment. */
FillZerobss:
  movs r3, #0
  str  r3, [r2], #4

LoopFillZerobss:
  ldr  r3, = __bss_end__
  cmp  r2, r3
  bcc  FillZerobss
/* Call the application's entry point.*/
  bl  main
  b   .
.size reset_handler, .-reset_handler

  .section .text
  .global Std_ExitCritical
  .type   Std_ExitCritical, %function
/* void Std_ExitCritical( imask_t intsts ); */
Std_ExitCritical:
  mrs     r1, primask
  msr     primask, r0
  mov     r0, r1
  bx      lr

  .global Std_EnterCritical
  .type   Std_EnterCritical, %function
/* imask_t Std_EnterCritical( void ); */
Std_EnterCritical:
  mrs     r0, primask
  ldr     r1, = 0x1 /* TS_PMK_D */
  msr     primask, r1
  bx      lr

/* void DisableInterrupt(void)   */
  .global DisableInterrupt
  .type   DisableInterrupt, %function
DisableInterrupt:
  cpsid   i
  bx      lr

/* void EnableInterrupt(void) */
  .global EnableInterrupt
  .type   EnableInterrupt, %function
EnableInterrupt:
  cpsie   i
  bx      lr

DEFAULT_ISR_HANDLER knl_isr_process
DEFAULT_ISR_HANDLER nmi_handler
DEFAULT_ISR_HANDLER hard_fault_handler
DEFAULT_ISR_HANDLER mpu_fault_handler
DEFAULT_ISR_HANDLER bus_fault_handler
DEFAULT_ISR_HANDLER usage_fault_handler
DEFAULT_ISR_HANDLER debug_monitor_handler
DEFAULT_ISR_HANDLER knl_system_tick
DEFAULT_ISR_HANDLER knl_dispatch_entry

