/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: about:tracing
 *      https://blog.csdn.net/u011331731/article/details/108354605
 */
#ifndef __STD_TRACE_H__
#define __STD_TRACE_H__
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include "ringbuffer.h"
/* ================================ [ MACROS    ] ============================================== */
#ifdef USE_TRACE
#define STD_TRACE_EVENT(area, ev) Std_TraceEvent(area, ev)
#define STD_TRACE_MAIN(area) Std_TraceMain(area)
#else
#define STD_TRACE_EVENT(area, ev)
#define STD_TRACE_MAIN(area)
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef uint32_t Std_TraceEventType;

typedef struct {
  const RingBufferType *rb;
} Std_TraceAreaType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Std_TraceEvent(const Std_TraceAreaType *area, Std_TraceEventType event);
void Std_TraceDump(const Std_TraceAreaType *area);
void Std_TraceMain(const Std_TraceAreaType *area);
#endif /* __STD_TRACE_H__ */
