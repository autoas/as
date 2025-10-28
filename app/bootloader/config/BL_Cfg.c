/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"

#include "RoD.h"

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
 *  |                     |
 *  |                     |
 *  |    APP B            |
 *  |                     |
 *  +---------------------+
 *  | APP B FINGER + INFO |
 *  +---------------------+
 *  | APP B META BACKUP   |
 *  +---------------------+
 */

#ifdef _WIN32
#define L_CONST
#else
#define L_CONST const
#endif

#ifndef BL_USE_AB
#define blAppMemoryLowA blAppMemoryLow
#define blAppMemoryHighA blAppMemoryHigh
#define blFingerPrintAddrA blFingerPrintAddr
#define blAppInfoAddrA blAppInfoAddr
#define blAppValidFlagAddrA blAppValidFlagAddr
#define blMemoryListA blMemoryList
#define blMemoryListASize blMemoryListSize
#ifdef BL_USE_META
#define blAppMetaAddrA blAppMetaAddr
#define blAppMetaBackupAddrA blAppMetaBackupAddr
#endif
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
const BL_MemoryInfoType blMemoryListA[] = {
  /* FLASH DRIVER */ {BL_FLSDRV_MEMORY_LOW, BL_FLSDRV_MEMORY_HIGH, BL_FLSDRV_IDENTIFIER},
  /* APPLICATION  */ {BL_APP_MEMORY_LOW, BL_APP_MEMORY_HIGH, BL_FLASH_IDENTIFIER},
};

const uint32_t blMemoryListASize = ARRAY_SIZE(blMemoryListA);

#ifdef BL_USE_AB
const BL_MemoryInfoType blMemoryListB[] = {
  /* FLASH DRIVER */ {BL_FLSDRV_MEMORY_LOW, BL_FLSDRV_MEMORY_HIGH, BL_FLSDRV_IDENTIFIER},
  /* APPLICATION  */
  {BL_FLS_TOTAL_SIZE + BL_APP_MEMORY_LOW, BL_FLS_TOTAL_SIZE + BL_APP_MEMORY_HIGH,
   BL_FLASH_IDENTIFIER},
};
const uint32_t blMemoryListBSize = ARRAY_SIZE(blMemoryListB);
#endif

static const RoD_ConfigType RoD_ConfigDummy = {
  NULL, 0, 0, 0, 0,
};

const uint32_t blFlsDriverMemoryLow = BL_FLSDRV_MEMORY_LOW;
L_CONST uint32_t blFlsDriverMemoryHigh = BL_FLSDRV_MEMORY_HIGH;

const uint32_t blAppMemoryLowA = BL_APP_MEMORY_LOW;
const uint32_t blAppMemoryHighA = BL_APP_MEMORY_HIGH;
const RoD_ConfigType *const RoD_AppConfigA = (const RoD_ConfigType *)&RoD_ConfigDummy;
const uint32_t blAppMetaAddrA = BL_APP_META_ADDR;
const uint32_t blFingerPrintAddrA = BL_FINGER_PRINT_ADDRESS;
const uint32_t blAppValidFlagAddrA = BL_APP_VALID_FLAG_ADDRESS;
const uint32_t blAppMetaBackupAddrA = BL_META_BACKUP_ADDRESS;
const uint32_t blAppInfoAddrA = BL_APP_INFO_ADDR;

#ifdef BL_USE_AB
const uint32_t blAppMemoryLowB = BL_FLS_TOTAL_SIZE + BL_APP_MEMORY_LOW;
const uint32_t blAppMemoryHighB = BL_FLS_TOTAL_SIZE + BL_APP_MEMORY_HIGH;
const RoD_ConfigType *const RoD_AppConfigB = (const RoD_ConfigType *)&RoD_ConfigDummy;
const uint32_t blAppMetaAddrB = BL_FLS_TOTAL_SIZE + BL_APP_META_ADDR;
const uint32_t blFingerPrintAddrB = BL_FLS_TOTAL_SIZE + BL_FINGER_PRINT_ADDRESS;
const uint32_t blAppValidFlagAddrB = BL_FLS_TOTAL_SIZE + BL_APP_VALID_FLAG_ADDRESS;
const uint32_t blAppMetaBackupAddrB = BL_FLS_TOTAL_SIZE + BL_META_BACKUP_ADDRESS;
const uint32_t blAppInfoAddrB = BL_FLS_TOTAL_SIZE + BL_APP_INFO_ADDR;
#endif
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
