/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __HEAP_H__
#define __HEAP_H__
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void heap_init(void);
void *heap_malloc(size_t size);
void heap_free(void *pMem);
size_t heap_free_size(void);
void *heap_memalign(size_t alignment, size_t size);
#if !defined(linux) && !defined(_WIN32)
void *malloc(size_t sz);
void free(void *ptr);
void *calloc(size_t nitems, size_t size);
void *kzmalloc(size_t size);
void *memalign(size_t alignment, size_t size);
#endif
#endif /* __HEAP_H__ */
