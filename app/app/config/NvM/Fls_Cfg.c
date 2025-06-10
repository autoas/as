/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash Driver AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Fls.h"
#include "Fls_Cfg.h"
#include "Fls_Priv.h"
#include "Fee.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef FLS_BANK0_ADDRESS
#define FLS_BANK0_ADDRESS 0
#endif

#ifndef FLS_BANK0_SIZE
#define FLS_BANK0_SIZE (512 * 1024)
#endif

#ifndef FLS_BANK1_ADDRESS
#define FLS_BANK1_ADDRESS (FLS_BANK0_ADDRESS + FLS_BANK0_SIZE)
#endif

#ifndef FLS_BANK1_SIZE
#define FLS_BANK1_SIZE (512 * 1024)
#endif

#ifndef FLS_MAX_READ_FAST
#define FLS_MAX_READ_FAST 4096
#endif

#ifndef FLS_SECTOR_SIZE
#define FLS_SECTOR_SIZE 512
#endif

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 8
#endif

#ifndef FLS_MAX_READ_NORM
#define FLS_MAX_READ_NORM 512
#endif

#ifndef FLS_MAX_WRITE_FAST
#define FLS_MAX_WRITE_FAST 4096
#endif

#ifndef FLS_MAX_WRITE_NORM
#define FLS_MAX_WRITE_NORM 512
#endif

#ifndef FLS_MAX_ERASE_FAST
#define FLS_MAX_ERASE_FAST 4096
#endif

#ifndef FLS_MAX_ERASE_NORM
#define FLS_MAX_ERASE_NORM 512
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const Fls_SectorType Fls_SectorList[] = {
  {
    FLS_BANK0_ADDRESS,
    FLS_BANK0_ADDRESS + FLS_BANK0_SIZE,
    FLS_SECTOR_SIZE,
    FLASH_PAGE_SIZE,
    FLS_BANK0_SIZE / FLS_SECTOR_SIZE,
  },
  {
    FLS_BANK1_ADDRESS,
    FLS_BANK1_ADDRESS + FLS_BANK1_SIZE,
    FLS_SECTOR_SIZE,
    FLASH_PAGE_SIZE,
    FLS_BANK1_SIZE / FLS_SECTOR_SIZE,
  },
};

const Fls_ConfigType Fls_Config = {
  Fee_JobEndNotification, Fee_JobErrorNotification,   MEMIF_MODE_FAST,
  FLS_MAX_READ_FAST,      FLS_MAX_READ_NORM,          FLS_MAX_WRITE_FAST,
  FLS_MAX_WRITE_NORM,     FLS_MAX_ERASE_FAST,         FLS_MAX_ERASE_NORM,
  Fls_SectorList,         ARRAY_SIZE(Fls_SectorList),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
