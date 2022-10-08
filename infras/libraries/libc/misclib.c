/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
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
