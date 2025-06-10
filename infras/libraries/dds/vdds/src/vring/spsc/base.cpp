/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vring/spsc/base.hpp"
#include "Std_Debug.h"
#include <assert.h>
#include <cinttypes>
#include <fcntl.h>
#include <unistd.h>

namespace as {
namespace vdds {
namespace vring {
namespace spsc {
/* ================================ [ MACROS    ] ============================================== */
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
  return VRING_SIZE_OF_META() + VRING_SIZE_OF_DESC(m_NumDesc) + VRING_SIZE_OF_AVAIL(m_NumDesc);
}

} // namespace spsc
} // namespace vring
} // namespace vdds
} // namespace as