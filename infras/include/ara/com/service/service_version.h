/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
#ifndef ARA_COM_SERVICE_SERVICE_VERSION_HPP
#define ARA_COM_SERVICE_SERVICE_VERSION_HPP

/* ================================ [ INCLUDES ] ============================================== */
#include <cstdint>

#include "ara/core/string_view.h"

namespace ara {
namespace com {

/* ================================ [ TYPES ] ============================================== */

/** @SWS_CM_11515 */
class ServiceVersionType {
public:
  /** @SWS_CM_11518 */
  ServiceVersionType &operator=(const ServiceVersionType &other) {
    m_version = other.m_version;
  }

  /** @SWS_CM_11519 */
  ara::core::StringView ToString() const;

  /** @SWS_CM_11517 */
  bool operator<(const ServiceVersionType &other) const {
    return m_version < other.m_version;
  }

  /** @SWS_CM_11516 */
  bool operator==(const ServiceVersionType &other) const {
    return m_version == other.m_version;
  }

  constexpr ServiceVersionType(uint32_t version) noexcept : m_version(version) {
  }

private:
  uint32_t m_version;
};

} // namespace com
} // namespace ara

#endif /* ARA_COM_SERVICE_SERVICE_VERSION_HPP */
