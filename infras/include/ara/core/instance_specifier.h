/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_INSTANCE_SPECIFIER_H
#define ARA_CORE_INSTANCE_SPECIFIER_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ara/core/string_view.h"
#include "ara/core/result.h"

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
class InstanceSpecifier final { /** @SWS_CORE_08001 */
public:
  /** @SWS_CORE_08023 */
  InstanceSpecifier(InstanceSpecifier &&other) noexcept
    : m_qualifiedShortName(other.m_qualifiedShortName) {
  }

  /** @SWS_CORE_08022 */
  InstanceSpecifier(const InstanceSpecifier &other) noexcept
    : m_qualifiedShortName(other.m_qualifiedShortName) {
  }

  /** @SWS_CORE_08024 */
  InstanceSpecifier &operator=(const InstanceSpecifier &other) noexcept {
    m_qualifiedShortName = other.m_qualifiedShortName;
    return *this;
  }

  /** @SWS_CORE_08025 */
  InstanceSpecifier &operator=(InstanceSpecifier &&other) noexcept {
    m_qualifiedShortName = other.m_qualifiedShortName;
    return *this;
  }

  /** @SWS_CORE_08029 */
  ~InstanceSpecifier() noexcept {
  }

  /** @SWS_CORE_08021 */
  explicit InstanceSpecifier(ara::core::StringView qualifiedShortName) noexcept(false)
    : m_qualifiedShortName(qualifiedShortName.data(), qualifiedShortName.size()) {
  }

  /** @SWS_CORE_08041 */
  ara::core::StringView ToString() const noexcept {
    return ara::core::StringView(m_qualifiedShortName.data(), m_qualifiedShortName.size());
  }

  /** @SWS_CORE_08045 */
  bool operator!=(ara::core::StringView other) const noexcept {
    return m_qualifiedShortName != std::string(other.data(), other.size());
  }

  /** @SWS_CORE_08044 */
  bool operator!=(const InstanceSpecifier &other) const noexcept {
    return m_qualifiedShortName != other.m_qualifiedShortName;
  }

  /** @SWS_CORE_08046 */
  bool operator<(const InstanceSpecifier &other) const noexcept {
    return m_qualifiedShortName < other.m_qualifiedShortName;
  }

  /** @SWS_CORE_08042 */
  bool operator==(const InstanceSpecifier &other) const noexcept {
    return m_qualifiedShortName == other.m_qualifiedShortName;
  }

  /** @SWS_CORE_08043 */
  bool operator==(ara::core::StringView other) const noexcept {
    return m_qualifiedShortName == std::string(other.data(), other.size());
  }

  /** @SWS_CORE_08032 */
  static ara::core::Result<InstanceSpecifier>
  Create(ara::core::StringView qualifiedShortName) noexcept {
    return ara::core::Result<InstanceSpecifier>(InstanceSpecifier(qualifiedShortName));
  }

private:
  std::string m_qualifiedShortName;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_INSTANCE_SPECIFIER_H */
