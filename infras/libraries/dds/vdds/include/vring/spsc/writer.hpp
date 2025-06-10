/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef _VRING_SPSC_WRITER_HPP_
#define _VRING_SPSC_WRITER_HPP_
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
/*The Virtio Ring Writer*/
class Writer : public Base {
public:
  Writer(std::string name, uint32_t msgSize = 256 * 1024, uint32_t numDesc = 8);
  ~Writer();

  int init();

  /* get an avaiable buffer from the avaiable ring
   * Positive errors: ETIMEDOUT, ENODATA
   */
  int get(void *&buf, uint32_t &idx, uint32_t &len, uint32_t timeoutMs = 1000);

  /* put the avaiable buffer to the used ring */
  int put(uint32_t idx, uint32_t len);

  /* drop the avaiable buffer to the avaiable ring */
  int drop(uint32_t idx);

private:
  int setup();
  void resetIfNeed();
  void readerHeartCheck();
  void threadMain();

private:
  uint32_t m_MsgSize; /* the size for each message */
  bool m_Reset = false;

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
#endif /* _VRING_SPSC_WRITER_HPP_ */
