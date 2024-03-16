/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/mman.h>
#include <fcntl.h>

#include <fcntl.h>
#include <errno.h>

#include <unistd.h>
#include <sys/mman.h>

#include "shared_memory.hpp"
#include "Std_Debug.h"

namespace as {
namespace vdds {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_MEMORY 0
#define AS_LOG_MEMORYI 2
#define AS_LOG_MEMORYE 3
/* ================================ [ TYPES     ] ============================================== */
SharedMemory::SharedMemory(std::string name, uint32_t size)
  : m_Name(name), m_Size(size), m_Type(MEMORY_TYPE_ALLOC) {
}

SharedMemory::SharedMemory(std::string name, uint64_t handle, uint32_t size)
  : m_Name(name), m_Handle(handle), m_Size(size), m_Type(MEMORY_TYPE_MMAP) {
}

int SharedMemory::create() {
  int ret = 0;
  int fd;
  void *addr;
  int oflag = O_RDWR;

  if (MEMORY_TYPE_ALLOC == m_Type) {
    oflag |= O_CREAT | O_EXCL;
  }

  fd = shm_open(m_Name.c_str(), oflag, 0600);
  if (fd > 0) {
#if defined(linux)
    if (oflag & O_CREAT) {
      ret = ftruncate(fd, m_Size);
    }
#endif
    if (0 == ret) {
      addr = mmap(NULL, m_Size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (nullptr != addr) {
        m_Addr = addr;
        m_Handle = fd;
      } else {
        ASLOG(MEMORYE, ("memory %s mmap failed\n", m_Name.c_str()));
      }
    } else {
      ASLOG(MEMORYE, ("memory %s reserve size %" PRIu64 " failed\n", m_Name.c_str(), m_Size));
      ret = ENOMEM;
    }
  } else {
    ASLOG(MEMORYE, ("memory %s open failed\n", m_Name.c_str()));
    ret = EEXIST;
  }
  return ret;
}

int SharedMemory::release() {
  int ret = 0;

  if (nullptr != m_Addr) {
    ret |= munmap(m_Addr, m_Size);
  }

#if defined(linux)
  if (m_Handle > 0) {
    ret |= close(m_Handle);
  }
#endif

  if (MEMORY_TYPE_ALLOC == m_Type) {
    if (m_Handle > 0) {
      ret |= shm_unlink(m_Name.c_str());
    }
  }

  m_Addr = nullptr;
  m_Handle = 0;

  if (0 != ret) {
    ASLOG(MEMORYE, ("memory %s release failed %d\n", m_Name.c_str(), ret));
  }

  return ret;
}

SharedMemory::~SharedMemory() {
  release();
}

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace vdds
} // namespace as
