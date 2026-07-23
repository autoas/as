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
#include <type_traits>
#include <utility>
#include <algorithm>

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/** @SWS_CORE_01100 @brief Tag type for empty Optional. */
struct nullopt_t {
  explicit nullopt_t() = default;
};

/** @SWS_CORE_01101 @brief Tag value for empty Optional. */
constexpr nullopt_t nullopt{};

/* ================================ [ CLASS    ] ============================================== */
/** @SWS_CORE_01033 @brief Implements std::optional (see [optional] in [11]). */
template <typename T> class Optional final {
public:
  using value_type = T; /* @SWS_CORE_01102 @brief Value type alias. */

  /** @SWS_CORE_01103 @brief Default constructor. */
  constexpr Optional() noexcept : m_hasValue(false) {}

  /** @SWS_CORE_01104 @brief Constructor from nullopt. */
  constexpr Optional(nullopt_t) noexcept : m_hasValue(false) {}

  /** @SWS_CORE_01105 @brief Copy constructor. */
  constexpr Optional(const Optional &other) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    if (other.m_hasValue) {
      new (&m_value) T(other.m_value);
      m_hasValue = true;
    } else {
      m_hasValue = false;
    }
  }

  /** @SWS_CORE_01106 @brief Move constructor. */
  constexpr Optional(Optional &&other) noexcept(std::is_nothrow_move_constructible<T>::value) {
    if (other.m_hasValue) {
      new (&m_value) T(std::move(other.m_value));
      m_hasValue = true;
    } else {
      m_hasValue = false;
    }
  }

  /** @SWS_CORE_01111 @brief Constructor from value (rvalue). */
  constexpr Optional(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value)
      : m_hasValue(true) {
    new (&m_value) T(std::move(value));
  }

  /** @SWS_CORE_01107 @brief Constructor from value (lvalue). */
  constexpr Optional(const T &value) noexcept(std::is_nothrow_copy_constructible<T>::value)
      : m_hasValue(true) {
    new (&m_value) T(value);
  }

  /** @SWS_CORE_01110 @brief Copy constructor from Optional<U>. */
  template <typename U>
  explicit Optional(const Optional<U> &other) noexcept(
    std::is_nothrow_constructible<T, const U &>::value) {
    if (other.has_value()) {
      new (&m_value) T(*other);
      m_hasValue = true;
    } else {
      m_hasValue = false;
    }
  }

  /** @SWS_CORE_01108 @brief Move constructor from Optional<U>. */
  template <typename U>
  explicit Optional(Optional<U> &&other) noexcept(
    std::is_nothrow_constructible<T, U &&>::value) {
    if (other.has_value()) {
      new (&m_value) T(std::move(*other));
      m_hasValue = true;
      other.reset();
    } else {
      m_hasValue = false;
    }
  }

  /** @SWS_CORE_01112 @brief Destructor. */
  ~Optional() noexcept {
    if (m_hasValue) {
      m_value.~T();
    }
  }

  /** @SWS_CORE_01114 @brief Copy assignment. */
  constexpr Optional &operator=(const Optional &other) noexcept(
    std::is_nothrow_copy_assignable<T>::value && std::is_nothrow_copy_constructible<T>::value) {
    if (m_hasValue && other.m_hasValue) {
      m_value = other.m_value;
    } else if (!m_hasValue && other.m_hasValue) {
      new (&m_value) T(other.m_value);
      m_hasValue = true;
    } else if (m_hasValue && !other.m_hasValue) {
      m_value.~T();
      m_hasValue = false;
    }
    return *this;
  }

  /** @SWS_CORE_01115 @brief Move assignment. */
  constexpr Optional &operator=(Optional &&other) noexcept(
    std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value) {
    if (m_hasValue && other.m_hasValue) {
      m_value = std::move(other.m_value);
    } else if (!m_hasValue && other.m_hasValue) {
      new (&m_value) T(std::move(other.m_value));
      m_hasValue = true;
      other.m_hasValue = false;
    } else if (m_hasValue && !other.m_hasValue) {
      m_value.~T();
      m_hasValue = false;
    }
    return *this;
  }

  /** @SWS_CORE_01117 @brief Assignment from nullopt. */
  constexpr Optional &operator=(nullopt_t) noexcept {
    if (m_hasValue) {
      m_value.~T();
      m_hasValue = false;
    }
    return *this;
  }

  /** @SWS_CORE_01118 @brief Assignment from value. */
  template <typename U,
            typename = typename std::enable_if<
              !std::is_same<typename std::decay<U>::type, Optional>::value>::type>
  constexpr Optional &operator=(U &&value) noexcept(
    std::is_nothrow_assignable<T &, U &&>::value && std::is_nothrow_constructible<T, U &&>::value) {
    if (m_hasValue) {
      m_value = std::forward<U>(value);
    } else {
      new (&m_value) T(std::forward<U>(value));
      m_hasValue = true;
    }
    return *this;
  }

  /** @SWS_CORE_01119 @brief Emplace value in-place. */
  template <typename... Args>
  constexpr T &emplace(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args...>::value) {
    if (m_hasValue) {
      m_value.~T();
    }
    new (&m_value) T(std::forward<Args>(args)...);
    m_hasValue = true;
    return m_value;
  }

  /** @SWS_CORE_01136 @brief Reset to empty state. */
  constexpr void reset() noexcept {
    if (m_hasValue) {
      m_value.~T();
      m_hasValue = false;
    }
  }

  /** @SWS_CORE_01121 @brief Swap with another Optional. */
  constexpr void swap(Optional &other) noexcept(
    std::is_nothrow_move_constructible<T>::value && noexcept(std::swap(std::declval<T &>(), std::declval<T &>()))) {
    if (m_hasValue && other.m_hasValue) {
      std::swap(m_value, other.m_value);
    } else if (!m_hasValue && other.m_hasValue) {
      new (&m_value) T(std::move(other.m_value));
      m_hasValue = true;
      other.m_value.~T();
      other.m_hasValue = false;
    } else if (m_hasValue && !other.m_hasValue) {
      new (&other.m_value) T(std::move(m_value));
      other.m_hasValue = true;
      m_value.~T();
      m_hasValue = false;
    }
  }

  /** @SWS_CORE_01129 @brief Check whether *this contains a value. */
  constexpr bool has_value() const noexcept { return m_hasValue; }

  /** @SWS_CORE_01128 @brief Check whether *this contains a value. */
  constexpr explicit operator bool() const noexcept { return m_hasValue; }

  /** @SWS_CORE_01125 @brief Access the contained value (lvalue). */
  constexpr T &operator*() & noexcept { return m_value; }

  /** @SWS_CORE_01127 @brief Access the contained value (const lvalue). */
  constexpr const T &operator*() const & noexcept { return m_value; }

  /** @SWS_CORE_01124 @brief Access the contained value (rvalue). */
  constexpr T &&operator*() && noexcept { return std::move(m_value); }

  /** @SWS_CORE_01126 @brief Access the contained value (const rvalue). */
  constexpr const T &&operator*() const && noexcept { return std::move(m_value); }

  /** @SWS_CORE_01122 @brief Access the contained value (lvalue). */
  constexpr T *operator->() noexcept { return &m_value; }

  /** @SWS_CORE_01123 @brief Access the contained value (const lvalue). */
  constexpr const T *operator->() const noexcept { return &m_value; }

  /** @SWS_CORE_01130 @brief Return the contained value (const lvalue). */
  constexpr const T &value() const & noexcept(false) {
    if (!m_hasValue) {
      throw std::bad_optional_access();
    }
    return m_value;
  }

  /** @SWS_CORE_01131 @brief Return the contained value (lvalue). */
  constexpr T &value() & noexcept(false) {
    if (!m_hasValue) {
      throw std::bad_optional_access();
    }
    return m_value;
  }

  /** @SWS_CORE_01132 @brief Return the contained value (const rvalue). */
  constexpr const T &&value() const && noexcept(false) {
    if (!m_hasValue) {
      throw std::bad_optional_access();
    }
    return std::move(m_value);
  }

  /** @SWS_CORE_01133 @brief Return the contained value (rvalue). */
  constexpr T &&value() && noexcept(false) {
    if (!m_hasValue) {
      throw std::bad_optional_access();
    }
    return std::move(m_value);
  }

  /** @SWS_CORE_01135 @brief Return the contained value or default (const lvalue). */
  template <typename U>
  constexpr T value_or(U &&defaultValue) const & noexcept(
    std::is_nothrow_copy_constructible<T>::value && std::is_nothrow_constructible<T, U &&>::value) {
    if (m_hasValue) {
      return m_value;
    }
    return static_cast<T>(std::forward<U>(defaultValue));
  }

  /** @SWS_CORE_01135 @brief Return the contained value or default (rvalue). */
  template <typename U>
  constexpr T value_or(U &&defaultValue) && noexcept(
    std::is_nothrow_move_constructible<T>::value && std::is_nothrow_constructible<T, U &&>::value) {
    if (m_hasValue) {
      return std::move(m_value);
    }
    return static_cast<T>(std::forward<U>(defaultValue));
  }

private:
  bool m_hasValue = false;
  union {
    T m_value;
  };
};

/* ================================ [ NON-MEMBER FUNCTIONS ] ==================================== */
/** @SWS_CORE_01121 @brief Swap two Optional instances. */
template <typename T>
void swap(Optional<T> &lhs, Optional<T> &rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

/** @SWS_CORE_01140 @brief Compare Optional for equality. */
template <typename T>
constexpr bool operator==(const Optional<T> &lhs, const Optional<T> &rhs) noexcept {
  if (lhs.has_value() != rhs.has_value()) return false;
  if (!lhs.has_value()) return true;
  return *lhs == *rhs;
}

/** @SWS_CORE_01141 @brief Compare Optional for inequality. */
template <typename T>
constexpr bool operator!=(const Optional<T> &lhs, const Optional<T> &rhs) noexcept {
  return !(lhs == rhs);
}

/** @SWS_CORE_01142 @brief Compare Optional for less than. */
template <typename T>
constexpr bool operator<(const Optional<T> &lhs, const Optional<T> &rhs) noexcept {
  if (!rhs.has_value()) return false;
  if (!lhs.has_value()) return true;
  return *lhs < *rhs;
}

/** @SWS_CORE_01143 @brief Compare Optional for greater than. */
template <typename T>
constexpr bool operator>(const Optional<T> &lhs, const Optional<T> &rhs) noexcept {
  return rhs < lhs;
}

/** @SWS_CORE_01144 @brief Compare Optional for less than or equal. */
template <typename T>
constexpr bool operator<=(const Optional<T> &lhs, const Optional<T> &rhs) noexcept {
  return !(rhs < lhs);
}

/** @SWS_CORE_01145 @brief Compare Optional for greater than or equal. */
template <typename T>
constexpr bool operator>=(const Optional<T> &lhs, const Optional<T> &rhs) noexcept {
  return !(lhs < rhs);
}

/** @SWS_CORE_01146 @brief Compare Optional with nullopt for equality. */
template <typename T>
constexpr bool operator==(const Optional<T> &opt, nullopt_t) noexcept {
  return !opt.has_value();
}

/** @SWS_CORE_01146 @brief Compare nullopt with Optional for equality. */
template <typename T>
constexpr bool operator==(nullopt_t, const Optional<T> &opt) noexcept {
  return !opt.has_value();
}

/** @SWS_CORE_01147 @brief Compare Optional with nullopt for inequality. */
template <typename T>
constexpr bool operator!=(const Optional<T> &opt, nullopt_t) noexcept {
  return opt.has_value();
}

/** @SWS_CORE_01147 @brief Compare nullopt with Optional for inequality. */
template <typename T>
constexpr bool operator!=(nullopt_t, const Optional<T> &opt) noexcept {
  return opt.has_value();
}

/** @SWS_CORE_01148 @brief Compare Optional with nullopt for less than. */
template <typename T>
constexpr bool operator<(const Optional<T> &opt, nullopt_t) noexcept {
  return false;
}

/** @SWS_CORE_01148 @brief Compare nullopt with Optional for less than. */
template <typename T>
constexpr bool operator<(nullopt_t, const Optional<T> &opt) noexcept {
  return opt.has_value();
}

/** @SWS_CORE_01149 @brief Compare Optional with nullopt for greater than. */
template <typename T>
constexpr bool operator>(const Optional<T> &opt, nullopt_t) noexcept {
  return opt.has_value();
}

/** @SWS_CORE_01149 @brief Compare nullopt with Optional for greater than. */
template <typename T>
constexpr bool operator>(nullopt_t, const Optional<T> &opt) noexcept {
  return false;
}

/** @SWS_CORE_01150 @brief Compare Optional with nullopt for less than or equal. */
template <typename T>
constexpr bool operator<=(const Optional<T> &opt, nullopt_t) noexcept {
  return !opt.has_value();
}

/** @SWS_CORE_01150 @brief Compare nullopt with Optional for less than or equal. */
template <typename T>
constexpr bool operator<=(nullopt_t, const Optional<T> &opt) noexcept {
  return true;
}

/** @SWS_CORE_01151 @brief Compare Optional with nullopt for greater than or equal. */
template <typename T>
constexpr bool operator>=(const Optional<T> &opt, nullopt_t) noexcept {
  return true;
}

/** @SWS_CORE_01151 @brief Compare nullopt with Optional for greater than or equal. */
template <typename T>
constexpr bool operator>=(nullopt_t, const Optional<T> &opt) noexcept {
  return !opt.has_value();
}

/** @SWS_CORE_01152 @brief Compare Optional with value for equality. */
template <typename T, typename U>
constexpr bool operator==(const Optional<T> &opt, const U &value) noexcept {
  return opt.has_value() && *opt == value;
}

/** @SWS_CORE_01152 @brief Compare value with Optional for equality. */
template <typename T, typename U>
constexpr bool operator==(const T &value, const Optional<U> &opt) noexcept {
  return opt.has_value() && value == *opt;
}

/** @SWS_CORE_01153 @brief Compare Optional with value for inequality. */
template <typename T, typename U>
constexpr bool operator!=(const Optional<T> &opt, const U &value) noexcept {
  return !(opt == value);
}

/** @SWS_CORE_01153 @brief Compare value with Optional for inequality. */
template <typename T, typename U>
constexpr bool operator!=(const T &value, const Optional<U> &opt) noexcept {
  return !(value == opt);
}

/** @SWS_CORE_01154 @brief Compare Optional with value for less than. */
template <typename T, typename U>
constexpr bool operator<(const Optional<T> &opt, const U &value) noexcept {
  return opt.has_value() && *opt < value;
}

/** @SWS_CORE_01154 @brief Compare value with Optional for less than. */
template <typename T, typename U>
constexpr bool operator<(const T &value, const Optional<U> &opt) noexcept {
  return !opt.has_value() || value < *opt;
}

/** @SWS_CORE_01155 @brief Compare Optional with value for greater than. */
template <typename T, typename U>
constexpr bool operator>(const Optional<T> &opt, const U &value) noexcept {
  return !opt.has_value() || *opt > value;
}

/** @SWS_CORE_01155 @brief Compare value with Optional for greater than. */
template <typename T, typename U>
constexpr bool operator>(const T &value, const Optional<U> &opt) noexcept {
  return opt.has_value() && value > *opt;
}

/** @SWS_CORE_01156 @brief Compare Optional with value for less than or equal. */
template <typename T, typename U>
constexpr bool operator<=(const Optional<T> &opt, const U &value) noexcept {
  return !opt.has_value() || *opt <= value;
}

/** @SWS_CORE_01156 @brief Compare value with Optional for less than or equal. */
template <typename T, typename U>
constexpr bool operator<=(const T &value, const Optional<U> &opt) noexcept {
  return opt.has_value() && value <= *opt;
}

/** @SWS_CORE_01157 @brief Compare Optional with value for greater than or equal. */
template <typename T, typename U>
constexpr bool operator>=(const Optional<T> &opt, const U &value) noexcept {
  return opt.has_value() && *opt >= value;
}

/** @SWS_CORE_01157 @brief Compare value with Optional for greater than or equal. */
template <typename T, typename U>
constexpr bool operator>=(const T &value, const Optional<U> &opt) noexcept {
  return !opt.has_value() || value >= *opt;
}

/** @SWS_CORE_01160 @brief Create Optional from value. */
template <typename T>
constexpr Optional<typename std::decay<T>::type> make_optional(T &&value) noexcept(
  std::is_nothrow_constructible<typename std::decay<T>::type, T &&>::value) {
  return Optional<typename std::decay<T>::type>(std::forward<T>(value));
}

/** @SWS_CORE_01161 @brief Create Optional by constructing value in-place. */
template <typename T, typename... Args>
constexpr Optional<T> make_optional(Args &&...args) noexcept(
  std::is_nothrow_constructible<T, Args...>::value) {
  Optional<T> opt;
  opt.emplace(std::forward<Args>(args)...);
  return opt;
}
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara
#endif /* ARA_CORE_OPTIONAL_H */