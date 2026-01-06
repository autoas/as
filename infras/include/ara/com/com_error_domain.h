/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_COM_ERROR_DOMAIN_HPP
#define ARA_COM_ERROR_DOMAIN_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include "ara/core/error_domain.h"
#include "ara/core/exception.h"

namespace ara {
namespace com {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/** @SWS_CM_10432 */
enum class ComErrc : ara::core::ErrorDomain::CodeType {
  kServiceNotAvailable = 1,
  kMaxSamplesExceeded = 2,
  kNetworkBindingFailure = 3,
  kFieldValueNotInitialized = 6,
  kFieldSetHandlerNotSet = 7,
  kServiceNotOffered = 11,
  kInstanceIDNotResolvable = 15,
  kMaxSampleCountNotRealizable = 16,
  kUnknownApplicationError = 22,
};

/** @SWS_CM_11327 */
class ComException final : public ara::core::Exception {
public:
  /** @SWS_CM_11328  */
  explicit ComException(ara::core::ErrorCode errorCode) noexcept;
};

/** @SWS_CM_11329 */
class ComErrorDomain final : public ara::core::ErrorDomain {
public:
  /** @SWS_CM_11336 */
  using Errc = ComErrc;

  /** @SWS_CM_11337 */
  using Exception = ComException;

  /** @SWS_CM_11330 */
  ComErrorDomain() = delete;

  /** @SWS_CM_11332 */
  const char *Message(CodeType errorCode) const noexcept override;

  /** @SWS_CM_11331 */
  const char *Name() const noexcept override;

  /** @SWS_CM_11333 */
  void ThrowAsException(const ara::core::ErrorCode &errorCode) const noexcept(false) override;
};
/* ================================ [ CLASS    ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace com
} // namespace ara
#endif /* ARA_COM_ERROR_DOMAIN_HPP */
