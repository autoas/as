/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
#ifndef NVM_CFG_H
#define NVM_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* NVM target is FEE, CRC is not used */
#define MEMIF_ZERO_COST_FEE
#define NVM_BLOCKID_Dem_NvmEventStatusRecord0 2
#define NVM_BLOCKID_Dem_NvmEventStatusRecord1 3
#define NVM_BLOCKID_Dem_NvmEventStatusRecord2 4
#define NVM_BLOCKID_Dem_NvmEventStatusRecord3 5
#define NVM_BLOCKID_Dem_NvmEventStatusRecord4 6
#define NVM_BLOCKID_Dem_NvmFreezeFrameRecord0 7
#define NVM_BLOCKID_Dem_NvmFreezeFrameRecord1 8
#define NVM_BLOCKID_Dem_NvmFreezeFrameRecord2 9
#define NVM_BLOCKID_Dem_NvmFreezeFrameRecord3 10
#define NVM_BLOCKID_Dem_NvmFreezeFrameRecord4 11
#define NVM_BLOCK_NUMBER 11
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t status;
  uint8_t testFailedCounter;
  uint8_t faultOccuranceCounter;
  uint8_t agingCounter;
  uint8_t agedCounter;
} Dem_NvmEventStatusRecordType;

typedef struct {
  uint16_t EventId;
  uint8_t record[2][13];
} Dem_NvmFreezeFrameRecordType;

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* NVM_CFG_H */
