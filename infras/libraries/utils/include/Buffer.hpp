/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _BUFFER_HPP_
#define _BUFFER_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdlib.h>
#include <stdint.h>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
struct Buffer {
  Buffer(void *data, size_t size, size_t idx = 0) : data(data), size(size), idx(idx) {
  }

  Buffer(size_t size, size_t idx = 0) : size(size), idx(idx) {
    data = malloc(size);
    if (nullptr != data) {
      allocated = true;
    }
  }

  ~Buffer() {
    if (allocated and (nullptr != data)) {
      free(data);
    }
  }

  void *data;
  size_t size;
  size_t idx;

private:
  bool allocated = false;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* _BUFFER_HPP_ */
