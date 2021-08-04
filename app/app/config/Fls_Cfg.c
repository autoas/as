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
#ifndef FLS_BASE_ADDRESS
#define FLS_BASE_ADDRESS 0
#endif

#ifndef FLS_MAX_READ_FAST
#define FLS_MAX_READ_FAST 4096
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
    FLS_BASE_ADDRESS,
    FLS_BASE_ADDRESS + 512 * 1024,
    512,
    8,
    1024,
  },
  {
    FLS_BASE_ADDRESS + 512 * 1024,
    FLS_BASE_ADDRESS + 1024 * 1024,
    4096,
    32,
    128,
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
