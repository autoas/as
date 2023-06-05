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
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_CAN
int trace_can_put(uint8_t *data, uint8_t dlc);
#endif
/* ================================ [ DATAS     ] ============================================== */
#ifdef USE_VFS
static VFS_FILE *lFp = NULL;
static uint32_t lFlushCounter = 0;
#endif
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Std_TraceEvent(const Std_TraceAreaType *area, Std_TraceEventType event) {
  EnterCritical();
  RB_Push(area->rb, &event, sizeof(Std_TraceEventType));
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
    r = RB_Pop(area->rb, &event, sizeof(Std_TraceEventType));
    while (r == sizeof(Std_TraceEventType)) {
      vfs_fwrite(&event, sizeof(Std_TraceEventType), 1, fp);
      r = RB_Pop(area->rb, &event, sizeof(Std_TraceEventType));
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
  sz = RB_Poll(area->rb, data, TRACE_CAN_DLC);
  ExitCritical();
  if (sz > 0) {
    r = trace_can_put(data, (uint8_t)sz);
    if (0 == r) {
      EnterCritical();
      (void)RB_Drop(area->rb, sz); /* consume it */
      ExitCritical();
    }
  }
}
#endif