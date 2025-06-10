/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
/* ================================ [ MACROS    ] ============================================== */
/*****************************************************************************/
/* Config */
/*****************************************************************************/
#ifndef PINKIE_CFG_SSCANF_MAX_INT
#ifdef IS_ARCH16
#define PINKIE_CFG_SSCANF_MAX_INT 4
#else
#define PINKIE_CFG_SSCANF_MAX_INT 8
#endif
#endif

/*****************************************************************************/
/* Defines */
/*****************************************************************************/
#if PINKIE_CFG_SSCANF_MAX_INT == 1
#define PINKIE_SSCAN_CHAR_CNT 4
#define PINKIE_SSCANF_INT_T int8_t
#define PINKIE_SSCANF_UINT_T uint8_t
#endif

#if PINKIE_CFG_SSCANF_MAX_INT == 2
#define PINKIE_SSCAN_CHAR_CNT 6
#define PINKIE_SSCANF_INT_T int16_t
#define PINKIE_SSCANF_UINT_T uint16_t
#endif

#if PINKIE_CFG_SSCANF_MAX_INT == 4
#define PINKIE_SSCAN_CHAR_CNT 11
#define PINKIE_SSCANF_INT_T int32_t
#define PINKIE_SSCANF_UINT_T uint32_t
#endif

#if PINKIE_CFG_SSCANF_MAX_INT == 8
#define PINKIE_SSCAN_CHAR_CNT 20
#define PINKIE_SSCANF_INT_T int64_t
#define PINKIE_SSCANF_UINT_T uint64_t
#endif

/* memset/memcpy/memmove copied from newlib 4.1.0 */
#define MEMSET_LBLOCKSIZE (sizeof(long))
#define MEMSET_UNALIGNED(X) ((uintptr_t)X & (MEMSET_LBLOCKSIZE - 1))
#define MEMSET_TOO_SMALL(LEN) ((LEN) < MEMSET_LBLOCKSIZE)

/* Nonzero if either X or Y is not aligned on a "long" boundary.  */
#define MEMCPY_UNALIGNED(X, Y)                                                                     \
  (((uintptr_t)X & (sizeof(long) - 1)) | ((uintptr_t)Y & (sizeof(long) - 1)))

/* How many bytes are copied each iteration of the 4X unrolled loop.  */
#define MEMCPY_BIGBLOCKSIZE (sizeof(long) << 2)

/* How many bytes are copied each iteration of the word copy loop.  */
#define MEMCPY_LITTLEBLOCKSIZE (sizeof(long))

/* Threshhold for punting to the byte copier.  */
#define MEMCPY_TOO_SMALL(LEN) ((LEN) < MEMCPY_BIGBLOCKSIZE)

/* Nonzero if either X or Y is not aligned on a "long" boundary.  */
#define MEMMOVE_UNALIGNED(X, Y)                                                                    \
  (((uintptr_t)X & (sizeof(long) - 1)) | ((uintptr_t)Y & (sizeof(long) - 1)))

/* How many bytes are copied each iteration of the 4X unrolled loop.  */
#define MEMMOVE_BIGBLOCKSIZE (sizeof(long) << 2)

/* How many bytes are copied each iteration of the word copy loop.  */
#define MEMMOVE_LITTLEBLOCKSIZE (sizeof(long))

/* Threshhold for punting to the byte copier.  */
#define MEMMOVE_TOO_SMALL(LEN) ((LEN) < MEMMOVE_BIGBLOCKSIZE)
/*
   Taken from glibc:
   Add the compiler optimization to inhibit loop transformation to library
   calls.  This is used to avoid recursive calls in memset and memmove
   default implementations.
*/
#ifdef __GNUC__
#define __inhibit_loop_to_libcall                                                                  \
  __attribute__((__optimize__("-fno-tree-loop-distribute-patterns")))
#else
#define __inhibit_loop_to_libcall
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static uint32_t IntH(char chr) {
  uint32_t v;
  if ((chr >= '0') && (chr <= '9')) {
    v = chr - '0';
  } else if ((chr >= 'A') && (chr <= 'F')) {
    v = chr - 'A' + 10;
  } else if ((chr >= 'a') && (chr <= 'f')) {
    v = chr - 'a' + 10;
  } else {
    v = (uint32_t)-1; /* -1 to indicate error */
  }

  return v;
}
/** Convert character to integer
 *
 * @returns number or -1 if not a number
 */
static int pinkie_c2i(const char chr,   /**< character */
                      unsigned int base /**< base */
) {
  if (('0' <= chr) && ('9' >= chr)) {
    return (chr - '0');
  }

  if (16 == base) {
    if (('a' <= chr) && ('f' >= chr)) {
      return (chr - 'a' + 10);
    }

    if (('A' <= chr) && ('F' >= chr)) {
      return (chr - 'A' + 10);
    }
  }

  return -1;
}
/** String To Integer
 */
static const char *pinkie_s2i(const char *str,              /**< string */
                              unsigned int width,           /**< width = sizeof(type) */
                              PINKIE_SSCANF_UINT_T num_max, /**< max num value */
                              void *val,                    /**< value */
                              unsigned int flg_neg,         /**< negative flag */
                              unsigned int base             /**< base */
) {
  PINKIE_SSCANF_UINT_T num = 0; /* number */
  PINKIE_SSCANF_UINT_T mul = 1; /* multiplicator */
  PINKIE_SSCANF_UINT_T cur;     /* current number */
  unsigned int cnt = 0;         /* counter */
  const char *str_end = NULL;   /* number end */

  /* detect number type */
  if (0 == base) {
    if (((str[0]) && ('0' == str[0])) && ((str[1]) && ('x' == str[1]))) {
      base = 16;
      str += 2;
    } else {
      base = 10;
    }
  }

  /* count numbers */
  for (; (*str) && (-1 != pinkie_c2i(*str, base)); str++) {
    cnt++;
  }

  /* store number end */
  str_end = str;

  /* check if anything was detected */
  if (!cnt) {
    goto bail;
  }

  /* convert integers */
  while (cnt--) {
    str--;

    /* apply multiplicator to conv result */
    cur = (PINKIE_SSCANF_UINT_T)pinkie_c2i(*str, base) * mul;

    if ((num_max - cur) < num) {
      str_end = 0;
      goto bail;
    }

    num += cur;
    mul *= base;
  }

bail:
  /* convert result to given width */
  if (sizeof(uint8_t) == width) {
    *((uint8_t *)val) = (flg_neg) ? (uint8_t)-num : (uint8_t)num;
  }
#if PINKIE_CFG_SSCANF_MAX_INT >= 2
  else if (sizeof(uint16_t) == width) {
    *((uint16_t *)val) = (flg_neg) ? (uint16_t)-num : (uint16_t)num;
  }
#endif
#if PINKIE_CFG_SSCANF_MAX_INT >= 4
  else if (sizeof(uint32_t) == width) {
    *((uint32_t *)val) = (flg_neg) ? (uint32_t)-num : (uint32_t)num;
  }
#endif
#if PINKIE_CFG_SSCANF_MAX_INT >= 8
  else if (sizeof(uint64_t) == width) {
    *((uint64_t *)val) = (flg_neg) ? (uint64_t)-num : (uint64_t)num;
  }
#endif

  return str_end;
}
/* ================================ [ FUNCTIONS ] ============================================== */
size_t strlen(const char *s) {
  const char *sc;

  for (sc = s; *sc != '\0'; ++sc) /* nothing */
    ;

  return sc - s;
}

void *__inhibit_loop_to_libcall memset(void *m, int c, size_t n) {
  char *s = (char *)m;

#if !defined(PREFER_SIZE_OVER_SPEED) && !defined(__OPTIMIZE_SIZE__)
  unsigned int i;
  unsigned long buffer;
  unsigned long *aligned_addr;
  unsigned int d = c & 0xff; /* To avoid sign extension, copy C to an
          unsigned variable.  */

  while (MEMSET_UNALIGNED(s)) {
    if (n--)
      *s++ = (char)c;
    else
      return m;
  }

  if (!MEMSET_TOO_SMALL(n)) {
    /* If we get this far, we know that n is large and s is word-aligned. */
    aligned_addr = (unsigned long *)s;

    /* Store D into each char sized location in BUFFER so that
       we can set large blocks quickly.  */
    buffer = (d << 8) | d;
    buffer |= (buffer << 16);
    for (i = 32; i < MEMSET_LBLOCKSIZE * 8; i <<= 1)
      buffer = (buffer << i) | buffer;

    /* Unroll the loop.  */
    while (n >= MEMSET_LBLOCKSIZE * 4) {
      *aligned_addr++ = buffer;
      *aligned_addr++ = buffer;
      *aligned_addr++ = buffer;
      *aligned_addr++ = buffer;
      n -= 4 * MEMSET_LBLOCKSIZE;
    }

    while (n >= MEMSET_LBLOCKSIZE) {
      *aligned_addr++ = buffer;
      n -= MEMSET_LBLOCKSIZE;
    }
    /* Pick up the remainder with a bytewise loop.  */
    s = (char *)aligned_addr;
  }

#endif /* not PREFER_SIZE_OVER_SPEED */

  while (n--)
    *s++ = (char)c;

  return m;
}

void *__inhibit_loop_to_libcall memcpy(void *dst0, const void *src0, size_t len0) {
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  char *dst = (char *)dst0;
  char *src = (char *)src0;

  void *save = dst0;

  while (len0--) {
    *dst++ = *src++;
  }

  return save;
#else
  char *dst = dst0;
  const char *src = src0;
  long *aligned_dst;
  const long *aligned_src;

  /* If the size is small, or either SRC or DST is unaligned,
     then punt into the byte copy loop.  This should be rare.  */
  if (!MEMCPY_TOO_SMALL(len0) && !MEMCPY_UNALIGNED(src, dst)) {
    aligned_dst = (long *)dst;
    aligned_src = (long *)src;

    /* Copy 4X long words at a time if possible.  */
    while (len0 >= MEMCPY_BIGBLOCKSIZE) {
      *aligned_dst++ = *aligned_src++;
      *aligned_dst++ = *aligned_src++;
      *aligned_dst++ = *aligned_src++;
      *aligned_dst++ = *aligned_src++;
      len0 -= MEMCPY_BIGBLOCKSIZE;
    }

    /* Copy one long word at a time if possible.  */
    while (len0 >= MEMCPY_LITTLEBLOCKSIZE) {
      *aligned_dst++ = *aligned_src++;
      len0 -= MEMCPY_LITTLEBLOCKSIZE;
    }

    /* Pick up any residual with a byte copier.  */
    dst = (char *)aligned_dst;
    src = (char *)aligned_src;
  }

  while (len0--)
    *dst++ = *src++;

  return dst0;
#endif /* not PREFER_SIZE_OVER_SPEED */
}

/*SUPPRESS 20*/
void *__inhibit_loop_to_libcall memmove(void *dst_void, const void *src_void, size_t length) {
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  char *dst = dst_void;
  const char *src = src_void;

  if (src < dst && dst < src + length) {
    /* Have to copy backwards */
    src += length;
    dst += length;
    while (length--) {
      *--dst = *--src;
    }
  } else {
    while (length--) {
      *dst++ = *src++;
    }
  }

  return dst_void;
#else
  char *dst = dst_void;
  const char *src = src_void;
  long *aligned_dst;
  const long *aligned_src;

  if (src < dst && dst < src + length) {
    /* Destructive overlap...have to copy backwards */
    src += length;
    dst += length;
    while (length--) {
      *--dst = *--src;
    }
  } else {
    /* Use optimizing algorithm for a non-destructive copy to closely
       match memcpy. If the size is small or either SRC or DST is unaligned,
       then punt into the byte copy loop.  This should be rare.  */
    if (!MEMMOVE_TOO_SMALL(length) && !MEMMOVE_UNALIGNED(src, dst)) {
      aligned_dst = (long *)dst;
      aligned_src = (long *)src;

      /* Copy 4X long words at a time if possible.  */
      while (length >= MEMMOVE_BIGBLOCKSIZE) {
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        length -= MEMMOVE_BIGBLOCKSIZE;
      }

      /* Copy one long word at a time if possible.  */
      while (length >= MEMMOVE_LITTLEBLOCKSIZE) {
        *aligned_dst++ = *aligned_src++;
        length -= MEMMOVE_LITTLEBLOCKSIZE;
      }

      /* Pick up any residual with a byte copier.  */
      dst = (char *)aligned_dst;
      src = (char *)aligned_src;
    }

    while (length--) {
      *dst++ = *src++;
    }
  }

  return dst_void;
#endif /* not PREFER_SIZE_OVER_SPEED */
}

void *memchr(const void *s, int c, size_t n) {
  void *rv = NULL;
  const char *s1 = (const char *)s;
  int i;

  for (i = 0; i < n; i++) {
    if (s1[i] == (char)c) {
      rv = (void *)&s1[i];
      break;
    }
  }

  return rv;
}

char *strcpy(char *__to, const char *__from) {
  char *dst = (char *)__to;
  const char *src = (const char *)__from;
  while ('\0' != *src) {
    *dst = *src;
    dst++;
    src++;
  }

  *dst = '\0';
  return __to;
}

char *strcat(char *__to, const char *__from) {
  char *dst = (char *)__to;
  const char *src = (const char *)__from;

  while ('\0' != *dst)
    dst++;

  while ('\0' != *src) {
    *dst = *src;
    dst++;
    src++;
  }

  *dst = '\0';
  return __to;
}

int strcmp(const char *s1, const char *s2) {
  for (; *s1 == *s2; s1++, s2++)
    if (*s1 == '\0') {
      return 0;
    }
  return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

int strncmp(const char *s1, const char *s2, size_t n) {
  for (; (*s1 == *s2) && (n > 0); s1++, s2++, n--)
    if (*s1 == '\0') {
      return 0;
    }

  if (n == 0) {
    return 0;
  }
  return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

int memcmp(const void *s1, const void *s2, size_t n) {
  unsigned char u1, u2;
  const char *p1 = (const char *)s1;
  const char *p2 = (const char *)s2;

  for (; n--; p1++, p2++) {
    u1 = *p1;
    u2 = *p2;
    if (u1 != u2) {
      return (u1 - u2);
    }
  }
  return 0;
}

/* strlcpy & strlcat: cpoy from glib/gstrfuncs.c  */
size_t strlcpy(char *dest, const char *src, size_t dest_size) {
  char *d = dest;
  const char *s = src;
  size_t n = dest_size;

  /* Copy as many bytes as will fit */
  if (n != 0 && --n != 0) {
    do {
      char c = *s++;

      *d++ = c;
      if (c == 0)
        break;
    } while (--n != 0);
  }

  /* If not enough room in dest, add NUL and traverse rest of src */
  if (n == 0) {
    if (dest_size != 0) {
      *d = 0;
    }
    while (*s++)
      ;
  }

  return s - src - 1; /* count does not include NUL */
}

size_t strlcat(char *dest, const char *src, size_t dest_size) {
  char *d = dest;
  const char *s = src;
  size_t bytes_left = dest_size;
  size_t dlength; /* Logically, MIN (strlen (d), dest_size) */

  /* Find the end of dst and adjust bytes left but don't go past end */
  while (*d != 0 && bytes_left-- != 0) {
    d++;
  }
  dlength = d - dest;
  bytes_left = dest_size - dlength;

  if (bytes_left == 0) {
    return dlength + strlen(s);
  }
  while (*s != 0) {
    if (bytes_left != 1) {
      *d++ = *s;
      bytes_left--;
    }
    s++;
  }
  *d = 0;

  return dlength + (s - src); /* count does not include NUL */
}

char *strncpy(char *__dest, const char *__src, size_t __n) {
  strlcpy(__dest, __src, __n + 1);

  return __dest;
}

unsigned long int strtoul(const char *string, char **tailptr, int base) {
  unsigned long int result;
  uint32_t v;
  const char *s;

  result = 0;
  s = string;

  while (*s != 0) {
    v = IntH(*s);
    if ((uint32_t)-1 == v) { /* ignore unknown character */
      s++;
      continue;
    }
    result = result * base + v;
    s++;
  }

  if (tailptr != NULL) {
    *tailptr = (char *)s;
  }

  return result;
}

size_t strnlen(const char *s, size_t maxlen) {
  const char *sc;

  for (sc = s; *sc != '\0'; ++sc) {
    if ((sc - s) >= maxlen) {
      break;
    }
  }

  return sc - s;
}

char *strrchr(const char *s, int c) {
  while (*s != '\0') {
    s++;
    if (*s == c) {
      return (char *)s;
    }
  }

  return NULL;
}

char *strstr(const char *s1, const char *s2) {
  int l1, l2;

  l2 = strlen(s2);
  if (!l2)
    return (char *)s1;
  l1 = strlen(s1);
  while (l1 >= l2) {
    l1--;
    if (!memcmp(s1, s2, l2))
      return (char *)s1;
    s1++;
  }

  return NULL;
}

int atoi(const char *s) {
  return strtoul(s, NULL, 10);
}

/*****************************************************************************/
/** Pinkie Just Enough Sscanf To Work
 *
 * Supports the following formatters:
 *   - %i and %u, both with ll and hh modifiers
 *   - %n
 *   https://github.com/sven/pinkie_sscanf
 */
int sscanf(const char *str, /**< input string */
           const char *fmt, /**< format string */
           ...              /**< variable arguments */
) {
  va_list ap;                  /* variable argument list */
  unsigned int flg_format = 0; /* format flag */
  unsigned int int_width = 0;  /* integer width */
  const char *str_beg = str;   /* string begin */
  int args = 0;                /* parsed arguments counter */
  unsigned int flg_neg = 0;    /* negative flag */

  va_start(ap, fmt);
  for (; (*fmt) && (*str); fmt++) {

    if (flg_format) {

      /* length field */
      if ('h' == *fmt) {
        int_width = (!int_width) ? sizeof(short) : sizeof(char);
        continue;
      }

      if ('l' == *fmt) {
        int_width = (!int_width) ? sizeof(long) : sizeof(long long);
        continue;
      }

      /* handle conversion */
      switch (*fmt) {

      case 'i':

        /* detect negative sign */
        if ('-' == *str) {
          flg_neg = 1;
          fmt--;
          str++;
          continue;
        }

        /* fallthrough to convert number */

      case 'x':
      case 'u':
        /* unsigned integer */
        if (!int_width) {
          str = pinkie_s2i(str, sizeof(unsigned int), UINT_MAX, va_arg(ap, unsigned int *), flg_neg,
                           ('x' == *fmt) ? 16 : 0);
        } else if (sizeof(uint8_t) == int_width) {
          str = pinkie_s2i(str, sizeof(uint8_t), UINT8_MAX, va_arg(ap, uint8_t *), flg_neg,
                           ('x' == *fmt) ? 16 : 0);
        }
#if (PINKIE_CFG_SSCANF_MAX_INT >= 2) && (UINT16_MAX != UINT_MAX)
        else if (sizeof(uint16_t) == int_width) {
          str = pinkie_s2i(str, sizeof(uint16_t), UINT16_MAX, va_arg(ap, uint16_t *), flg_neg,
                           ('x' == *fmt) ? 16 : 0);
        }
#endif
#if (PINKIE_CFG_SSCANF_MAX_INT >= 4) && (UINT32_MAX != UINT_MAX)
        else if (sizeof(uint32_t) == int_width) {
          str = pinkie_s2i(str, sizeof(uint32_t), UINT32_MAX, va_arg(ap, uint32_t *), flg_neg,
                           ('x' == *fmt) ? 16 : 0);
        }
#endif
#ifndef IS_ARCH16
#if (PINKIE_CFG_SSCANF_MAX_INT >= 8) && (UINT64_MAX != UINT_MAX)
        else if (sizeof(uint64_t) == int_width) {
          str = pinkie_s2i(str, sizeof(uint64_t), UINT64_MAX, va_arg(ap, uint64_t *), flg_neg,
                           ('x' == *fmt) ? 16 : 0);
        }
#endif
#endif

        /* reset integer width */
        int_width = 0;

        /* update args */
        args++;

        break;

      case '%':
        /* percent char */
        flg_format = 0;
        goto pinkie_sscanf_match;

      case 'n':
        /* position */
        *(va_arg(ap, int *)) = (int)(str - str_beg);
        break;
      }

      flg_format = 0;
      flg_neg = 0;
      continue;
    }

    if ('%' == *fmt) {
      flg_format = 1;
      continue;
    }

  pinkie_sscanf_match:
    /* string content must match format */
    if (*fmt != *str++) {
      break;
    }
  }
  va_end(ap);

  return args;
}
