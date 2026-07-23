/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_FUTURE_H
#define ARA_CORE_FUTURE_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ara/core/error_code.h"
#include "ara/core/future_error_domain.h"
#include "ara/core/result.h"
#include <chrono>
#include <future>

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @SWS_CORE_00361 @brief Future status enumeration. */
enum class FutureStatus : std::uint8_t {
  kReady,
  kTimeout,
};

/** @SWS_CORE_00321 @brief Future class template. */
template <typename T, typename E = ErrorCode> class Future final {
public:
  /** @SWS_CORE_00334 @brief Copy constructor deleted. */
  Future(const Future &) = delete;

  /** @SWS_CORE_00322 @brief Default constructor. */
  Future() noexcept = default;

  /** @SWS_CORE_00323 @brief Move constructor. */
  Future(Future &&other) noexcept : m_future(std::move(other.m_future)) {
  }

  /** @SWS_CORE_00335 @brief Copy assignment deleted. */
  Future &operator=(const Future &) = delete;

  /** @SWS_CORE_00325 @brief Move assignment. */
  Future &operator=(Future &&other) noexcept {
    m_future = std::move(other.m_future);
    return *this;
  }

  /** @SWS_CORE_00333 @brief Destructor. */
  ~Future() noexcept {
  }

  /** @SWS_CORE_00336 @brief Get Result from Future. */
  Result<T, E> GetResult() noexcept(std::is_nothrow_move_constructible<T>::value &&
                                    std::is_nothrow_move_constructible<E>::value) {
    if (is_ready()) {
      /* no wait */
    } else {
      wait();
    }

    return m_future.get();
  }

  /** @SWS_CORE_00326 @brief Get value from Future. */
  T get() noexcept(false) {
    Result<T, E> rslt = m_future.get();
    return rslt.ValueOrThrow();
  }

  /** @SWS_CORE_00332 @brief Check if Future is ready. */
  bool is_ready() const noexcept {
    std::future_status status = m_future.wait_for(std::chrono::seconds(0));
    return (status == std::future_status::ready);
  }

  /** @SWS_CORE_00337 @brief Attach continuation with executor. */
  template <typename F, typename ExecutorT>
  auto then(F &&func, ExecutorT &&executor) noexcept -> Future {
    return Future<T, E>(executor([this, func = std::forward<F>(func)]() mutable {
      auto result = this->GetResult();
      if (result.HasValue()) {
        return func(result.Value());
      }
      return Result<T, E>(result.Error());
    }));
  }

  /** @SWS_CORE_00331 @brief Attach continuation. */
  template <typename F> auto then(F &&func) noexcept -> Future {
    return Future<T, E>(std::async([this, func = std::forward<F>(func)]() mutable {
      auto result = this->GetResult();
      if (result.HasValue()) {
        return func(result.Value());
      }
      return Result<T, E>(result.Error());
    }));
  }

  /** @SWS_CORE_00327 @brief Check if Future is valid. */
  bool valid() const noexcept {
    return m_future.valid();
  }

  /** @SWS_CORE_00328 @brief Wait for Future to complete. */
  void wait() const noexcept {
    return m_future.wait();
  }

  /** @SWS_CORE_00329 @brief Wait with duration timeout. */
  template <typename Rep, typename Period>
  FutureStatus wait_for(const std::chrono::duration<Rep, Period> &timeoutDuration) const noexcept {
    auto status = m_future.wait_for(timeoutDuration);
    return (status == std::future_status::ready) ? FutureStatus::kReady : FutureStatus::kTimeout;
  }

  /** @SWS_CORE_00330 @brief Wait until deadline. */
  template <typename Clock, typename Duration>
  FutureStatus wait_until(const std::chrono::time_point<Clock, Duration> &deadline) const noexcept {
    auto status = m_future.wait_until(deadline);
    return (status == std::future_status::ready) ? FutureStatus::kReady : FutureStatus::kTimeout;
  }

public:
  Future(std::future<Result<T, E>> &&other) noexcept : m_future(std::move(other)) {
  }

private:
  std::future<Result<T, E>> m_future;
};

/** @SWS_CORE_06221 @brief Future specialization for void type. */
template <typename E> class Future<void, E> final {
public:
  /** @SWS_CORE_06234 @brief Copy constructor deleted. */
  Future(const Future &other) = delete;

  /** @SWS_CORE_06222 @brief Default constructor. */
  Future() noexcept = default;

  /** @SWS_CORE_06223 @brief Move constructor. */
  Future(Future &&other) noexcept : m_future(std::move(other.m_future)) {}

  /** @SWS_CORE_06235 @brief Copy assignment deleted. */
  Future &operator=(const Future &other) = delete;

  /** @SWS_CORE_06225 @brief Move assignment. */
  Future &operator=(Future &&other) noexcept {
    m_future = std::move(other.m_future);
    return *this;
  }

  /** @SWS_CORE_06233 @brief Destructor. */
  ~Future() noexcept = default;

  /** @SWS_CORE_06236 @brief Get Result from Future. */
  Result<void, E> GetResult() noexcept(std::is_nothrow_move_constructible<E>::value) {
    if (is_ready()) {
    } else {
      wait();
    }
    return m_future.get();
  }

  /** @SWS_CORE_06226 @brief Get from Future. */
  void get() noexcept(false) {
    Result<void, E> rslt = m_future.get();
    rslt.ValueOrThrow();
  }

  /** @SWS_CORE_06232 @brief Check if Future is ready. */
  bool is_ready() const noexcept {
    std::future_status status = m_future.wait_for(std::chrono::seconds(0));
    return (status == std::future_status::ready);
  }

  /** @SWS_CORE_06237 @brief Attach continuation with executor. */
  template <typename F, typename ExecutorT>
  auto then(F &&func, ExecutorT &&executor) noexcept -> Future {
    return Future<void, E>(executor([this, func = std::forward<F>(func)]() mutable {
      auto result = this->GetResult();
      if (result.HasValue()) {
        return func();
      }
      return Result<void, E>(result.Error());
    }));
  }

  /** @SWS_CORE_06231 @brief Attach continuation. */
  template <typename F> auto then(F &&func) noexcept -> Future {
    return Future<void, E>(std::async([this, func = std::forward<F>(func)]() mutable {
      auto result = this->GetResult();
      if (result.HasValue()) {
        return func();
      }
      return Result<void, E>(result.Error());
    }));
  }

  /** @SWS_CORE_00327 @brief Check if Future is valid. */
  bool valid() const noexcept {
    return m_future.valid();
  }

  /** @SWS_CORE_00328 @brief Wait for Future to complete. */
  void wait() const noexcept {
    m_future.wait();
  }

  /** @SWS_CORE_00329 @brief Wait with duration timeout. */
  template <typename Rep, typename Period>
  FutureStatus wait_for(const std::chrono::duration<Rep, Period> &timeoutDuration) const noexcept {
    auto status = m_future.wait_for(timeoutDuration);
    return (status == std::future_status::ready) ? FutureStatus::kReady : FutureStatus::kTimeout;
  }

  /** @SWS_CORE_06230 @brief Wait until deadline. */
  template <typename Clock, typename Duration>
  FutureStatus wait_until(const std::chrono::time_point<Clock, Duration> &deadline) const noexcept {
    auto status = m_future.wait_until(deadline);
    return (status == std::future_status::ready) ? FutureStatus::kReady : FutureStatus::kTimeout;
  }

public:
  Future(std::future<Result<void, E>> &&other) noexcept : m_future(std::move(other)) {}

private:
  std::future<Result<void, E>> m_future;
};

/** @SWS_CORE_00340 @brief Promise class template. */
template <typename T, typename E = ErrorCode> class Promise final {
public:
  /** @SWS_CORE_00350 @brief Copy constructor deleted. */
  Promise(const Promise &) = delete;

  /** @SWS_CORE_00341 @brief Default constructor. */
  Promise() noexcept = default;

  /** @SWS_CORE_00342 @brief Move constructor. */
  Promise(Promise &&other) noexcept : m_promise(std::move(other.m_promise)) {};

  /** @SWS_CORE_00351 @brief Copy assignment deleted. */
  Promise &operator=(const Promise &) = delete;

  /** @SWS_CORE_00343 @brief Move assignment. */
  Promise &operator=(Promise &&other) noexcept {
    m_promise = std::move(other.m_promise);
  }

  /** @SWS_CORE_00349 @brief Destructor. */
  ~Promise() noexcept {
  }

  /** @SWS_CORE_00353 @brief Set error (rvalue). */
  void SetError(E &&error) noexcept(std::is_nothrow_move_constructible<E>::value) {
    m_promise.set_value(Result<T, E>(std::move(error)));
  }

  /** @SWS_CORE_00354 @brief Set error (lvalue). */
  void SetError(const E &error) noexcept(std::is_nothrow_copy_constructible<E>::value) {
    m_promise.set_value(Result<T, E>(error));
  }

  /** @SWS_CORE_00356 @brief Set result (rvalue). */
  void SetResult(Result<T, E> &&result) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                                 std::is_nothrow_move_constructible<E>::value) {
    m_promise.set_value(result);
  }

  /** @SWS_CORE_00355 @brief Set result (lvalue). */
  void
  SetResult(const Result<T, E> &result) noexcept(std::is_nothrow_copy_constructible<T>::value &&
                                                 std::is_nothrow_copy_constructible<E>::value) {
    m_promise.set_value(result);
  }

  /** @SWS_CORE_00344 @brief Get Future from Promise. */
  Future<T, E> get_future() noexcept {
    return Future<T, E>(m_promise.get_future());
  }

  /** @SWS_CORE_00345 @brief Set value (lvalue). */
  void set_value(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    m_promise.set_value(Result<T, E>(value));
  }

  /** @SWS_CORE_00346 @brief Set value (rvalue). */
  void set_value(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value) {
    m_promise.set_value(Result<T, E>(std::move(value)));
  }

  /** @SWS_CORE_00352 @brief Swap with another Promise. */
  void swap(Promise &other) noexcept {
    m_promise.swap(other.m_promise);
  }

private:
  std::promise<Result<T, E>> m_promise;
};

/** @SWS_CORE_06340 @brief Promise specialization for void type. */
template <typename E> class Promise<void, E> final {
public:
  /** @SWS_CORE_06342 @brief Move constructor. */
  Promise(Promise &&other) noexcept : m_promise(std::move(other.m_promise)) {}

  /** @SWS_CORE_06341 @brief Default constructor. */
  Promise() noexcept = default;

  /** @SWS_CORE_06350 @brief Copy constructor deleted. */
  Promise(const Promise &) = delete;

  /** @SWS_CORE_06351 @brief Copy assignment deleted. */
  Promise &operator=(const Promise &) = delete;

  /** @SWS_CORE_06343 @brief Move assignment. */
  Promise &operator=(Promise &&other) noexcept {
    m_promise = std::move(other.m_promise);
    return *this;
  }

  /** @SWS_CORE_06349 @brief Destructor. */
  ~Promise() noexcept = default;

  /** @SWS_CORE_06353 @brief Set error (rvalue). */
  void SetError(E &&error) noexcept(std::is_nothrow_move_constructible<E>::value) {
    m_promise.set_value(Result<void, E>(std::move(error)));
  }

  /** @SWS_CORE_06354 @brief Set error (lvalue). */
  void SetError(const E &error) noexcept(std::is_nothrow_copy_constructible<E>::value) {
    m_promise.set_value(Result<void, E>(error));
  }

  /** @SWS_CORE_06356 @brief Set result (rvalue). */
  void SetResult(Result<void, E> &&result) noexcept(std::is_nothrow_move_constructible<E>::value) {
    m_promise.set_value(std::move(result));
  }

  /** @SWS_CORE_06355 @brief Set result (lvalue). */
  void
  SetResult(const Result<void, E> &result) noexcept(std::is_nothrow_copy_constructible<E>::value) {
    m_promise.set_value(result);
  }

  /** @SWS_CORE_06344 @brief Get Future from Promise. */
  Future<void, E> get_future() noexcept {
    return Future<void, E>(m_promise.get_future());
  }

  /** @SWS_CORE_06345 @brief Set void value. */
  void set_value() noexcept {
    m_promise.set_value(Result<void, E>());
  }

  /** @SWS_CORE_06352 @brief Swap with another Promise. */
  void swap(Promise &other) noexcept {
    m_promise.swap(other.m_promise);
  }

private:
  std::promise<Result<void, E>> m_promise;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_FUTURE_H */
