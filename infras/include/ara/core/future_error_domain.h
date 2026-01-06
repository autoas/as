/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_FUTURE_ERROR_DOMAIN_H
#define ARA_CORE_FUTURE_ERROR_DOMAIN_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ara/core/error_code.h"
#include "ara/core/exception.h"
#include <chrono>
#include <future>

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @SWS_CORE_00400 */
enum class FutureErrc : ara::core::ErrorDomain::CodeType {
  kBrokenPromise = 101,
  kNoState = 104,
};

/** @SWS_CORE_00411 */
class FutureException : public Exception {
public:
  /** @SWS_CORE_00412 */
  explicit FutureException(ErrorCode err) noexcept;
};

/** @SWS_CORE_00421 */
class FutureErrorDomain final : public ErrorDomain {
public:
  /** @SWS_CORE_00431 */
  using Errc = FutureErrc;
  /** @SWS_CORE_00432 */
  using Exception = FutureException;

  /** @SWS_CORE_00441 */
  constexpr FutureErrorDomain() noexcept;

  /** @SWS_CORE_00443 */
  const char *Message(ErrorDomain::CodeType errorCode) const noexcept override;

  /** @SWS_CORE_00442 */
  const char *Name() const noexcept override;

  /** @SWS_CORE_00444 */
  void ThrowAsException(const ErrorCode &errorCode) const noexcept(false) override;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/** @SWS_CORE_00480 */
constexpr const ErrorDomain &GetFutureErrorDomain() noexcept;

/** @SWS_CORE_00490 */
constexpr ErrorCode MakeErrorCode(FutureErrc code, ErrorDomain::SupportDataType data) noexcept;
} // namespace core
} // namespace ara
#endif /* ARA_CORE_FUTURE_ERROR_DOMAIN_H */
