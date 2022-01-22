/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "NvM.h"
#include "NvM_Cfg.h"
#include "NvM_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
Dem_NvmEventStatusRecordType Dem_NvmEventStatusRecord0_Ram;
Dem_NvmEventStatusRecordType Dem_NvmEventStatusRecord1_Ram;
Dem_NvmEventStatusRecordType Dem_NvmEventStatusRecord2_Ram;
Dem_NvmEventStatusRecordType Dem_NvmEventStatusRecord3_Ram;
Dem_NvmEventStatusRecordType Dem_NvmEventStatusRecord4_Ram;
Dem_NvmFreezeFrameRecordType Dem_NvmFreezeFrameRecord0_Ram;
Dem_NvmFreezeFrameRecordType Dem_NvmFreezeFrameRecord1_Ram;
Dem_NvmFreezeFrameRecordType Dem_NvmFreezeFrameRecord2_Ram;
Dem_NvmFreezeFrameRecordType Dem_NvmFreezeFrameRecord3_Ram;
Dem_NvmFreezeFrameRecordType Dem_NvmFreezeFrameRecord4_Ram;
static const NvM_BlockDescriptorType NvM_BlockDescriptors[] = {
  { &Dem_NvmEventStatusRecord0_Ram, 1, sizeof(Dem_NvmEventStatusRecordType) },
  { &Dem_NvmEventStatusRecord1_Ram, 2, sizeof(Dem_NvmEventStatusRecordType) },
  { &Dem_NvmEventStatusRecord2_Ram, 3, sizeof(Dem_NvmEventStatusRecordType) },
  { &Dem_NvmEventStatusRecord3_Ram, 4, sizeof(Dem_NvmEventStatusRecordType) },
  { &Dem_NvmEventStatusRecord4_Ram, 5, sizeof(Dem_NvmEventStatusRecordType) },
  { &Dem_NvmFreezeFrameRecord0_Ram, 6, sizeof(Dem_NvmFreezeFrameRecordType) },
  { &Dem_NvmFreezeFrameRecord1_Ram, 7, sizeof(Dem_NvmFreezeFrameRecordType) },
  { &Dem_NvmFreezeFrameRecord2_Ram, 8, sizeof(Dem_NvmFreezeFrameRecordType) },
  { &Dem_NvmFreezeFrameRecord3_Ram, 9, sizeof(Dem_NvmFreezeFrameRecordType) },
  { &Dem_NvmFreezeFrameRecord4_Ram, 10, sizeof(Dem_NvmFreezeFrameRecordType) },
};

static uint16_t NvM_JobReadMasks[(NVM_BLOCK_NUMBER+15)/16];
static uint16_t NvM_JobWriteMasks[(NVM_BLOCK_NUMBER+15)/16];
const NvM_ConfigType NvM_Config = {
  NvM_BlockDescriptors,
  ARRAY_SIZE(NvM_BlockDescriptors),
  NvM_JobReadMasks,
  NvM_JobWriteMasks,
};
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
