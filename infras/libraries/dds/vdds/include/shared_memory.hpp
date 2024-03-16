/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VDDS_SHARED_MEMORY_HPP_
#define _VDDS_SHARED_MEMORY_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <string>
#include <cinttypes>

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  MEMORY_TYPE_ALLOC,
  MEMORY_TYPE_MMAP
} MemoryType_t;

class SharedMemory {
public:
  /* This constructor will create a shared memory */
  SharedMemory(std::string name, uint32_t size);

  /* This constructor will open an existed shared memory, handle can be any value, the handle is
   * just there as to be API compatible with DmaMemory */
  SharedMemory(std::string name, uint64_t handle, uint32_t size);

  ~SharedMemory();

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
  std::string m_Name;
  uint64_t m_Handle = 0;
  uint32_t m_Size = 0;
  MemoryType_t m_Type;
  void *m_Addr = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace vdds
} // namespace as
#endif /* _VDDS_SHARED_MEMORY_HPP_ */
