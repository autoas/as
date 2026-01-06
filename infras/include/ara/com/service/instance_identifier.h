/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_COM_SERVICE_INSTANCE_IDENTIFIER_HPP
#define ARA_COM_SERVICE_INSTANCE_IDENTIFIER_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include "ara/core/result.h"
#include "ara/core/string_view.h"
#include <string>

namespace ara {
namespace com {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
class InstanceIdentifier { /* @SWS_CM_00302 */
public:
  /** @SWS_CM_00056 */
  explicit InstanceIdentifier(const InstanceIdentifier &other) noexcept
    : m_serializedFormat(other.m_serializedFormat) {
  }

  /** @SWS_CM_11525 */
  InstanceIdentifier &operator=(const InstanceIdentifier &other) noexcept {
    m_serializedFormat = other.m_serializedFormat;
    return *this;
  }

  /** @SWS_CM_00054 */
  InstanceIdentifier &operator=(InstanceIdentifier &&other) noexcept {
    m_serializedFormat = other.m_serializedFormat;
    return *this;
  }

  /** @SWS_CM_00055 */
  ~InstanceIdentifier() noexcept {
  }

  /** @SWS_CM_11521 */
  explicit InstanceIdentifier(ara::core::StringView serializedFormat)
    : m_serializedFormat(serializedFormat.data(), serializedFormat.size()) {
  }

  /** @SWS_CM_00053 */
  explicit InstanceIdentifier(const InstanceIdentifier &&other) noexcept
    : m_serializedFormat(other.m_serializedFormat) {
  }

  /** @SWS_CM_11520 */
  static InstanceIdentifier Create(ara::core::StringView serializedFormat) noexcept {
    return InstanceIdentifier(serializedFormat);
  }

  /** @SWS_CM_11524 */
  bool operator<(const InstanceIdentifier &other) const noexcept {
    return m_serializedFormat < other.m_serializedFormat;
  }

  /** @SWS_CM_11523 */
  bool operator==(const InstanceIdentifier &other) const noexcept {
    return m_serializedFormat == other.m_serializedFormat;
  }

  /** @SWS_CM_11522 */
  ara::core::StringView toString() const noexcept {
    return ara::core::StringView(m_serializedFormat.data(), m_serializedFormat.size());
  }

private:
  std::string m_serializedFormat;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace com
} // namespace ara
#endif /* ARA_COM_SERVICE_INSTANCE_IDENTIFIER_HPP */
