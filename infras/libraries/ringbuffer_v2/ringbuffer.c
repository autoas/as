/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "ringbuffer.h"
#include "Std_Types.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  eRB_POLL,
  eRB_POP,
  eRB_DROP
} rb_action_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static rb_size_t RB_Action(const RingBufferType *rb, void *data, rb_size_t len,
                           rb_action_t action) {
  rb_size_t l = 0;
  rb_size_t doSz, used;
  rb_size_t num = rb->C->num;
  rb_size_t size = rb->C->size;
  rb_size_t in = rb->V->in & (num - 1);
  rb_size_t out = rb->V->out & (num - 1);
  uint8_t *src = (uint8_t *)rb->C->buffer;
  uint8_t *dst = (uint8_t *)data;

  used = rb->V->in - rb->V->out;
  if (len > used) {
    len = used;
  }
  for (l = 0; l < len;) {
    if (out < in) {
      doSz = in - out;
      if (doSz > (len - l)) {
        doSz = len - l;
      }
      if (action != eRB_DROP) {
        memcpy(dst, src + out * size, doSz * size);
        dst += doSz * size;
      }
      l += doSz;
    } else {
      doSz = num - out;
      if (doSz > (len - l)) {
        doSz = len - l;
      }
      if (action != eRB_DROP) {
        memcpy(dst, src + out * size, doSz * size);
        dst += doSz * size;
      }
      l += doSz;
      out = 0;
    }
  }

  if (action != eRB_POLL) {
    rb->V->out += l;
  }

  return l;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void RB_Init(const RingBufferType *rb) {
  rb->V->in = 0;
  rb->V->out = 0;
}

rb_size_t RB_Push(const RingBufferType *rb, void *data, rb_size_t len) {
  rb_size_t l = 0;
  rb_size_t doSz, used;
  rb_size_t num = rb->C->num;
  rb_size_t size = rb->C->size;
  rb_size_t in = rb->V->in & (num - 1);
  rb_size_t out = rb->V->out & (num - 1);
  uint8_t *src = (uint8_t *)data;
  uint8_t *dst = (uint8_t *)rb->C->buffer;

  /* in < out             out
   *                       v
   * | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
   *           ^
   *          in
   *  in > out             in
   *                       v
   * | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
   *           ^
   *          out
   */

  used = rb->V->in - rb->V->out;
  if ((used + len) <= num) {
    for (l = 0; l < len;) {
      if (in < out) {
        doSz = out - in;
        if (doSz > len) {
          doSz = len;
        }
        memcpy(dst + in * size, src, doSz * size);
        src += doSz * size;
        l += doSz;
      } else {
        doSz = num - in;
        if (doSz > len) {
          doSz = len;
        }
        memcpy(dst + in * size, src, doSz * size);
        src += doSz * size;
        l += doSz;
        in = 0;
      }
    }

    rb->V->in += l;
  } else {
    /* full, do nothing */
  }

  return l;
}

rb_size_t RB_Pop(const RingBufferType *rb, void *data, rb_size_t len) {
  return RB_Action(rb, data, len, eRB_POP);
}

rb_size_t RB_Poll(const RingBufferType *rb, void *data, rb_size_t len) {
  return RB_Action(rb, data, len, eRB_POLL);
}

rb_size_t RB_Drop(const RingBufferType *rb, rb_size_t len) {
  return RB_Action(rb, NULL, len, eRB_DROP);
}

void *RB_OutP(const RingBufferType *rb) {
  rb_size_t used;
  rb_size_t num = rb->C->num;
  rb_size_t size = rb->C->size;
  rb_size_t out = rb->V->out & (num - 1);
  uint8_t *dst = (uint8_t *)rb->C->buffer;

  used = rb->V->in - rb->V->out;
  if (used > 0) {
    dst = dst + out * size;
  } else {
    dst = NULL;
  }

  return dst;
}

void *RB_InP(const RingBufferType *rb) {
  rb_size_t used;
  rb_size_t num = rb->C->num;
  rb_size_t size = rb->C->size;
  rb_size_t in = rb->V->in & (num - 1);
  uint8_t *dst = (uint8_t *)rb->C->buffer;

  used = rb->V->in - rb->V->out;
  if (used < num) {
    dst = dst + in * size;
  } else {
    dst = NULL;
  }

  return dst;
}

rb_size_t RB_Left(const RingBufferType *rb) {
  rb_size_t left;

  left = rb->V->in - rb->V->out;

  left = rb->C->num - left;

  return left;
}

rb_size_t RB_Size(const RingBufferType *rb) {
  rb_size_t size;

  size = rb->V->in - rb->V->out;

  return size;
}
