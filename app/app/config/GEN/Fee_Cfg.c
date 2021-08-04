/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Fee.h"
#include "Fee_Cfg.h"
#include "Fee_Priv.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef FLS_BASE_ADDRESS
#define FLS_BASE_ADDRESS 0
#endif

#ifndef FEE_MAX_JOB_RETRY
#define FEE_MAX_JOB_RETRY 0xFF
#endif

#ifndef FEE_WORKING_AREA_SIZE
#define FEE_WORKING_AREA_SIZE 128
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void NvM_JobEndNotification(void);
void NvM_JobErrorNotification(void);
/* ================================ [ DATAS     ] ============================================== */
#define Dem_NvmEventStatusRecord1_Rom Dem_NvmEventStatusRecord0_Rom
#define Dem_NvmEventStatusRecord2_Rom Dem_NvmEventStatusRecord0_Rom
#define Dem_NvmEventStatusRecord3_Rom Dem_NvmEventStatusRecord0_Rom
#define Dem_NvmEventStatusRecord4_Rom Dem_NvmEventStatusRecord0_Rom
static const Dem_NvmEventStatusRecordType Dem_NvmEventStatusRecord0_Rom = { 80, 0, 0, 0, 0, };

#define Dem_NvmFreezeFrameRecord1_Rom Dem_NvmFreezeFrameRecord0_Rom
#define Dem_NvmFreezeFrameRecord2_Rom Dem_NvmFreezeFrameRecord0_Rom
#define Dem_NvmFreezeFrameRecord3_Rom Dem_NvmFreezeFrameRecord0_Rom
#define Dem_NvmFreezeFrameRecord4_Rom Dem_NvmFreezeFrameRecord0_Rom
static const Dem_NvmFreezeFrameRecordType Dem_NvmFreezeFrameRecord0_Rom = { 65535, {{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}, {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}}, };

static const Fee_BlockConfigType Fee_BlockConfigs[] = {
  { FEE_NUMBER_Dem_NvmEventStatusRecord0, 5, 10000000, &Dem_NvmEventStatusRecord0_Rom },
  { FEE_NUMBER_Dem_NvmEventStatusRecord1, 5, 10000000, &Dem_NvmEventStatusRecord1_Rom },
  { FEE_NUMBER_Dem_NvmEventStatusRecord2, 5, 10000000, &Dem_NvmEventStatusRecord2_Rom },
  { FEE_NUMBER_Dem_NvmEventStatusRecord3, 5, 10000000, &Dem_NvmEventStatusRecord3_Rom },
  { FEE_NUMBER_Dem_NvmEventStatusRecord4, 5, 10000000, &Dem_NvmEventStatusRecord4_Rom },
  { FEE_NUMBER_Dem_NvmFreezeFrameRecord0, 28, 10000000, &Dem_NvmFreezeFrameRecord0_Rom },
  { FEE_NUMBER_Dem_NvmFreezeFrameRecord1, 28, 10000000, &Dem_NvmFreezeFrameRecord1_Rom },
  { FEE_NUMBER_Dem_NvmFreezeFrameRecord2, 28, 10000000, &Dem_NvmFreezeFrameRecord2_Rom },
  { FEE_NUMBER_Dem_NvmFreezeFrameRecord3, 28, 10000000, &Dem_NvmFreezeFrameRecord3_Rom },
  { FEE_NUMBER_Dem_NvmFreezeFrameRecord4, 28, 10000000, &Dem_NvmFreezeFrameRecord4_Rom },
};

static uint32_t Fee_BlockDataAddress[10];
static const Fee_BankType Fee_Banks[] = {
  {FLS_BASE_ADDRESS, FLS_BASE_ADDRESS + 64 * 1024},
  {FLS_BASE_ADDRESS + 64 * 1024, FLS_BASE_ADDRESS + 128 * 1024},
};

static uint32_t Fee_WorkingArea[FEE_WORKING_AREA_SIZE/sizeof(uint32_t)];
const Fee_ConfigType Fee_Config = {
  NvM_JobEndNotification,
  NvM_JobErrorNotification,
  Fee_BlockDataAddress,
  Fee_BlockConfigs,
  ARRAY_SIZE(Fee_BlockConfigs),
  Fee_Banks,
  ARRAY_SIZE(Fee_Banks),
  (uint8_t*)Fee_WorkingArea,
  sizeof(Fee_WorkingArea),
  FEE_MAX_JOB_RETRY,
};
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
