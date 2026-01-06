/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_RESULT_H
#define ARA_CORE_RESULT_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <cstdint>
#include <cstddef>
#include <utility>
#include <stdexcept>
#include <optional>
#include <type_traits>

#include "ara/core/error_code.h"
#include "ara/core/optional.h"

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @brief This class is a type that contains either a value or an error. */
template <typename T, typename E = ErrorCode> class Result final { /* @SWS_CORE_00701 */
public:
  /** @SWS_CORE_00712 @brief Type alias for the error type. */
  using error_type = E;

  /** @SWS_CORE_00711 @brief Type alias for the value type. */
  using value_type = T;

  /** @SWS_CORE_00726 @brief Constructs a Result containing a value. */
  Result(Result &&other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                  std::is_nothrow_move_constructible<E>::value)
    : m_hasValue(other.m_hasValue),
      m_value(std::move(other.m_value)),
      m_error(other.m_error.Value()) {
    if (m_hasValue) {
      other.m_hasValue = false;
    }
  }

  /** @SWS_CORE_00725 @brief Copy-construct a new Result from another instance. */
  Result(const Result &other) noexcept(std::is_nothrow_copy_constructible<T>::value &&
                                       std::is_nothrow_copy_constructible<E>::value)
    : m_hasValue(other.m_hasValue),
      m_value(std::move(other.m_value)),
      m_error(other.m_error.Value()) {
  }

  /** @SWS_CORE_00742 @brief Move-assign another Result to this instance. */
  Result &operator=(Result &&other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                             std::is_nothrow_move_assignable<T>::value &&
                                             std::is_nothrow_move_constructible<E>::value &&
                                             std::is_nothrow_move_assignable<E>::value) {
    if (this != &other) {
      m_hasValue = other.m_hasValue;
      m_value = std::move(other.m_value);
      m_error = other.m_error.Value();
      if (m_hasValue) {
        other.m_hasValue = false;
      }
    }
    return *this;
  }

  /** @SWS_CORE_00741 @brief Copy-assign another Result to this instance. */
  Result &operator=(const Result &other) noexcept(std::is_nothrow_copy_constructible<T>::value &&
                                                  std::is_nothrow_copy_assignable<T>::value &&
                                                  std::is_nothrow_copy_constructible<E>::value &&
                                                  std::is_nothrow_copy_assignable<E>::value);

  /** @SWS_CORE_00727 */
  ~Result() noexcept {
  }

  /** @SWS_CORE_00724 @brief Construct a new Result from the specified error (given as rvalue). */
  explicit Result(E &&e) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_hasValue(false), m_value(), m_error(e.Value()) {
  }

  /** @SWS_CORE_00722 @brief Construct a new Result from the specified value (given as rvalue). */
  Result(T &&t) noexcept(std::is_nothrow_move_constructible<T>::value)
    : m_hasValue(true), m_value(std::move(t)), m_error(0) {
  }

  /** @SWS_CORE_00721 @brief Construct a new Result from the specified value (given as lvalue). */
  Result(const T &t) noexcept(std::is_nothrow_copy_constructible<T>::value)
    : m_hasValue(true), m_value(t), m_error(0) {
  }

  /** @SWS_CORE_00723 @brief Construct a new Result from the specified error (given as lvalue). */
  explicit Result(const E &e) noexcept(std::is_nothrow_copy_constructible<E>::value)
    : m_hasValue(false), m_value(), m_error(e.Value()) {
  }

  /** @SWS_CORE_00768 @brief Apply the given Callable to the value of this instance, and return a
   * new Result with the result of the call. */
  template <typename F> auto Bind(F &&f) const {
    if (HasValue()) {
      return f(Value());
    }
    return Result<typename std::result_of<F(T)>::type, E>(Error());
  }

  /** @SWS_CORE_00765 @brief Return whether this instance contains the given error. */
  template <typename G> bool CheckError(G &&error) const noexcept {
    return HasValue() ? false : (Error() == error);
  }

  /** @SWS_CORE_00744 @brief Put a new error into this instance, constructed in-place from the given
   * arguments. */
  template <typename... Args>
  void EmplaceError(Args &&...args) noexcept(std::is_nothrow_constructible<E, Args...>::value);

  /** @SWS_CORE_00743 @brief Put a new value into this instance, constructed in-place from the given
   * arguments. */
  template <typename... Args>
  void EmplaceValue(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args...>::value);

  /** @SWS_CORE_00773 @brief Return the contained error as an Optional. */
  Optional<E> Err() && noexcept(std::is_nothrow_constructible<Optional<E>, E &&>::value);

  /** @SWS_CORE_00772 @brief Return the contained error as an Optional. */
  Optional<E> Err() const & noexcept(std::is_nothrow_constructible<Optional<E>, const E &>::value);

  /** @SWS_CORE_00758 @brief Access the contained error. */
  E &&Error() && noexcept {
    return m_error;
  }

  /** @SWS_CORE_00776 @brief Access the contained error. */
  E &Error() & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00757 @brief Access the contained error. */
  const E &Error() const & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00764 @brief Return the contained error or the given default error. */
  template <typename G>
  E ErrorOr(G &&defaultError) && noexcept(std::is_nothrow_move_constructible<E>::value &&
                                          std::is_nothrow_constructible<E, G &&>::value);

  /** @SWS_CORE_00763 @brief Return the contained error or the given default error. */
  template <typename G>
  E ErrorOr(G &&defaultError) const & noexcept(std::is_nothrow_copy_constructible<E>::value &&
                                               std::is_nothrow_constructible<E, G &&>::value);

  /** @SWS_CORE_00751 @brief Check whether *this contains a value. */
  bool HasValue() const noexcept {
    return m_hasValue;
  }

  /** @SWS_CORE_00771 @brief Return the contained value as an Optional. */
  Optional<T> Ok() && noexcept(std::is_nothrow_constructible<Optional<T>, T &&>::value);

  /** @SWS_CORE_00770 @brief Return the contained value as an Optional */
  Optional<T> Ok() const & noexcept(std::is_nothrow_constructible<Optional<T>, const T &>::value);

  /** @SWS_CORE_00767 @brief Return the contained value or return the result of a function call. */
  template <typename F> T Resolve(F &&f) const;

  /** @SWS_CORE_00745 @brief Exchange the contents of this instance with those of other. */
  void Swap(Result &other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                    std::is_nothrow_move_assignable<T>::value &&
                                    std::is_nothrow_move_constructible<E>::value &&
                                    std::is_nothrow_move_assignable<E>::value);

  /** @SWS_CORE_00756 @brief Access the contained value. */
  T &&Value() && noexcept {
    return std::move(m_value);
  }

  /** @SWS_CORE_00775 @brief Access the contained value. */
  T &Value() & noexcept {
    return m_value;
  }

  /** @SWS_CORE_00755 @brief Access the contained value. */
  const T &Value() const & noexcept {
    return m_value;
  }

  /** @SWS_CORE_00761 @brief Return the contained value or the given default value. */
  template <typename U>
  T ValueOr(U &&defaultValue) const & noexcept(std::is_nothrow_copy_constructible<T>::value &&
                                               std::is_nothrow_constructible<T, U &&>::value);

  /** @SWS_CORE_00762 @brief Return the contained value or the given default value. */
  template <typename U>
  T ValueOr(U &&defaultValue) && noexcept(std::is_nothrow_move_constructible<T>::value &&
                                          std::is_nothrow_constructible<T, U &&>::value);

  /** @SWS_CORE_00769 @brief Return the contained value or throw an exception */
  T &&ValueOrThrow() && noexcept(false) {
    if (HasValue()) {
      return std::move(m_value);
    }
    throw std::runtime_error("Result contains error");
  }

  /** @SWS_CORE_00766 @brief Return the contained value or throw an exception */
  const T &ValueOrThrow() const & noexcept(false) {
    if (HasValue()) {
      return m_value;
    }
    throw std::runtime_error("Result contains error");
  }

  /** @SWS_CORE_00752 @brief Check whether *this contains a value. */
  explicit operator bool() const noexcept;

  /** @SWS_CORE_00753 @brief Access the contained value. */
  const T &operator*() const & noexcept;

  /** @SWS_CORE_00759 @brief Access the contained value. */
  T &&operator*() && noexcept;

  /** @SWS_CORE_00744 @brief Access the contained value. */
  T &operator*() & noexcept;

  /** @SWS_CORE_00754 @brief Access the contained value. */
  const T *operator->() const noexcept;

  /** @SWS_CORE_00777 @brief Access the contained value. */
  T *operator->() noexcept;

  /** @SWS_CORE_00736 @brief Build a new Result from an error that is constructed in-place from the
   * given arguments. */
  template <typename... Args>
  static Result
  FromError(Args &&...args) noexcept(std::is_nothrow_constructible<E, Args...>::value);

  /** @SWS_CORE_00734 @brief Build a new Result from the specified error (given as lvalue). */
  static Result FromError(const E &e) noexcept(std::is_nothrow_copy_constructible<E>::value);

  /** @SWS_CORE_00735 @brief Build a new Result from the specified error (given as rvalue). */
  static Result FromError(E &&e) noexcept(std::is_nothrow_move_constructible<E>::value);

  /** @SWS_CORE_00733 @brief Build a new Result from a value that is constructed in-place from the
   * given arguments. */
  template <typename... Args>
  static Result
  FromValue(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args...>::value);

  /** @SWS_CORE_00732 @brief Build a new Result from the specified value (given as rvalue). */
  static Result FromValue(T &&t) noexcept(std::is_nothrow_move_constructible<T>::value);

  /** @SWS_CORE_00731 @brief Build a new Result from the specified value (given as lvalue) */
  static Result FromValue(const T &t) noexcept(std::is_nothrow_copy_constructible<T>::value);

private:
  bool m_hasValue = false;
  T m_value;
  E m_error;
};

template <typename T, typename E> class Result<T &, E> final { /* @SWS_CORE_00901 */
public:
  using error_type = E;   /* @SWS_CORE_00903 */
  using value_type = T &; /* @SWS_CORE_00902 */

  /** @SWS_CORE_00912 @brief Move-construct a new Result from another instance. */
  Result(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value);

  /** @SWS_CORE_00911 @brief Copy-construct a new Result from another instance. */
  Result(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value);

  /** @SWS_CORE_00914 @brief Copy-assignment operator. */
  Result &operator=(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value &&
                                                  std::is_nothrow_copy_assignable<E>::value);

  /** @SWS_CORE_00915 @brief Move-assignment operator. */
  Result &operator=(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value &&
                                             std::is_nothrow_move_assignable<E>::value);
};

template <typename E> class Result<void, E> final { /** @SWS_CORE_00801 */
public:
  /** @SWS_CORE_00812 */
  using error_type = E;

  /** @SWS_CORE_00811 */
  using value_type = void;

  /** @SWS_CORE_00825 */
  Result(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value)
    : m_error(other.m_error.Value()) {
  }

  /** @SWS_CORE_00826 */
  Result(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_error(std::move(other.m_error.Value())) {
  }

  /** @SWS_CORE_00821 */
  Result() noexcept : m_error(0) {
  }

  /** @SWS_CORE_00842 */
  Result &operator=(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value &&
                                             std::is_nothrow_move_assignable<E>::value) {
    m_error = std::move(other.m_error);
    return *this;
  }

  /** @SWS_CORE_00841 */
  Result &operator=(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value &&
                                                  std::is_nothrow_copy_assignable<E>::value) {
    m_error = other.m_error;
    return *this;
  }

  /** @SWS_CORE_00824 */
  explicit Result(E &&e) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_error(std::move(e.Value())) {
  }

  /** @SWS_CORE_00823 */
  explicit Result(const E &e) noexcept(std::is_nothrow_copy_constructible<E>::value)
    : m_error(e.Value()) {
  }

  /** @SWS_CORE_00865 */
  template <typename G> bool CheckError(G &&error) const noexcept {
    return (Error() == error);
  }

  /** @SWS_CORE_00844 */
  template <typename... Args>
  void EmplaceError(Args &&...args) noexcept(std::is_nothrow_constructible<E, Args...>::value);

  /** @SWS_CORE_00843 */
  template <typename... Args> void EmplaceValue(Args &&...args) noexcept;

  /** @SWS_CORE_00868 */
  Optional<E> Err() const & noexcept(std::is_nothrow_constructible<Optional<E>, const E &>::value);

  /** @SWS_CORE_00869 */
  Optional<E> Err() && noexcept(std::is_nothrow_constructible<Optional<E>, E &&>::value);

  /** @SWS_CORE_00858 */
  E &&Error() && noexcept;

  /** @SWS_CORE_00878 */
  E &Error() & noexcept;

  /** @SWS_CORE_00857 */
  const E &Error() const & noexcept;

  /** @SWS_CORE_00864 */
  template <typename G>
  E ErrorOr(G &&defaultError) && noexcept(std::is_nothrow_move_constructible<E>::value &&
                                          std::is_nothrow_constructible<E, G &&>::value);

private:
  E m_error;
};

/** @SWS_CORE_00788, @brief Compare a Result instance for inequality to an error. */
template <typename T, typename E> bool operator!=(const Result<T, E> &lhs, const E &rhs);

/** @SWS_CORE_00781, @brief Compare two Result instances for inequality. */
template <typename T, typename E> bool operator!=(const Result<T, E> &lhs, const Result<T, E> &rhs);

/** @SWS_CORE_00785 */
template <typename T, typename E> bool operator!=(const T &lhs, const Result<T, E> &rhs);

/** @SWS_CORE_00789 */
template <typename T, typename E> bool operator!=(const E &lhs, const Result<T, E> &rhs);

/** @SWS_CORE_00784 */
template <typename T, typename E> bool operator!=(const Result<T, E> &lhs, const T &rhs);

/** @SWS_CORE_00782 */
template <typename T, typename E> bool operator==(const Result<T, E> &lhs, const T &rhs);

/** @SWS_CORE_00786 */
template <typename T, typename E> bool operator==(const Result<T, E> &lhs, const E &rhs);

/** @SWS_CORE_00787 */
template <typename T, typename E> bool operator==(const E &lhs, const Result<T, E> &rhs);

/** @SWS_CORE_00780 */
template <typename T, typename E> bool operator==(const Result<T, E> &lhs, const Result<T, E> &rhs);

/** @SWS_CORE_00783 */
template <typename T, typename E> bool operator==(const T &lhs, const Result<T, E> &rhs);

/* @SWS_CORE_00796 */
template <typename T, typename E>
void swap(Result<T, E> &lhs, Result<T, E> &rhs) noexcept(noexcept(lhs.Swap(rhs)));
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_RESULT_H */
