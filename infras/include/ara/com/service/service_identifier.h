/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */

#ifndef ARA_COM_SERVICE_SERVICE_IDENTIFIER_HPP
#define ARA_COM_SERVICE_SERVICE_IDENTIFIER_HPP

/* ================================ [ INCLUDES  ] ============================================== */
#include <cstdint>
#include "ara/core/string_view.h"

namespace ara {
namespace com {

/* ================================ [ CLASS    ] ============================================== */
/**
 * @SWS_CM_11510
 * Identifies a service by its service ID.
 */
class ServiceIdentifierType {
public:
  /** @SWS_CM_11513 */
  ServiceIdentifierType &operator=(const ServiceIdentifierType &other);

  constexpr explicit ServiceIdentifierType(uint16_t serviceId) noexcept : m_serviceId(serviceId) {
  }

  /** @SWS_CM_11512 */
  bool operator<(const ServiceIdentifierType &other) const {
    return m_serviceId < other.m_serviceId;
  }

  /** @SWS_CM_11511 */
  bool operator==(const ServiceIdentifierType &other) const {
    return m_serviceId == other.m_serviceId;
  }

  /** @SWS_CM_11514 */
  ara::core::StringView toString() const;

private:
  uint16_t m_serviceId;
};

/* ================================ [ FUNCTIONS ] ============================================== */

} // namespace com
} // namespace ara

#endif /* ARA_COM_SERVICE_SERVICE_IDENTIFIER_HPP */
