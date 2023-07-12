/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/queue.h>
#include "Std_Debug.h"
#include "Std_Critical.h"
#include "heap.h"
#ifdef USE_SHELL
#include "shell.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef HEAP_TEST
#define AS_LOG_HEAP 1
#else
#define AS_LOG_HEAP 0
#endif

#if defined(linux) || defined(_WIN32)
#ifndef HEAP_TRACK
#define HEAP_TRACK
#endif
#endif

#ifdef HEAP_TEST
#define HEAP_LOCK()
#define HEAP_UNLOCK()
#else
#define HEAP_LOCK() EnterCritical()
#define HEAP_UNLOCK() ExitCritical()
#endif

#define AS_LOG_HEAPE 2

#ifndef HEAP_SYSTEM_BASE_TYPE
#define HEAP_SYSTEM_BASE_TYPE uint64_t
#endif

#ifndef HEAP_MIN_ALIGNED_SIZE
/* must >= max(sizeof(heap_block_t), sizeof(heap_magic_t)) */
#define HEAP_MIN_ALIGNED_SIZE 16
#endif

#define HEAP_ALIGN_BY(x, alignment) (((x) + (alignment)-1) & (~((alignment)-1)))

#define HEAP_ALIGN(x) HEAP_ALIGN_BY(x, HEAP_MIN_ALIGNED_SIZE)

#define HEAP_MAGIC_SIZE HEAP_ALIGN(sizeof(heap_magic_t))

#ifndef HEAP_SIZE
#define HEAP_SIZE (1 * 1024 * 1024)
#endif

#define HEAP_ADDR(addr, offset) (((uint8_t *)(addr)) + offset)
/* ================================ [ TYPES     ] ============================================== */
typedef HEAP_SYSTEM_BASE_TYPE heap_base_t;

/* a heap block is a memory that is free */
typedef struct heap_block_s {
  SLIST_ENTRY(heap_block_s) entry;
  size_t size;
} heap_block_t;

typedef struct heap_magic_s {
#ifdef HEAP_TRACK
  SLIST_ENTRY(heap_magic_s) entry;
#endif
  size_t size;
} heap_magic_t;

typedef struct {
  /* sort heap block from small to large */
  SLIST_HEAD(heap_block_free_s, heap_block_s) free;
#ifdef HEAP_TRACK
  SLIST_HEAD(heap_block_used_s, heap_magic_s) used;
#endif
  uint8_t initialized;
} heap_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static heap_base_t lHeapMem[HEAP_SIZE / sizeof(heap_base_t)];
static heap_t lHeap = {
  SLIST_HEAD_INITIALIZER(free),
#ifdef HEAP_TRACK
  SLIST_HEAD_INITIALIZER(used),
#endif
  0,
};
/* ================================ [ LOCALS    ] ============================================== */
static void heap_add_block(heap_block_t *block) {
  heap_block_t *b;
  heap_block_t *prev = NULL;
  ASLOG(HEAP, ("  Add Heap: %u@%p\n", (uint32_t)block->size, block));
  asAssert(block->size <= HEAP_SIZE);
  asAssert(HEAP_ADDR(block, 0) >= HEAP_ADDR(lHeapMem, 0));
  asAssert(HEAP_ADDR(block, block->size) <= HEAP_ADDR(lHeapMem, HEAP_SIZE));
  SLIST_FOREACH(b, &lHeap.free, entry) {
    if (b->size >= block->size) {
      break;
    }
    prev = b;
  }

  if (NULL == prev) {
    SLIST_INSERT_HEAD(&lHeap.free, block, entry);
  } else {
    SLIST_INSERT_AFTER(prev, block, entry);
  }
#if AS_LOG_HEAP > 0
  SLIST_FOREACH(b, &lHeap.free, entry) {
    ASLOG(HEAP, ("    Heap: %u@%p\n", (uint32_t)b->size, b));
  }
#endif
}

#ifdef USE_SHELL
static int freeFunc(int argc, const char *argv[]) {
  heap_block_t *b;
#ifdef HEAP_TRACK
  heap_magic_t *m;
#endif
  size_t free_size = heap_free_size();
  printf("free %u%%(%ub)\n", (uint32_t)(free_size * 100 / sizeof(lHeapMem)), (uint32_t)free_size);
  SLIST_FOREACH(b, &lHeap.free, entry) {
    printf("  free: %u@%p\n", (uint32_t)b->size, b);
  }
#ifdef HEAP_TRACK
  SLIST_FOREACH(m, &lHeap.used, entry) {
    printf("  used: %u@%p\n", (uint32_t)m->size, m);
  }
#endif
  return 0;
}
SHELL_REGISTER(free, "free - show heap status\n", freeFunc);
#endif

/* ================================ [ FUNCTIONS ] ============================================== */
void heap_init(void) {
  heap_block_t *block;

  if (0 == lHeap.initialized) {
    block = (heap_block_t *)lHeapMem;
    SLIST_INIT(&lHeap.free);
    block->size = sizeof(lHeapMem);
    SLIST_INSERT_HEAD(&lHeap.free, block, entry);
    ASLOG(HEAP, ("Heap: %u@%p\n", (uint32_t)block->size, block));
#ifdef HEAP_TRACK
    SLIST_INIT(&lHeap.used);
#endif
    lHeap.initialized = 1;
  }
}

void *heap_malloc(size_t size) {
  heap_magic_t *pMagic;
  void *pMem = NULL;
  size_t aligned_size = HEAP_ALIGN(size) + HEAP_MAGIC_SIZE;
  size_t left_size;
  heap_block_t *b;
  heap_block_t *prev = NULL;
  heap_block_t *best = NULL;

  HEAP_LOCK();
  if (0 == lHeap.initialized) {
    heap_init();
  }
  ASLOG(HEAP, ("malloc(%u)\n", (uint32_t)size));
  SLIST_FOREACH(b, &lHeap.free, entry) {
    if (b->size >= aligned_size) {
      best = b;
      break;
    }
    prev = b;
  }

  if (best) {
    ASLOG(HEAP, ("  Best Heap: %u@%p\n", (uint32_t)best->size, best));
    pMem = HEAP_ADDR(best, HEAP_MAGIC_SIZE);
    pMagic = (heap_magic_t *)best;

    if (NULL == prev) {
      SLIST_REMOVE_HEAD(&lHeap.free, entry);
    } else {
      SLIST_REMOVE_AFTER(prev, entry);
    }

    left_size = best->size - aligned_size;
    if (left_size >= sizeof(heap_block_t)) {
      b = (heap_block_t *)HEAP_ADDR(best, aligned_size);
      b->size = left_size;
      (void)heap_add_block(b);
      pMagic->size = aligned_size;
    } else {
      pMagic->size = aligned_size + left_size;
    }
    ASLOG(HEAP, ("  malloc(%u@%p)\n", (uint32_t)pMagic->size, HEAP_ADDR(pMem, -HEAP_MAGIC_SIZE)));
#ifdef HEAP_TRACK
    SLIST_INSERT_HEAD(&lHeap.used, pMagic, entry);
#endif
  } else {
    ASLOG(HEAPE, ("  malloc OoM for %u\n", (uint32_t)size));
  }
  HEAP_UNLOCK();

  return pMem;
}

void heap_free(void *pMem) {
  heap_magic_t *pMagic = (heap_magic_t *)HEAP_ADDR(pMem, -HEAP_MAGIC_SIZE);
  size_t size = pMagic->size;
  heap_block_t *block = (heap_block_t *)pMagic;
  heap_block_t *b;
  heap_block_t *prev = NULL;
  heap_block_t *before = NULL;
  heap_block_t *after = NULL;
  heap_block_t *prev_before = NULL;
  heap_block_t *prev_after = NULL;

  HEAP_LOCK();
  if (0 == lHeap.initialized) {
    heap_init();
  }
  ASLOG(HEAP, ("free(%u@%p)\n", (uint32_t)size, block));
  SLIST_FOREACH(b, &lHeap.free, entry) {
    if (HEAP_ADDR(b, b->size) == HEAP_ADDR(block, 0)) {
      asAssert(before == NULL);
      before = b;
      prev_before = prev;
    } else if (HEAP_ADDR(block, size) == HEAP_ADDR(b, 0)) {
      asAssert(after == NULL);
      after = b;
      prev_after = prev;
    }
    if ((before != NULL) && (after != NULL)) {
      break;
    }
    prev = b;
  }

  ASLOG(HEAP, ("  prev_b %u@%p\n", (uint32_t)(prev_before ? prev_before->size : 0), prev_before));
  ASLOG(HEAP, ("  before %u@%p\n", (uint32_t)(before ? before->size : 0), before));
  ASLOG(HEAP, ("  block  %u@%p\n", (uint32_t)size, block));
  ASLOG(HEAP, ("  prev_a %u@%p\n", (uint32_t)(prev_after ? prev_after->size : 0), prev_after));
  ASLOG(HEAP, ("  after  %u@%p\n", (uint32_t)(after ? after->size : 0), after));

#ifdef HEAP_TRACK
  SLIST_REMOVE(&lHeap.used, pMagic, heap_magic_s, entry);
#endif

  if ((NULL != before) && (NULL != after)) {
    ASLOG(HEAP, ("  merge with before %u@%p and after %u@%p\n", (uint32_t)before->size, before,
                 (uint32_t)after->size, after));
    if (NULL == prev_before) {
      SLIST_REMOVE_HEAD(&lHeap.free, entry);
    } else {
      SLIST_REMOVE_AFTER(prev_before, entry);
    }
    if (before == prev_after) {
      prev_after = prev_before;
    }
    if (NULL == prev_after) {
      SLIST_REMOVE_HEAD(&lHeap.free, entry);
    } else {
      SLIST_REMOVE_AFTER(prev_after, entry);
    }
    before->size += size + after->size;
    heap_add_block(before);
  } else if (NULL != before) {
    ASLOG(HEAP, ("  merge with before %u@%p\n", (uint32_t)before->size, before));
    if (NULL == prev_before) {
      SLIST_REMOVE_HEAD(&lHeap.free, entry);
    } else {
      SLIST_REMOVE_AFTER(prev_before, entry);
    }
    before->size += size;
    heap_add_block(before);
  } else if (NULL != after) {
    ASLOG(HEAP, ("  merge with after %u@%p\n", (uint32_t)after->size, after));
    if (NULL == prev_after) {
      SLIST_REMOVE_HEAD(&lHeap.free, entry);
    } else {
      SLIST_REMOVE_AFTER(prev_after, entry);
    }
    block->size = size + after->size;
    heap_add_block(block);
  } else {
    block->size = size;
    heap_add_block(block);
  }
  HEAP_UNLOCK();
}

size_t heap_free_size(void) {
  size_t sz = 0;
  heap_block_t *b;

  HEAP_LOCK();
  if (0 == lHeap.initialized) {
    heap_init();
  }
  SLIST_FOREACH(b, &lHeap.free, entry) {
    sz += b->size;
  }
  HEAP_UNLOCK();

  return sz;
}

void *heap_memalign(size_t alignment, size_t size) {
  heap_magic_t *pMagic;
  void *pMem;
  size_t aligned_size = HEAP_ALIGN(size) + HEAP_MAGIC_SIZE;
  size_t offset;
  size_t left_size;
  heap_block_t *b;
  heap_block_t *prev = NULL;
  heap_block_t *best = NULL;

  asAssert(0 == (alignment % HEAP_MIN_ALIGNED_SIZE));
  asAssert(alignment >= HEAP_MIN_ALIGNED_SIZE);

  HEAP_LOCK();
  if (0 == lHeap.initialized) {
    heap_init();
  }
  ASLOG(HEAP, ("memalign(%u, %u)\n", (uint32_t)alignment, (uint32_t)size));
  SLIST_FOREACH(b, &lHeap.free, entry) {
    if (b->size >= aligned_size) {
      pMem = (void *)HEAP_ALIGN_BY((uintptr_t)b, alignment);
      offset = (uintptr_t)pMem - (uintptr_t)b;
      if ((offset == HEAP_MAGIC_SIZE) || (offset >= (2 * HEAP_MAGIC_SIZE))) {
        if (b->size >= (offset - HEAP_MAGIC_SIZE + aligned_size)) {
          best = b;
          break;
        }
      }
    }
    prev = b;
  }

  pMem = NULL;

  if (best) {
    ASLOG(HEAP, ("  Best Heap: %u@%p\n", (uint32_t)best->size, best));
    pMem = (void *)HEAP_ALIGN_BY((uintptr_t)best, alignment);
    pMagic = (heap_magic_t *)HEAP_ADDR(pMem, -HEAP_MAGIC_SIZE);

    if (NULL == prev) {
      SLIST_REMOVE_HEAD(&lHeap.free, entry);
    } else {
      SLIST_REMOVE_AFTER(prev, entry);
    }

    offset = (uintptr_t)pMem - (uintptr_t)best;

    left_size = best->size - aligned_size - (offset - HEAP_MAGIC_SIZE);
    if (left_size >= sizeof(heap_block_t)) {
      b = (heap_block_t *)HEAP_ADDR(pMagic, aligned_size);
      b->size = left_size;
      (void)heap_add_block(b);
      pMagic->size = aligned_size;
    } else {
      pMagic->size = aligned_size + left_size;
    }

    left_size = offset - HEAP_MAGIC_SIZE;
    if (left_size > 0) {
      asAssert(left_size >= HEAP_MAGIC_SIZE);
      b = (heap_block_t *)best;
      b->size = left_size;
      (void)heap_add_block(b);
    }
    ASLOG(HEAP, ("  memalign(%u@%p) = %p\n", (uint32_t)pMagic->size,
                 HEAP_ADDR(pMem, -HEAP_MAGIC_SIZE), pMem));
#ifdef HEAP_TRACK
    SLIST_INSERT_HEAD(&lHeap.used, pMagic, entry);
#endif
  } else {
    ASLOG(HEAPE, ("  memalign OoM for %u\n", (uint32_t)size));
  }
  HEAP_UNLOCK();

  return pMem;
}

#if !defined(linux) && !defined(_WIN32)
void *malloc(size_t sz) {
  return heap_malloc(sz);
}

void free(void *ptr) {
  return heap_free(ptr);
}

void *calloc(size_t nitems, size_t size) {
  void *ptr = heap_malloc(nitems * size);
  if (NULL != ptr) {
    memset(ptr, 0, nitems * size);
  }
  return ptr;
}

void *kzmalloc(size_t size) {

  void *p = heap_malloc(size);
  if (NULL != p) {
    memset(p, 0, size);
  }

  return p;
}

void *memalign(size_t alignment, size_t size) {
  return heap_memalign(alignment, size);
}
#endif

#ifdef HEAP_TEST
/* gcc -g infras\libraries\heap\heap.c -I infras\include -DHEAP_TEST -DAS_LOG_DEFAULT=1 */
int main(int argc, char *argv[]) {
  int N;
  void **ptr;
  size_t *sz;
  int i, k;
  int doFree;
  heap_block_t *b;
  int try;
  size_t used = 0;

  if (argc > 1) {
    N = atoi(argv[1]);
  }

  if (N < 10) {
    N = 10;
  }

  ptr = malloc(N * sizeof(void *));
  sz = malloc(N * sizeof(size_t));

  for (i = 0; i < N; i++) {
    doFree = (1 == (rand() % 2));
    sz[i] = 1 + (rand() % 1000);
    // ptr[i] = heap_malloc(sz[i]);
    ptr[i] = heap_memalign(4096, sz[i]);
    if (ptr[i]) {
      used += ((heap_magic_t *)HEAP_ADDR(ptr[i], -HEAP_MAGIC_SIZE))->size;
      memset(ptr[i], i, sz[i]);
    }
    asAssert((used + heap_free_size()) == HEAP_SIZE);
    if (doFree) {
      try = N * 3;
      do {
        k = rand() % (i + 1);
        if (ptr[k]) {
          used -= ((heap_magic_t *)HEAP_ADDR(ptr[k], -HEAP_MAGIC_SIZE))->size;
          heap_free(ptr[k]);
          ptr[k] = NULL;
          break;
        }
      } while (--try > 0);
    }
    asAssert((used + heap_free_size()) == HEAP_SIZE);
  }

  for (i = 0; i < N; i++) {
    if (NULL != ptr[i]) {
      used -= ((heap_magic_t *)HEAP_ADDR(ptr[i], -HEAP_MAGIC_SIZE))->size;
      heap_free(ptr[i]);

      asAssert((used + heap_free_size()) == HEAP_SIZE);
    }
  }

  printf("Test Done\n");
  SLIST_FOREACH(b, &lHeap.free, entry) {
    ASLOG(HEAP, ("Heap: %u@%p\n", (uint32_t)b->size, b));
  }
  return 0;
}
#endif