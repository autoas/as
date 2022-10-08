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
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_EA 0
#define AS_LOG_EAI 1
#define AS_LOG_EAE 2

#define EA_CONFIG (&Ea_Config)

/* the low 4 bit used for state, the high 4 bit used for job type */
#define EA_STATE_MASK ((Ea_StateType)0x0F)
#define EA_JOB_MASK ((Ea_StateType)0xF0)

#define EA_IDEL ((Ea_StateType)0)
#define EA_PENDING ((Ea_StateType)1)
#define EA_WAITING ((Ea_StateType)2) /* Waiting Fls Job Finished or Error Exited */

#define EA_JOB_NONE ((Ea_StateType)0x00)
#define EA_JOB_READ ((Ea_StateType)0x10)
#define EA_JOB_WRITE ((Ea_StateType)0x20)

#define EA_STEP_IDEL ((Ea_StepType)0)

#define EA_READ_STEP1_READ ((Ea_StepType)0)

#define EA_WRITE_STEP1_ERASE ((Ea_StepType)0)
#define EA_WRITE_STEP2_WRITE ((Ea_StepType)1)

#define EA_JOB_EVENT_START ((Ea_JobEventType)0)
#define EA_JOB_EVENT_ERROR ((Ea_JobEventType)1)
#define EA_JOB_EVENT_END ((Ea_JobEventType)2)

#define ALIGNED(sz, alignsz) ((sz + alignsz - 1) & (~(alignsz - 1)))
#define EA_ALIGNED(sz) ALIGNED(sz, EA_SECTOR_SIZE)

#define EA_DOJOB_TEMPLATE(jobName, retryMax)                                                       \
  static void Ea_Do##jobName(Ea_StepType step, Ea_JobEventType event) {                            \
    Ea_ContextType *context = &Ea_Context;                                                         \
    const Ea_ConfigType *config = EA_CONFIG;                                                       \
    switch (event) {                                                                               \
    case EA_JOB_EVENT_START:                                                                       \
    case EA_JOB_EVENT_ERROR:                                                                       \
      if (context->retryCounter <= retryMax) {                                                     \
        context->retryCounter++;                                                                   \
        Ea_Do##jobName##_OnEventStart(step);                                                       \
      } else {                                                                                     \
        context->state = EA_IDEL;                                                                  \
        context->retryCounter = 0;                                                                 \
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
  Ea_StateType state;
  Ea_StepType step;
  struct {
    uint16_t BlockId;
    uint16_t BlockOffset;
    uint8_t *DataBufferPtr;
    uint16_t Length;
  } job;
  uint8_t retryCounter;
} Ea_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern const Ea_ConfigType Ea_Config;
/* ================================ [ DATAS     ] ============================================== */
static Ea_ContextType Ea_Context;
/* ================================ [ LOCALS    ] ============================================== */
static void Ea_Panic(void) {
  Ea_ContextType *context = &Ea_Context;
  ASLOG(EAE, ("Ea panic in state 0x%X step %d\n", context->state, context->step));
  context->state = EA_IDEL;
  context->retryCounter = 0;
}

static void Ea_DoRead_OnEventStart(Ea_StepType step) {
  const Ea_ConfigType *config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r;
  switch (step) {
  case EA_READ_STEP1_READ:
    r = Eep_Read(config->Blocks[context->job.BlockId].BlockAddress + context->job.BlockOffset,
                 context->job.DataBufferPtr, context->job.Length);
    if (E_OK == r) {
      context->state = EA_JOB_READ | EA_WAITING;
    } else {
      context->state = EA_JOB_READ | EA_PENDING;
    }
    break;
  default:
    Ea_Panic();
    break;
  }
}

static void Ea_DoRead_OnEventEnd(Ea_StepType step) {
  const Ea_ConfigType *config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  switch (step) {
  case EA_READ_STEP1_READ:
    context->state = EA_IDEL;
    context->retryCounter = 0;
    config->JobEndNotification();
    break;
  default:
    Ea_Panic();
    break;
  }
}

static void Ea_DoWrite_OnEventStart(Ea_StepType step) {
  const Ea_ConfigType *config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r;
  switch (step) {
  case EA_WRITE_STEP1_ERASE:
    r = Eep_Erase(config->Blocks[context->job.BlockId].BlockAddress,
                  EA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize));
    if (E_OK == r) {
      context->state = EA_JOB_WRITE | EA_WAITING;
    } else {
      context->state = EA_JOB_WRITE | EA_PENDING;
    }
    break;
  case EA_WRITE_STEP2_WRITE:
    r = Eep_Write(config->Blocks[context->job.BlockId].BlockAddress, context->job.DataBufferPtr,
                  config->Blocks[context->job.BlockId].BlockSize);
    if (E_OK == r) {
      context->state = EA_JOB_WRITE | EA_WAITING;
    } else {
      context->state = EA_JOB_WRITE | EA_PENDING;
    }
    context->step = EA_WRITE_STEP2_WRITE;
    break;
  default:
    Ea_Panic();
    break;
  }
}

static void Ea_DoWrite_OnEventEnd(Ea_StepType step) {
  const Ea_ConfigType *config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  switch (step) {
  case EA_WRITE_STEP1_ERASE:
    Ea_DoWrite_OnEventStart(EA_WRITE_STEP2_WRITE);
    break;
  case EA_WRITE_STEP2_WRITE:
    context->state = EA_IDEL;
    context->retryCounter = 0;
    config->JobEndNotification();
    break;
  default:
    Ea_Panic();
    break;
  }
}

EA_DOJOB_TEMPLATE(Read, 0)
EA_DOJOB_TEMPLATE(Write, 0)
/* ================================ [ FUNCTIONS ] ============================================== */
void Ea_Init(const Ea_ConfigType *ConfigPtr) {
  Ea_ContextType *context = &Ea_Context;
  (void)ConfigPtr;

  context->state = EA_IDEL;
  context->step = EA_STEP_IDEL;
  context->retryCounter = 0;
}

void Ea_SetMode(MemIf_ModeType Mode) {
  Eep_SetMode(Mode);
}

Std_ReturnType Ea_Read(uint16_t BlockNumber, uint16_t BlockOffset, uint8_t *DataBufferPtr,
                       uint16_t Length) {
  const Ea_ConfigType *config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r = E_NOT_OK;

  if (BlockNumber > 0 && ((BlockNumber - 1) < config->numOfBlocks)) {
    if (config->Blocks[BlockNumber - 1].BlockSize >= ((uint32_t)BlockOffset + Length)) {
      r = E_OK;
    }
  }

  if (E_OK == r) {
    EnterCritical();
    if (EA_IDEL == (EA_STATE_MASK & context->state)) {
      context->state = EA_JOB_READ | EA_PENDING;
      context->step = EA_STEP_IDEL;
      context->retryCounter = 0;
      r = E_OK;
    } else {
      r = E_NOT_OK;
    }
    ExitCritical();
  }

  if (E_OK == r) {
    context->job.BlockId = BlockNumber - 1;
    context->job.BlockOffset = BlockOffset;
    context->job.DataBufferPtr = DataBufferPtr;
    context->job.Length = Length;
  }

  return r;
}

Std_ReturnType Ea_Write(uint16_t BlockNumber, const uint8_t *DataBufferPtr) {
  const Ea_ConfigType *config = EA_CONFIG;
  Ea_ContextType *context = &Ea_Context;
  Std_ReturnType r = E_NOT_OK;

  if (BlockNumber > 0 && ((BlockNumber - 1) < config->numOfBlocks)) {
    r = E_OK;
  }

  if (E_OK == r) {
    EnterCritical();
    if (EA_IDEL == (EA_STATE_MASK & context->state)) {
      context->state = EA_JOB_WRITE | EA_PENDING;
      context->step = EA_STEP_IDEL;
      context->retryCounter = 0;
      r = E_OK;
    } else {
      r = E_NOT_OK;
    }
    ExitCritical();
  }

  if (E_OK == r) {
    context->job.BlockId = BlockNumber - 1;
    context->job.DataBufferPtr = (uint8_t *)DataBufferPtr;
  }

  return r;
}

void Ea_Cancel(void) {
  return Eep_Cancel();
}

MemIf_StatusType Ea_GetStatus(void) {
  return Eep_GetStatus();
}

MemIf_JobResultType Ea_GetJobResult(void) {
  return Eep_GetJobResult();
}

Std_ReturnType Ea_InvalidateBlock(uint16_t BlockNumber) {
  return E_NOT_OK;
}

Std_ReturnType Ea_EraseImmediateBlock(uint16_t BlockNumber) {
  return E_NOT_OK;
}

void Ea_JobEndNotification(void) {
  Ea_ContextType *context = &Ea_Context;
  Ea_StateType jobType = EA_JOB_NONE;
  Ea_StepType step;

  ASLOG(EA, ("Job End in state %X, step %d\n", context->state, context->step));

  EnterCritical();
  if (EA_WAITING == (EA_STATE_MASK & context->state)) {
    jobType = context->state & EA_JOB_MASK;
    step = context->step;
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

  ASLOG(EA, ("Job Error in state %X, step %d\n", context->state, context->step));

  EnterCritical();
  if (EA_WAITING == (EA_STATE_MASK & context->state)) {
    jobType = context->state & EA_JOB_MASK;
    step = context->step;
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
  if (EA_PENDING == (EA_STATE_MASK & context->state)) {
    jobType = context->state & EA_JOB_MASK;
    step = context->step;
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
