/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/spmc/base.hpp"
#include "Std_Debug.h"
#include <assert.h>
#include <cinttypes>
#include <fcntl.h>
#include <unistd.h>

namespace as {
namespace vdds {
namespace vring {
namespace spmc {
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
Base::Base(std::string name, uint32_t numDesc) : vring::Base(name, numDesc) {
}

Base::~Base() {
}

uint32_t Base::size() {
  return VRING_SIZE_OF_META() + VRING_SIZE_OF_DESC(m_NumDesc) + VRING_SIZE_OF_AVAIL(m_NumDesc) +
         VRING_SIZE_OF_ALL_USED(m_NumDesc);
}
} // namespace spmc
} // namespace vring
} // namespace vdds
} // namespace as