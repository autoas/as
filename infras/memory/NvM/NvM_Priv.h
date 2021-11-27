/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of NVRAM Manager AUTOSAR CP Release 4.4.0
 */
#ifndef NVM_PRIV_H
#define NVM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "NvM.h"
/* ================================ [ MACROS    ] ============================================== */
#define NVM_CRC16 ((NvM_BlockCrcType)0)
#define NVM_CRC32 ((NvM_BlockCrcType)1)
#define NVM_CRC8 ((NvM_BlockCrcType)2)
#define NVM_CRC_OFF ((NvM_BlockCrcType)3) /* @ECUC_NvM_00036 */

#define NVM_BLOCK_DATASET ((NvM_BlockManagementType)0)
#define NVM_BLOCK_NATIVE ((NvM_BlockManagementType)1)
#define NVM_BLOCK_REDUNDANT ((NvM_BlockManagementType)2)

#define NVM_INIT_READ_BLOCK ((NvM_InitBlockRequestType)0)
#define NVM_INIT_RESTORE_BLOCK_DEFAULTS ((NvM_InitBlockRequestType)1)
#define NVM_INIT_READ_ALL_BLOCK ((NvM_InitBlockRequestType)2)
#define NVM_INIT_FIRST_INIT_ALL ((NvM_InitBlockRequestType)3)

#ifdef NVM_ZERO_COST_FEE
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t NvM_BlockCrcType;

typedef uint8_t NvM_BlockManagementType;

/* @SWS_NvM_91123 */
typedef uint8_t NvM_InitBlockRequestType;

typedef Std_ReturnType (*NvM_ReadRamBlockFromNvCallbackType)(const uint8_t *data, uint16_t length);
typedef Std_ReturnType (*NvM_WriteRamBlockToNvCallbackType)(uint8_t *data, uint16_t length);

/* @ECUC_NvM_00061 */
typedef struct {
  void *RamBlockDataAddress;
#if 0
  NvM_ReadRamBlockFromNvCallbackType ReadRamBlockFromNvCallback;
  NvM_WriteRamBlockToNvCallbackType WriteRamBlockToNvCallback;
#endif
  uint16_t NvBlockBaseNumber; /* Ea or Fee Number */
  uint16_t NvBlockLength;
#if 0
  NvM_BlockIdType NvramBlockIdentifier;
  /* @ECUC_NvM_00480 */
  uint8_t NvBlockNum;
#endif
#ifdef NVM_BLOCK_USE_CRC
  NvM_BlockCrcType CrcType;
  const void *Rom;
#endif
#if 0
  uint8_t JobPriority; /* 0 = Immediate priority */
  uint8_t MaxNumOfReadRetries;
  uint8_t MaxNumOfWriteRetries;
#endif
} NvM_BlockDescriptorType;

struct NvM_Config_s {
  const NvM_BlockDescriptorType *Blocks;
  uint16_t numOfBlocks;
  /* If NVM builtin Job Queue is FULL, use this masks to request read/write */
  uint16_t *readMasks;
  uint16_t *writeMasks;
#ifdef NVM_BLOCK_USE_CRC
  uint8_t *workingArea;
#endif
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* NVM_PRIV_H */
