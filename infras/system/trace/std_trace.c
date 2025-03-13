/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Trace.h"
#include "Std_Critical.h"
#include <stdio.h>
#ifdef USE_VFS
#include "vfs.h"
#endif
#ifdef USE_CAN
#include "Can.h"
#ifdef USE_STARTUP_TRACE /* NOTE: do undef USE_CANSM to enable trace during startup */
#undef USE_CANSM
#endif
#ifdef USE_CANSM
#include "CanSM.h"
#endif
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef TRACE_CAN_DLC
#define TRACE_CAN_DLC 8
#endif
/* ================================ [ TYPES     ] ============================================== */
#ifdef RB_PUSH_FAST
RB_PUSH_FAST(TraceEvent, Std_TraceEventType)
#endif
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#if TRACE_CAN_DLC > 8
static const uint8_t lLL_DLs[] = {8, 12, 16, 20, 24, 32, 48, 64};
#else
#define Std_TraceGetDlc(sz) sz
#endif
/* ================================ [ LOCALS    ] ============================================== */
#if TRACE_CAN_DLC > 8
static PduLengthType Std_TraceGetDlc(PduLengthType len) {
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
/* ================================ [ FUNCTIONS ] ============================================== */
void Std_TraceEvent(const Std_TraceAreaType *area, Std_TraceEventType event) {
  EnterCritical();
#ifdef RB_PUSH_FAST
  RB_PushTraceEvent(area->rb, &event);
#else
  RB_Push(area->rb, &event, 1);
#endif
  ExitCritical();
}

void Std_TraceDump(const Std_TraceAreaType *area) {
#ifdef USE_VFS
  VFS_FILE *fp;
  rb_size_t r;
  Std_TraceEventType event;
  fp = vfs_fopen("share/.trace.bin", "wb");
  if (NULL != fp) {
    EnterCritical();
    r = RB_Pop(area->rb, &event, 1);
    while (r == 1) {
      vfs_fwrite(&event, sizeof(Std_TraceEventType), 1, fp);
      r = RB_Pop(area->rb, &event, 1);
    }
    ExitCritical();
    vfs_fclose(fp);
  }
#endif
}

#ifdef USE_CAN
void Std_TraceMain(const Std_TraceAreaType *area) {
  uint8_t data[TRACE_CAN_DLC];
  rb_size_t sz;
#if TRACE_CAN_DLC > 8
  rb_size_t i;
#endif
  Std_ReturnType ret;
  Can_PduType PduInfo;

#ifdef USE_CANSM
  ComM_ModeType mode = COMM_NO_COMMUNICATION;
#endif

#ifdef USE_CANSM
  CanSM_GetCurrentComMode(0, &mode);
  if (CANSM_BSWM_FULL_COMMUNICATION == mode) {
#endif
    EnterCritical();
    sz = RB_Poll(area->rb, data, TRACE_CAN_DLC / sizeof(Std_TraceEventType));
    ExitCritical();
    if (sz > 0) {
      PduInfo.id = TRACE_TX_CANID;
      PduInfo.length = Std_TraceGetDlc(sz * sizeof(Std_TraceEventType));
      PduInfo.sdu = data;
#if TRACE_CAN_DLC > 8
      for (i = sz * sizeof(Std_TraceEventType); i < PduInfo.length; i++) {
        data[i] = 0;
      }
#endif
      PduInfo.swPduHandle = TRACE_TX_CAN_HANDLE;
      ret = Can_Write(STDIO_TX_CAN_HTH, &PduInfo);
      if (E_OK == ret) {
        EnterCritical();
        (void)RB_Drop(area->rb, sz); /* consume it */
        ExitCritical();
      }
    }
#ifdef USE_CANSM
  }
#endif
}
#endif
