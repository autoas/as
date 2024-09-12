/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
/* ================================ [ MACROS    ] ============================================== */
#ifndef RB_SIZE_TYPE
#define RB_SIZE_TYPE uint32_t
#endif
#define RB_DECLARE(name, type, size)                                                               \
  static type rbBuf_##name[size];                                                                  \
  static const RingBufferConstType rbC_##name = {(char *)rbBuf_##name, sizeof(rbBuf_##name),       \
                                                 sizeof(type)};                                    \
  static RingBufferVariantType rbV_##name = {sizeof(rbBuf_##name) - 1, sizeof(rbBuf_##name) - 1};  \
  const RingBufferType rb_##name = {&rbC_##name, &rbV_##name}

#define RB_EXTERN(name) extern RingBufferType rb_##name;

#define RB_PUSH(name, data, sz) RB_Push(&rb_##name, data, sz)
#define RB_POLL(name, data, sz) RB_Poll(&rb_##name, data, sz)
#define RB_DROP(name, sz) RB_Drop(&rb_##name, sz)
#define RB_POP(name, data, sz) RB_Pop(&rb_##name, data, sz)
#define RB_INIT(name) RB_Init(&rb_##name)
#define RB_LEFT(name) RB_Left(&rb_##name)
#define RB_SIZE(name) RB_Size(&rb_##name)
#define RB_INP(name) RB_InP(&rb_##name)
#define RB_OUTP(name) RB_OutP(&rb_##name)
#define IS_RB_EMPTY(name) ((rb_##name.V->in) == (rb_##name.V->out))
/*
 * If in  == out, empty. So must keep 1 min slot free not used if queue has data
 *
 * max = N * min
 * Init:
 *                                                                       in = max - 1
 *                                                                        v
 * | min | min | min | min | min | min | min | min | min | min | min | min |
 *                                                                        ^
 * First Push: len = min                                                  out
 *      in = min - 1
 *      v
 * | min | min | min | min | min | min | min | min | min | min | min | min |
 *                                                                        ^
 *                                    in = 6*min - 1                      out = max - 1
 *                                    v
 * | min | min | min | min | min | min | min | min | min | min | min | min |
 *            ^
 *            out = 2*min -1
 *
 *
 **/
/* ================================ [ TYPES     ] ============================================== */
typedef RB_SIZE_TYPE rb_size_t;

typedef struct {
  char *buffer;
  rb_size_t max;
  rb_size_t min;
} RingBufferConstType;

typedef struct {
  rb_size_t in;
  rb_size_t out;
} RingBufferVariantType;

typedef struct {
  const RingBufferConstType *C;
  RingBufferVariantType *V;
} RingBufferType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void RB_Init(const RingBufferType *rb);
rb_size_t RB_Push(const RingBufferType *rb, void *data, rb_size_t len);
rb_size_t RB_Pop(const RingBufferType *rb, void *data, rb_size_t len);
rb_size_t RB_Poll(const RingBufferType *rb, void *data, rb_size_t len);
rb_size_t RB_Drop(const RingBufferType *rb, rb_size_t len);
rb_size_t RB_Left(const RingBufferType *rb);
rb_size_t RB_Size(const RingBufferType *rb);
void *RB_OutP(const RingBufferType *rb);
void *RB_InP(const RingBufferType *rb);
#endif /* RING_BUFFER_H */
