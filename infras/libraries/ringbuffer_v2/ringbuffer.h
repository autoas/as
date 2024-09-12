/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef RING_BUFFER_V2_H
#define RING_BUFFER_V2_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
/* ================================ [ MACROS    ] ============================================== */
#ifndef RB_SIZE_TYPE
#define RB_SIZE_TYPE uint16_t
#endif
#define RB_DECLARE(name, type, size)                                                               \
  static type rbBuf_##name[size];                                                                  \
  static const RingBufferConstType rbC_##name = {                                                  \
    (void *)rbBuf_##name, sizeof(rbBuf_##name) / sizeof(rbBuf_##name[0]), sizeof(type)};           \
  static RingBufferVariantType rbV_##name = {0, 0};                                                \
  const RingBufferType rb_##name = {&rbC_##name, &rbV_##name}

#define RB_EXTERN(name) extern RingBufferType rb_##name;

#define RB_PUSH_FAST(api, type)                                                                    \
  static inline rb_size_t RB_Push##api(const RingBufferType *rb, type *typeV) {                    \
    rb_size_t used = rb->V->in - rb->V->out;                                                       \
    if (used < rb->C->num) {                                                                       \
      ((type *)rb->C->buffer)[rb->V->in & (rb->C->num - 1)] = *typeV;                              \
      rb->V->in++;                                                                                 \
      used = 1;                                                                                    \
    } else {                                                                                       \
      used = 0;                                                                                    \
    }                                                                                              \
    return used;                                                                                   \
  }

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
/* ================================ [ TYPES     ] ============================================== */
typedef RB_SIZE_TYPE rb_size_t;

typedef struct {
  char *buffer;
  uint16_t num;  /* number of elements, must be power of 2. */
  uint16_t size; /* element size */
} RingBufferConstType;

typedef struct {
  rb_size_t in;
  rb_size_t out;
} RingBufferVariantType;

typedef struct RingBuffer_s {
  const RingBufferConstType *C;
  RingBufferVariantType *V;
} RingBufferType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
RB_PUSH_FAST(Char, char)
RB_PUSH_FAST(U8, uint8_t)
RB_PUSH_FAST(U16, uint16_t)
RB_PUSH_FAST(U32, uint32_t)
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
#endif /* RING_BUFFER_V2_H */
