/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdint.h>
#include "Std_Compiler.h"
#ifdef USE_STDIO_CAN
#include "ringbuffer.h"
#include "Std_Critical.h"
#include "Std_Debug.h"
#include "Can.h"
#ifdef USE_CANSM
#include "CanSM.h"
#endif
#ifdef USE_CANNM
#include "CanNm.h"
#endif
#ifdef USE_SHELL
#include "shell.h"
#endif
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef STDIO_CAN_DLC
#define STDIO_CAN_DLC 8
#endif
#ifndef STDIO_CAN_IBUFFER_SIZE
#define STDIO_CAN_IBUFFER_SIZE 512
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void __putchar(char chr);
extern int stdio_can_put(uint8_t *data, uint8_t dlc);
#ifdef USE_STDIO_CAN
static void stdio_can_main(void);
#endif
/* ================================ [ DATAS     ] ============================================== */
#ifdef USE_STDIO_CAN
RB_DECLARE(stdio_can, char, STDIO_CAN_IBUFFER_SIZE);
static uint32_t wmissing = 0;
#endif
#if STDIO_CAN_DLC > 8
static const uint8_t lLL_DLs[] = {8, 12, 16, 20, 24, 32, 48, 64};
#else
#define stdio_get_dlc(sz) sz
#endif
/* ================================ [ LOCALS    ] ============================================== */
#if STDIO_CAN_DLC > 8
static PduLengthType stdio_get_dlc(PduLengthType len) {
  PduLengthType dl = len;
  int i;
  if (len > 8) {
    for (i = 0; i < ARRAY_SIZE(lLL_DLs); i++) {
      if (len <= lLL_DLs[i]) {
        dl = lLL_DLs[i];
        break;
      }
    }
  }
  return dl;
}
#endif

#ifdef USE_STDIO_CAN
static void can_putc(char chr) {
  rb_size_t r;
  EnterCritical();
  r = RB_PUSH(stdio_can, &chr, 1);
  ExitCritical();

  if (1 != r) {
#ifdef USE_STDIO_CAN_FLUSH
    stdio_can_main();
    EnterCritical();
    r = RB_PUSH(stdio_can, &chr, 1);
    ExitCritical();
    if (1 != r) {
      wmissing++;
    }
#else
    /* do noting as full */
    wmissing++;
#endif
  }
}

static void stdio_can_main(void) {
  uint8_t data[STDIO_CAN_DLC];
  rb_size_t sz;
#if STDIO_CAN_DLC > 8
  rb_size_t i;
#endif
  Std_ReturnType ret;
  Can_PduType PduInfo;

#ifdef USE_CANNM
  Nm_StateType nmState = NM_STATE_OFFLINE;
  Nm_ModeType nmMode = NM_MODE_BUS_SLEEP;
#endif

#ifdef USE_CANSM
  ComM_ModeType mode = COMM_NO_COMMUNICATION;
#endif

#ifdef USE_CANNM
  CanNm_GetState(0, &nmState, &nmMode);
  if (NM_STATE_NORMAL_OPERATION != nmState) {
    return;
  }
#endif

#ifdef USE_CANSM
  CanSM_GetCurrentComMode(0, &mode);
  if (CANSM_BSWM_FULL_COMMUNICATION != mode) {
    return;
  }
#endif
  EnterCritical();
  sz = RB_POLL(stdio_can, data, STDIO_CAN_DLC);
  ExitCritical();
  if (sz > 0) {
    PduInfo.id = STDIO_TX_CANID;
    PduInfo.length = stdio_get_dlc(sz);
    PduInfo.sdu = data;
#if STDIO_CAN_DLC > 8
    for (i = sz; i < PduInfo.length; i++) {
      data[i] = 0;
    }
#endif
    PduInfo.swPduHandle = STDIO_TX_CAN_HANDLE;
    ret = Can_Write(STDIO_TX_CAN_HTH, &PduInfo);
    if (E_OK == ret) {
      EnterCritical();
      (void)RB_DROP(stdio_can, sz); /* consume it */
      ExitCritical();
    }
  }
}
#endif

#if !defined(__HIWARE__) && !defined(__CSP__)
void __weak __putchar(char chr) {
#if defined(_WIN32) || defined(linux)
  fputc(chr, stdout);
#endif
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void stdio_putc(char chr) {
  __putchar(chr);
#ifdef USE_STDIO_CAN
  can_putc(chr);
#endif
}
#ifdef USE_STDIO_CAN
void stdio_can_putc(char chr) {
  can_putc(chr);
}
#endif

void stdio_main_function(void) {
#ifdef USE_STDIO_CAN
  stdio_can_main();
#endif
}

#if defined(USE_STDIO_CAN) && defined(USE_SHELL)
void UserStdin_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  int i;
  for (i = 0; i < PduInfoPtr->SduLength; i++) {
    Shell_Input((char)PduInfoPtr->SduDataPtr[i]);
  }
}
#endif
