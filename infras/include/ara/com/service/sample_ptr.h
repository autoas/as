/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */

#ifndef ARA_COM_SERVICE_SAMPLE_PTR_HPP
#define ARA_COM_SERVICE_SAMPLE_PTR_HPP

/* ================================ [ INCLUDES  ] ============================================== */
#include <cstddef>
#include <memory>

namespace ara {
namespace com {

/* ================================ [ CLASS    ] ============================================== */
/**
 * @SWS_CM_00306
 * Emulates a std::unique_ptr to an event sample.
 * The ara::com::SamplePtr behaves as a std::unique_ptr as long as the event/field
 * is subscribed to, or the Proxy it belongs to is not destroyed.
 * The precondition defined in [SWS_CM_00085] and [SWS_CM_00087] must be fulfilled.
 * Otherwise it is considered a violation.
 */
template <typename T> class SamplePtr {
public:
  /** @SWS_CM_11534 */
  constexpr SamplePtr() noexcept : m_ptr(nullptr) {
  }

  /** @SWS_CM_11536 */
  SamplePtr(const SamplePtr &) = delete;

  /** @SWS_CM_11537 */
  SamplePtr(SamplePtr &&other) noexcept : m_ptr(std::move(other.m_ptr)) {
  }

  /** @SWS_CM_11538 */
  SamplePtr &operator=(const SamplePtr &) = delete;

  /** @SWS_CM_11540 */
  SamplePtr &operator=(SamplePtr &&other) noexcept {
    if (this != &other) {
      m_ptr = other.m_ptr;
    }
    return *this;
  }

  /** @SWS_CM_11547 */
  ~SamplePtr() noexcept {
  }

  /** @SWS_CM_11535 */
  constexpr SamplePtr(std::nullptr_t other) noexcept : m_ptr(other) {
  }

  /** @SWS_CM_11546 */
  T *Get() const noexcept {
    return m_ptr.get();
  }

  /** @SWS_CM_11545 */
  void Reset(std::nullptr_t other) noexcept {
    m_ptr.reset(other);
  }

  /** @SWS_CM_11544 */
  void Swap(SamplePtr &other) noexcept {
    m_ptr.swap(other);
  }

  /** @SWS_CM_11543 */
  explicit operator bool() const noexcept {
    return m_ptr.get() == nullptr;
  }

  /** @SWS_CM_11541 */
  T &operator*() const noexcept {
    return *m_ptr;
  }

  /** @SWS_CM_11542 */
  T *operator->() const noexcept {
    return m_ptr.get();
  }

  /** @SWS_CM_11539 */
  SamplePtr &operator=(std::nullptr_t other) noexcept {
    m_ptr = other;
    return *this;
  }

public:
  SamplePtr(T *other) noexcept : m_ptr(other) {
  }

private:
  std::unique_ptr<T> m_ptr;
};

/* ================================ [ FUNCTIONS ] ============================================== */

} // namespace com
} // namespace ara

#endif /* ARA_COM_SERVICE_SAMPLE_PTR_HPP */
