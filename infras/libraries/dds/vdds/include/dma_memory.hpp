/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VDDS_DMA_MEMORY_HPP_
#define _VDDS_DMA_MEMORY_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <string>
#include <cinttypes>
#include "shared_memory.hpp"

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
#ifdef USE_DMA_BUF
class DmaMemory {
public:
  DmaMemory(std::string name, uint32_t size);
  DmaMemory(std::string name, uint64_t handle, uint32_t size);
  ~DmaMemory();

  int create();
  int release();

  uint64_t getHandle() {
    return m_Handle;
  }

  void *getVA() {
    return m_Addr;
  }

  uint32_t getSize() {
    return m_Size;
  }

private:
  int alloc();
  int remap();

private:
  std::string m_Name;
  uint64_t m_Handle = 0;
  uint32_t m_Size = 0;
  MemoryType_t m_Type;
  void *m_Addr = nullptr;
};
#else
using DmaMemory = SharedMemory;
#endif
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace vdds
} // namespace as
#endif /* _VDDS_DMA_MEMORY_HPP_ */
