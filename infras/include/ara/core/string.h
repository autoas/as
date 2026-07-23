/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_STRING_H
#define ARA_CORE_STRING_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ara/core/string_view.h"
#include <memory>
#include <string>

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
template <typename Allocator = std::allocator<char>> class BasicString;
/** @SWS_CORE_03001 @brief String type alias. */
using String = BasicString<>;
/* ================================ [ CLASS    ] ============================================== */
/** @SWS_CORE_03000 @brief BasicString type */
template <typename Allocator> class BasicString final {
public:
  /* @SWS_CORE_03005 @brief Allocator type. */
  using allocator_type = Allocator;

  /* @SWS_CORE_00072 @brief Constant iterator type. */
  using const_iterator = const char *;

  /* @SWS_CORE_03015 @brief Constant pointer type. */
  using const_pointer = const char *;

  /* @SWS_CORE_03017 @brief Constant reference type. */
  using const_reference = const char &;

  /* @SWS_CORE_03019 @brief Constant reverse iterator type. */
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /* @SWS_CORE_03013 @brief Difference type. */
  using difference_type = std::ptrdiff_t;

  /* @SWS_CORE_00071 @brief Iterator type. */
  using iterator = char *;

  /* @SWS_CORE_03014 @brief Pointer type. */
  using pointer = char *;

  /* @SWS_CORE_03016 @brief Reference type. */
  using reference = char &;

  /* @SWS_CORE_03018 @brief Reverse iterator type. */
  using reverse_iterator = std::reverse_iterator<iterator>;

  /* @SWS_CORE_03003 @brief Traits type. */
  using traits_type = std::char_traits<char>;

  /* @SWS_CORE_03012 @brief Size type. */
  using size_type = std::size_t;

  /* @SWS_CORE_03004 @brief Value type. */
  using value_type = char;

  /* @SWS_CORE_03001 @brief Default constructor. */
  BasicString() = default;

  BasicString(const BasicString &) = default;
  BasicString(BasicString &&) = default;
  BasicString &operator=(const BasicString &) = default;
  BasicString &operator=(BasicString &&) = default;

  /* @SWS_CORE_03302 @brief Constructor from StringView. */
  explicit BasicString(StringView sv) : m_data(sv.data(), sv.size()) {
  }

  /* @SWS_CORE_03303 @brief Constructor from substring. */
  template <typename T>
  BasicString(const T &t, size_type pos, size_type n, const Allocator &alloc = Allocator())
    : m_data(alloc) {
    StringView sv(t);
    sv = sv.substr(pos, n);
    m_data.append(sv.data(), sv.size());
  }

  /** @SWS_CORE_03309 @brief Concatenation from implicit StringView. */
  template <typename T>
  BasicString &append(const T &t, size_type pos, size_type n = npos) {
    StringView sv(t);
    sv = sv.substr(pos, n);
    m_data.append(sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03308 @brief Concatenation from StringView */
  BasicString &append(StringView sv) {
    m_data.append(sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03305 @brief Assignment from StringView. */
  BasicString &assign(StringView sv) {
    m_data.assign(sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03306 @brief Assignment from implicit StringView. */
  template <typename T> BasicString &assign(const T &t, size_type pos, size_type n = npos) {
    StringView sv(t);
    sv = sv.substr(pos, n);
    m_data.assign(sv.data(), sv.size());
    return *this;
  }

  /* @SWS_CORE_03069 @brief Returns pointer to null-terminated character array. */
  const char *c_str() const noexcept {
    return m_data.c_str();
  }

  /* @SWS_CORE_03071 @brief Returns pointer to underlying character array (const). */
  const char *data() const noexcept {
    return m_data.data();
  }

  /* @SWS_CORE_03052 @brief Returns the number of characters. */
  size_type size() const noexcept {
    return m_data.size();
  }

  /* @SWS_CORE_03060 @brief Returns the number of characters. */
  size_type length() const noexcept {
    return m_data.size();
  }

  /* @SWS_CORE_03051 @brief Checks if the string is empty. */
  bool empty() const noexcept {
    return m_data.empty();
  }

  /* @SWS_CORE_03039 @brief Returns an iterator to the beginning. */
  iterator begin() noexcept {
    return m_data.data();
  }

  /* @SWS_CORE_03040 @brief Returns a const iterator to the beginning. */
  const_iterator begin() const noexcept {
    return m_data.data();
  }

  /* @SWS_CORE_03041 @brief Returns an iterator to the end. */
  iterator end() noexcept {
    return m_data.data() + m_data.size();
  }

  /* @SWS_CORE_03042 @brief Returns a const iterator to the end. */
  const_iterator end() const noexcept {
    return m_data.data() + m_data.size();
  }

  /* @SWS_CORE_03061 @brief Accesses the specified character. */
  char &operator[](size_type pos) {
    return m_data[pos];
  }

  /* @SWS_CORE_03062 @brief Accesses the specified character (const). */
  const char &operator[](size_type pos) const {
    return m_data[pos];
  }

  /** @SWS_CORE_03321 @brief Compare with StringView. */
  int compare(StringView sv) const noexcept {
    return m_data.compare(0, m_data.size(), sv.data(), sv.size());
  }

  /** @SWS_CORE_03322 @brief Compare substring with StringView. */
  int compare(size_type pos1, size_type n1, StringView sv) const {
    return m_data.compare(pos1, n1, sv.data(), sv.size());
  }

  /** @SWS_CORE_03323 @brief Compare substring with another substring. */
  template <typename T>
  int compare(size_type pos1, size_type n1, const T &t, size_type pos2, size_type n2 = npos) const {
    StringView sv(t);
    sv = sv.substr(pos2, n2);
    return m_data.compare(pos1, n1, sv.data(), sv.size());
  }

  /** @SWS_CORE_03315 @brief Find substring. */
  size_type find(StringView sv, size_type pos = 0) const noexcept {
    return m_data.find(sv.data(), pos, sv.size());
  }

  /** @SWS_CORE_03319 @brief Find first character not in StringView. */
  size_type find_first_not_of(StringView sv, size_type pos = 0) const noexcept {
    return m_data.find_first_not_of(sv.data(), pos, sv.size());
  }

  /** @SWS_CORE_03317 @brief Find first character in StringView. */
  size_type find_first_of(StringView sv, size_type pos = 0) const noexcept {
    return m_data.find_first_of(sv.data(), pos, sv.size());
  }

  /** @SWS_CORE_03320 @brief Find last character not in StringView. */
  size_type find_last_not_of(StringView sv, size_type pos = npos) const noexcept {
    return m_data.find_last_not_of(sv.data(), pos, sv.size());
  }

  /** @SWS_CORE_03318 @brief Find last character in StringView. */
  size_type find_last_of(StringView sv, size_type pos = npos) const noexcept {
    return m_data.find_last_of(sv.data(), pos, sv.size());
  }

  /** @SWS_CORE_03311 @brief Insert substring. */
  template <typename T>
  BasicString &insert(size_type pos1, const T &t, size_type pos2, size_type n = npos) {
    StringView sv(t);
    sv = sv.substr(pos2, n);
    m_data.insert(pos1, sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03310 @brief Insert StringView. */
  BasicString &insert(size_type pos, StringView sv) {
    m_data.insert(pos, sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03301 @brief Conversion to StringView. */
  operator StringView() const noexcept {
    return StringView(m_data.data(), m_data.size());
  }

  /** @SWS_CORE_03307 @brief Append StringView. */
  BasicString &operator+=(StringView sv) {
    m_data.append(sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03304 @brief Assignment from StringView. */
  BasicString &operator=(StringView sv) {
    m_data.assign(sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03314 @brief Replace range with StringView. */
  BasicString &replace(const_iterator i1, const_iterator i2, StringView sv) {
    size_type pos = static_cast<size_type>(i1 - m_data.data());
    size_type n = static_cast<size_type>(i2 - i1);
    return replace(pos, n, sv);
  }

  /** @SWS_CORE_03313 @brief Replace substring with another substring. */
  template <typename T>
  BasicString &replace(size_type pos1, size_type n1, const T &t, size_type pos2,
                       size_type n2 = npos) {
    StringView sv(t);
    sv = sv.substr(pos2, n2);
    m_data.replace(pos1, n1, sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03312 @brief Replace substring with StringView. */
  BasicString &replace(size_type pos1, size_type n1, StringView sv) {
    m_data.replace(pos1, n1, sv.data(), sv.size());
    return *this;
  }

  /** @SWS_CORE_03316 @brief Reverse find substring. */
  size_type rfind(StringView sv, size_type pos = npos) const noexcept {
    return m_data.rfind(sv.data(), pos, sv.size());
  }

  /** @SWS_CORE_03337 @brief Swap contents. */
  void swap(BasicString &other) noexcept {
    m_data.swap(other.m_data);
  }

  /** @SWS_CORE_03055 @brief Returns the number of characters that can be held without reallocation. */
  size_type capacity() const noexcept {
    return m_data.capacity();
  }

  /** @SWS_CORE_03057 @brief Erases all characters. */
  void clear() noexcept {
    m_data.clear();
  }

  /** @SWS_CORE_03053 @brief Returns the maximum number of characters. */
  size_type max_size() const noexcept {
    return m_data.max_size();
  }

  /** @SWS_CORE_03054 @brief Requests that the string capacity be at least the specified number. */
  void reserve(size_type n = 0) {
    m_data.reserve(n);
  }

  /** @SWS_CORE_03056 @brief Reduces memory usage by freeing unused memory. */
  void shrink_to_fit() {
    m_data.shrink_to_fit();
  }

public:
  /** @SWS_CORE_00073 @brief Special value for invalid position. */
  static constexpr size_type const npos = size_type(-1);

private:
  std::basic_string<char, std::char_traits<char>, Allocator> m_data;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/** @SWS_CORE_03296 @brief Exchange the state of lhs with that of rhs */
template <typename Allocator>
void swap(BasicString<Allocator> &lhs, BasicString<Allocator> &rhs) noexcept {
  lhs.swap(rhs);
}

} // namespace core
} // namespace ara
#endif /* ARA_CORE_STRING_H */
