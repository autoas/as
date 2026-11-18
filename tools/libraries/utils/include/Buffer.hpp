/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef BUFFER_HPP
#define BUFFER_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdlib.h>
#include <stdint.h>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
struct Buffer {
  Buffer(void *data, size_t size, size_t idx = 0) : Data(data), Size(size), Idx(idx) {
  }

  Buffer(size_t size, size_t idx = 0) : Size(size), Idx(idx) {
    Data = malloc(size);
    if (nullptr != Data) {
      m_IsAllocated = true;
    }
  }

  ~Buffer() {
    if (m_IsAllocated and (nullptr != Data)) {
      free(Data);
    }
  }

  void *Data;
  size_t Size;
  size_t Idx;

private:
  bool m_IsAllocated = false;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* BUFFER_HPP */
