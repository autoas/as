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
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_NVM 0
#define AS_LOG_NVMI 1
#define AS_LOG_NVME 2

#define NVM_CONFIG (&NvM_Config)

/* the low 4 bit used for state, the high 4 bit used for job type */
#define NVM_STATE_MASK ((NvM_StateType)0x0F)
#define NVM_JOB_MASK ((NvM_StateType)0xF0)

#define NVM_IDEL ((NvM_StateType)0)
#define NVM_PENDING ((NvM_StateType)1)
#define NVM_WAITING ((NvM_StateType)2) /* Waiting Fls Job Finished or Error Exited */

#define NVM_JOB_NONE ((NvM_StateType)0x00)
#define NVM_JOB_INIT ((NvM_StateType)0x10)
#define NVM_JOB_READ_ALL ((NvM_StateType)0x20)
#define NVM_JOB_READ ((NvM_StateType)0x30)
#define NVM_JOB_WRITE_ALL ((NvM_StateType)0x40)
#define NVM_JOB_WRITE ((NvM_StateType)0x50)

#define NVM_STEP_IDEL ((NvM_StepType)0)

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

#define NVM_INVALID_BLOCKID 0xFFFF

#define NVM_DOJOB_TEMPLATE(jobName, retryMax)                                                      \
  static void NvM_Do##jobName(NvM_StepType step, NvM_JobEventType event) {                         \
    NvM_ContextType *context = &NvM_Context;                                                       \
    switch (event) {                                                                               \
    case NVM_JOB_EVENT_START:                                                                      \
    case NVM_JOB_EVENT_ERROR:                                                                      \
      if (context->retryCounter <= retryMax) {                                                     \
        context->retryCounter++;                                                                   \
        NvM_Do##jobName##_OnEventStart(step);                                                      \
      } else {                                                                                     \
        context->state = NVM_IDEL;                                                                 \
        context->retryCounter = 0;                                                                 \
        ASLOG(NVME, ("NvM Do " #jobName " Job Failed\n"));                                         \
      }                                                                                            \
      break;                                                                                       \
    case NVM_JOB_EVENT_END:                                                                        \
      context->retryCounter = 0;                                                                   \
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
  uint16_t blockId;
  uint8_t *data;
} NvM_JobInfoType;

typedef struct NvM_Job_s {
  NvM_StateType jobType;
  NvM_JobInfoType job;
  STAILQ_ENTRY(NvM_Job_s) entry;
} NvM_JobType;

typedef struct {
  NvM_StepType step;
  NvM_JobInfoType job;
#if NVM_JOB_QUEUE_SIZE > 0
  STAILQ_HEAD(Nvm_JobReqQ_s, NvM_Job_s) jobReqQ;
  STAILQ_HEAD(Nvm_JobFreeQ_s, NvM_Job_s) jobFreeQ;
#endif
  NvM_StateType state;
  uint8_t retryCounter;
} NvM_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern const NvM_ConfigType NvM_Config;
/* ================================ [ DATAS     ] ============================================== */
static NvM_ContextType NvM_Context;
#if NVM_JOB_QUEUE_SIZE > 0
static NvM_JobType NvM_Jobs[NVM_JOB_QUEUE_SIZE];
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void NvM_Panic(void) {
  NvM_ContextType *context = &NvM_Context;
  ASLOG(NVME, ("NVM panic in state 0x%X step %d\n", context->state, context->step));
  context->state = NVM_IDEL;
}

static void NvM_DoInit_OnEventStart(NvM_StepType step) {
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  MemIf_StatusType ifStatus;
  (void)config;

  switch (step) {
  case NVM_STEP_INIT_WAIT_MEMIF_IDLE:
    ifStatus = MemIf_GetStatus(config->DeviceIndex);
    if (MEMIF_IDLE == ifStatus) {
      context->state = NVM_IDEL;
      context->step = NVM_STEP_IDEL;
      ASLOG(NVM, ("MemIf is idel\n"));
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoInit_OnEventEnd(NvM_StepType step) {
  NvM_Panic();
}
#ifdef NVM_BLOCK_USE_CRC
static uint16_t getCrcSize(NvM_BlockCrcType crcType) {
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
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r;
  const NvM_BlockDescriptorType *BlockDesc;
  uint8_t *DataPtr;
  uint16_t Length;

  switch (step) {
  case NVM_STEP_READ_ALL_BLOCK:
    if (context->job.blockId < config->numOfBlocks) {
      BlockDesc = &config->Blocks[context->job.blockId];
#ifdef NVM_BLOCK_USE_CRC
      DataPtr = config->workingArea;
      Length = BlockDesc->NvBlockLength + getCrcSize(BlockDesc->CrcType);
#else
      DataPtr = BlockDesc->RamBlockDataAddress;
      Length = BlockDesc->NvBlockLength;
#endif
      r = MemIf_Read(config->DeviceIndex, BlockDesc->NvBlockBaseNumber, 0, DataPtr, Length);
      if (E_OK == r) {
        context->state = NVM_JOB_READ_ALL | NVM_WAITING;
      } else {
        context->state = NVM_JOB_READ_ALL | NVM_PENDING;
      }
    } else {
      context->state = NVM_IDEL;
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

#ifdef NVM_BLOCK_USE_CRC
static void NvM_DoRead_OnEventEnd_CheckCRC(void) {
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  const NvM_BlockDescriptorType *BlockDesc;
  uint8_t *DataPtr;
  uint8_t *dstPtr;
  uint16_t Length;
  uint32_t CrcR, CrcC;
  if (context->job.blockId < config->numOfBlocks) {
    BlockDesc = &config->Blocks[context->job.blockId];
    dstPtr = context->job.data;
    if (NULL == dstPtr) {
      dstPtr = BlockDesc->RamBlockDataAddress;
    }
    DataPtr = config->workingArea;
    Length = BlockDesc->NvBlockLength;
    /* NOTE: now only support CRC16 and NONE */
    if (NVM_CRC16 == BlockDesc->CrcType) {
      CrcR = ((uint32_t)DataPtr[Length] << 8) + DataPtr[Length + 1];
      CrcC = Crc_CalculateCRC16(DataPtr, Length, 0, TRUE);
      if (CrcC != CrcR) {
        ASLOG(NVME, ("Block %d CRC invalid\n", context->job.blockId));
        memcpy(dstPtr, BlockDesc->Rom, Length);
      } else {
        memcpy(dstPtr, DataPtr, Length);
      }
    } else {
      memcpy(dstPtr, DataPtr, Length);
    }
  } else {
    NvM_Panic();
  }
}
#endif

static void NvM_DoReadAll_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;

  switch (step) {
  case NVM_STEP_READ_ALL_BLOCK:
#ifdef NVM_BLOCK_USE_CRC
    NvM_DoRead_OnEventEnd_CheckCRC();
#endif
    context->job.blockId++;
    NvM_DoReadAll_OnEventStart(step);
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoRead_OnEventStart(NvM_StepType step) {
  Std_ReturnType r = E_NOT_OK;
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  uint8_t *DataPtr;
  uint16_t Length;
  const NvM_BlockDescriptorType *BlockDesc;

  switch (step) {
  case NVM_STEP_READ:
    BlockDesc = &config->Blocks[context->job.blockId];
    DataPtr = context->job.data;
    if (NULL == DataPtr) {
      DataPtr = BlockDesc->RamBlockDataAddress;
    }
#ifdef NVM_BLOCK_USE_CRC
    DataPtr = config->workingArea;
    Length = BlockDesc->NvBlockLength + getCrcSize(BlockDesc->CrcType);
#else
    Length = BlockDesc->NvBlockLength;
#endif

    r = MemIf_Read(config->DeviceIndex, BlockDesc->NvBlockBaseNumber, 0, DataPtr, Length);
    if (E_OK == r) {
      context->state = NVM_JOB_READ | NVM_WAITING;
    } else {
      context->state = NVM_JOB_READ | NVM_PENDING;
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoRead_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;

  switch (step) {
  case NVM_STEP_READ:
#ifdef NVM_BLOCK_USE_CRC
    NvM_DoRead_OnEventEnd_CheckCRC();
#endif
    context->state = NVM_IDEL;
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWriteAll_OnEventStart(NvM_StepType step) {
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r;
  const NvM_BlockDescriptorType *BlockDesc;
  uint8_t *DataPtr;
#ifdef NVM_BLOCK_USE_CRC
  uint32_t Crc;
#endif

  switch (step) {
  case NVM_STEP_READ_ALL_BLOCK:
    if (context->job.blockId < config->numOfBlocks) {
      BlockDesc = &config->Blocks[context->job.blockId];
#ifdef NVM_BLOCK_USE_CRC
      if (NVM_CRC16 == BlockDesc->CrcType) {
        DataPtr = config->workingArea;
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
        context->state = NVM_JOB_WRITE_ALL | NVM_WAITING;
      } else {
        context->state = NVM_JOB_WRITE_ALL | NVM_PENDING;
      }
    } else {
      context->state = NVM_IDEL;
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWriteAll_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;

  switch (step) {
  case NVM_STEP_WRITE_ALL_BLOCK:
    context->job.blockId++;
    NvM_DoWriteAll_OnEventStart(step);
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWrite_OnEventStart(NvM_StepType step) {
  Std_ReturnType r = E_NOT_OK;
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  uint8_t *data;
  uint8_t *DataPtr;
#ifdef NVM_BLOCK_USE_CRC
  uint32_t Crc;
#endif
  const NvM_BlockDescriptorType *BlockDesc;

  switch (step) {
  case NVM_STEP_WRITE:
    BlockDesc = &config->Blocks[context->job.blockId];
    data = context->job.data;
    if (NULL == data) {
      data = BlockDesc->RamBlockDataAddress;
    }
#ifdef NVM_BLOCK_USE_CRC
    if (NVM_CRC16 == BlockDesc->CrcType) {
      DataPtr = config->workingArea;
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
      context->state = NVM_JOB_WRITE | NVM_WAITING;
    } else {
      context->state = NVM_JOB_WRITE | NVM_PENDING;
    }
    break;
  default:
    NvM_Panic();
    break;
  }
}

static void NvM_DoWrite_OnEventEnd(NvM_StepType step) {
  NvM_ContextType *context = &NvM_Context;

  switch (step) {
  case NVM_STEP_WRITE:
    context->state = NVM_IDEL;
    break;
  default:
    NvM_Panic();
    break;
  }
}

NVM_DOJOB_TEMPLATE(Init, 0xFF)
NVM_DOJOB_TEMPLATE(ReadAll, 3)
NVM_DOJOB_TEMPLATE(Read, 3)
NVM_DOJOB_TEMPLATE(WriteAll, 3)
NVM_DOJOB_TEMPLATE(Write, 3)

#if NVM_JOB_QUEUE_SIZE > 0
static void Nvm_AddJob(NvM_JobType *pJob) {
  NvM_ContextType *context = &NvM_Context;
  /* TODO: put into Queue according to priority */
  STAILQ_INSERT_TAIL(&context->jobReqQ, pJob, entry);
}
#endif

static uint16_t Nvm_GetBlockId(uint16_t *masks) {
  const NvM_ConfigType *config = NVM_CONFIG;
  uint16_t i, j, r = NVM_INVALID_BLOCKID;

  for (i = 0; (i < (config->numOfBlocks + 15) / 16) && (NVM_INVALID_BLOCKID == r); i++) {
    if (masks[i] != 0) {
      for (j = 0; (j < 16) && (NVM_INVALID_BLOCKID == r); j++) {
        if (masks[i] & (1 << j)) {
          r = i * 16 + j;
          masks[i] &= ~(1 << j);
        }
      }
    }
  }

  return r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void NvM_Init(const NvM_ConfigType *ConfigPtr) {
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  int i;
  (void)ConfigPtr;

#if NVM_JOB_QUEUE_SIZE > 0
  STAILQ_INIT(&context->jobReqQ);
  STAILQ_INIT(&context->jobFreeQ);
  for (i = 0; i < ARRAY_SIZE(NvM_Jobs); i++) {
    STAILQ_INSERT_TAIL(&context->jobFreeQ, &NvM_Jobs[i], entry);
  }
#endif
  for (i = 0; i < (config->numOfBlocks + 15) / 16; i++) {
    config->readMasks[i] = 0;
    config->writeMasks[i] = 0;
  }
  context->state = NVM_JOB_INIT | NVM_PENDING;
  context->step = NVM_STEP_IDEL;
  context->retryCounter = 0;
}

void NvM_ReadAll(void) {
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r = E_NOT_OK;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif

  EnterCritical();
  if (NVM_IDEL == (NVM_STATE_MASK & context->state)) {
    context->state = NVM_JOB_READ_ALL | NVM_PENDING;
    context->step = NVM_STEP_IDEL;
    context->job.blockId = 0;
    context->job.data = NULL;
    context->retryCounter = 0;
    r = E_OK;
  } else {
    r = E_NOT_OK;
  }
  ExitCritical();
#if NVM_JOB_QUEUE_SIZE > 0
  if (E_OK != r) {
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->jobFreeQ)) {
      pJob = STAILQ_FIRST(&context->jobFreeQ);
      STAILQ_REMOVE_HEAD(&context->jobFreeQ, entry);
      pJob->jobType = NVM_JOB_READ_ALL;
      pJob->job.blockId = 0;
      pJob->job.data = NULL;
      Nvm_AddJob(pJob);
      r = E_OK;
    }
    ExitCritical();
  }
#endif

  if (E_OK != r) {
    EnterCritical();
    memset(config->readMasks, 0xFF, 2 * ((config->numOfBlocks + 15) / 16));
    ExitCritical();
  }
}

void NvM_WriteAll(void) {
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  Std_ReturnType r = E_NOT_OK;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif

  EnterCritical();
  if (NVM_IDEL == (NVM_STATE_MASK & context->state)) {
    context->state = NVM_JOB_WRITE_ALL | NVM_PENDING;
    context->step = NVM_STEP_IDEL;
    context->job.blockId = 0;
    context->job.data = NULL;
    context->retryCounter = 0;
    r = E_OK;
  } else {
    r = E_NOT_OK;
  }

  ExitCritical();
#if NVM_JOB_QUEUE_SIZE > 0
  if (E_OK != r) {
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->jobFreeQ)) {
      pJob = STAILQ_FIRST(&context->jobFreeQ);
      STAILQ_REMOVE_HEAD(&context->jobFreeQ, entry);
      pJob->jobType = NVM_JOB_WRITE_ALL;
      pJob->job.blockId = 0;
      pJob->job.data = NULL;
      Nvm_AddJob(pJob);
      r = E_OK;
    }
    ExitCritical();
  }
#endif

  if (E_OK != r) {
    EnterCritical();
    memset(config->writeMasks, 0xFF, 2 * ((config->numOfBlocks + 15) / 16));
    ExitCritical();
  }
}

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr) {
  NvM_ContextType *context = &NvM_Context;
  const NvM_ConfigType *config = NVM_CONFIG;
  Std_ReturnType r = E_NOT_OK;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif
  (void)context;

  if ((BlockId >= 2) && ((BlockId - 2) < config->numOfBlocks)) {
#if NVM_JOB_QUEUE_SIZE > 0
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->jobFreeQ)) {
      pJob = STAILQ_FIRST(&context->jobFreeQ);
      STAILQ_REMOVE_HEAD(&context->jobFreeQ, entry);
      pJob->jobType = NVM_JOB_READ;
      pJob->job.blockId = BlockId - 2;
      pJob->job.data = (uint8_t *)NvM_DstPtr;
      Nvm_AddJob(pJob);
      r = E_OK;
    }
    ExitCritical();
#endif
    if (E_OK != r) {
      EnterCritical();
      config->readMasks[(BlockId - 2) >> 4] |= 1 << ((BlockId - 2) & 0xF);
      ExitCritical();
      r = E_OK;
    }
  }

  if (E_OK != r) {
    ASLOG(NVME, ("request to read block %d failed\n", BlockId));
  }

  return r;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr) {
  NvM_ContextType *context = &NvM_Context;
  const NvM_ConfigType *config = NVM_CONFIG;
  Std_ReturnType r = E_NOT_OK;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif
  (void)context;

  if ((BlockId >= 2) && ((BlockId - 2) < config->numOfBlocks)) {
#if NVM_JOB_QUEUE_SIZE > 0
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->jobFreeQ)) {
      pJob = STAILQ_FIRST(&context->jobFreeQ);
      STAILQ_REMOVE_HEAD(&context->jobFreeQ, entry);
      pJob->jobType = NVM_JOB_WRITE;
      pJob->job.blockId = BlockId - 2;
      pJob->job.data = (uint8_t *)NvM_SrcPtr;
      Nvm_AddJob(pJob);
      r = E_OK;
    }
    ExitCritical();
#endif
    if (E_OK != r) {
      EnterCritical();
      config->writeMasks[(BlockId - 2) >> 4] |= 1 << ((BlockId - 2) & 0xF);
      ExitCritical();
      r = E_OK;
    }
  }

  if (E_OK != r) {
    ASLOG(NVME, ("request to write block %d failed\n", BlockId));
  }

  return r;
}

void NvM_JobEndNotification(void) {
  NvM_ContextType *context = &NvM_Context;
  NvM_StateType jobType = NVM_JOB_NONE;
  NvM_StepType step;

  ASLOG(NVM, ("Job End in state %X, step %d\n", context->state, context->step));

  EnterCritical();
  if (NVM_WAITING == (NVM_STATE_MASK & context->state)) {
    jobType = context->state & NVM_JOB_MASK;
    step = context->step;
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
  NvM_StepType step;

  ASLOG(NVM, ("Job Error in state %X, step %d\n", context->state, context->step));

  EnterCritical();
  if (NVM_WAITING == (NVM_STATE_MASK & context->state)) {
    jobType = context->state & NVM_JOB_MASK;
    step = context->step;
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
  const NvM_ConfigType *config = NVM_CONFIG;
  NvM_ContextType *context = &NvM_Context;
  NvM_StateType jobType = NVM_JOB_NONE;
  NvM_StepType step;
  uint16_t i;
#if NVM_JOB_QUEUE_SIZE > 0
  NvM_JobType *pJob = NULL;
#endif

  EnterCritical();
  if (NVM_PENDING == (NVM_STATE_MASK & context->state)) {
    jobType = context->state & NVM_JOB_MASK;
    step = context->step;
  }
  ExitCritical();
#if NVM_JOB_QUEUE_SIZE > 0
  if ((NVM_JOB_NONE == jobType) && (NVM_IDEL == context->state)) {
    EnterCritical();
    if (FALSE == STAILQ_EMPTY(&context->jobReqQ)) {
      pJob = STAILQ_FIRST(&context->jobReqQ);
      STAILQ_REMOVE_HEAD(&context->jobReqQ, entry);
      jobType = pJob->jobType;
      step = NVM_STEP_IDEL;
      context->job = pJob->job;
      context->state = jobType | NVM_PENDING;
      context->step = step;
      context->retryCounter = 0;
      STAILQ_INSERT_TAIL(&context->jobFreeQ, pJob, entry);
    }
    ExitCritical();
  }
#endif

  if ((NVM_JOB_NONE == jobType) && (NVM_IDEL == context->state)) {
    EnterCritical();
    i = Nvm_GetBlockId(config->writeMasks);
    if (i <= config->numOfBlocks) {
      jobType = NVM_JOB_WRITE;
      step = NVM_STEP_IDEL;
      context->job.blockId = i;
      context->job.data = NULL;
      context->state = jobType | NVM_PENDING;
      context->step = step;
      context->retryCounter = 0;
    }
    ExitCritical();
  }

  if ((NVM_JOB_NONE == jobType) && (NVM_IDEL == context->state)) {
    EnterCritical();
    i = Nvm_GetBlockId(config->readMasks);
    if (i <= config->numOfBlocks) {
      jobType = NVM_JOB_READ;
      step = NVM_STEP_IDEL;
      context->job.blockId = i;
      context->job.data = NULL;
      context->state = jobType | NVM_PENDING;
      context->step = step;
      context->retryCounter = 0;
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

  switch ((NVM_STATE_MASK & context->state)) {
  case NVM_PENDING:
  case NVM_WAITING:
    status = MEMIF_BUSY;
    break;
  default:
    break;
  }

  return status;
}
