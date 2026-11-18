/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: about:tracing
 *      https://blog.csdn.net/u011331731/article/details/108354605
 */
#ifndef STD_TRACE_H
#define STD_TRACE_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include "ringbuffer.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef USE_STDTRACE
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
#ifdef __cplusplus
}
#endif
#endif /* STD_TRACE_H */
