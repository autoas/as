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
  rb_size_t doSz;
  rb_size_t in, out;
  rb_size_t max;
  char *buffer;

  buffer = rb->C->buffer;
  in = rb->V->in;
  out = rb->V->out;
  max = rb->C->max;

  len = len * rb->C->min;

  if (out == in) {
    /* already empty */
  } else {
    out++;
    if (out >= max) {
      out = 0;
    }

    if (out <= in) {
      l = in - out + 1;
      if (l > len) {
        l = len;
      }
      if (action != eRB_DROP)
        memcpy(data, &buffer[out], l);
      out += l - 1;

      if (action != eRB_POLL)
        rb->V->out = out;
    } else {
      doSz = max - out;
      if (doSz > len) {
        doSz = len;
      }
      if (action != eRB_DROP) {
        memcpy(data, &buffer[out], doSz);
      }
      out += doSz;
      l += doSz;
      len = len - doSz;

      if (out >= max) {
        out = 0;
      }

      if (len > 0) {
        doSz = out - in + 1;
        if (doSz > len) {
          doSz = len;
        }
        if (action != eRB_DROP) {
          memcpy(&((char *)data)[l], &buffer[out], doSz);
        }
        out += doSz;
        l += doSz;
      }

      if (action != eRB_POLL) {
        if (0 == out) {
          rb->V->out = max - 1;
        } else {
          rb->V->out = out - 1;
        }
      }
    }
  }

  return l / rb->C->min;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void RB_Init(const RingBufferType *rb) {
  rb->V->in = rb->C->max - 1;
  rb->V->out = rb->C->max - 1;
}

rb_size_t RB_Push(const RingBufferType *rb, void *data, rb_size_t len) {
  rb_size_t l = 0;
  rb_size_t doSz;
  rb_size_t in, out;
  rb_size_t min, max;
  char *buffer;

  buffer = rb->C->buffer;
  in = rb->V->in;
  out = rb->V->out;
  min = rb->C->min;
  max = rb->C->max;

  len = len * min;

  in++;
  if (in >= max) {
    in = 0;
  }

  if ((in + min - 1) == out) {
    /* full, do nothing */
  } else if (in <= out) {
    /* in < out
     *                                    out
     *                                    v
     * | min | min | min | min | min | min |
     *      ^
     *      in = min -1
     */
    l = out - in + 1;
    if (l > len) {
      l = len;
    } else {
      l = l - min;
    }
    if (data != NULL) {
      memcpy(&buffer[in], data, l);
    }
    in += l - 1;

    rb->V->in = in;
  } else {
    /* in > out
     *                              in = 5*min - 1
     *                              v
     * | min | min | min | min | min | min |
     *      ^
     *      out = min -1
     */
    doSz = max - in;
    if (doSz > len) {
      doSz = len;
    } else if (0 == out) {
      doSz -= min;
    } else {
    }
    if (data != NULL) {
      memcpy(&buffer[in], data, doSz);
    }
    in += doSz;
    l += doSz;
    len = len - doSz;

    if (in >= max) {
      in = 0;
    }

    if (len > 0) {
      doSz = out - in + 1;
      if (doSz > len) {
        doSz = len;
      } else {
        doSz = doSz - min;
      }
      if (data != NULL) {
        memcpy(&buffer[in], &((char *)data)[l], doSz);
      }
      in += doSz;
      l += doSz;
    }

    if (0 == in) {
      rb->V->in = max - 1;
    } else {
      rb->V->in = in - 1;
    }
  }

  return l / min;
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
  rb_size_t in, out;
  rb_size_t max;
  char *buffer;

  buffer = rb->C->buffer;
  in = rb->V->in;
  out = rb->V->out;
  max = rb->C->max;

  if (out == in) {
    buffer = NULL;
  } else {
    out++;
    if (out >= max) {
      out = 0;
    }

    buffer = &buffer[out];
  }

  return buffer;
}

void *RB_InP(const RingBufferType *rb) {
  rb_size_t in, out;
  rb_size_t min, max;
  char *buffer;

  buffer = rb->C->buffer;
  in = rb->V->in;
  out = rb->V->out;
  min = rb->C->min;
  max = rb->C->max;

  in++;
  if (in >= max) {
    in = 0;
  }

  if ((in + min - 1) == out) {
    buffer = NULL;
  } else {
    buffer = &buffer[in];
  }

  return buffer;
}

rb_size_t RB_Left(const RingBufferType *rb) {
  rb_size_t left;

  if (rb->V->in < rb->V->out) {
    left = rb->V->out - rb->V->in - rb->C->min;
  } else {
    left = rb->C->max - rb->V->in + rb->V->out - rb->C->min;
  }

  return left;
}

rb_size_t RB_Size(const RingBufferType *rb) {
  rb_size_t size;

  if (rb->V->in < rb->V->out) {
    size = rb->C->max - rb->V->out + rb->V->in;
  } else {
    size = rb->V->in - rb->V->out;
  }

  return size;
}
