/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#include "Std_Debug.h"
#include "mpc56xx.h"
#include "IntcInterrupts.h"

/** This macro allows to use C defined address with the inline assembler */
#define MAKE_HLI_ADDRESS(hli_name, c_expr) /*lint -e753 */                                         \
  enum                                                                                             \
  { hli_name = /*lint -e30*/ ((int)(c_expr)) /*lint -esym(749, hli_name) */ };

/** Address of the IACKR Interrupt Controller register. */
MAKE_HLI_ADDRESS(INTC_IACKR, &INTC.IACKR.R)
/** Address of the EOIR End-of-Interrupt Register register. */
MAKE_HLI_ADDRESS(INTC_EOIR, &INTC.EOIR.R)

/** Address of the MCR -- used for e200z0h initialization */
MAKE_HLI_ADDRESS(INTC_MCR, &INTC.MCR.R)
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_OS 0

/***************************************************************************************************
    Regeister Map
***************************************************************************************************/
#define XR1 0x00 /* SP */
#define XBLK 0x04
#define XR2 0x08 /* _SDA2_BASE_ */
#define XR3 0x0C
#define XR4 0x10
#define XR5 0x14
#define XR6 0x18
#define XR7 0x1C
#define XR8 0x20
#define XR9 0x24
#define XR10 0x28
#define XR11 0x2C /* Generally used as BSP */
#define XR12 0x30
#define XR13 0x34 /* _SDA_BASE_ */
#define XR14 0x38
#define XR15 0x3C
#define XR16 0x40
#define XR17 0x44
#define XR18 0x48
#define XR19 0x4C
#define XR20 0x50
#define XR21 0x54
#define XR22 0x58
#define XR23 0x5C
#define XR24 0x60
#define XR25 0x64
#define XR26 0x68
#define XR27 0x6C
#define XR28 0x70
#define XR29 0x74
#define XR30 0x78
#define XR31 0x7C
#define XR0 0x80
#define XSRR0 0x84
#define XSRR1 0x88
#define XUSPRG 0x8C
#define XCTR 0x90
#define XXER 0x94
#define XCR 0x98
#define XLR 0x9C
#define XSPEFSCR 0xA0
#define XPAD2 0xA4
#define XPAD3 0xA8
#define XMSR 0xAC

#define STACK_FRAME_SIZE 0xB0

#define STACK_PROTECT_SIZE 32

#define OS_RESTORE_CONTEXT()                                                                       \
  wrteei 0;                                                                                        \
  /*restore R0,R2-R31*/                                                                            \
  lmw r2, XR2(r1);                                                                                 \
  /*restore CR*/                                                                                   \
  lwz r0, XCR(r1);                                                                                 \
  mtcrf 0xff, r0;                                                                                  \
  /*restore XER*/                                                                                  \
  lwz r0, XXER(r1);                                                                                \
  mtxer r0;                                                                                        \
  /*restore CTR*/                                                                                  \
  lwz r0, XCTR(r1);                                                                                \
  mtctr r0;                                                                                        \
  /*restore LR */                                                                                  \
  lwz r0, XLR(r1);                                                                                 \
  mtlr r0;                                                                                         \
  /*restore SRR1*/                                                                                 \
  lwz r0, XSRR1(r1);                                                                               \
  mtspr SRR1, r0;                                                                                  \
  /*restore SRR0*/                                                                                 \
  lwz r0, XSRR0(r1);                                                                               \
  mtspr SRR0, r0;                                                                                  \
  /*restore USPRG*/                                                                                \
  lwz r0, XUSPRG(r1);                                                                              \
  mtspr USPRG0, r0;                                                                                \
  /*restore SPEFSCR :for float point, if not used, canbe not saved */                              \
  lwz r0, XSPEFSCR(r1);                                                                            \
  mtspr SPEFSCR, r0;                                                                               \
  /*restore rsp*/                                                                                  \
  lwz r0, XR0(r1);                                                                                 \
  addi r1, r1, STACK_FRAME_SIZE

#define OS_SAVE_CONTEXT()                                                                          \
  /* Remain frame from stack */                                                                    \
  subi r1, r1, STACK_FRAME_SIZE;                                                                   \
  /*Store R0,R2,R3-R31*/                                                                           \
  stw r0, XR0(r1);                                                                                 \
  stmw r2, XR2(r1);                                                                                \
  /*Store XMSR ang SPEFSCR  */                                                                     \
  mfmsr r0;                                                                                        \
  stw r0, XMSR(r1);                                                                                \
  mfspr r0, SPEFSCR;                                                                               \
  stw r0, XSPEFSCR(r1);                                                                            \
  /*Store LR(SRR0)*/                                                                               \
  mfspr r0, SRR0;                                                                                  \
  stw r0, XSRR0(r1);                                                                               \
  /*Store MSR(SRR1)*/                                                                              \
  mfspr r0, SRR1;                                                                                  \
  stw r0, XSRR1(r1);                                                                               \
  /*Store USPRG0*/                                                                                 \
  mfspr r0, USPRG0;                                                                                \
  stw r0, XUSPRG(r1);                                                                              \
  /*Store LR*/                                                                                     \
  mflr r0;                                                                                         \
  stw r0, XLR(r1);                                                                                 \
  /*Store CTR*/                                                                                    \
  mfctr r0;                                                                                        \
  stw r0, XCTR(r1);                                                                                \
  /*Store XER*/                                                                                    \
  mfxer r0;                                                                                        \
  stw r0, XXER(r1);                                                                                \
  /*Store CR*/                                                                                     \
  mfcr r0;                                                                                         \
  stw r0, XCR(r1);                                                                                 \
  mfmsr r0

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void Os_PortResume(void);
void Os_PortDispatchEntry(void);
void Os_PortTickISR(void);
void Os_PortExtISR(void);
/* ================================ [ DATAS     ] ============================================== */
uint32 ISR2Counter;
extern int _stack_addr[];
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Os_PortActivate(void) {
  /* get internal resource or NON schedule */
  RunningVar->priority = RunningVar->pConst->runPriority;

  ASLOG(OS, ("%s(%d) is running\n", RunningVar->pConst->name, RunningVar->pConst->initPriority));

  CallLevel = TCL_TASK;
  Irq_Enable();

  RunningVar->pConst->entry();

  /* Should not return here */
  TerminateTask();
  /* Fatal Error */
  ShutdownOS(E_OS_CALLEVEL);
}

void Os_PortInit(void) {
  ISR2Counter = 0;
  __asm {
    /* IVOR8 System call interrupt (SPR 408) */
    lis r0, Os_PortDispatchEntry@h;
    ori r0, r0, Os_PortDispatchEntry@l;
    mtivor8 r0;
    /* IVOR10 Decrementer interrupt (SPR 410) */
    lis     r0, Os_PortTickISR@h;
    ori     r0, r0, Os_PortTickISR@l;
    mtivor10 r0;
    /* IVOR4 External input interrupt (SPR 404) */
    lis     r0, Os_PortExtISR@h;
    ori     r0, r0, Os_PortExtISR@l;
    mtivor4 r0;
  }
  INTC.CPR.B.PRI = 0; /* Lower INTC's current priority */
}

void Os_PortInitContext(TaskVarType *pTaskVar) {
  pTaskVar->context.sp =
    pTaskVar->pConst->pStack + pTaskVar->pConst->stackSize - STACK_PROTECT_SIZE;
  pTaskVar->context.pc = Os_PortActivate;
}

#ifdef OS_USE_POSTTASK_HOOK
void Os_PortPostTaskHook(void) {
  OSPostTaskHook();
}
#endif

#ifdef OS_USE_PRETASK_HOOK
void Os_PortPreTaskHook(void) {
  OSPreTaskHook();
}
#endif
#ifdef USE_PTHREAD_SIGNAL
void Os_PortCallSignal(int sig, void (*handler)(int), void *sp, void (*pc)(void)) {
  asAssert(NULL != handler);

  handler(sig);

  /* restore its previous stack */
  RunningVar->context.sp = sp;
  RunningVar->context.pc = pc;
}

void Os_PortExitSignalCall(void) {
  Sched_GetReady();
  Os_PortStartDispatch();
}

int Os_PortInstallSignal(TaskVarType *pTaskVar, int sig, void *handler) {
  void *sp;
  uint32_t *stk;

  sp = pTaskVar->context.sp;

  if ((sp - pTaskVar->pConst->pStack) < (pTaskVar->pConst->stackSize * 3 / 4)) {
    /* stack 75% usage, ignore this signal call */
    ASLOG(OS, ("install signal %d failed\n", sig));
    return -1;
  }

  stk = sp;

  /* TODO */

  pTaskVar->context.sp = stk;
  pTaskVar->context.pc = Os_PortResume;

  return 0;
}
#endif

__asm void Os_PortDispatch(void) {
nofralloc
  sc;
  blr;
}

#pragma section RX ".__exception_handlers"
#pragma push /* Save the current state */
__declspec(section ".__exception_handlers") extern long EXCEPTION_HANDLERS;
#pragma force_active on
#pragma function_align 16 /* We use 16 bytes alignment for Exception handlers */

__asm void Os_PortStartDispatch(void) {
nofralloc
  /* Interrupt disable */
  wrteei 0;
  /* R10 = &ReadyVar */
  lis r10, ReadyVar @h;
  ori r10, r10, ReadyVar @l;
  lwz r12, 0(r10);
  cmpwi r12, 0;
  bne l_exit;
  /* load system stack */
  lis r1, _stack_addr @h;
  ori r1, r1, _stack_addr @l;
  subi r1, r1, STACK_PROTECT_SIZE;
l_loop:
  /* Interrupt enable */
  wrteei 1;
  nop;
  nop;
  nop;
  nop;
  wrteei 0;
  bl Sched_GetReady;
  lis r10, ReadyVar @h;
  ori r10, r10, ReadyVar @l;
  lwz r12, 0(r10);
  cmpwi r12, 0;
  bne l_exit;
  b l_loop;
l_exit:
  lis r11, RunningVar @h;
  stw r12, RunningVar @l(r11);

#ifdef OS_USE_PRETASK_HOOK
  bl Os_PortPreTaskHook;
  lis r11, RunningVar @h;
  stw r12, RunningVar @l(r11);
#endif

  /* Restore 'sp' from TCB */
  lwz r1, 0(r12);

  /* Restore 'pc' from TCB */
  lwz r11, 4(r12);
  mtctr r11;
  se_bctrl;
}

__asm void Os_PortResume(void) {
nofralloc
  OS_RESTORE_CONTEXT();
  rfi;
}

__asm void EnterISR(void) {
nofralloc
  lis r11, RunningVar @h;
  lwz r10, RunningVar @l(r11);
  cmpwi r10, 0;
  beq l_nosave;

  lis r11, ISR2Counter @h;
  lwz r12, ISR2Counter @l(r11);
  addi r12, r12, 1;
  stw r12, ISR2Counter @l(r11);
  cmpwi r12, 1;
  bne l_nosave;

  /* Save 'ssp' to TCB */
  stw r1, 0(r10);

  lis r11, Os_PortResume @h;
  ori r11, r11, Os_PortResume @l;
  stw r11, 4(r10);

  /* load system stack */
  lis r1, _stack_addr @h;
  ori r1, r1, _stack_addr @l;
  subi r1, r1, STACK_PROTECT_SIZE;

#ifdef OS_USE_POSTTASK_HOOK
  mflr r0;
  subi r1, r1, 4; /* save LR in stack */
  stw r0, 0(r1);
  bl Os_PortPostTaskHook;
  lwz r0, 0(r1);
  addi r1, r1, 4;
  mtlr r0;
#endif

l_nosave:
  lis r11, CallLevel @h;
  lwz r12, CallLevel @l(r11);
  subi r1, r1, 4; /* save CallLevel in stack */
  stw r12, 0(r1);
  li r3, 2; /* TCL_ISR2 */
  stw r3, CallLevel @l(r11);
  blr;
}

__asm void LeaveISR(void) {
nofralloc
  wrteei 0;
  lwz r12, 0(r1);
  addi r1, r1, 4;
  lis r11, CallLevel @h;
  stw r12, CallLevel @l(r11);

  lis r11, RunningVar @h;
  lwz r10, RunningVar @l(r11);
  cmpwi r10, 0;
  beq l_nodispatch;

  lis r11, ISR2Counter @h;
  lwz r12, ISR2Counter @l(r11);
  subi r12, r12, 1;
  stw r12, ISR2Counter @l(r11);
  cmpwi r12, 0;
  bne l_nodispatch;

  lis r11, CallLevel @h;
  lwz r12, CallLevel @l(r11);
  cmpwi r12, 1; /* TCL_TASK */
  bne l_nopreempt;

  lis r11, ReadyVar @h;
  lwz r12, ReadyVar @l(r11);
  cmpwi r12, 0;
  beq l_nopreempt;

  lbz r3, 8(r12); /* priority of ReadyVar */
  lbz r4, 8(r10); /* priority of RunningVar */
  cmpw r4, r3;
  bge l_nopreempt;

  bl Sched_Preempt;

  b Os_PortStartDispatch;

l_nopreempt:
#ifdef OS_USE_PRETASK_HOOK
  bl Os_PortPreTaskHook;
#endif
  lis r11, RunningVar @h;
  lwz r12, RunningVar @l(r11);
  lwz r1, 0(r12);

l_nodispatch:
  OS_RESTORE_CONTEXT();
  rfi;
}

__declspec(interrupt) __declspec(section
                                 ".__exception_handlers") __asm void Os_PortDispatchEntry(void) {
nofralloc
  OS_SAVE_CONTEXT();

  /* Save 'ssp' to TCB */
  lis r11, RunningVar @h;
  lwz r12, RunningVar @l(r11);
  stw r1, 0(r12);

  lis r11, Os_PortResume @h;
  ori r11, r11, Os_PortResume @l;
  stw r11, 4(r12);

#ifdef OS_USE_POSTTASK_HOOK
  mflr r0;
  subi r1, r1, 4; /* save LR in stack */
  stw r0, 0(r1);
  bl Os_PortPostTaskHook;
  lwz r0, 0(r1);
  addi r1, r1, 4;
  mtlr r0;
#endif

  b Os_PortStartDispatch;
}

__declspec(interrupt) __declspec(section ".__exception_handlers") __asm void Os_PortTickISR(void) {
nofralloc
  OS_SAVE_CONTEXT();

  bl EnterISR;

  /*Clear TSR[DIS]*/
  lis r0, 0x0800;
  mtspr TSR, r0;

  wrteei 1;
  bl OsTick;
  li r3, 0;
  bl SignalCounter;

  b LeaveISR;
}

__declspec(interrupt) __declspec(section ".__exception_handlers") __asm void Os_PortExtISR(void) {
nofralloc
  OS_SAVE_CONTEXT();

  bl EnterISR;

  /* Clear request to processor; r3 contains the address of the ISR */
  lis r3, INTC_IACKR @h; /* Read pointer into ISR Vector Table & store in r3     */
  ori r3, r3, INTC_IACKR @l;
  lwz r3, 0x0(r3); /* Load INTC_IACKR, which clears request to processor   */
  lwz r3, 0x0(r3); /* Read ISR address from ISR Vector Table using pointer */

  wrteei 1;
  /* Branch to ISR handler address from SW vector table */
  mtlr r3; /* Store ISR address to LR to use for branching later */
  blrl;    /* Branch to ISR, but return here */
  wrteei 0;

  /* Ensure interrupt flag has finished clearing */
  mbar 0;

  /* Write 0 to INTC_EOIR, informing INTC to lower priority */
  li r3, 0;
  lis r4, INTC_EOIR @h; /* Load upper half of INTC_EOIR address to r4 */
  ori r4, r4, INTC_EOIR @l;
  stw r3, 0(r4); /* Write 0 to INTC_EOIR */

  b LeaveISR;
}
