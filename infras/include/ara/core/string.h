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

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/** @SWS_CORE_03001 */
using String = BasicString<>;
/* ================================ [ CLASS    ] ============================================== */
/** @brief BasicString type */
template <typename Allocator = std::allocator<char>> class BasicString final { /* @SWS_CORE_03000 */
public:
  /* @SWS_CORE_00072 */
  using const_iterator = const char *;

  /* @SWS_CORE_00071 */
  using iterator = char *;

  /* @SWS_CORE_03012 */
  using size_type = std::size_t;

  /* @SWS_CORE_03302 */
  explicit BasicString(StringView sv);

  /* @SWS_CORE_03303 */
  template <typename T>
  BasicString(const T &t, size_type pos, size_type n, const Allocator &alloc = Allocator());

  /** @SWS_CORE_03309 @brief Concatenation from implicit StringView. */
  template <typename T> BasicString &append(const T &t, size_type pos, size_type n = npos);

  /** @SWS_CORE_03308 @brief Concatenation from StringView */
  BasicString &append(StringView sv);

  /** @SWS_CORE_03305 @brief Assignment from StringView. */
  BasicString &assign(StringView sv);

  /** @SWS_CORE_03306 @brief Assignment from implicit StringView. */
  template <typename T> BasicString &assign(const T &t, size_type pos, size_type n = npos);

  /** @SWS_CORE_03321 */
  int compare(StringView sv) const noexcept;

  /** @SWS_CORE_03322 */
  int compare(size_type pos1, size_type n1, StringView sv) const;

  /** @SWS_CORE_03323 */
  template <typename T>
  int compare(size_type pos1, size_type n1, const T &t, size_type pos2, size_type n2 = npos) const;

  /** @SWS_CORE_03315 */
  size_type find(StringView sv, size_type pos = 0) const noexcept;

  /** @SWS_CORE_03319 */
  size_type find_first_not_of(StringView sv, size_type pos = 0) const noexcept;

  /** @SWS_CORE_03317 */
  size_type find_first_of(StringView sv, size_type pos = 0) const noexcept;

  /** @SWS_CORE_03320 */
  size_type find_last_not_of(StringView sv, size_type pos = npos) const noexcept;

  /** @SWS_CORE_03318 */
  size_type find_last_of(StringView sv, size_type pos = npos) const noexcept;

  /** @SWS_CORE_03311 */
  template <typename T>
  BasicString &insert(size_type pos1, const T &t, size_type pos2, size_type n = npos);

  /** @SWS_CORE_03310 */
  BasicString &insert(size_type pos, StringView sv);

  /** @SWS_CORE_03301 */
  operator StringView() const noexcept;

  /** @SWS_CORE_03307 */
  BasicString &operator+=(StringView sv);

  /** @SWS_CORE_03304 */
  BasicString &operator=(StringView sv);

  /** @SWS_CORE_03314 */
  BasicString &replace(const_iterator i1, const_iterator i2, StringView sv);

  /** @SWS_CORE_03313 */
  template <typename T>
  BasicString &replace(size_type pos1, size_type n1, const T &t, size_ type pos2,
                       size_type n2 = npos);

  /** @SWS_CORE_03312 */
  BasicString &replace(size_type pos1, size_type n1, StringView sv);

  /** @SWS_CORE_03316 */
  size_type rfind(StringView sv, size_type pos = npos) const noexcept;

public:
  /** @SWS_CORE_00073 */
  static constexpr size_type const npos = size_type(-1);
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/** @SWS_CORE_03296 @brief Exchange the state of lhs with that of rhs */
template <typename Allocator> void swap(BasicString<Allocator> &lhs, BasicString<Allocator> &rhs);

} // namespace core
} // namespace ara
#endif /* ARA_CORE_STRING_H */
