/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_VECTOR_H
#define ARA_CORE_VECTOR_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <cstddef>
#include <cinttypes>
#include <memory>
#include <vector>

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/* @SWS_CORE_01301 */
template <typename T, typename Allocator = std::allocator<T>> class Vector {
private:
  std::vector<T, Allocator> m_data;

public:
  Vector() = default;

  explicit Vector(size_t count) : m_data(count) {
  }

  Vector(size_t count, const T &value) : m_data(count, value) {
  }

  template <typename InputIt> Vector(InputIt first, InputIt last) : m_data(first, last) {
  }

  Vector(std::initializer_list<T> init) : m_data(init) {
  }

  T &operator[](size_t index) {
    return m_data[index];
  }
  const T &operator[](size_t index) const {
    return m_data[index];
  }

  T &at(size_t index) {
    return m_data.at(index);
  }
  const T &at(size_t index) const {
    return m_data.at(index);
  }

  T &front() {
    return m_data.front();
  }
  const T &front() const {
    return m_data.front();
  }

  T &back() {
    return m_data.back();
  }
  const T &back() const {
    return m_data.back();
  }

  T *data() noexcept {
    return m_data.data();
  }
  const T *data() const noexcept {
    return m_data.data();
  }

  bool empty() const noexcept {
    return m_data.empty();
  }
  size_t size() const noexcept {
    return m_data.size();
  }
  size_t capacity() const noexcept {
    return m_data.capacity();
  }

  void reserve(size_t new_cap) {
    m_data.reserve(new_cap);
  }
  void shrink_to_fit() {
    m_data.shrink_to_fit();
  }

  void clear() noexcept {
    m_data.clear();
  }

  void push_back(const T &value) {
    m_data.push_back(value);
  }
  void push_back(T &&value) {
    m_data.push_back(std::move(value));
  }

  template <typename... Args> decltype(auto) emplace_back(Args &&...args) {
    return m_data.emplace_back(std::forward<Args>(args)...);
  }

  void pop_back() {
    m_data.pop_back();
  }

  auto begin() noexcept {
    return m_data.begin();
  }
  auto end() noexcept {
    return m_data.end();
  }

  auto begin() const noexcept {
    return m_data.begin();
  }
  auto end() const noexcept {
    return m_data.end();
  }

  auto cbegin() const noexcept {
    return m_data.cbegin();
  }
  auto cend() const noexcept {
    return m_data.cend();
  }
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CORE_01391 */
template <typename T, typename Allocator>
bool operator!=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);

/* @SWS_CORE_01392 */
template <typename T, typename Allocator>
bool operator<(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);

/* @SWS_CORE_01393 */
template <typename T, typename Allocator>
bool operator<=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);

/* @SWS_CORE_01390 */
template <typename T, typename Allocator>
bool operator==(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);

/* @SWS_CORE_01394 */
template <typename T, typename Allocator>
bool operator>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);

/* @SWS_CORE_01395 */
template <typename T, typename Allocator>
bool operator>=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);

/* @SWS_CORE_01396 */
template <typename T, typename Allocator>
void swap(Vector<T, Allocator> &lhs, Vector<T, Allocator> &rhs);

} // namespace core
} // namespace ara
#endif /* ARA_CORE_VECTOR_H */
