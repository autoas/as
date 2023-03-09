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
#define BL_FLSDRV_MEMORY_HIGH (4 * 1024)
#endif

#ifndef BL_APP_MEMORY_LOW
#define BL_APP_MEMORY_LOW (4 * 1024)
#endif

#ifndef BL_APP_MEMORY_HIGH
#define BL_APP_MEMORY_HIGH (1 * 1024 * 1024)
#endif

#ifndef BL_FINGER_PRINT_ADDRESS
#define BL_FINGER_PRINT_ADDRESS (1 * 1024 * 1024 - 1024)
#endif

#ifndef BL_FINGER_PRINT_SIZE
#define BL_FINGER_PRINT_SIZE 32
#endif

#ifndef BL_APP_VALID_FLAG_ADDRESS
#define BL_APP_VALID_FLAG_ADDRESS (BL_FINGER_PRINT_ADDRESS + BL_FINGER_PRINT_SIZE)
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
const uint32_t blFingerPrintAddr = BL_FINGER_PRINT_ADDRESS;
const uint32_t blAppValidFlagAddr = BL_APP_VALID_FLAG_ADDRESS;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
