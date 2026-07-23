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
/** @SWS_CORE_00701 @brief This class is a type that contains either a value or an error. */
template <typename T, typename E = ErrorCode> class Result final {
public:
  /** @SWS_CORE_00712 @brief Type alias for the error type. */
  using error_type = E;

  /** @SWS_CORE_00711 @brief Type alias for the value type. */
  using value_type = T;

  /** @brief Default constructor. */
  Result() noexcept(std::is_nothrow_default_constructible<T>::value &&
                    std::is_nothrow_default_constructible<E>::value)
    : m_hasValue(false), m_value(), m_error() {
  }

  /** @SWS_CORE_00726 @brief Constructs a Result containing a value. */
  Result(Result &&other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                  std::is_nothrow_move_constructible<E>::value)
    : m_hasValue(other.m_hasValue),
      m_value(std::move(other.m_value)),
      m_error(std::move(other.m_error)) {
    if (m_hasValue) {
      other.m_hasValue = false;
    }
  }

  /** @SWS_CORE_00725 @brief Copy-construct a new Result from another instance. */
  Result(const Result &other) noexcept(std::is_nothrow_copy_constructible<T>::value &&
                                       std::is_nothrow_copy_constructible<E>::value)
    : m_hasValue(other.m_hasValue), m_value(other.m_value), m_error(other.m_error) {
  }

  /** @SWS_CORE_00742 @brief Move-assign another Result to this instance. */
  Result &operator=(Result &&other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                             std::is_nothrow_move_assignable<T>::value &&
                                             std::is_nothrow_move_constructible<E>::value &&
                                             std::is_nothrow_move_assignable<E>::value) {
    if (this != &other) {
      m_hasValue = other.m_hasValue;
      m_value = std::move(other.m_value);
      m_error = std::move(other.m_error);
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
                                                  std::is_nothrow_copy_assignable<E>::value) {
    if (this != &other) {
      m_hasValue = other.m_hasValue;
      m_value = other.m_value;
      m_error = other.m_error;
    }
    return *this;
  }

  /** @SWS_CORE_00727 @brief Destructor. */
  ~Result() noexcept {
  }

  /** @SWS_CORE_00724 @brief Construct a new Result from the specified error (given as rvalue). */
  explicit Result(E &&e) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_hasValue(false), m_value(), m_error(std::move(e)) {
  }

  /** @SWS_CORE_00722 @brief Construct a new Result from the specified value (given as rvalue). */
  Result(T &&t) noexcept(std::is_nothrow_move_constructible<T>::value)
    : m_hasValue(true), m_value(std::move(t)), m_error() {
  }

  /** @SWS_CORE_00721 @brief Construct a new Result from the specified value (given as lvalue). */
  Result(const T &t) noexcept(std::is_nothrow_copy_constructible<T>::value)
    : m_hasValue(true), m_value(t), m_error() {
  }

  /** @SWS_CORE_00723 @brief Construct a new Result from the specified error (given as lvalue). */
  explicit Result(const E &e) noexcept(std::is_nothrow_copy_constructible<E>::value)
    : m_hasValue(false), m_value(), m_error(e) {
  }

  /** @SWS_CORE_00768 @brief Apply the given Callable to the value of this instance. */
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

  /** @SWS_CORE_00744 @brief Put a new error into this instance. */
  template <typename... Args>
  void EmplaceError(Args &&...args) noexcept(std::is_nothrow_constructible<E, Args...>::value) {
    m_hasValue = false;
    m_error = E(std::forward<Args>(args)...);
  }

  /** @SWS_CORE_00743 @brief Put a new value into this instance. */
  template <typename... Args>
  void EmplaceValue(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args...>::value) {
    m_hasValue = true;
    m_value = T(std::forward<Args>(args)...);
  }

  /** @SWS_CORE_00773 @brief Return the contained error as an Optional (rvalue). */
  Optional<E> Err() && noexcept(std::is_nothrow_constructible<Optional<E>, E &&>::value) {
    return HasValue() ? nullopt : Optional<E>(std::move(m_error));
  }

  /** @SWS_CORE_00772 @brief Return the contained error as an Optional (const lvalue). */
  Optional<E> Err() const & noexcept(std::is_nothrow_constructible<Optional<E>, const E &>::value) {
    return HasValue() ? nullopt : Optional<E>(m_error);
  }

  /** @SWS_CORE_00758 @brief Access the contained error (rvalue). */
  E &&Error() && noexcept {
    return std::move(m_error);
  }

  /** @SWS_CORE_00776 @brief Access the contained error (lvalue). */
  E &Error() & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00757 @brief Access the contained error (const lvalue). */
  const E &Error() const & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00764 @brief Return the contained error or the given default error (rvalue). */
  template <typename G>
  E ErrorOr(G &&defaultError) && noexcept(std::is_nothrow_move_constructible<E>::value &&
                                          std::is_nothrow_constructible<E, G &&>::value) {
    return HasValue() ? std::move(m_error) : E(std::forward<G>(defaultError));
  }

  /** @SWS_CORE_00763 @brief Return the contained error or the given default error (const lvalue).
   */
  template <typename G>
  E ErrorOr(G &&defaultError) const & noexcept(std::is_nothrow_copy_constructible<E>::value &&
                                               std::is_nothrow_constructible<E, G &&>::value) {
    return HasValue() ? m_error : E(std::forward<G>(defaultError));
  }

  /** @SWS_CORE_00751 @brief Check whether *this contains a value. */
  bool HasValue() const noexcept {
    return m_hasValue;
  }

  /** @SWS_CORE_00771 @brief Return the contained value as an Optional (rvalue). */
  Optional<T> Ok() && noexcept(std::is_nothrow_constructible<Optional<T>, T &&>::value) {
    return HasValue() ? Optional<T>(std::move(m_value)) : nullopt;
  }

  /** @SWS_CORE_00770 @brief Return the contained value as an Optional (const lvalue). */
  Optional<T> Ok() const & noexcept(std::is_nothrow_constructible<Optional<T>, const T &>::value) {
    return HasValue() ? Optional<T>(m_value) : nullopt;
  }

  /** @SWS_CORE_00767 @brief Return the contained value or return the result of a function call. */
  template <typename F> T Resolve(F &&f) const {
    if (HasValue()) {
      return Value();
    }
    return f(Error());
  }

  /** @SWS_CORE_00745 @brief Exchange the contents of this instance with those of other. */
  void Swap(Result &other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                    std::is_nothrow_move_assignable<T>::value &&
                                    std::is_nothrow_move_constructible<E>::value &&
                                    std::is_nothrow_move_assignable<E>::value) {
    std::swap(m_hasValue, other.m_hasValue);
    std::swap(m_value, other.m_value);
    std::swap(m_error, other.m_error);
  }

  /** @SWS_CORE_00756 @brief Access the contained value (rvalue). */
  T &&Value() && noexcept {
    return std::move(m_value);
  }

  /** @SWS_CORE_00775 @brief Access the contained value (lvalue). */
  T &Value() & noexcept {
    return m_value;
  }

  /** @SWS_CORE_00755 @brief Access the contained value (const lvalue). */
  const T &Value() const & noexcept {
    return m_value;
  }

  /** @SWS_CORE_00761 @brief Return the contained value or the given default value (const lvalue).
   */
  template <typename U>
  T ValueOr(U &&defaultValue) const & noexcept(std::is_nothrow_copy_constructible<T>::value &&
                                               std::is_nothrow_constructible<T, U &&>::value) {
    return HasValue() ? m_value : T(std::forward<U>(defaultValue));
  }

  /** @SWS_CORE_00762 @brief Return the contained value or the given default value (rvalue). */
  template <typename U>
  T ValueOr(U &&defaultValue) && noexcept(std::is_nothrow_move_constructible<T>::value &&
                                          std::is_nothrow_constructible<T, U &&>::value) {
    return HasValue() ? std::move(m_value) : T(std::forward<U>(defaultValue));
  }

  /** @SWS_CORE_00769 @brief Return the contained value or throw an exception (rvalue). */
  T &&ValueOrThrow() && noexcept(false) {
    if (HasValue()) {
      return std::move(m_value);
    }
    throw std::runtime_error("Result contains error");
  }

  /** @SWS_CORE_00766 @brief Return the contained value or throw an exception (const lvalue). */
  const T &ValueOrThrow() const & noexcept(false) {
    if (HasValue()) {
      return m_value;
    }
    throw std::runtime_error("Result contains error");
  }

  /** @SWS_CORE_00752 @brief Check whether *this contains a value. */
  explicit operator bool() const noexcept {
    return m_hasValue;
  }

  /** @SWS_CORE_00753 @brief Access the contained value (const lvalue). */
  const T &operator*() const & noexcept {
    return m_value;
  }

  /** @SWS_CORE_00759 @brief Access the contained value (rvalue). */
  T &&operator*() && noexcept {
    return std::move(m_value);
  }

  /** @SWS_CORE_00744 @brief Access the contained value (lvalue). */
  T &operator*() & noexcept {
    return m_value;
  }

  /** @SWS_CORE_00754 @brief Access the contained value (const). */
  const T *operator->() const noexcept {
    return &m_value;
  }

  /** @SWS_CORE_00777 @brief Access the contained value. */
  T *operator->() noexcept {
    return &m_value;
  }

  /** @SWS_CORE_00736 @brief Build a new Result from an error constructed in-place. */
  template <typename... Args>
  static Result
  FromError(Args &&...args) noexcept(std::is_nothrow_constructible<E, Args...>::value) {
    return Result(E(std::forward<Args>(args)...));
  }

  /** @SWS_CORE_00734 @brief Build a new Result from the specified error (lvalue). */
  static Result FromError(const E &e) noexcept(std::is_nothrow_copy_constructible<E>::value) {
    return Result(e);
  }

  /** @SWS_CORE_00735 @brief Build a new Result from the specified error (rvalue). */
  static Result FromError(E &&e) noexcept(std::is_nothrow_move_constructible<E>::value) {
    return Result(std::move(e));
  }

  /** @SWS_CORE_00733 @brief Build a new Result from a value constructed in-place. */
  template <typename... Args>
  static Result
  FromValue(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args...>::value) {
    return Result(T(std::forward<Args>(args)...));
  }

  /** @SWS_CORE_00732 @brief Build a new Result from the specified value (rvalue). */
  static Result FromValue(T &&t) noexcept(std::is_nothrow_move_constructible<T>::value) {
    return Result(std::move(t));
  }

  /** @SWS_CORE_00731 @brief Build a new Result from the specified value (lvalue). */
  static Result FromValue(const T &t) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    return Result(t);
  }

private:
  bool m_hasValue = false;
  T m_value;
  E m_error;
};

template <typename T, typename E>
class Result<T &, E> final { /* @SWS_CORE_00901 @brief Result specialization for reference type. */
public:
  using error_type = E;   /* @SWS_CORE_00903 @brief Error type alias. */
  using value_type = T &; /* @SWS_CORE_00902 @brief Value type alias. */

  /** @SWS_CORE_00912 @brief Move-construct a new Result from another instance. */
  Result(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_hasValue(other.m_hasValue), m_value(other.m_value), m_error(std::move(other.m_error)) {
    if (m_hasValue) {
      other.m_hasValue = false;
    }
  }

  /** @SWS_CORE_00911 @brief Copy-construct a new Result from another instance. */
  Result(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value)
    : m_hasValue(other.m_hasValue), m_value(other.m_value), m_error(other.m_error) {
  }

  /** @SWS_CORE_00909 @brief Construct from value reference. */
  explicit Result(T &value) noexcept : m_hasValue(true), m_value(&value), m_error() {
  }

  /** @SWS_CORE_00910 @brief Construct from error rvalue. */
  explicit Result(E &&error) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_hasValue(false), m_value(nullptr), m_error(std::move(error)) {
  }

  /** @SWS_CORE_00908 @brief Construct from error lvalue. */
  explicit Result(const E &error) noexcept(std::is_nothrow_copy_constructible<E>::value)
    : m_hasValue(false), m_value(nullptr), m_error(error) {
  }

  /** @SWS_CORE_00914 @brief Copy-assignment operator. */
  Result &operator=(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value &&
                                                  std::is_nothrow_copy_assignable<E>::value) {
    m_hasValue = other.m_hasValue;
    m_value = other.m_value;
    m_error = other.m_error;
    return *this;
  }

  /** @SWS_CORE_00915 @brief Move-assignment operator. */
  Result &operator=(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value &&
                                             std::is_nothrow_move_assignable<E>::value) {
    m_hasValue = other.m_hasValue;
    m_value = other.m_value;
    m_error = std::move(other.m_error);
    if (m_hasValue) {
      other.m_hasValue = false;
    }
    return *this;
  }

  /** @SWS_CORE_00913 @brief Destructor. */
  ~Result() noexcept {
  }

  /** @SWS_CORE_00937 @brief Check for error. */
  template <typename G> bool CheckError(G &&error) const noexcept {
    return HasValue() ? false : (Error() == error);
  }

  /** @SWS_CORE_00916 @brief Emplace error in-place. */
  template <typename... Args>
  void EmplaceError(Args &&...args) noexcept(std::is_nothrow_constructible<E, Args...>::value) {
    m_hasValue = false;
    m_value = nullptr;
    m_error = E(std::forward<Args>(args)...);
  }

  /** @SWS_CORE_00928 @brief Access error (const lvalue). */
  const E &Error() const & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00929 @brief Access error (lvalue). */
  E &Error() & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00930 @brief Access error (rvalue). */
  E &&Error() && noexcept {
    return std::move(m_error);
  }

  /** @SWS_CORE_00935 @brief Return error or default (const lvalue). */
  template <typename G>
  E ErrorOr(G &&defaultError) const & noexcept(std::is_nothrow_copy_constructible<E>::value &&
                                               std::is_nothrow_constructible<E, G &&>::value) {
    if (HasValue()) {
      return m_error;
    }
    return E(std::forward<G>(defaultError));
  }

  /** @SWS_CORE_00936 @brief Return error or default (rvalue). */
  template <typename G>
  E ErrorOr(G &&defaultError) && noexcept(std::is_nothrow_move_constructible<E>::value &&
                                          std::is_nothrow_constructible<E, G &&>::value) {
    if (HasValue()) {
      return std::move(m_error);
    }
    return E(std::forward<G>(defaultError));
  }

  /** @SWS_CORE_00931 @brief Return error as Optional (const lvalue). */
  Optional<E> Err() const & noexcept(std::is_nothrow_constructible<Optional<E>, const E &>::value) {
    return HasValue() ? nullopt : Optional<E>(m_error);
  }

  /** @SWS_CORE_00932 @brief Return error as Optional (rvalue). */
  Optional<E> Err() && noexcept(std::is_nothrow_constructible<Optional<E>, E &&>::value) {
    return HasValue() ? nullopt : Optional<E>(std::move(m_error));
  }

  /** @SWS_CORE_00918 @brief Check whether *this contains a value. */
  bool HasValue() const noexcept {
    return m_hasValue;
  }

  /** @SWS_CORE_00926 @brief Return value as Optional (const lvalue). */
  Optional<T &> Ok() const & noexcept {
    return HasValue() ? Optional<T &>(*m_value) : nullopt;
  }

  /** @SWS_CORE_00927 @brief Return value as Optional (rvalue). */
  Optional<T &> Ok() && noexcept {
    return HasValue() ? Optional<T &>(*m_value) : nullopt;
  }

  /** @SWS_CORE_00939 @brief Reset to default state. */
  void Reset() noexcept {
    m_hasValue = false;
    m_value = nullptr;
  }

  /** @SWS_CORE_00940 @brief Swap with another Result. */
  void Swap(Result &other) noexcept {
    std::swap(m_hasValue, other.m_hasValue);
    std::swap(m_value, other.m_value);
    std::swap(m_error, other.m_error);
  }

  /** @SWS_CORE_00924 @brief Return the contained value (const lvalue). */
  const T &Value() const & noexcept(false) {
    if (!HasValue()) {
      throw std::runtime_error("Result contains error");
    }
    return *m_value;
  }

  /** @SWS_CORE_00925 @brief Return the contained value (lvalue). */
  T &Value() & noexcept(false) {
    if (!HasValue()) {
      throw std::runtime_error("Result contains error");
    }
    return *m_value;
  }

  /** @SWS_CORE_00934 @brief Return the contained value or throw an exception (const lvalue). */
  const T &ValueOrThrow() const & noexcept(false) {
    if (!HasValue()) {
      throw std::runtime_error("Result contains error");
    }
    return *m_value;
  }

  /** @SWS_CORE_00933 @brief Return the contained value or throw an exception (lvalue). */
  T &ValueOrThrow() & noexcept(false) {
    if (!HasValue()) {
      throw std::runtime_error("Result contains error");
    }
    return *m_value;
  }

  /** @SWS_CORE_00917 @brief Swap two Result instances (non-member). */
  friend void swap(Result &lhs, Result &rhs) noexcept(noexcept(lhs.Swap(rhs))) {
    lhs.Swap(rhs);
  }

private:
  bool m_hasValue = false;
  T *m_value = nullptr;
  E m_error;
};

template <typename E>
class Result<void, E> final { /** @SWS_CORE_00801 @brief Result specialization for void type. */
public:
  /** @SWS_CORE_00812 @brief Error type alias. */
  using error_type = E;

  /** @SWS_CORE_00811 @brief Value type alias. */
  using value_type = void;

  /** @SWS_CORE_00825 @brief Copy constructor. */
  Result(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value)
    : m_error(other.m_error) {
  }

  /** @SWS_CORE_00826 @brief Move constructor. */
  Result(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_error(std::move(other.m_error)) {
  }

  /** @SWS_CORE_00821 @brief Default constructor. */
  Result() noexcept : m_error() {
  }

  /** @SWS_CORE_00842 @brief Move assignment. */
  Result &operator=(Result &&other) noexcept(std::is_nothrow_move_constructible<E>::value &&
                                             std::is_nothrow_move_assignable<E>::value) {
    m_error = std::move(other.m_error);
    return *this;
  }

  /** @SWS_CORE_00841 @brief Copy assignment. */
  Result &operator=(const Result &other) noexcept(std::is_nothrow_copy_constructible<E>::value &&
                                                  std::is_nothrow_copy_assignable<E>::value) {
    m_error = other.m_error;
    return *this;
  }

  /** @SWS_CORE_00824 @brief Constructor from error rvalue. */
  explicit Result(E &&e) noexcept(std::is_nothrow_move_constructible<E>::value)
    : m_error(std::move(e)) {
  }

  /** @SWS_CORE_00823 @brief Constructor from error lvalue. */
  explicit Result(const E &e) noexcept(std::is_nothrow_copy_constructible<E>::value) : m_error(e) {
  }

  /** @SWS_CORE_00865 @brief Check for error. */
  template <typename G> bool CheckError(G &&error) const noexcept {
    return (Error() == error);
  }

  /** @SWS_CORE_00844 @brief Emplace error in-place. */
  template <typename... Args>
  void EmplaceError(Args &&...args) noexcept(std::is_nothrow_constructible<E, Args...>::value) {
    m_error = E(std::forward<Args>(args)...);
  }

  /** @SWS_CORE_00843 @brief Emplace value in-place. */
  template <typename... Args> void EmplaceValue(Args &&...args) noexcept {
    m_error = E();
  }

  /** @SWS_CORE_00868 @brief Return error as Optional (const lvalue). */
  Optional<E> Err() const & noexcept(std::is_nothrow_constructible<Optional<E>, const E &>::value) {
    return (m_error == E()) ? nullopt : Optional<E>(m_error);
  }

  /** @SWS_CORE_00869 @brief Return error as Optional (rvalue). */
  Optional<E> Err() && noexcept(std::is_nothrow_constructible<Optional<E>, E &&>::value) {
    return (m_error == E()) ? nullopt : Optional<E>(std::move(m_error));
  }

  /** @SWS_CORE_00858 @brief Access error (rvalue). */
  E &&Error() && noexcept {
    return std::move(m_error);
  }

  /** @SWS_CORE_00876 @brief Access error (lvalue). */
  E &Error() & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00857 @brief Access error (const lvalue). */
  const E &Error() const & noexcept {
    return m_error;
  }

  /** @SWS_CORE_00864 @brief Return error or default (rvalue). */
  template <typename G>
  E ErrorOr(G &&defaultError) && noexcept(std::is_nothrow_move_constructible<E>::value &&
                                          std::is_nothrow_constructible<E, G &&>::value) {
    return (m_error != E()) ? std::move(m_error) : E(std::forward<G>(defaultError));
  }

  /** @SWS_CORE_00866 @brief Return the contained value or throw an exception (void). */
  void ValueOrThrow() const & noexcept(false) {
    if (m_error != E()) {
      throw std::runtime_error("Result contains error");
    }
  }

private:
  E m_error;
};

/** @SWS_CORE_00788 @brief Compare a Result instance for inequality to an error. */
template <typename T, typename E> bool operator!=(const Result<T, E> &lhs, const E &rhs) {
  return !(lhs == rhs);
}

/** @SWS_CORE_00781 @brief Compare two Result instances for inequality. */
template <typename T, typename E>
bool operator!=(const Result<T, E> &lhs, const Result<T, E> &rhs) {
  return !(lhs == rhs);
}

/** @SWS_CORE_00785 @brief Value vs Result inequality. */
template <typename T, typename E> bool operator!=(const T &lhs, const Result<T, E> &rhs) {
  return !(lhs == rhs);
}

/** @SWS_CORE_00789 @brief Error vs Result inequality. */
template <typename T, typename E> bool operator!=(const E &lhs, const Result<T, E> &rhs) {
  return !(lhs == rhs);
}

/** @SWS_CORE_00784 @brief Result vs Value inequality. */
template <typename T, typename E> bool operator!=(const Result<T, E> &lhs, const T &rhs) {
  return !(lhs == rhs);
}

/** @SWS_CORE_00782 @brief Result vs Value equality. */
template <typename T, typename E> bool operator==(const Result<T, E> &lhs, const T &rhs) {
  return lhs.HasValue() && lhs.Value() == rhs;
}

/** @SWS_CORE_00786 @brief Result vs Error equality. */
template <typename T, typename E> bool operator==(const Result<T, E> &lhs, const E &rhs) {
  return !lhs.HasValue() && lhs.Error() == rhs;
}

/** @SWS_CORE_00787 @brief Error vs Result equality. */
template <typename T, typename E> bool operator==(const E &lhs, const Result<T, E> &rhs) {
  return rhs == lhs;
}

/** @SWS_CORE_00780 @brief Result vs Result equality. */
template <typename T, typename E>
bool operator==(const Result<T, E> &lhs, const Result<T, E> &rhs) {
  if (lhs.HasValue() != rhs.HasValue())
    return false;
  if (lhs.HasValue())
    return lhs.Value() == rhs.Value();
  return lhs.Error() == rhs.Error();
}

/** @SWS_CORE_00783 @brief Value vs Result equality. */
template <typename T, typename E> bool operator==(const T &lhs, const Result<T, E> &rhs) {
  return rhs == lhs;
}

/** @SWS_CORE_00796 @brief Swap two Result instances. */
template <typename T, typename E>
void swap(Result<T, E> &lhs, Result<T, E> &rhs) noexcept(noexcept(lhs.Swap(rhs))) {
  lhs.Swap(rhs);
}
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_RESULT_H */
