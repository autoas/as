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
/** @SWS_CORE_00361 */
enum class FutureStatus : std::uint8_t {
  kReady,
  kTimeout,
};

/** @SWS_CORE_00321 */
template <typename T, typename E = ErrorCode> class Future final {
public:
  /** @SWS_CORE_00334 */
  Future(const Future &) = delete;

  /** @SWS_CORE_00322 */
  Future() noexcept = default;

  /** @SWS_CORE_00323 */
  Future(Future &&other) noexcept : m_future(std::move(other.m_future)) {
  }

  /** @SWS_CORE_00335 */
  Future &operator=(const Future &) = delete;

  /** @SWS_CORE_00325 */
  Future &operator=(Future &&other) noexcept {
    m_future = std::move(other.m_future);
    return *this;
  }

  /** @SWS_CORE_00333 */
  ~Future() noexcept {
  }

  /** @SWS_CORE_00336 */
  Result<T, E> GetResult() noexcept(std::is_nothrow_move_constructible<T>::value &&
                                    std::is_nothrow_move_constructible<E>::value) {
    if (is_ready()) {
      /* no wait */
    } else {
      wait();
    }

    return m_future.get();
  }

  /** @SWS_CORE_00326 */
  T get() noexcept(false) {
    Result<T, E> rslt = m_future.get();
    return rslt.ValueOrThrow();
  }

  /** @SWS_CORE_00332 */
  bool is_ready() const noexcept {
    bool bReady = false;
    std::future_status status = m_future.wait_for(std::chrono::seconds(0));
    if (status == std::future_status::ready) {
      bReady = true;
    }
    return bReady;
  }

  /** @SWS_CORE_00337 */
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

  /** @SWS_CORE_00331 */
  template <typename F> auto then(F &&func) noexcept -> Future {
    return Future<T, E>(std::async([this, func = std::forward<F>(func)]() mutable {
      auto result = this->GetResult();
      if (result.HasValue()) {
        return func(result.Value());
      }
      return Result<T, E>(result.Error());
    }));
  }

  /** @SWS_CORE_00337 */
  bool valid() const noexcept {
    return m_future.valid();
  }

  /** @SWS_CORE_00338 */
  void wait() const noexcept {
    return m_future.wait();
  }

  /** @SWS_CORE_00339 */
  template <typename Rep, typename Period>
  FutureStatus wait_for(const std::chrono::duration<Rep, Period> &timeoutDuration) const noexcept {
    return m_future.wait_for(timeoutDuration);
  }

  /** @SWS_CORE_00330 */
  template <typename Clock, typename Duration>
  FutureStatus wait_until(const std::chrono::time_point<Clock, Duration> &deadline) const noexcept {
    return m_future.wait_until(deadline);
  }

public:
  Future(std::future<Result<T, E>> &&other) noexcept : m_future(std::move(other)) {
  }

private:
  std::future<Result<T, E>> m_future;
};

/** @SWS_CORE_06221 */
template <typename E> class Future<void, E> final {
public:
  /** @SWS_CORE_06234 */
  Future(const Future &other) = delete;

  /** @SWS_CORE_06222 */
  Future() noexcept;

  /** @SWS_CORE_06223 */
  Future(Future &&other) noexcept;

  /** @SWS_CORE_06235 */
  Future &operator=(const Future &other) = delete;

  /** @SWS_CORE_06225 */
  Future &operator=(Future &&other) noexcept;

  /** @SWS_CORE_06233 */
  ~Future() noexcept;

  /** @SWS_CORE_06236 */
  Result<void, E> GetResult() noexcept(std::is_nothrow_move_constructible<E>::value);

  /** @SWS_CORE_06226 */
  void get() noexcept(false);

  /** @SWS_CORE_06232 */
  bool is_ready() const noexcept;

  /** @SWS_CORE_06237 */
  template <typename F, typename ExecutorT>
  auto then(F &&func, ExecutorT &&executor) noexcept -> Future;

  /** @SWS_CORE_06231 */
  template <typename F> auto then(F &&func) noexcept -> Future;

  /** @SWS_CORE_06227 */
  bool valid() const noexcept;

  /** @SWS_CORE_06228 */
  void wait() const noexcept;

  /** @SWS_CORE_06229 */
  template <typename Rep, typename Period>
  FutureStatus wait_for(const std::chrono::duration<Rep, Period> &timeoutDuration) const noexcept;

  /** @SWS_CORE_06230 */
  template <typename Clock, typename Duration>
  FutureStatus wait_until(const std::chrono::time_point<Clock, Duration> &deadline) const noexcept;
};

/** @SWS_CORE_00340 */
template <typename T, typename E = ErrorCode> class Promise final {
public:
  /** @SWS_CORE_00350 */
  Promise(const Promise &) = delete;

  /** @SWS_CORE_00341 */
  Promise() noexcept = default;

  /** @SWS_CORE_00342 */
  Promise(Promise &&other) noexcept : m_promise(std::move(other.m_promise)) {};

  /** @SWS_CORE_00351 */
  Promise &operator=(const Promise &) = delete;

  /** @SWS_CORE_00343 */
  Promise &operator=(Promise &&other) noexcept {
    m_promise = std::move(other.m_promise);
  }

  /** @SWS_CORE_00349 */
  ~Promise() noexcept {
  }

  /** @SWS_CORE_00353 */
  void SetError(E &&error) noexcept(std::is_nothrow_move_constructible<E>::value) {
    m_promise.set_value(Result<T, E>(std::move(error)));
  }

  /** @SWS_CORE_00354 */
  void SetError(const E &error) noexcept(std::is_nothrow_copy_constructible<E>::value) {
    m_promise.set_value(Result<T, E>(error));
  }

  /** @SWS_CORE_00356 */
  void SetResult(Result<T, E> &&result) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                                 std::is_nothrow_move_constructible<E>::value) {
    m_promise.set_value(result);
  }

  /** @SWS_CORE_00355 */
  void
  SetResult(const Result<T, E> &result) noexcept(std::is_nothrow_copy_constructible<T>::value &&
                                                 std::is_nothrow_copy_constructible<E>::value) {
    m_promise.set_value(result);
  }

  /** @SWS_CORE_00344 */
  Future<T, E> get_future() noexcept {
    return Future<T, E>(m_promise.get_future());
  }

  /** @SWS_CORE_00345 */
  void set_value(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    m_promise.set_value(Result<T, E>(value));
  }

  /** @SWS_CORE_00346 */
  void set_value(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value) {
    m_promise.set_value(Result<T, E>(std::move(value)));
  }

  /** @SWS_CORE_00352 */
  void swap(Promise &other) noexcept {
    m_promise.swap(other.m_promise);
  }

private:
  std::promise<Result<T, E>> m_promise;
};

/** @SWS_CORE_06340 */
template <typename E> class Promise<void, E> final {
public:
  /** @SWS_CORE_06342 */
  Promise(Promise &&other) noexcept;

  /** @SWS_CORE_06341 */
  Promise() noexcept;

  /** @SWS_CORE_06350 */
  Promise(const Promise &) = delete;

  /** @SWS_CORE_06351 */
  Promise &operator=(const Promise &) = delete;

  /** @SWS_CORE_06343 */
  Promise &operator=(Promise &&other) noexcept;

  /** @SWS_CORE_06349 */
  ~Promise() noexcept;

  /** @SWS_CORE_06353 */
  void SetError(E &&error) noexcept(std::is_nothrow_move_constructible<E>::value);

  /** @SWS_CORE_06354 */
  void SetError(const E &error) noexcept(std::is_nothrow_copy_constructible<E>::value);

  /** @SWS_CORE_06356 */
  void SetResult(Result<void, E> &&result) noexcept(std::is_nothrow_move_constructible<E>::value);

  /** @SWS_CORE_06355 */
  void
  SetResult(const Result<void, E> &result) noexcept(std::is_nothrow_copy_constructible<E>::value);

  /** @SWS_CORE_06344 */
  Future<void, E> get_future() noexcept;

  /** @SWS_CORE_06345 */
  void set_value() noexcept;

  /** @SWS_CORE_06353 */
  void swap(Promise &other) noexcept;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_FUTURE_H */
