/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _MEM_POOL_H
#define _MEM_POOL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <sys/queue.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct mp_pool_s {
  SLIST_ENTRY(mp_pool_s) entry;
} mp_pool_t;

typedef struct {
  SLIST_HEAD(mp_head, mp_pool_s) head;
} mempool_t;

typedef struct {
  uint8_t *buffer;
  uint32_t size;
  uint16_t number;
} mem_cluster_cfg_t;

typedef struct {
  mempool_t *pools;
  const mem_cluster_cfg_t *configs;
  uint16_t numOfPools;
} mem_cluster_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void mp_init(mempool_t *mp, uint8_t *buffer, uint32_t size, uint16_t number);
uint8_t *mp_alloc(mempool_t *mp);
void mp_free(mempool_t *mp, uint8_t *buffer);

void mc_init(const mem_cluster_t *mc);
uint8_t *mc_alloc(const mem_cluster_t *mc, uint32_t size);
uint8_t *mc_get(const mem_cluster_t *mc, uint32_t *size);
void mc_free(const mem_cluster_t *mc, uint8_t *buffer);
#endif /* _MEM_POOL_H */
