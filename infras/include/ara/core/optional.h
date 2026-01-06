/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_OPTIONAL_H
#define ARA_CORE_OPTIONAL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <cstddef>
#include <cinttypes>

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @brief Implements std::optional (see [optional] in [11]). Unless explicitly overriden in the
 * member documentation, members always adhere in behavior to the ISO specification in [11]. */
template <typename T> class Optional final { /* @SWS_CORE_01033 */
public:
  using value_type = T; /* @SWS_CORE_01102 */

  /** @SWS_CORE_01105 @brief As per std::optional::optional(const optional& rhs) in [11] except for
   * the following deviations:
   *  1. Function is conditionally noexcept */
  constexpr Optional(const Optional &) noexcept(std::is_nothrow_copy_constructible<T>::value);

  /** @SWS_CORE_01106 @brief As per std::optional::optional(optional&& rhs) in [11] including
   * noexcept conditions */
  constexpr Optional(Optional &&) noexcept(std::is_nothrow_move_constructible<T>::value);

  /** @SWS_CORE_01110 @brief As per std::optional::optional(const optional<U>&) in [11] except for
   * the following deviations:
   * 1. Function is conditionally noexcept
   * This constructor is explicit if and only if is_convertible<const U&, T>::value is false*/
  template <typename U>
  explicit Optional(const Optional<U> &) noexcept(
    std::is_nothrow_constructible<T, const U &>::value);
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_OPTIONAL_H */
