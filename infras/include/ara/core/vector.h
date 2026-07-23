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
template <typename T, typename Allocator> class Vector;

template <typename T, typename Allocator>
bool operator==(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
template <typename T, typename Allocator>
bool operator!=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
template <typename T, typename Allocator>
bool operator<(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
template <typename T, typename Allocator>
bool operator<=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
template <typename T, typename Allocator>
bool operator>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
template <typename T, typename Allocator>
bool operator>=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
template <typename T, typename Allocator>
void swap(Vector<T, Allocator> &lhs, Vector<T, Allocator> &rhs);

/* @SWS_CORE_01301 @brief Vector class template. */
template <typename T, typename Allocator = std::allocator<T>> class Vector {
private:
  std::vector<T, Allocator> m_data;

public:
  friend bool operator==<>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
  friend bool operator!=<>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
  friend bool operator< <>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
  friend bool operator<=<>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
  friend bool operator> <>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
  friend bool operator>=<>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs);
  friend void swap<>(Vector<T, Allocator> &lhs, Vector<T, Allocator> &rhs);
  /* @SWS_CORE_01303 @brief Allocator type. */
  using allocator_type = Allocator;

  /* @SWS_CORE_01311 @brief Constant iterator type. */
  using const_iterator = typename std::vector<T, Allocator>::const_iterator;

  /* @SWS_CORE_01305 @brief Constant pointer type. */
  using const_pointer = typename std::vector<T, Allocator>::const_pointer;

  /* @SWS_CORE_01307 @brief Constant reference type. */
  using const_reference = typename std::vector<T, Allocator>::const_reference;

  /* @SWS_CORE_01313 @brief Constant reverse iterator type. */
  using const_reverse_iterator = typename std::vector<T, Allocator>::const_reverse_iterator;

  /* @SWS_CORE_01309 @brief Difference type. */
  using difference_type = typename std::vector<T, Allocator>::difference_type;

  /* @SWS_CORE_01310 @brief Iterator type. */
  using iterator = typename std::vector<T, Allocator>::iterator;

  /* @SWS_CORE_01304 @brief Pointer type. */
  using pointer = typename std::vector<T, Allocator>::pointer;

  /* @SWS_CORE_01306 @brief Reference type. */
  using reference = typename std::vector<T, Allocator>::reference;

  /* @SWS_CORE_01312 @brief Reverse iterator type. */
  using reverse_iterator = typename std::vector<T, Allocator>::reverse_iterator;

  /* @SWS_CORE_01308 @brief Size type. */
  using size_type = typename std::vector<T, Allocator>::size_type;

  /* @SWS_CORE_01302 @brief Value type. */
  using value_type = T;

  /* @SWS_CORE_01316 @brief Default constructor. */
  Vector() = default;

  explicit Vector(size_t count) : m_data(count) {
  }

  Vector(size_t count, const T &value) : m_data(count, value) {
  }

  /* @SWS_CORE_01324 @brief Constructor from input iterators. */
  template <typename InputIt> Vector(InputIt first, InputIt last) : m_data(first, last) {
  }

  /* @SWS_CORE_01325 @brief Constructor from initializer list. */
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

  /* @SWS_CORE_01362 @brief Construct element in-place at end. */
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
/* @SWS_CORE_01391 @brief Compare for non-equality. */
template <typename T, typename Allocator>
bool operator!=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs) {
  return lhs.m_data != rhs.m_data;
}

/* @SWS_CORE_01392 @brief Compare for less than. */
template <typename T, typename Allocator>
bool operator<(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs) {
  return lhs.m_data < rhs.m_data;
}

/* @SWS_CORE_01393 @brief Compare for less than or equal. */
template <typename T, typename Allocator>
bool operator<=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs) {
  return lhs.m_data <= rhs.m_data;
}

/* @SWS_CORE_01390 @brief Compare for equality. */
template <typename T, typename Allocator>
bool operator==(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs) {
  return lhs.m_data == rhs.m_data;
}

/* @SWS_CORE_01394 @brief Compare for greater than. */
template <typename T, typename Allocator>
bool operator>(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs) {
  return lhs.m_data > rhs.m_data;
}

/* @SWS_CORE_01395 @brief Compare for greater than or equal. */
template <typename T, typename Allocator>
bool operator>=(const Vector<T, Allocator> &lhs, const Vector<T, Allocator> &rhs) {
  return lhs.m_data >= rhs.m_data;
}

/* @SWS_CORE_01396 @brief Swap contents of two Vectors. */
template <typename T, typename Allocator>
void swap(Vector<T, Allocator> &lhs, Vector<T, Allocator> &rhs) {
  lhs.m_data.swap(rhs.m_data);
}

} // namespace core
} // namespace ara
#endif /* ARA_CORE_VECTOR_H */
