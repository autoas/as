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

void *memset(void *__s, int __c, size_t __n) {
  size_t i;
  char *ptr = (char *)__s;

  for (i = 0; i < __n; i++) {
    ptr[i] = (char)(__c & 0xFFu);
  }

  return __s;
}

void *memcpy(void *__to, const void *__from, size_t __size) {
  size_t i;
  char *dst = (char *)__to;
  const char *src = (const char *)__from;

  for (i = 0; i < __size; i++) {
    dst[i] = src[i];
  }

  return __to;
}

void *memmove(void *dest, const void *src, size_t len) {
  char *d = dest;
  const char *s = src;

  if (d < s) {
    while (len--) {
      *d++ = *s++;
    }
  } else {
    const char *lasts = s + (len - 1);
    char *lastd = d + (len - 1);
    while (len--)
      *lastd-- = *lasts--;
  }

  return dest;
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