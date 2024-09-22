/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of EEPROM Driver AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Eep_Cfg.h"
#include "Eep_Priv.h"
#include "Ea.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef EEP_BASE_ADDRESS
#define EEP_BASE_ADDRESS 0
#endif

#ifndef EEP_MAX_READ_FAST
#define EEP_MAX_READ_FAST 4096
#endif

#ifndef EEP_MAX_READ_NORM
#define EEP_MAX_READ_NORM 512
#endif

#ifndef EEP_MAX_WRITE_FAST
#define EEP_MAX_WRITE_FAST 4096
#endif

#ifndef EEP_MAX_WRITE_NORM
#define EEP_MAX_WRITE_NORM 512
#endif

#ifndef EEP_MAX_ERASE_FAST
#define EEP_MAX_ERASE_FAST 4096
#endif

#ifndef EEP_MAX_ERASE_NORM
#define EEP_MAX_ERASE_NORM 512
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const Eep_SectorType Eep_SectorList[] = {
  {
    EEP_BASE_ADDRESS,
    EEP_BASE_ADDRESS + 4 * 1024,
    4,
    1,
    1,
  },
};

const Eep_ConfigType Eep_Config = {
  Ea_JobEndNotification, Ea_JobErrorNotification,   MEMIF_MODE_FAST,
  EEP_MAX_READ_FAST,      EEP_MAX_READ_NORM,          EEP_MAX_WRITE_FAST,
  EEP_MAX_WRITE_NORM,     EEP_MAX_ERASE_FAST,         EEP_MAX_ERASE_NORM,
  Eep_SectorList,         ARRAY_SIZE(Eep_SectorList),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
