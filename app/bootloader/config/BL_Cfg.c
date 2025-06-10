/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
#ifdef _WIN32
#include <stdlib.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef BL_FLSDRV_MEMORY_LOW
#define BL_FLSDRV_MEMORY_LOW 0
#endif

#ifndef BL_FLSDRV_MEMORY_HIGH
#define BL_FLSDRV_MEMORY_HIGH (2 * 1024)
#endif

#ifndef BL_APP_MEMORY_LOW
#define BL_APP_MEMORY_LOW (4 * 1024)
#endif

#ifndef BL_FLS_TOTAL_SIZE
#define BL_FLS_TOTAL_SIZE (1 * 1024 * 1024)
#endif

#ifndef BL_APP_MEMORY_HIGH
#define BL_APP_MEMORY_HIGH (BL_FLS_TOTAL_SIZE - 3 * FLASH_ERASE_SIZE)
#endif

#ifndef BL_FINGER_PRINT_ADDRESS
#define BL_FINGER_PRINT_ADDRESS BL_APP_MEMORY_HIGH
#endif

#ifndef BL_APP_META_ADDR
#define BL_APP_META_ADDR (BL_FINGER_PRINT_ADDRESS + FLASH_ERASE_SIZE / 2)
#endif

#ifndef BL_APP_INFO_ADDR
#define BL_APP_INFO_ADDR (BL_FLS_TOTAL_SIZE - 2 * FLASH_ERASE_SIZE)
#endif

#ifndef BL_APP_VALID_FLAG_ADDRESS
#define BL_APP_VALID_FLAG_ADDRESS (BL_APP_INFO_ADDR + FLASH_ERASE_SIZE / 2)
#endif

#ifndef BL_META_BACKUP_ADDRESS
#define BL_META_BACKUP_ADDRESS (BL_FLS_TOTAL_SIZE - FLASH_ERASE_SIZE)
#endif

/*     Flash Layout
 *  +---------------------+ <-- 0
 *  |    Boot Loader      |                              FINGER + INFO
 *  +---------------------+ <-- AppMemLow          +--------------------------+ <-- AppMemHigh
 *  |                     |                        |     Finger Print         |
 *  |                     |                        |         Meta             |
 *  |    APP A            |                        +--------------------------+ <-- 1 sector
 *  |                     |                        |      INFO                |
 *  +---------------------+ <-- AppMemHigh         |    APP VALID FLAG        |
 *  | APP A FINGER + INFO |                        +--------------------------+ <-- 2 sector
 *  +---------------------+ <-- MetaBackupAddr
 *  | APP A META BACKUP   |
 *  +---------------------+ <-- END
 */

#ifdef _WIN32
#define L_CONST
#else
#define L_CONST const
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
L_CONST uint32_t blFlsDriverMemoryHigh = BL_FLSDRV_MEMORY_HIGH;
const uint32_t blAppMemoryLow = BL_APP_MEMORY_LOW;
const uint32_t blAppMemoryHigh = BL_APP_MEMORY_HIGH;
const uint32_t blAppMetaAddr = BL_APP_META_ADDR;
const uint32_t blFingerPrintAddr = BL_FINGER_PRINT_ADDRESS;
const uint32_t blAppValidFlagAddr = BL_APP_VALID_FLAG_ADDRESS;
const uint32_t blAppMetaBackupAddr = BL_META_BACKUP_ADDRESS;
const uint32_t blAppInfoAddr = BL_APP_INFO_ADDR;
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void __attribute__((constructor)) _blcfg_start(void) {
  char *hiStr = getenv("BL_FLSDRV_MEMORY_HIGH");
  if (hiStr != NULL) {
    blFlsDriverMemoryHigh = strtoul(hiStr, NULL, 10);
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
