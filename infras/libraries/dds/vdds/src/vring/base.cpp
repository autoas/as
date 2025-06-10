/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/base.hpp"
#include "Std_Debug.h"
#include <assert.h>
#include <cinttypes>
#include <fcntl.h>
#include <unistd.h>

namespace as {
namespace vdds {
namespace vring {
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_VRING 0
#define AS_LOG_VRINGI 1
#define AS_LOG_VRINGW 2
#define AS_LOG_VRINGE 3

#ifndef VRING_SPIN_MAX_COUNTER
#define VRING_SPIN_MAX_COUNTER (1000000)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
std::string Base::replace(std::string resource_str, std::string sub_str, std::string new_str) {
  std::string dst_str = resource_str;
  std::string::size_type pos = 0;
  while ((pos = dst_str.find(sub_str)) != std::string::npos) {
    dst_str.replace(pos, sub_str.length(), new_str);
  }
  return dst_str;
}

std::string Base::toAsName(std::string name) {
  std::string fname = "as" + replace(name, "/", "_");
  return fname;
}

Base::Base(std::string name, uint32_t numDesc) : m_Name(toAsName(name)), m_NumDesc(numDesc) {
  m_DmaMems.reserve(numDesc);
}

Base::~Base() {
}

uint64_t Base::timestamp() {
  uint64_t tsp = 0;
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  tsp = (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);

  return tsp;
}

/* https://rigtorp.se/spinlock/ */
int Base::spinLock(int32_t *pLock) {
  int ret = 0;
  volatile uint64_t timeout = 0;
  /* NOTE: if the others do spinLock and then crashed, that's a diaster to the things left */
  while (__atomic_exchange_n(pLock, 1, __ATOMIC_ACQUIRE) && (0 == ret)) {
    timeout = 0;
    while (__atomic_load_n(pLock, __ATOMIC_RELAXED) && (0 == ret)) {
      timeout++;
      if (timeout > VRING_SPIN_MAX_COUNTER) {
        ret = EDEADLK;
        ASLOG(VRINGE, ("vring %s spin lock %p dead\n", m_Name.c_str(), pLock));
      }
    }
  }
  return ret;
}

void Base::spinUnlock(int32_t *pLock) {
  __atomic_store_n(pLock, 0, __ATOMIC_RELEASE);
}
} // namespace vring
} // namespace vdds
} // namespace as