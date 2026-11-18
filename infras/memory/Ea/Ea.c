/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of EEPROM Abstraction AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Ea.h"
#include "Ea_Cfg.h"
#include "Ea_Priv.h"
#include "Eep.h"
#include "Std_Critical.h"
#include "Std_Debug.h"

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_EA 0
#define AS_LOG_EAI 1
#define AS_LOG_EAE 2

#define EA_CONFIG (&Ea_Config)

/* the low 4 bit used for state, the high 4 bit used for job type */
#define EA_STATE_MASK ((Ea_StateType)0x0F)
#define EA_JOB_MASK ((Ea_StateType)0xF0)

#define EA_IDLE ((Ea_StateType)0)
#define EA_PENDING ((Ea_StateType)1)
#define EA_WAITING ((Ea_StateType)2) /* Waiting Fls Job Finished or Error Exited */

#define EA_JOB_NONE ((Ea_StateType)0x00)
#define EA_JOB_READ ((Ea_StateType)0x10)
#define EA_JOB_WRITE ((Ea_StateType)0x20)

#define EA_STEP_IDLE ((Ea_StepType)0)

#define EA_READ_STEP1_READ ((Ea_StepType)0)

#define EA_WRITE_STEP1_ERASE ((Ea_StepType)0)
#define EA_WRITE_STEP2_WRITE ((Ea_StepType)1)

#define EA_JOB_EVENT_START ((Ea_JobEventType)0)
#define EA_JOB_EVENT_ERROR ((Ea_JobEventType)1)
#define EA_JOB_EVENT_END ((Ea_JobEventType)2)

#define ALIGNED(sz, alignsz) ((sz + alignsz - 1) & (~(alignsz - 1)))
#define EA_ALIGNED(sz) ALIGNED(sz, EA_SECTOR_SIZE)

#define EaDoJobTemplate(jobName, retryMax)                                                         \
  static void Ea_Do##jobName(Ea_StepType step, Ea_JobEventType event) {                            \
    Ea_ContextType *context = &Ea_Context;                                                         \
    P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) config = EA_CONFIG;                                \
    switch (event) {                                                                               \
    case EA_JOB_EVENT_START:                                                                       \
    case EA_JOB_EVENT_ERROR:                                                                       \
      if (context->RetryCounter <= retryMax) {                                                     \
        context->RetryCounter++;                                                                   \
        Ea_Do##jobName##_OnEventStart(step);                                                       \
      } else {                                                                                     \
        context->State = EA_IDLE;                                                                  \
        context->RetryCounter = 0;                                                                 \
        ASLOG(EAE, ("Ea Do " #jobName " Job Failed\n"));                                           \
        config->JobErrorNotification();                                                            \
      }                                                                                            \
      break;                                                                                       \
    case EA_JOB_EVENT_END:                                                                         \
      Ea_Do##jobName##_OnEventEnd(step);                                                           \
      break;                                                                                       \
    default:                                                                                       \
      Ea_Panic();                                                                                  \
      break;                                                                                       \
    }                                                                                              \
  }
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t Ea_StateType;
typedef uint8_t Ea_StepType;
typedef uint8_t Ea_JobEventType;
typedef struct {
  Ea_StateType State;
  Ea_StepType Step;
  struct {
    uint16_t BlockId;
    uint16_t BlockOffset;
    uint8_t *DataBufferPtr;
    uint16_t Length;
  } Job;
  uint8_t RetryCounter;
} Ea_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern CONSTANT(Ea_ConfigType, EA_CONST) Ea_Config;
/* ================================ [ DATAS     ] ============================================== */
static Ea_ContextType Ea_Context;
/* ================================ [ LOCALS    ] ============================================== */
static void Ea_Panic(void) {
  Ea_ContextType *context = &Ea_Context;
  ASLOG(EAE, ("Ea panic in state 0x%X step %d\n", context->State, context->Step));
  context->State = EA_IDLE;
  context->RetryCounter = 0;
}

static void Ea_DoRead_OnEventStart(Ea_StepType step) {
  P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r;
  switch (step) {
  case EA_READ_STEP1_READ:
    r = Eep_Read(config->Blocks[context->Job.BlockId].BlockAddress + context->Job.BlockOffset,
                 context->Job.DataBufferPtr, context->Job.Length);
    if (E_OK == r) {
      context->State = EA_JOB_READ | EA_WAITING;
    } else {
      context->State = EA_JOB_READ | EA_PENDING;
    }
    break;
  default:
    Ea_Panic();
    break;
  }
}

static void Ea_DoRead_OnEventEnd(Ea_StepType step) {
  P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  switch (step) {
  case EA_READ_STEP1_READ:
    context->State = EA_IDLE;
    context->RetryCounter = 0;
    config->JobEndNotification();
    break;
  default:
    Ea_Panic();
    break;
  }
}

static void Ea_DoWrite_OnEventStart(Ea_StepType step) {
  P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r;
  switch (step) {
  case EA_WRITE_STEP1_ERASE:
    r = Eep_Erase(config->Blocks[context->Job.BlockId].BlockAddress,
                  EA_ALIGNED(config->Blocks[context->Job.BlockId].BlockSize));
    if (E_OK == r) {
      context->State = EA_JOB_WRITE | EA_WAITING;
    } else {
      context->State = EA_JOB_WRITE | EA_PENDING;
    }
    break;
  case EA_WRITE_STEP2_WRITE:
    r = Eep_Write(config->Blocks[context->Job.BlockId].BlockAddress, context->Job.DataBufferPtr,
                  config->Blocks[context->Job.BlockId].BlockSize);
    if (E_OK == r) {
      context->State = EA_JOB_WRITE | EA_WAITING;
    } else {
      context->State = EA_JOB_WRITE | EA_PENDING;
    }
    context->Step = EA_WRITE_STEP2_WRITE;
    break;
  default:
    Ea_Panic();
    break;
  }
}

static void Ea_DoWrite_OnEventEnd(Ea_StepType step) {
  P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  switch (step) {
  case EA_WRITE_STEP1_ERASE:
    Ea_DoWrite_OnEventStart(EA_WRITE_STEP2_WRITE);
    break;
  case EA_WRITE_STEP2_WRITE:
    context->State = EA_IDLE;
    context->RetryCounter = 0;
    config->JobEndNotification();
    break;
  default:
    Ea_Panic();
    break;
  }
}

EaDoJobTemplate(Read, 0) EaDoJobTemplate(Write, 0)
  /* ================================ [ FUNCTIONS ] ==============================================
   */
  void Ea_Init(P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) configPtr) {
  Ea_ContextType *context = &Ea_Context;
  (void)configPtr;

  context->State = EA_IDLE;
  context->Step = EA_STEP_IDLE;
  context->RetryCounter = 0;
}

void Ea_SetMode(MemIf_ModeType mode) {
  Eep_SetMode(mode);
}

Std_ReturnType Ea_Read(uint16_t blockNumber, uint16_t blockOffset, uint8_t *dataBufferPtr,
                       uint16_t length) {
#if defined(USE_DET) && defined(DET_THIS_MODULE_ID)
  P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) config = EA_CONFIG;
#endif
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r;

  DET_VALIDATE((blockNumber > 0) && ((blockNumber - 1) < config->NumOfBlocks), 0x02,
               EA_E_INVALID_BLOCK_NO, return E_NOT_OK);
  DET_VALIDATE(config->Blocks[blockNumber - 1].BlockSize >= ((uint32_t)blockOffset + length), 0x02,
               EA_E_INVALID_BLOCK_OFS, return E_NOT_OK);

  EnterCritical();
  if (EA_IDLE == (EA_STATE_MASK & context->State)) {
    context->State = EA_JOB_READ | EA_PENDING;
    context->Step = EA_STEP_IDLE;
    context->RetryCounter = 0;
    r = E_OK;
  } else {
    r = E_NOT_OK;
  }
  ExitCritical();

  if (E_OK == r) {
    context->Job.BlockId = blockNumber - 1;
    context->Job.BlockOffset = blockOffset;
    context->Job.DataBufferPtr = dataBufferPtr;
    context->Job.Length = length;
  }

  return r;
}

Std_ReturnType Ea_Write(uint16_t blockNumber, const uint8_t *dataBufferPtr) {
#if defined(USE_DET) && defined(DET_THIS_MODULE_ID)
  P2CONST(Ea_ConfigType, AUTOMATIC, EA_CONST) config = EA_CONFIG;
#endif
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r;

  DET_VALIDATE((blockNumber > 0) && ((blockNumber - 1) < config->NumOfBlocks), 0x03,
               EA_E_INVALID_BLOCK_NO, return E_NOT_OK);
  DET_VALIDATE(NULL != dataBufferPtr, 0x03, EA_E_PARAM_POINTER, return E_NOT_OK);

  EnterCritical();
  if (EA_IDLE == (EA_STATE_MASK & context->State)) {
    context->State = EA_JOB_WRITE | EA_PENDING;
    context->Step = EA_STEP_IDLE;
    context->RetryCounter = 0;
    r = E_OK;
  } else {
    r = E_NOT_OK;
  }
  ExitCritical();

  if (E_OK == r) {
    context->Job.BlockId = blockNumber - 1;
    context->Job.DataBufferPtr = (uint8_t *)dataBufferPtr;
  }

  return r;
}

void Ea_Cancel(void) {
  Eep_Cancel();
}

MemIf_StatusType Ea_GetStatus(void) {
  return Eep_GetStatus();
}

MemIf_JobResultType Ea_GetJobResult(void) {
  return Eep_GetJobResult();
}

Std_ReturnType Ea_InvalidateBlock(uint16_t blockNumber) {
  return E_NOT_OK;
}

Std_ReturnType Ea_EraseImmediateBlock(uint16_t blockNumber) {
  return E_NOT_OK;
}

void Ea_JobEndNotification(void) {
  Ea_ContextType *context = &Ea_Context;
  Ea_StateType jobType = EA_JOB_NONE;
  Ea_StepType step;

  ASLOG(EA, ("Job End in state %X, step %d\n", context->State, context->Step));

  EnterCritical();
  if (EA_WAITING == (EA_STATE_MASK & context->State)) {
    jobType = context->State & EA_JOB_MASK;
    step = context->Step;
  }
  ExitCritical();

  switch (jobType) {
  case EA_JOB_READ:
    Ea_DoRead(step, EA_JOB_EVENT_END);
    break;
  case EA_JOB_WRITE:
    Ea_DoWrite(step, EA_JOB_EVENT_END);
    break;
  default:
    Ea_Panic();
    break;
  }
}

void Ea_JobErrorNotification(void) {
  Ea_ContextType *context = &Ea_Context;
  Ea_StateType jobType = EA_JOB_NONE;
  Ea_StepType step;

  ASLOG(EA, ("Job Error in state %X, step %d\n", context->State, context->Step));

  EnterCritical();
  if (EA_WAITING == (EA_STATE_MASK & context->State)) {
    jobType = context->State & EA_JOB_MASK;
    step = context->Step;
  }
  ExitCritical();

  switch (jobType) {
  case EA_JOB_READ:
    Ea_DoRead(step, EA_JOB_EVENT_ERROR);
    break;
  case EA_JOB_WRITE:
    Ea_DoWrite(step, EA_JOB_EVENT_ERROR);
    break;
  default:
    Ea_Panic();
    break;
  }
}

void Ea_MainFunction(void) {
  Ea_ContextType *context = &Ea_Context;
  Ea_StateType jobType = EA_JOB_NONE;
  Ea_StepType step;

  EnterCritical();
  if (EA_PENDING == (EA_STATE_MASK & context->State)) {
    jobType = context->State & EA_JOB_MASK;
    step = context->Step;
  }
  ExitCritical();

  switch (jobType) {
  case EA_JOB_NONE:
    break;
  case EA_JOB_READ:
    Ea_DoRead(step, EA_JOB_EVENT_START);
    break;
  case EA_JOB_WRITE:
    Ea_DoWrite(step, EA_JOB_EVENT_START);
    break;
  default:
    Ea_Panic();
    break;
  }
}

void Ea_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x08, EA_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_EA;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
