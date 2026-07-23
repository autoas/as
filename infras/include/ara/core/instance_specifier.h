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
  /** @SWS_CORE_08023 @brief Move constructor. */
  InstanceSpecifier(InstanceSpecifier &&other) noexcept
    : m_qualifiedShortName(other.m_qualifiedShortName) {
  }

  /** @SWS_CORE_08022 @brief Copy constructor. */
  InstanceSpecifier(const InstanceSpecifier &other) noexcept
    : m_qualifiedShortName(other.m_qualifiedShortName) {
  }

  /** @SWS_CORE_08024 @brief Copy assignment. */
  InstanceSpecifier &operator=(const InstanceSpecifier &other) noexcept {
    m_qualifiedShortName = other.m_qualifiedShortName;
    return *this;
  }

  /** @SWS_CORE_08025 @brief Move assignment. */
  InstanceSpecifier &operator=(InstanceSpecifier &&other) noexcept {
    m_qualifiedShortName = other.m_qualifiedShortName;
    return *this;
  }

  /** @SWS_CORE_08029 @brief Destructor. */
  ~InstanceSpecifier() noexcept {
  }

  /** @SWS_CORE_08021 @brief Constructor from StringView. */
  explicit InstanceSpecifier(ara::core::StringView qualifiedShortName) noexcept(false)
    : m_qualifiedShortName(qualifiedShortName.data(), qualifiedShortName.size()) {
  }

  /** @SWS_CORE_08041 @brief Convert to StringView. */
  ara::core::StringView ToString() const noexcept {
    return ara::core::StringView(m_qualifiedShortName.data(), m_qualifiedShortName.size());
  }

  /** @SWS_CORE_08045 @brief Compare for non-equality with StringView. */
  bool operator!=(ara::core::StringView other) const noexcept {
    return m_qualifiedShortName != std::string(other.data(), other.size());
  }

  /** @SWS_CORE_08044 @brief Compare for non-equality with another InstanceSpecifier. */
  bool operator!=(const InstanceSpecifier &other) const noexcept {
    return m_qualifiedShortName != other.m_qualifiedShortName;
  }

  /** @SWS_CORE_08046 @brief Compare for less than. */
  bool operator<(const InstanceSpecifier &other) const noexcept {
    return m_qualifiedShortName < other.m_qualifiedShortName;
  }

  /** @SWS_CORE_08042 @brief Compare for equality with another InstanceSpecifier. */
  bool operator==(const InstanceSpecifier &other) const noexcept {
    return m_qualifiedShortName == other.m_qualifiedShortName;
  }

  /** @SWS_CORE_08043 @brief Compare for equality with StringView. */
  bool operator==(ara::core::StringView other) const noexcept {
    return m_qualifiedShortName == std::string(other.data(), other.size());
  }

  /** @SWS_CORE_08032 @brief Create InstanceSpecifier from StringView. */
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
