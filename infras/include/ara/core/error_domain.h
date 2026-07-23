/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_ERROR_DOMAIN_H
#define ARA_CORE_ERROR_DOMAIN_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <cstddef>
#include <cinttypes>
#include <cstdint>
#include "ara/core/core_fwd.h"
namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @brief Encapsulation of an error domain.
 * An error domain is the controlling entity for ErrorCode's error code values, and defines the
 * mapping of such error code values to textual representations.
 * This type constitutes a base class for error domain implementations.
 * This class is a literal type, and subclasses are strongly advised to be literal types as well.
 */
class ErrorDomain { /* @SWS_CORE_00110 */
public:
  /* @SWS_CORE_00122 @brief Error code type. */
  using CodeType = std::int32_t;

  /* @SWS_CORE_00121 @brief Domain identifier type. */
  using IdType = std::uint64_t;

  /* @SWS_CORE_00123 @brief Support data type. */
  using SupportDataType = std::uint64_t;

  /* @SWS_CORE_00131 @brief Copy construction shall be disabled. */
  ErrorDomain(const ErrorDomain &) = delete;

  /* @SWS_CORE_00132 @brief Move construction shall be disabled */
  ErrorDomain(ErrorDomain &&) = delete;

  /* @SWS_CORE_00134 @brief Move assignment shall be disabled. */
  ErrorDomain &operator=(ErrorDomain &&other) = delete;

  /* @SWS_CORE_00133 @brief Copy assignment shall be disabled. */
  ErrorDomain &operator=(const ErrorDomain &other) = delete;

  /* @SWS_CORE_00151 @brief Return the unique domain identifier. */
  constexpr IdType Id() const noexcept {
    return m_id;
  }

  /** @SWS_CORE_00153 @brief Return a textual representation of the given error code.
   * The returned pointer remains owned by the ErrorDomain subclass and shall not be freed by
   * clients
   */
  virtual const char *Message(CodeType errorCode) const noexcept = 0;

  /** @SWS_CORE_00152 @brief Return the name of this error domain.
   * The returned pointer remains owned by class ErrorDomain and shall not be freed by clients.
   */
  virtual const char *Name() const noexcept = 0;

  /** @SWS_CORE_00154 @brief Throw the given error as exception.
   * This function will determine the appropriate exception type for the given ErrorCode according
   *to [SWS_CORE_10953] and throw it. The thrown exception will contain the given ErrorCode. As per
   *[SWS_CORE_10304], this function does not participate in overload resolution when C++ exceptions
   *are disabled in the compiler toolchain.
   */
  virtual void ThrowAsException(const ErrorCode &errorCode) const noexcept(false) = 0;

  /* @SWS_CORE_00138 @brief Compare for non-equality with another ErrorDomain instance */
  constexpr bool operator!=(const ErrorDomain &other) const noexcept {
    return m_id != other.m_id;
  }

  /** @SWS_CORE_00137 @brief @brief Compare for equality with another ErrorDomain instance.
   * Two ErrorDomain instances compare equal when their identifiers (returned by Id()) are equal */
  constexpr bool operator==(const ErrorDomain &other) const noexcept {
    return m_id == other.m_id;
  }

protected:
  /* @SWS_CORE_00136 @brief Destructor shall be protected default. */
  ~ErrorDomain() noexcept = default;
  /* @SWS_CORE_00135 @brief Constructor shall be explicit with domain identifier. */
  explicit constexpr ErrorDomain(IdType id) noexcept : m_id(id){};

protected:
  IdType m_id;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_ERROR_DOMAIN_H */
