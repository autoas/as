/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_SPSC_READER_HPP_
#define _VRING_SPSC_READER_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <array>
#include <chrono>
#include <cstring>
#include <errno.h>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include "vring/spsc/base.hpp"

namespace as {
namespace vdds {
namespace vring {
namespace spsc {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class Reader : public Base {
public:
  Reader(std::string name, uint32_t numDesc = 8);
  ~Reader();

  int init();

  /* get an buffer with data from the used ring
   * Positive errors: ETIMEDOUT, ENOMSG */
  int get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs = 1000);

  /* put the buffer back to the avaiable ring */
  int put(uint32_t idx);

private:
  void threadMain();

  std::shared_ptr<NamedSemaphore> m_SemUsed = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace spsc
} // namespace vring
} // namespace vdds
} // namespace as
#endif /* _VRING_SPSC_READER_HPP_ */
