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
/* ================================ [ MACROS    ] ============================================== */
#ifndef TRACE_CAN_DLC
#define TRACE_CAN_DLC 8
#endif
/* ================================ [ TYPES     ] ============================================== */
#ifdef RB_PUSH_FAST
RB_PUSH_FAST(TraceEvent, Std_TraceEventType)
#endif
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_CAN
int trace_can_put(uint8_t *data, uint8_t dlc);
#endif
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
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
  int r;

  EnterCritical();
  sz = RB_Poll(area->rb, data, TRACE_CAN_DLC / sizeof(Std_TraceEventType));
  ExitCritical();
  if (sz > 0) {
    r = trace_can_put(data, (uint8_t)(sz * sizeof(Std_TraceEventType)));
    if (0 == r) {
      EnterCritical();
      (void)RB_Drop(area->rb, sz); /* consume it */
      ExitCritical();
    }
  }
}
#endif