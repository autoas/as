/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_EXCEPTION_H
#define ARA_CORE_EXCEPTION_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <exception>
#include "ara/core/error_code.h"
namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @brief Base type for all exception types defined by the Adaptive Platform. */
class Exception : public std::exception { /* @SWS_CORE_00601 */
public:
  /** @SWS_CORE_00615 @brief Move constructor from another instance. */
  Exception(Exception &&other) noexcept = default;

  /** @SWS_CORE_00616 @brief Move assignment operator from another instance. */
  Exception &operator=(Exception &&other) & noexcept = default;

  /** @SWS_CORE_00617 @brief Destructs the Exception object. */
  virtual ~Exception() noexcept override = default;

  /** @SWS_CORE_00611 @brief Construct a new Exception object with a specific ErrorCode. */
  explicit Exception(ErrorCode err) noexcept;

  /** @SWS_CORE_00613 @brief Return the embedded ErrorCode that was given to the constructor. */
  const ErrorCode &Error() const noexcept;

  /** @SWS_CORE_00612 @brief Return the explanatory string.
   * This function overrides the virtual function std::exception::what. All guarantees about the
   * lifetime of the returned pointer that are given for std::exception::what are preserved.
   */
  const char *what() const noexcept override;

protected:
  /** @SWS_CORE_00618 @brief Copy constructor from another instance. */
  Exception(const Exception &other) noexcept = default;

  /**@SWS_CORE_00614 @brief Copy assignment operator from another instance. */
  Exception &operator=(const Exception &other) noexcept = default;

private:
  ErrorCode m_errorCode;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_EXCEPTION_H */
