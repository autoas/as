/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef BL_FLSDRV_MEMORY_LOW
#define BL_FLSDRV_MEMORY_LOW 0
#endif

#ifndef BL_FLSDRV_MEMORY_HIGH
#define BL_FLSDRV_MEMORY_HIGH (2 * 1024)
#endif

#ifndef BL_APP_MEMORY_LOW
#define BL_APP_MEMORY_LOW (2 * 1024)
#endif

#ifndef BL_APP_MEMORY_HIGH
#define BL_APP_MEMORY_HIGH (1 * 1024 * 1024)
#endif

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
const BL_MemoryInfoType blMemoryList[] = {
  /* FLASH DRIVER */ {BL_FLSDRV_MEMORY_LOW, BL_FLSDRV_MEMORY_HIGH, BL_FLSDRV_IDENTIFIER},
  /* APPLICATION  */ {BL_APP_MEMORY_LOW, BL_APP_MEMORY_HIGH, BL_FLASH_IDENTIFIER},
};

const uint32_t blMemoryListSize = ARRAY_SIZE(blMemoryList);

const uint32_t blFlsDriverMemoryLow = BL_FLSDRV_MEMORY_LOW;
const uint32_t blFlsDriverMemoryHigh = BL_FLSDRV_MEMORY_HIGH;
const uint32_t blAppMemoryLow = BL_APP_MEMORY_LOW;
const uint32_t blAppMemoryHigh = BL_APP_MEMORY_HIGH;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
