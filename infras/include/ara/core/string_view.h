/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ARA_CORE_STRING_VIEW_H
#define ARA_CORE_STRING_VIEW_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <cstddef>
#include <cinttypes>
#include <functional>
#include <string>
#include <cstring>
#include <iterator>
#include <algorithm>

namespace ara {
namespace core {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ CLASS    ] ============================================== */
/** @brief Implements std::string_view in [11] Unless explicitly overriden in the member
 * documentation, members always adhere in behavior to the ISO specification in [11]. */
class StringView final { /* @SWS_CORE_02001 */
public:
  /* @SWS_CORE_02100 */
  using value_type = char;

  /** @SWS_CORE_02105 @brief The value_type of the iterator is const char */
  using const_iterator = const value_type *;

  /** @SWS_CORE_02102 */
  using const_pointer = const value_type *;

  /** @SWS_CORE_02104 */
  using const_reference = const value_type &;

  /** @SWS_CORE_02107 */
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /** @SWS_CORE_02110 */
  using difference_type = std::ptrdiff_t;

  /** @SWS_CORE_02106 */
  using iterator = const_iterator;

  /** @SWS_CORE_02101 */
  using pointer = value_type *;

  /** @SWS_CORE_02103 */
  using reference = value_type &;

  /** @SWS_CORE_02108 */
  using reverse_iterator = const_reverse_iterator;

  /* @SWS_CORE_02109 */
  using size_type = std::size_t;

  /** @SWS_CORE_02117 */
  constexpr StringView(StringView &&other) noexcept = default;

  /** @SWS_CORE_02112 */
  constexpr StringView() noexcept : m_data(nullptr), m_size(0), m_offset(0), m_validSize(0) {
  }

  /** @SWS_CORE_02115 */
  constexpr StringView(const StringView &other) noexcept = default;

  /** @SWS_CORE_02118 */
  constexpr StringView &operator=(StringView &&other) noexcept = default;

  /** @SWS_CORE_02116 */
  constexpr StringView &operator=(const StringView &other) noexcept = default;

  /** @SWS_CORE_02119 */
  ~StringView() noexcept = default;

  /** @SWS_CORE_02114 */
  constexpr StringView(const char *str, size_type len) noexcept
    : m_data(str), m_size(len), m_offset(0), m_validSize(len) {
  }

  /** @SWS_CORE_02113 */
  constexpr StringView(const char *str) noexcept
    : m_data(str), m_size(len(str)), m_offset(0), m_validSize(m_size) {
  }

  /** @SWS_CORE_02133 */
  constexpr const_reference at(size_type pos) const noexcept {
    return m_data[m_offset + pos];
  }

  /** @SWS_CORE_02135 */
  constexpr const_reference back() const noexcept {
    return at(size() - 1);
  }

  /** @SWS_CORE_02120 */
  constexpr const_iterator begin() const noexcept {
    return &at(0);
  }

  /** @SWS_CORE_02122 */
  constexpr const_iterator cbegin() const noexcept {
    return &at(0);
  }

  /** @SWS_CORE_02123 */
  constexpr const_iterator cend() const noexcept {
    return &at(size() - 1);
  }

  /** @SWS_CORE_02147 */
  constexpr int compare(size_type pos1, size_type n1, const char *s, size_type n2) const noexcept {
    int rslt = 0;

    if (pos1 >= size()) {
      rslt = strncmp("", (s != nullptr) ? s : "", n2);
    } else {
      size_type avail = size() - pos1;
      size_type len1 = (n1 > avail) ? avail : n1;

      rslt = strncmp(begin() + pos1, (s != nullptr) ? s : "", std::min(len1, n2));
      if (0 == rslt) {
        rslt = len1 - n2;
      }
    }
    return rslt;
  }

  /** @SWS_CORE_02143 */
  constexpr int compare(size_type pos1, size_type n1, StringView s) const noexcept {
    return compare(pos1, n1, s.begin(), s.size());
  }

  /** @SWS_CORE_02142 */
  constexpr int compare(StringView s) const noexcept {
    return compare(0, size(), s.begin(), s.size());
  }

  /** @SWS_CORE_02145 */
  constexpr int compare(const char *s) const noexcept {
    return compare(0, size(), s, len(s));
  }

  /** @SWS_CORE_02146 */
  constexpr int compare(size_type pos1, size_type n1, const char *s) const noexcept {
    return compare(pos1, n1, s, len(s));
  }

  /** @SWS_CORE_02144 */
  constexpr int compare(size_type pos1, size_type n1, StringView s, size_type pos2,
                        size_type n2) const noexcept {
    int rslt = 0;
    if (pos2 >= s.size()) {
      rslt = -1;
    } else if ((pos2 + n2) >= s.size()) {
      rslt = -1;
    } else {
      rslt = compare(pos1, n1, &s.at(pos2), n2);
    }

    return rslt;
  }

  /** @SWS_CORE_02154 */
  constexpr bool contains(StringView sv) const noexcept {
    return find(sv) != npos;
  }

  /** @SWS_CORE_02156 */
  constexpr bool contains(const char *str) const noexcept {
    return find(str) != npos;
  }

  /** @SWS_CORE_02155 */
  constexpr bool contains(char c) const noexcept {
    return find(c) != npos;
  }

  /** @SWS_CORE_02140 */
  size_type copy(char *s, size_type n, size_type pos = 0) const noexcept {
    size_type rslt = 0;
    if (pos >= size()) {
      rslt = 0;
    } else {
      size_type available = size() - pos;
      size_type rslt = (n < available) ? n : available;

      if (rslt > 0) {
        std::memcpy(s, &at(pos), rslt);
      }
    }

    return rslt;
  }

  /** @SWS_CORE_02126 */
  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  /** @SWS_CORE_02127 */
  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(begin());
  }

  /** @SWS_CORE_02136 */
  constexpr const_pointer data() const noexcept {
    return &at(0);
  }

  /** @SWS_CORE_02131 */
  constexpr bool empty() const noexcept {
    return 0 == size();
  }

  /** @SWS_CORE_02121 */
  constexpr const_iterator end() const noexcept {
    return &at(size() - 1);
  }

  /** @SWS_CORE_02151 */
  constexpr bool ends_with(StringView sv) const noexcept {
    bool rslt = false;
    if ((nullptr != data()) && (sv.data() != nullptr)) {
      size_type alen = size();
      size_type blen = sv.size();
      if (blen <= alen) {
        int scmpRslt = strncmp(data() + alen - blen, sv.data(), blen);
        if (0 == scmpRslt) {
          rslt = true;
        }
      }
    }
    return rslt;
  }

  /** @SWS_CORE_02152 */
  constexpr bool ends_with(char c) const noexcept {
    return ends_with(StringView(&c, 1));
  }

  /** @SWS_CORE_02153 */
  constexpr bool ends_with(const char *str) const noexcept {
    return ends_with(StringView(str));
  }

  /** @SWS_CORE_02160 */
  constexpr size_type find(const char *s, size_type pos = 0) const noexcept {
    return find(s, pos, len(s));
  }

  /** @SWS_CORE_02158 */
  constexpr size_type find(char c, size_type pos = 0) const noexcept {
    return find(&c, pos, 1);
  }

  /** @SWS_CORE_02157 */
  constexpr size_type find(StringView s, size_type pos = 0) const noexcept {
    return find(s.begin(), pos, s.size());
  }

  /** @SWS_CORE_02159 */
  constexpr size_type find(const char *s, size_type pos, size_type n) const noexcept {
    size_type rslt = npos;
    if (s == nullptr) {
      rslt = npos;
    } else if (pos >= size()) {
      rslt = npos;
    } else if (n == 0) {
      rslt = pos; /* Empty string is always found */
    } else {
      const size_type available = size() - pos;
      if (n > available) {
        rslt = npos;
      } else {
        for (size_type i = pos; i <= size() - n; ++i) {
          if (std::memcmp(&at(i), s, n) == 0) {
            rslt = i; /*  Found match at index i */
          }
        }
      }
    }

    return rslt;
  }

  /** @SWS_CORE_02173 */
  constexpr size_type find_first_not_of(StringView s, size_type pos = 0) const noexcept {
    return find_first_not_of(s.data(), pos, s.size());
  }

  /** @SWS_CORE_02176 */
  constexpr size_type find_first_not_of(const char *s, size_type pos = 0) const noexcept {
    return find_first_not_of(s, pos, len(s));
  }

  /** @SWS_CORE_02175 */
  constexpr size_type find_first_not_of(const char *s, size_type pos, size_type n) const noexcept {
    size_type rslt = npos;

    if (s == nullptr) {
      rslt = npos;
    } else if (pos >= size()) {
      rslt = npos;
    } else {
      bool lookup[256] = {false};
      for (size_type j = 0; j < n; j++) {
        unsigned char c = static_cast<unsigned char>(s[j]);
        lookup[c] = true;
      }
      for (size_type i = pos; i < size(); i++) {
        unsigned char c = static_cast<unsigned char>(at(i));
        if (false == lookup[c]) {
          rslt = i;
          break;
        }
      }
    }

    return rslt;
  }

  /** @SWS_CORE_02174 */
  constexpr size_type find_first_not_of(char c, size_type pos = 0) const noexcept {
    return find_first_not_of(&c, pos, 1);
  }

  /** @SWS_CORE_02166 */
  constexpr size_type find_first_of(char c, size_type pos = 0) const noexcept {
    return find_first_of(&c, pos, 1);
  }

  /** @SWS_CORE_02168 */
  constexpr size_type find_first_of(const char *s, size_type pos = 0) const noexcept {
    return find_first_of(s, pos, len(s));
  }

  /** @SWS_CORE_02167 */
  constexpr size_type find_first_of(const char *s, size_type pos, size_type n) const noexcept {
    size_type rslt = npos;
    if (pos >= size()) {
      rslt = npos;
    } else {
      bool lookup[256] = {false};

      for (size_type j = 0; j < n; j++) {
        unsigned char c = static_cast<unsigned char>(s[j]);
        lookup[c] = true;
      }

      for (size_type i = pos; i < size(); i++) {
        unsigned char c = static_cast<unsigned char>(at(i));
        if (true == lookup[c]) {
          rslt = i;
          break;
        }
      }
    }

    return rslt;
  }

  /** @SWS_CORE_02165 */
  constexpr size_type find_first_of(StringView s, size_type pos = 0) const noexcept {
    return find_first_of(s.data(), pos, s.size());
  }

  /** @SWS_CORE_02177 */
  constexpr size_type find_last_not_of(StringView s, size_type pos = npos) const noexcept {
    return find_last_not_of(s.data(), pos, s.size());
  }

  /** @SWS_CORE_02180 */
  constexpr size_type find_last_not_of(const char *s, size_type pos = npos) const noexcept {
    return find_last_not_of(s, pos, len(s));
  }

  /** @SWS_CORE_02178 */
  constexpr size_type find_last_not_of(char c, size_type pos = npos) const noexcept {
    return find_last_not_of(&c, pos, 1);
  }

  /** @SWS_CORE_02179 */
  constexpr size_type find_last_not_of(const char *s, size_type pos, size_type n) const noexcept {
    size_type rslt = npos;
    if ((data() != nullptr) && (nullptr != s)) {
      size_type i = (pos >= size()) ? size() - 1 : pos;
      for (; i != npos; --i) {
        char c = at(i);
        bool found = false;
        for (size_type j = 0; j < n; ++j) {
          if (s[j] == c) {
            found = true;
            break;
          }
        }
        if (false == found) {
          rslt = i;
          break;
        }
      }
    }

    return rslt;
  }

  /** @SWS_CORE_02170 */
  constexpr size_type find_last_of(char c, size_type pos = npos) const noexcept {
    return find_last_of(&c, pos, 1);
  }

  /** @SWS_CORE_02171 */
  constexpr size_type find_last_of(const char *s, size_type pos, size_type n) const noexcept {
    size_type rslt = npos;
    if ((data() != nullptr) && (nullptr != s)) {
      size_type i = (pos >= size()) ? size() - 1 : pos;
      for (; i != npos; --i) {
        char c = at(i);
        for (size_type j = 0; j < n; ++j) {
          if (s[j] == c) {
            rslt = i;
            break;
          }
        }
        if (npos != rslt) {
          break;
        }
      }
    }

    return rslt;
  }

  /** @SWS_CORE_02169 */
  constexpr size_type find_last_of(StringView s, size_type pos = npos) const noexcept {
    return find_last_of(s.data(), pos, s.size());
  }

  /** @SWS_CORE_02172 */
  constexpr size_type find_last_of(const char *s, size_type pos = npos) const noexcept {
    return find_last_of(s, pos, len(s));
  }

  /** @SWS_CORE_02134 */
  constexpr const_reference front() const noexcept {
    return at(0);
  }

  /** @SWS_CORE_02129 */
  constexpr size_type length() const noexcept {
    return size();
  }

  /** @SWS_CORE_02130 */
  constexpr size_type max_size() const noexcept {
    return m_size;
  }

  /** @SWS_CORE_02132 */
  constexpr const_reference operator[](size_type pos) const noexcept {
    return at(pos);
  }

  /** @SWS_CORE_02124 */
  constexpr const_reverse_iterator rbegin() const noexcept;

  /** @SWS_CORE_02137 */
  constexpr void remove_prefix(size_type n) noexcept;

  /** @SWS_CORE_02138 */
  constexpr void remove_suffix(size_type n) noexcept;

  /** @SWS_CORE_02125 */
  constexpr const_reverse_iterator rend() const noexcept;

  /** @SWS_CORE_02164 */
  constexpr size_type rfind(const char *s, size_type pos = npos) const noexcept;

  /** @SWS_CORE_02163 */
  constexpr size_type rfind(const char *s, size_type pos, size_type n) const noexcept;

  /** @SWS_CORE_02162 */
  constexpr size_type rfind(char c, size_type pos = npos) const noexcept;

  /** @SWS_CORE_02161 */
  constexpr size_type rfind(StringView s, size_type pos = npos) const noexcept;

  /** @SWS_CORE_02128 */
  constexpr size_type size() const noexcept {
    return m_validSize;
  }

  /** @SWS_CORE_02148 */
  constexpr bool starts_with(StringView sv) const noexcept {
    bool rslt = false;
    if ((nullptr != data()) && (sv.data() != nullptr)) {
      size_type alen = size();
      size_type blen = sv.size();
      if (blen <= alen) {
        int scmpRslt = strncmp(data(), sv.data(), blen);
        if (0 == scmpRslt) {
          rslt = true;
        }
      }
    }
    return rslt;
  }

  /** @SWS_CORE_02150 */
  constexpr bool starts_with(const char *str) const noexcept {
    return starts_with(StringView(str));
  }

  /** @SWS_CORE_02149 */
  constexpr bool starts_with(char c) const noexcept {
    return starts_with(StringView(&c, 1));
  }

  /** @SWS_CORE_02141 */
  constexpr StringView substr(size_type pos = 0, size_type n = npos) const noexcept {
    StringView rslt;
    if (pos < size()) {
      const size_type max_available = size() - pos;
      const size_type count = ((n == npos) || (n > max_available)) ? max_available : n;
      rslt = StringView(data() + pos, count);
    }

    return rslt;
  }

  /** @SWS_CORE_02139 */
  constexpr void swap(StringView &other) noexcept {
    StringView tmp = other;
    other = *this;
    *this = tmp;
  }

public:
  /** @SWS_CORE_02111 */
  static constexpr size_type npos = size_type(-1);

private:
  constexpr size_type len(const char *s) const noexcept {
    size_type rslt = 0;
    if (nullptr != s) {
      rslt = std::strlen(s);
    }
    return rslt;
  }

private:
  const value_type *m_data;
  size_type m_size;

  size_type m_offset;
  size_type m_validSize;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} // namespace core
} // namespace ara

namespace std {
template <> struct hash<ara::core::StringView> final { /* @SWS_CORE_02189 */
public:
  /** @SWS_CORE_02190 */
  size_t operator()(ara::core::StringView const &v) const noexcept;
};
};     // namespace std
#endif /* ARA_CORE_STRING_VIEW_H */
