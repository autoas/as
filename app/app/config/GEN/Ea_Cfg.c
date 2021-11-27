/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Sat Nov 27 10:33:30 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Ea.h"
#include "Ea_Cfg.h"
#include "Ea_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void NvM_JobEndNotification(void);
void NvM_JobErrorNotification(void);
/* ================================ [ DATAS     ] ============================================== */
static const Ea_BlockConfigType Ea_BlockConfigs[] = {
  { EA_NUMBER_Dem_NvmEventStatusRecord0, 0, 5+2, 10000000 },
  { EA_NUMBER_Dem_NvmEventStatusRecord1, 8, 5+2, 10000000 },
  { EA_NUMBER_Dem_NvmEventStatusRecord2, 16, 5+2, 10000000 },
  { EA_NUMBER_Dem_NvmEventStatusRecord3, 24, 5+2, 10000000 },
  { EA_NUMBER_Dem_NvmEventStatusRecord4, 32, 5+2, 10000000 },
  { EA_NUMBER_Dem_NvmFreezeFrameRecord0, 40, 28+2, 10000000 },
  { EA_NUMBER_Dem_NvmFreezeFrameRecord1, 72, 28+2, 10000000 },
  { EA_NUMBER_Dem_NvmFreezeFrameRecord2, 104, 28+2, 10000000 },
  { EA_NUMBER_Dem_NvmFreezeFrameRecord3, 136, 28+2, 10000000 },
  { EA_NUMBER_Dem_NvmFreezeFrameRecord4, 168, 28+2, 10000000 },
};

const Ea_ConfigType Ea_Config = {
  NvM_JobEndNotification,
  NvM_JobErrorNotification,
  Ea_BlockConfigs,
  ARRAY_SIZE(Ea_BlockConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
