/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of NVRAM Manager AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "NvM_Cfg.h"
#include "NvM.h"
#include "NvM_Priv.h"
#include "MemIf.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
#include <string.h>
#include <sys/queue.h>
#include "Crc.h"

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_NVM 0
#define AS_LOG_NVMI 1
#define AS_LOG_NVME 2

#define NVM_CONFIG (&NvM_Config)

/* the low 4 bit used for state, the high 4 bit used for job type */
#define NVM_STATE_MASK ((NvM_StateType)0x0F)
#define NVM_JOB_MASK ((NvM_StateType)0xF0)

#define NVM_IDLE ((NvM_StateType)0)
#define NVM_PENDING ((NvM_StateType)1)
#define NVM_WAITING ((NvM_StateType)2) /* Waiting Fls Job Finished or Error Exited */

#define NVM_JOB_NONE ((NvM_StateType)0x00)
#define NVM_JOB_INIT ((NvM_StateType)0x10)
#define NVM_JOB_READ_ALL ((NvM_StateType)0x20)
#define NVM_JOB_READ ((NvM_StateType)0x30)
#define NVM_JOB_WRITE_ALL ((NvM_StateType)0x40)
#define NVM_JOB_WRITE ((NvM_StateType)0x50)

#define NVM_STEP_IDLE ((NvM_StepType)0)

#define NVM_STEP_INIT_WAIT_MEMIF_IDLE ((NvM_StepType)0)

#define NVM_STEP_READ_ALL_BLOCK ((NvM_StepType)0)
#define NVM_STEP_READ ((NvM_StepType)0)

#define NVM_STEP_WRITE_ALL_BLOCK ((NvM_StepType)0)
#define NVM_STEP_WRITE ((NvM_StepType)0)

#define NVM_JOB_EVENT_START ((NvM_JobEventType)0)
#define NVM_JOB_EVENT_ERROR ((NvM_JobEventType)1)
#define NVM_JOB_EVENT_END ((NvM_JobEventType)2)

#ifndef NVM_JOB_QUEUE_SIZE
#define NVM_JOB_QUEUE_SIZE 0
#endif

#define NVM_INVALID_BLOCKID 0xFFFFu

#define NvmDoJobTemplate(jobName, retryMax)                                                        \
  static void NvM_Do##jobName(NvM_StepType step, NvM_JobEventType event) {                         \
    NvM_ContextType *context = &NvM_Context;                                                       \
    switch (event) {                                                                               \
    case NVM_JOB_EVENT_START:                                                                      \
    case NVM_JOB_EVENT_ERROR:                                                                      \
      if (context->RetryCounter <= retryMax) {                                                     \
        context->RetryCounter++;                                                                   \
        NvM_Do##jobName##_OnEventStart(step);                                                      \
      } else {                                                                                     \
        context->State = NVM_IDLE;                                                                 \
        context->RetryCounter = 0;                                                                 \
        NvM_Do##jobName##_OnEventError();                                                          \
        ASLOG(NVME, ("NvM Do " #jobName " Job Failed\n"));                                         \
      }                                                                                            \
      break;                                                                                       \
    case NVM_JOB_EVENT_END:                                                                        \
      context->RetryCounter = 0;                                                                   \
      NvM_Do##jobName##_OnEventEnd(step);                                                          \
      break;                                                                                       \
    default:                                                                                       \
      NvM_Panic();                                                                                 \
      break;                                                                                       \
    }                                                                                              \
  }
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t NvM_StateType;
typedef uint16_t NvM_StepType;

typedef uint8_t NvM_JobEventType;

typedef struct {
  uint16_t BlockId;
  uint8_t *Data;
} NvM_JobInfoType;

typedef struct NvM_Job_s {
  NvM_StateType JobType;
  NvM_JobInfoType Job;
  STAILQ_ENTRY(NvM_Job_s) entry;
} NvM_JobType;

typedef struct {
  NvM_StepType Step;
  NvM_JobInfoType Job;
#if NVM_JOB_QUEUE_SIZE > 0
  STAILQ_HEAD(Nvm_JobReqQ_s, NvM_Job_s) JobReqQ;
  STAILQ_HEAD(Nvm_JobFreeQ_s, NvM_Job_s) JobFreeQ;
#endif
  NvM_StateType State;
  uint8_t RetryCounter;
} NvM_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern CONSTANT(NvM_ConfigType, NVM_CONST) NvM_Config;
/* ================================ [ DATAS     ] ============================================== */
static NvM_ContextType NvM_Context;
#if NVM_JOB_QUEUE_SIZE > 0
static NvM_JobType NvM_Jobs[NVM_JOB_QUEUE_SIZE];
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void NvM_Panic(void) {
  NvM_ContextType *context = &NvM_Context;
  ASLOG(NVME, ("NVM panic in state 0x%X step %d\n", context->State, context->Step));
  context->State = NVM_IDLE;
}

static void NvM_DoInit_OnEventStart(NvM_StepType step) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  MemIf_StatusType ifStatus;
  (void)config;

  switch (step) {
  case NVM_STEP_INIT_WAIT_MEMIF_IDLE:
    ifStatus = MemIf_GetStatus(config->DeviceIndex);
    if (MEMIF_IDLE == ifStatus) {
      context->State = NVM_IDLE;
      context->Step = NVM_STEP_IDLE;
      ASLOG(NVM, ("MemIf is idle\n"));
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoInit_OnEventEnd(NvM_StepType step) {
  (void)step;
  NvM_Panic();
}

static void NvM_DoInit_OnEventError(void) {
  NvM_Panic();
}

#ifdef NVM_BLOCK_USE_CRC
static uint16_t NvM_GetCrcSize(NvM_BlockCrcType crcType) {
  uint16_t sz = 0;
  switch (crcType) {
  case NVM_CRC8:
    sz = 1;
    break;
  case NVM_CRC16:
    sz = 2;
    break;
  case NVM_CRC32:
    sz = 4;
    break;
  default:
    sz = 0;
    break;
  }
  return sz;
}
#endif
static void NvM_DoReadAll_OnEventStart(NvM_StepType step) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r;
  P2CONST(NvM_BlockDescriptorType, AUTOMATIC, NVM_CONST) BlockDesc;
  uint8_t *DataPtr;
  uint16_t Length;

  switch (step) {
  case NVM_STEP_READ_ALL_BLOCK:
    if (context->Job.BlockId < config->NumOfBlocks) {
      BlockDesc = &config->Blocks[context->Job.BlockId];
#ifdef NVM_BLOCK_USE_CRC
      DataPtr = config->WorkingArea;
      Length = BlockDesc->NvBlockLength + NvM_GetCrcSize(BlockDesc->CrcType);
#else
      DataPtr = (uint8_t *)BlockDesc->RamBlockDataAddress;
      Length = BlockDesc->NvBlockLength;
#endif
      r = MemIf_Read(config->DeviceIndex, BlockDesc->NvBlockBaseNumber, 0, DataPtr, Length);
      if (E_OK == r) {
        context->State = NVM_JOB_READ_ALL | NVM_WAITING;
      } else {
        context->State = NVM_JOB_READ_ALL | NVM_PENDING;
      }
#ifdef NVM_BLOCK_USE_STATUS
      config->BlockStatus[context->Job.BlockId] = NVM_REQ_PENDING;
#endif
    } else {
      context->State = NVM_IDLE;
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

#ifdef NVM_BLOCK_USE_CRC
static void NvM_DoRead_OnEventEnd_CheckCRC(void) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  P2CONST(NvM_BlockDescriptorType, AUTOMATIC, NVM_CONST) BlockDesc;
  uint8_t *DataPtr;
  uint8_t *dstPtr;
  uint16_t Length;
  uint32_t CrcR, CrcC;
  if (context->Job.BlockId < config->NumOfBlocks) {
    BlockDesc = &config->Blocks[context->Job.BlockId];
    dstPtr = context->Job.Data;
    if (NULL == dstPtr) {
      dstPtr = BlockDesc->RamBlockDataAddress;
    }
    DataPtr = config->WorkingArea;
    Length = BlockDesc->NvBlockLength;
    /* NOTE: now only support CRC16 and NONE */
    if (NVM_CRC16 == BlockDesc->CrcType) {
      CrcR = ((uint32_t)DataPtr[Length] << 8) + DataPtr[Length + 1];
      CrcC = Crc_CalculateCRC16(DataPtr, Length, 0, TRUE);
      if (CrcC != CrcR) {
        ASLOG(NVME, ("Block %d CRC invalid\n", context->Job.BlockId));
        memcpy(dstPtr, BlockDesc->Rom, Length);
#ifdef NVM_BLOCK_USE_STATUS
        config->BlockStatus[context->Job.BlockId] = NVM_REQ_INTEGRITY_FAILED;
#endif
      } else {
        memcpy(dstPtr, DataPtr, Length);
#ifdef NVM_BLOCK_USE_STATUS
        config->BlockStatus[context->Job.BlockId] = NVM_REQ_OK;
#endif
      }
    } else {
      memcpy(dstPtr, DataPtr, Length);
#ifdef NVM_BLOCK_USE_STATUS
      config->BlockStatus[context->Job.BlockId] = NVM_REQ_OK;
#endif
    }
  } else {
    NvM_Panic();
  }
}
#endif

static void NvM_DoReadAll_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;
#ifdef NVM_BLOCK_USE_STATUS
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
#endif
  switch (step) {
  case NVM_STEP_READ_ALL_BLOCK:
#ifdef NVM_BLOCK_USE_CRC
    NvM_DoRead_OnEventEnd_CheckCRC();
#else
#ifdef NVM_BLOCK_USE_STATUS
    if (context->Job.BlockId < config->NumOfBlocks) {
      config->BlockStatus[context->Job.BlockId] = NVM_REQ_OK;
    }
#endif
#endif
    context->Job.BlockId++;
    NvM_DoReadAll_OnEventStart(step);
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoReadAll_OnEventError(void) {
#ifdef NVM_BLOCK_USE_STATUS
  NvM_ContextType *context = &NvM_Context;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  if (context->Job.BlockId < config->NumOfBlocks) {
    config->BlockStatus[context->Job.BlockId] = NVM_REQ_NOT_OK;
  }
#endif
}

static void NvM_DoRead_OnEventStart(NvM_StepType step) {
  Std_ReturnType r = E_NOT_OK;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  uint8_t *DataPtr;
  uint16_t Length;
  P2CONST(NvM_BlockDescriptorType, AUTOMATIC, NVM_CONST) BlockDesc;

  switch (step) {
  case NVM_STEP_READ:
    BlockDesc = &config->Blocks[context->Job.BlockId];
    DataPtr = context->Job.Data;
    if (NULL == DataPtr) {
      DataPtr = (uint8_t *)BlockDesc->RamBlockDataAddress;
    }
#ifdef NVM_BLOCK_USE_CRC
    DataPtr = config->WorkingArea;
    Length = BlockDesc->NvBlockLength + NvM_GetCrcSize(BlockDesc->CrcType);
#else
    Length = BlockDesc->NvBlockLength;
#endif

    r = MemIf_Read(config->DeviceIndex, BlockDesc->NvBlockBaseNumber, 0, DataPtr, Length);
    if (E_OK == r) {
      context->State = NVM_JOB_READ | NVM_WAITING;
    } else {
      context->State = NVM_JOB_READ | NVM_PENDING;
    }
#ifdef NVM_BLOCK_USE_STATUS
    config->BlockStatus[context->Job.BlockId] = NVM_REQ_PENDING;
#endif
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoRead_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;
#ifdef NVM_BLOCK_USE_STATUS
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
#endif
  switch (step) {
  case NVM_STEP_READ:
#ifdef NVM_BLOCK_USE_CRC
    NvM_DoRead_OnEventEnd_CheckCRC();
#else
#ifdef NVM_BLOCK_USE_STATUS
    if (context->Job.BlockId < config->NumOfBlocks) {
      config->BlockStatus[context->Job.BlockId] = NVM_REQ_OK;
    }
#endif
#endif
    context->State = NVM_IDLE;
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoRead_OnEventError(void) {
#ifdef NVM_BLOCK_USE_STATUS
  NvM_ContextType *context = &NvM_Context;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  if (context->Job.BlockId < config->NumOfBlocks) {
    config->BlockStatus[context->Job.BlockId] = NVM_REQ_NOT_OK;
  }
#endif
}

static void NvM_DoWriteAll_OnEventStart(NvM_StepType step) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r;
  P2CONST(NvM_BlockDescriptorType, AUTOMATIC, NVM_CONST) BlockDesc;
  uint8_t *DataPtr;
#ifdef NVM_BLOCK_USE_CRC
  uint32_t Crc;
#endif

  switch (step) {
  case NVM_STEP_WRITE_ALL_BLOCK:
    if (context->Job.BlockId < config->NumOfBlocks) {
      BlockDesc = &config->Blocks[context->Job.BlockId];
#ifdef NVM_BLOCK_USE_CRC
      if (NVM_CRC16 == BlockDesc->CrcType) {
        DataPtr = config->WorkingArea;
        memcpy(DataPtr, BlockDesc->RamBlockDataAddress, BlockDesc->NvBlockLength);
        Crc = Crc_CalculateCRC16(DataPtr, BlockDesc->NvBlockLength, 0, TRUE);
        DataPtr[BlockDesc->NvBlockLength] = (Crc >> 8) & 0xFF;
        DataPtr[BlockDesc->NvBlockLength + 1] = Crc & 0xFF;
      } else {
        DataPtr = BlockDesc->RamBlockDataAddress;
      }
#else
      DataPtr = BlockDesc->RamBlockDataAddress;
#endif
      r = MemIf_Write(config->DeviceIndex, BlockDesc->NvBlockBaseNumber, DataPtr);
      if (E_OK == r) {
        context->State = NVM_JOB_WRITE_ALL | NVM_WAITING;
      } else {
        context->State = NVM_JOB_WRITE_ALL | NVM_PENDING;
      }
#ifdef NVM_BLOCK_USE_STATUS
      config->BlockStatus[context->Job.BlockId] = NVM_REQ_PENDING;
#endif
    } else {
      context->State = NVM_IDLE;
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWriteAll_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;
#ifdef NVM_BLOCK_USE_STATUS
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
#endif

  switch (step) {
  case NVM_STEP_WRITE_ALL_BLOCK:
#ifdef NVM_BLOCK_USE_STATUS
    if (context->Job.BlockId < config->NumOfBlocks) {
      config->BlockStatus[context->Job.BlockId] = NVM_REQ_OK;
    }
#endif
    context->Job.BlockId++;
    NvM_DoWriteAll_OnEventStart(step);
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWriteAll_OnEventError(void) {
#ifdef NVM_BLOCK_USE_STATUS
  NvM_ContextType *context = &NvM_Context;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  if (context->Job.BlockId < config->NumOfBlocks) {
    config->BlockStatus[context->Job.BlockId] = NVM_REQ_NOT_OK;
  }
#endif
}

static void NvM_DoWrite_OnEventStart(NvM_StepType step) {
  Std_ReturnType r = E_NOT_OK;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  uint8_t *data;
  uint8_t *DataPtr;
#ifdef NVM_BLOCK_USE_CRC
  uint32_t Crc;
#endif
  P2CONST(NvM_BlockDescriptorType, AUTOMATIC, NVM_CONST) BlockDesc;

  switch (step) {
  case NVM_STEP_WRITE:
    BlockDesc = &config->Blocks[context->Job.BlockId];
    data = context->Job.Data;
    if (NULL == data) {
      data = BlockDesc->RamBlockDataAddress;
    }
#ifdef NVM_BLOCK_USE_CRC
    if (NVM_CRC16 == BlockDesc->CrcType) {
      DataPtr = config->WorkingArea;
      memcpy(DataPtr, data, BlockDesc->NvBlockLength);
      Crc = Crc_CalculateCRC16(DataPtr, BlockDesc->NvBlockLength, 0, TRUE);
      DataPtr[BlockDesc->NvBlockLength] = (Crc >> 8) & 0xFF;
      DataPtr[BlockDesc->NvBlockLength + 1] = Crc & 0xFF;
    } else {
      DataPtr = data;
    }
#else
    DataPtr = data;
#endif
    r = MemIf_Write(config->DeviceIndex, BlockDesc->NvBlockBaseNumber, DataPtr);
    if (E_OK == r) {
      context->State = NVM_JOB_WRITE | NVM_WAITING;
    } else {
      context->State = NVM_JOB_WRITE | NVM_PENDING;
    }
#ifdef NVM_BLOCK_USE_STATUS
    config->BlockStatus[context->Job.BlockId] = NVM_REQ_PENDING;
#endif
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWrite_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;
#ifdef NVM_BLOCK_USE_STATUS
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
#endif
  switch (step) {
  case NVM_STEP_WRITE:
#ifdef NVM_BLOCK_USE_STATUS
    if (context->Job.BlockId < config->NumOfBlocks) {
      config->BlockStatus[context->Job.BlockId] = NVM_REQ_OK;
    }
#endif
    context->State = NVM_IDLE;
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWrite_OnEventError(void) {
#ifdef NVM_BLOCK_USE_STATUS
  NvM_ContextType *context = &NvM_Context;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  if (context->Job.BlockId < config->NumOfBlocks) {
    config->BlockStatus[context->Job.BlockId] = NVM_REQ_NOT_OK;
  }
#endif
}

NvmDoJobTemplate(Init, 0xFFu) NvmDoJobTemplate(ReadAll, 3u) NvmDoJobTemplate(Read, 3u)
  NvmDoJobTemplate(WriteAll, 3u) NvmDoJobTemplate(Write, 3u)

#if NVM_JOB_QUEUE_SIZE > 0
    static void Nvm_AddJob(NvM_JobType *pJob) {
  NvM_ContextType *context = &NvM_Context;
  /* TODO: put into Queue according to priority */
  STAILQ_INSERT_TAIL(&context->JobReqQ, pJob, entry);
}
#endif

static uint16_t NvM_GetBlockId(uint16_t *masks) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  uint16_t i;
  uint16_t j;
  uint16_t r = NVM_INVALID_BLOCKID;

  for (i = 0; (i < (config->NumOfBlocks + 15u) / 16u) && (NVM_INVALID_BLOCKID == r); i++) {
    if (masks[i] != 0u) {
      for (j = 0; (j < 16u) && (NVM_INVALID_BLOCKID == r); j++) {
        if (0u != (masks[i] & (1u << j))) {
          r = (i * 16u) + j;
          masks[i] &= ~(1u << j);
        }
      }
    }
  }

  return r;
}

static boolean NvM_IsPendingReadWrite(void) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  uint16_t i;
  boolean r = FALSE;

  for (i = 0u; (i < (config->NumOfBlocks + 15u) / 16u) && (FALSE == r); i++) {
    if ((config->ReadMasks[i] != 0u) || (config->WriteMasks[i] != 0u)) {
      r = TRUE;
    }
  }

  return r;
}

static boolean NvM_IsBlockPendingReadWrite(NvM_BlockIdType blockId) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  boolean r = FALSE;

  if (0u != (config->WriteMasks[blockId >> 4] & (1u << (blockId & 0xFu)))) {
    r = TRUE;
  } else if (0u != (config->ReadMasks[blockId >> 4] & (1u << (blockId & 0xFu)))) {
    r = TRUE;
  } else {
    /* idel */
  }

  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void NvM_Init(P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) configPtr) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  uint16_t i;
  (void)configPtr;

#if NVM_JOB_QUEUE_SIZE > 0
  STAILQ_INIT(&context->JobReqQ);
  STAILQ_INIT(&context->JobFreeQ);
  for (i = 0; i < ARRAY_SIZE(NvM_Jobs); i++) {
    STAILQ_INSERT_TAIL(&context->JobFreeQ, &NvM_Jobs[i], entry);
  }
#endif
  for (i = 0u; i < (config->NumOfBlocks + 15u) / 16u; i++) {
    config->ReadMasks[i] = 0;
    config->WriteMasks[i] = 0;
  }
#ifdef NVM_BLOCK_USE_STATUS
  (void)memset(config->BlockStatus, NVM_REQ_OK, config->NumOfBlocks);
#endif
  context->State = NVM_JOB_INIT | NVM_PENDING;
  context->Step = NVM_STEP_IDLE;
  context->RetryCounter = 0;
}

void NvM_ReadAll(void) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r = E_NOT_OK;
  uint16_t i;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif

  EnterCritical();
  if (NVM_IDLE == (NVM_STATE_MASK & context->State)) {
    context->State = NVM_JOB_READ_ALL | NVM_PENDING;
    context->Step = NVM_STEP_IDLE;
    context->Job.BlockId = 0;
    context->Job.Data = NULL;
    context->RetryCounter = 0;
    r = E_OK;
  } else {
    r = E_NOT_OK;
  }
  ExitCritical();
#if NVM_JOB_QUEUE_SIZE > 0
  if (E_OK != r) {
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->JobFreeQ)) {
      pJob = STAILQ_FIRST(&context->JobFreeQ);
      STAILQ_REMOVE_HEAD(&context->JobFreeQ, entry);
      pJob->JobType = NVM_JOB_READ_ALL;
      pJob->Job.BlockId = 0;
      pJob->Job.Data = NULL;
      Nvm_AddJob(pJob);
      r = E_OK;
    }
    ExitCritical();
  }
#endif

  if (E_OK != r) {
    EnterCritical();
    for (i = 0u; i < (config->NumOfBlocks / 16u); i++) {
      config->ReadMasks[i] = 0xFFFFu;
    }
    config->ReadMasks[((config->NumOfBlocks + 15u) / 16u) - 1u] =
      (1u << (config->NumOfBlocks & 0xFu)) - 1u;
    ExitCritical();
  }
}

void NvM_WriteAll(void) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r = E_NOT_OK;
  uint16_t i;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif

  EnterCritical();
  if (NVM_IDLE == (NVM_STATE_MASK & context->State)) {
    context->State = NVM_JOB_WRITE_ALL | NVM_PENDING;
    context->Step = NVM_STEP_IDLE;
    context->Job.BlockId = 0;
    context->Job.Data = NULL;
    context->RetryCounter = 0;
    r = E_OK;
  } else {
    r = E_NOT_OK;
  }
  ExitCritical();

#if NVM_JOB_QUEUE_SIZE > 0
  if (E_OK != r) {
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->JobFreeQ)) {
      pJob = STAILQ_FIRST(&context->JobFreeQ);
      STAILQ_REMOVE_HEAD(&context->JobFreeQ, entry);
      pJob->JobType = NVM_JOB_WRITE_ALL;
      pJob->Job.BlockId = 0;
      pJob->Job.Data = NULL;
      Nvm_AddJob(pJob);
      r = E_OK;
    }
    ExitCritical();
  }
#endif

  if (E_OK != r) {
    EnterCritical();
    for (i = 0; i < (config->NumOfBlocks / 16u); i++) {
      config->WriteMasks[i] = 0xFFFFu;
    }
    config->WriteMasks[((config->NumOfBlocks + 15u) / 16u) - 1u] =
      (1u << (config->NumOfBlocks & 0xFu)) - 1u;
    ExitCritical();
  }
}

Std_ReturnType NvM_GetBlockDataPtrAndLength(NvM_BlockIdType blockId, void **pDataPtr,
                                            uint16_t *pLength) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  Std_ReturnType r = E_NOT_OK;
  if ((blockId >= 2u) && ((blockId - 2u) < config->NumOfBlocks)) {
    *pDataPtr = config->Blocks[blockId - 2u].RamBlockDataAddress;
    *pLength = config->Blocks[blockId - 2u].NvBlockLength;
    r = E_OK;
  }
  return r;
}

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType blockId, void *NvM_DstPtr) {
  NvM_ContextType *context = &NvM_Context;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  Std_ReturnType r = E_NOT_OK;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif
  (void)context;

  DET_VALIDATE((blockId >= 2u) && ((blockId - 2u) < config->NumOfBlocks), 0x06,
               NVM_E_PARAM_BLOCK_ID, return E_NOT_OK);

#if NVM_JOB_QUEUE_SIZE > 0
  EnterCritical();
  if (FALSE == STAILQ_EMPTY(&context->JobFreeQ)) {
    pJob = STAILQ_FIRST(&context->JobFreeQ);
    STAILQ_REMOVE_HEAD(&context->JobFreeQ, entry);
    pJob->JobType = NVM_JOB_READ;
    pJob->Job.BlockId = blockId - 2u;
    pJob->Job.Data = (uint8_t *)NvM_DstPtr;
    Nvm_AddJob(pJob);
    r = E_OK;
  }
  ExitCritical();
#endif
  if (E_OK != r) {
    EnterCritical();
    config->ReadMasks[(blockId - 2u) >> 4] |= 1u << ((blockId - 2u) & 0xFu);
    ExitCritical();
    r = E_OK;
  }

  if (E_OK != r) {
    ASLOG(NVME, ("request to read block %d failed\n", blockId));
  }

  return r;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType blockId, const void *NvM_SrcPtr) {
  NvM_ContextType *context = &NvM_Context;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  Std_ReturnType r = E_NOT_OK;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif
  (void)context;

  DET_VALIDATE((blockId >= 2u) && ((blockId - 2u) < config->NumOfBlocks), 0x07,
               NVM_E_PARAM_BLOCK_ID, return E_NOT_OK);
#if NVM_JOB_QUEUE_SIZE > 0
  EnterCritical();
  if (FALSE == STAILQ_EMPTY(&context->JobFreeQ)) {
    pJob = STAILQ_FIRST(&context->JobFreeQ);
    STAILQ_REMOVE_HEAD(&context->JobFreeQ, entry);
    pJob->JobType = NVM_JOB_WRITE;
    pJob->Job.BlockId = blockId - 2u;
    pJob->Job.Data = (uint8_t *)NvM_SrcPtr;
    Nvm_AddJob(pJob);
    r = E_OK;
  }
  ExitCritical();
#endif
  if (E_OK != r) {
    EnterCritical();
    config->WriteMasks[(blockId - 2u) >> 4] |= 1u << ((blockId - 2u) & 0xFu);
    ExitCritical();
    r = E_OK;
  }

  if (E_OK != r) {
    ASLOG(NVME, ("request to write block %d failed\n", blockId));
  }

  return r;
}

Std_ReturnType NvM_SetRamBlockStatus(NvM_BlockIdType blockId, boolean blockChanged) {
  Std_ReturnType ret = E_OK;

  if (TRUE == blockChanged) {
    ret = NvM_WriteBlock(blockId, NULL);
  }

  return ret;
}

void NvM_JobEndNotification(void) {
  NvM_ContextType *context = &NvM_Context;
  NvM_StateType jobType = NVM_JOB_NONE;
  NvM_StepType step = NVM_STEP_IDLE;

  ASLOG(NVM, ("Job End in state %X, step %d\n", context->State, context->Step));

  EnterCritical();
  if (NVM_WAITING == (NVM_STATE_MASK & context->State)) {
    jobType = context->State & NVM_JOB_MASK;
    step = context->Step;
  }
  ExitCritical();

  switch (jobType) {
  case NVM_JOB_INIT:
    NvM_DoInit(step, NVM_JOB_EVENT_END);
    break;
  case NVM_JOB_READ_ALL:
    NvM_DoReadAll(step, NVM_JOB_EVENT_END);
    break;
  case NVM_JOB_READ:
    NvM_DoRead(step, NVM_JOB_EVENT_END);
    break;
  case NVM_JOB_WRITE_ALL:
    NvM_DoWriteAll(step, NVM_JOB_EVENT_END);
    break;
  case NVM_JOB_WRITE:
    NvM_DoWrite(step, NVM_JOB_EVENT_END);
    break;
  default:
    NvM_Panic();
    break;
  }
}

void NvM_JobErrorNotification(void) {
  NvM_ContextType *context = &NvM_Context;
  NvM_StateType jobType = NVM_JOB_NONE;
  NvM_StepType step = NVM_STEP_IDLE;

  ASLOG(NVME, ("Job Error in stsate %X, step %d\n", context->State, context->Step));

  EnterCritical();
  if (NVM_WAITING == (NVM_STATE_MASK & context->State)) {
    jobType = context->State & NVM_JOB_MASK;
    step = context->Step;
  }
  ExitCritical();

  switch (jobType) {
  case NVM_JOB_INIT:
    NvM_DoInit(step, NVM_JOB_EVENT_ERROR);
    break;
  case NVM_JOB_READ_ALL:
    NvM_DoReadAll(step, NVM_JOB_EVENT_ERROR);
    break;
  case NVM_JOB_READ:
    NvM_DoRead(step, NVM_JOB_EVENT_ERROR);
    break;
  case NVM_JOB_WRITE_ALL:
    NvM_DoWriteAll(step, NVM_JOB_EVENT_ERROR);
    break;
  case NVM_JOB_WRITE:
    NvM_DoWrite(step, NVM_JOB_EVENT_ERROR);
    break;
  default:
    NvM_Panic();
    break;
  }
}

void NvM_MainFunction(void) {
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  NvM_StateType jobType = NVM_JOB_NONE;
  NvM_StepType step = NVM_STEP_IDLE;
  uint16_t i;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif

#ifdef USE_FEE
  /* Fee is possible in busy status for backup if bank is full, so need to wait to ensure Fee is
   * idel
   */
  MemIf_StatusType status;
  status = MemIf_GetStatus(config->DeviceIndex);
  if (MEMIF_BUSY == status) {
    return;
  }
#endif

  EnterCritical();
  if (NVM_PENDING == (NVM_STATE_MASK & context->State)) {
    jobType = context->State & NVM_JOB_MASK;
    step = context->Step;
  }
  ExitCritical();
#if NVM_JOB_QUEUE_SIZE > 0
  if ((NVM_JOB_NONE == jobType) && (NVM_IDLE == context->State)) {
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->JobReqQ)) {
      pJob = STAILQ_FIRST(&context->JobReqQ);
      STAILQ_REMOVE_HEAD(&context->JobReqQ, entry);
      jobType = pJob->JobType;
      step = NVM_STEP_IDLE;
      context->Job = pJob->Job;
      context->State = jobType | NVM_PENDING;
      context->Step = step;
      context->RetryCounter = 0;
      STAILQ_INSERT_TAIL(&context->JobFreeQ, pJob, entry);
    }
    ExitCritical();
  }
#endif

  if ((NVM_JOB_NONE == jobType) && (NVM_IDLE == context->State)) {
    EnterCritical();
    i = NvM_GetBlockId(config->WriteMasks);
    if (i < config->NumOfBlocks) {
      jobType = NVM_JOB_WRITE;
      step = NVM_STEP_IDLE;
      context->Job.BlockId = i;
      context->Job.Data = NULL;
      context->State = jobType | NVM_PENDING;
      context->Step = step;
      context->RetryCounter = 0;
    }
    ExitCritical();
  }

  if ((NVM_JOB_NONE == jobType) && (NVM_IDLE == context->State)) {
    EnterCritical();
    i = NvM_GetBlockId(config->ReadMasks);
    if (i < config->NumOfBlocks) {
      jobType = NVM_JOB_READ;
      step = NVM_STEP_IDLE;
      context->Job.BlockId = i;
      context->Job.Data = NULL;
      context->State = jobType | NVM_PENDING;
      context->Step = step;
      context->RetryCounter = 0;
    }
    ExitCritical();
  }

  switch (jobType) {
  case NVM_JOB_NONE:
    break;
  case NVM_JOB_INIT:
    NvM_DoInit(step, NVM_JOB_EVENT_START);
    break;
  case NVM_JOB_READ_ALL:
    NvM_DoReadAll(step, NVM_JOB_EVENT_START);
    break;
  case NVM_JOB_READ:
    NvM_DoRead(step, NVM_JOB_EVENT_START);
    break;
  case NVM_JOB_WRITE_ALL:
    NvM_DoWriteAll(step, NVM_JOB_EVENT_START);
    break;
  case NVM_JOB_WRITE:
    NvM_DoWrite(step, NVM_JOB_EVENT_START);
    break;
  default:
    NvM_Panic();
    break;
  }
}

MemIf_StatusType NvM_GetStatus(void) {
  MemIf_StatusType status = MEMIF_IDLE;
  NvM_ContextType *context = &NvM_Context;
  boolean bPendingRW;

  EnterCritical();
  switch ((NVM_STATE_MASK & context->State)) {
  case NVM_PENDING:
  case NVM_WAITING:
    status = MEMIF_BUSY;
    break;
  default:
    break;
  }

#if NVM_JOB_QUEUE_SIZE > 0
  if (MEMIF_IDLE == status) {
    if (FALSE == STAILQ_EMPTY(&context->JobReqQ)) {
      status = MEMIF_BUSY;
    }
  }
#endif

  if (MEMIF_IDLE == status) {
    bPendingRW = NvM_IsPendingReadWrite();
    if (TRUE == bPendingRW) {
      status = MEMIF_BUSY;
    }
  }

#ifdef USE_FEE
  if (MEMIF_IDLE == status) {
    status = MemIf_GetStatus(config->DeviceIndex);
  }
#endif
  ExitCritical();

  return status;
}

#ifdef NVM_BLOCK_USE_STATUS
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType blockId,
                                  NvM_RequestResultType *requestResultPtr) {
  Std_ReturnType ret = E_OK;
  P2CONST(NvM_ConfigType, AUTOMATIC, NVM_CONST) config = NVM_CONFIG;
  boolean bRWPending;

  DET_VALIDATE(NULL != requestResultPtr, 0xFE, NVM_E_PARAM_POINTER, return E_NOT_OK);

  if ((blockId >= 2u) && ((blockId - 2u) < config->NumOfBlocks)) {
    *requestResultPtr = config->BlockStatus[blockId - 2u];
    if (NVM_REQ_OK == *requestResultPtr) {
      bRWPending = NvM_IsBlockPendingReadWrite(blockId - 2u);
      if (TRUE == bRWPending) {
        *requestResultPtr = NVM_REQ_PENDING;
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}

void NvM_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x0f, NVM_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_NVM;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 1;
}
#endif

/** @brief release notes
 * - 4.0.1: Typo Fix and add DET validate
 */
