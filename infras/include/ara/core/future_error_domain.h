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
/** @SWS_CORE_00400 @brief Future error codes enumeration. */
enum class FutureErrc : ara::core::ErrorDomain::CodeType {
  kBrokenPromise = 101,
  kNoState = 104,
};

/** @SWS_CORE_00411 @brief Exception class for future errors. */
class FutureException : public Exception {
public:
  /** @SWS_CORE_00412 @brief Constructor with ErrorCode. */
  explicit FutureException(ErrorCode err) noexcept : Exception(err) {}
};

/** @SWS_CORE_00421 @brief Error domain for future errors. */
class FutureErrorDomain final : public ErrorDomain {
public:
  /** @SWS_CORE_00431 @brief Error code type alias. */
  using Errc = FutureErrc;
  /** @SWS_CORE_00432 @brief Exception type alias. */
  using Exception = FutureException;

  /** @SWS_CORE_00441 @brief Constructor. */
  constexpr FutureErrorDomain() noexcept : ErrorDomain(0x8000000000000001ULL) {}

  /** @SWS_CORE_00443 @brief Return textual representation of error code. */
  const char *Message(ErrorDomain::CodeType errorCode) const noexcept override {
    switch (static_cast<FutureErrc>(errorCode)) {
    case FutureErrc::kBrokenPromise:
      return "broken promise";
    case FutureErrc::kNoState:
      return "no state";
    default:
      return "unknown future error";
    }
  }

  /** @SWS_CORE_00442 @brief Return domain name. */
  const char *Name() const noexcept override {
    return "FutureErrorDomain";
  }

  /** @SWS_CORE_00444 @brief Throw error as exception. */
  void ThrowAsException(const ErrorCode &errorCode) const noexcept(false) override {
    throw FutureException(errorCode);
  }
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/** @SWS_CORE_00480 @brief Return reference to FutureErrorDomain singleton. */
inline const ErrorDomain &GetFutureErrorDomain() noexcept {
  static const FutureErrorDomain instance;
  return instance;
}

/** @SWS_CORE_00490 @brief Create ErrorCode from FutureErrc. */
inline ErrorCode MakeErrorCode(FutureErrc code, ErrorDomain::SupportDataType data) noexcept {
  return ErrorCode(static_cast<ErrorDomain::CodeType>(code), GetFutureErrorDomain(), data);
}
} // namespace core
} // namespace ara
#endif /* ARA_CORE_FUTURE_ERROR_DOMAIN_H */
