/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_ERROR_CODE_H
#define ARA_CORE_ERROR_CODE_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <cstddef>
#include <cinttypes>
#include <cstdint>
#include <string>
#include "ara/core/error_domain.h"
#include "ara/core/string_view.h"
namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @brief Encapsulation of an error code.
 *An ErrorCode contains a raw error code value and an error domain. The raw error code value is
 *specific to this error domain. */
class ErrorCode final { /* @SWS_CORE_00501 */
public:
  /** @SWS_CORE_00512 @brief Construct a new ErrorCode instance with parameters.
   * This constructor does not participate in overload resolution unless EnumT is an enum type. */
  template <typename EnumT>
  constexpr ErrorCode(EnumT e,
                      ErrorDomain::SupportDataType data = ErrorDomain::SupportDataType()) noexcept
    : m_code(static_cast<ErrorDomain::CodeType>(e)), m_domain(&m_domainDummy), m_supportData(data) {
  }

  /** @SWS_CORE_00513 @brief Construct a new ErrorCode instance with parameters. */
  constexpr ErrorCode(ErrorDomain::CodeType value, const ErrorDomain &domain,
                      ErrorDomain::SupportDataType data = ErrorDomain::SupportDataType()) noexcept
    : m_code(value), m_domain(&domain), m_supportData(data) {
  }

  /** @SWS_CORE_00515 @brief Return the domain with which this ErrorCode is associated. */
  constexpr const ErrorDomain &Domain() const noexcept {
    return *m_domain;
  }

  /** @SWS_CORE_00518 @brief Return a textual representation of this ErrorCode. */
  StringView Message() const noexcept;

  /** @SWS_CORE_00516 @brief Return the supplementary error context data.
   * The underlying type and the meaning of the returned value are implementation-defined.
   */
  constexpr ErrorDomain::SupportDataType SupportData() const noexcept {
    return m_supportData;
  }

  /** @SWS_CORE_00519 @brief Throw this error as exception.
   * This function will determine the appropriate exception type for this ErrorCode and throw it.
   * The thrown exception will contain this ErrorCode. Behaves as if this->Domain().ThrowAs
   * Exception(*this).
   * This function shall not participate in overload resolution when C++ exceptions are disabled in
   * the compiler toolchain.
   */
  void ThrowAsException() const noexcept(false);

  /** @SWS_CORE_00514 @brief Return the raw error code value. */
  constexpr ErrorDomain::CodeType Value() const noexcept {
    return m_code;
  }

private:
  class ErrorDomainDummy : public ErrorDomain {
  public:
    explicit constexpr ErrorDomainDummy() : ErrorDomain(UINT64_MAX) {
    }

    const char *Message(CodeType errorCode) const noexcept {
      return nullptr;
    }
    const char *Name() const noexcept {
      return nullptr;
    }
    void ThrowAsException(const ErrorCode &errorCode) const noexcept(false) {
    }

    ErrorDomainDummy &operator=(ErrorDomainDummy &&other) {
      m_id = other.m_id;
      return *this;
    }
  };

private:
  ErrorDomain::CodeType m_code;
  ErrorDomainDummy m_domainDummy;
  const ErrorDomain *m_domain;
  ErrorDomain::SupportDataType m_supportData;
};

/* @SWS_CORE_00572 */
constexpr bool operator!=(const ErrorCode &lhs, const ErrorCode &rhs) noexcept;

/* @SWS_CORE_00571 */
constexpr bool operator==(const ErrorCode &lhs, const ErrorCode &rhs) noexcept;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_ERROR_CODE_H */
