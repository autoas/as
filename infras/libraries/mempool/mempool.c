/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "mempool.h"
#include "Std_Critical.h"
#include "Std_Types.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_MCI 0
#define AS_LOG_MCE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void mp_init(mempool_t *mp, uint8_t *buffer, uint32_t size, uint16_t number) {
  uint16_t i;
  mp_pool_t *pool;

  SLIST_INIT(&mp->head);
  for (i = 0; i < number; i++) {
    pool = (mp_pool_t *)&buffer[size * i];
    SLIST_INSERT_HEAD(&mp->head, pool, entry);
  }
}

uint8_t *mp_alloc(mempool_t *mp) {
  uint8_t *buffer = NULL;

  EnterCritical();
  if (NULL != SLIST_FIRST(&mp->head)) {
    buffer = (uint8_t *)SLIST_FIRST(&mp->head);
    SLIST_REMOVE_HEAD(&mp->head, entry);
  }
  ExitCritical();

  return buffer;
}

void mp_free(mempool_t *mp, uint8_t *buffer) {
  mp_pool_t *pool = (mp_pool_t *)buffer;

  EnterCritical();
  SLIST_INSERT_HEAD(&mp->head, pool, entry);
  ExitCritical();
}

void mc_init(const mem_cluster_t *mc) {
  uint16_t i;

  for (i = 0; i < mc->numOfPools; i++) {
    mp_init(&mc->pools[i], mc->configs[i].buffer, mc->configs[i].size, mc->configs[i].number);
  }
}

uint8_t *mc_alloc(const mem_cluster_t *mc, uint32_t size) {
  uint16_t i = 0;
  mempool_t *mp = NULL;
  uint8_t *buffer = NULL;

  do {
    mp = NULL;
    for (; i < mc->numOfPools; i++) {
      if (mc->configs[i].size >= size) {
        mp = &mc->pools[i];
        break;
      }
    }

    if (mp != NULL) {
      buffer = mp_alloc(mp);
    }
  } while ((i < mc->numOfPools) && (NULL == buffer));

  if (NULL == buffer) {
    ASLOG(MCE, ("alloc %u fail\n", size));
  } else {
    ASLOG(MCI, ("alloc %u @%p from %p\n", size, buffer, mc));
  }

  return buffer;
}

uint8_t *mc_get(const mem_cluster_t *mc, uint32_t *size) {
  uint16_t i = 0;
  uint16_t j = 0;
  mempool_t *mp = NULL;
  uint8_t *buffer = NULL;

  do {
    mp = NULL;
    for (; i < mc->numOfPools; i++) {
      if (mc->configs[i].size >= *size) {
        if (0 == j) {
          j = i;
        }
        mp = &mc->pools[i];
        break;
      }
    }

    if (mp != NULL) {
      buffer = mp_alloc(mp);
    }
  } while ((i < mc->numOfPools) && (NULL == buffer));

  for (i = 0; (i < j) && (NULL == buffer); i++) {
    mp = &mc->pools[j - 1 - i];
    *size = mc->configs[j - 1 - i].size;
    buffer = mp_alloc(mp);
  }

  if (NULL == buffer) {
    ASLOG(MCE, ("alloc %u fail\n", *size));
  } else {
    ASLOG(MCI, ("alloc %u @%p from %p\n", *size, buffer, mc));
  }

  return buffer;
}

void mc_free(const mem_cluster_t *mc, uint8_t *buffer) {
  uint16_t i;
  mempool_t *mp = NULL;

  for (i = 0; i < mc->numOfPools; i++) {
    if ((buffer >= mc->configs[i].buffer) &&
        (buffer < (mc->configs[i].buffer + mc->configs[i].size * mc->configs[i].number))) {
      if (0 == ((buffer - mc->configs[i].buffer) % mc->configs[i].size)) {
        mp = &mc->pools[i];
      }
      break;
    }
  }

  if (mp) {
    ASLOG(MCI, ("free %p to %p\n", buffer, mc));
    mp_free(mp, buffer);
  } else {
    ASLOG(MCE, ("free %p fail\n", buffer));
  }
}