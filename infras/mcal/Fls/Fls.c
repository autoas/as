/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash Driver AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Fls.h"
#include "Fls_Priv.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_FLS 0
#define AS_LOG_FLSI 1
#define AS_LOG_FLSE 2

#define FLS_CONFIG (&Fls_Config)

/* the low 4 bit used for state, the high 4 bit used for job type */
#define FLS_STATE_MASK ((Fls_StateType)0x0F)
#define FLS_JOB_MASK ((Fls_StateType)0xF0)

#define FLS_IDEL ((Fls_StateType)0x00)
#define FLS_PENDING ((Fls_StateType)0x01)
#define FLS_CANCELED ((Fls_StateType)0x02)
#define FLS_DONE ((Fls_StateType)0x03)
#define FLS_FAIL ((Fls_StateType)0x04)
#define FLS_INCONSISTENT ((Fls_StateType)0x05)

#define FLS_JOB_NONE ((Fls_StateType)0x00)
#define FLS_JOB_ERASE ((Fls_StateType)0x10)
#define FLS_JOB_WRITE ((Fls_StateType)0x20)
#define FLS_JOB_READ ((Fls_StateType)0x30)
#define FLS_JOB_COMPARE ((Fls_StateType)0x40)
#define FLS_JOB_BLANK_CHECK ((Fls_StateType)0x50)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t Fls_StateType;

typedef enum
{
  FLS_ERASE_CHECK,
  FLS_WRITE_CHECK,
  FLS_READ_CHECK
} Fls_CheckType;

typedef struct {
  uint8_t *data;
  Fls_AddressType address;
  Fls_LengthType length;
  Fls_LengthType offset;
  Fls_StateType state;
  MemIf_ModeType mode;
} Fls_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern const Fls_ConfigType Fls_Config;
/* ================================ [ DATAS     ] ============================================== */
static Fls_ContextType Fls_Context;
/* ================================ [ LOCALS    ] ============================================== */
static const Fls_SectorType *Fls_GetSectorByAddress(Fls_AddressType TargetAddress) {
  int i = 0;
  const Fls_SectorType *sector = NULL;
  const Fls_ConfigType *config = FLS_CONFIG;

  for (i = 0; i < config->numOfSectors; i++) {
    if ((TargetAddress >= config->SectorList[i].SectorStartAddress) &&
        (TargetAddress <= config->SectorList[i].SectorEndAddress)) {
      sector = &config->SectorList[i];
    }
  }

  return sector;
}

static Std_ReturnType Fls_IsOffsetFineInSector(const Fls_SectorType *sector, Fls_LengthType offset,
                                               Fls_CheckType check) {
  Fls_LengthType aligned;
  Std_ReturnType r = E_OK;
  switch (check) {
  case FLS_ERASE_CHECK:
    aligned = sector->SectorSize;
    break;
  case FLS_WRITE_CHECK:
    aligned = sector->PageSize;
    break;
  default:
    /* TODO: get the read minimum size according to configuration,
     * but for most the case, it was one */
    aligned = 1;
    break;
  }
  if (0 != (offset & (aligned - 1))) {
    r = E_NOT_OK;
  }
  return r;
}

static Std_ReturnType Fls_IsAddressLengthFine(Fls_AddressType TargetAddress, Fls_LengthType Length,
                                              Fls_CheckType check) {
  Std_ReturnType r = E_NOT_OK;
  const Fls_SectorType *start = Fls_GetSectorByAddress(TargetAddress);
  const Fls_SectorType *end = NULL;

  if (NULL != start) {
    end = Fls_GetSectorByAddress(TargetAddress + Length);
  }

  if (NULL != end) {
    r = E_OK;
  }

  if (E_OK == r) {
    r = Fls_IsOffsetFineInSector(start, TargetAddress - start->SectorStartAddress, check);
  }

  if (E_OK == r) {
    r = Fls_IsOffsetFineInSector(end, TargetAddress + Length - end->SectorStartAddress, check);
  }

  return r;
}

static Fls_LengthType Fls_GetMaxWorkingSize(Fls_StateType jobType) {
  const Fls_ConfigType *config = FLS_CONFIG;
  Fls_ContextType *context = &Fls_Context;
  Fls_LengthType maxSize;

  switch (jobType) {
  case FLS_JOB_ERASE:
    if (MEMIF_MODE_SLOW == context->mode) {
      maxSize = config->MaxEraseNormalMode;
    } else {
      maxSize = config->MaxEraseFastMode;
    }
    break;
  case FLS_JOB_WRITE:
    if (MEMIF_MODE_SLOW == context->mode) {
      maxSize = config->MaxWriteNormalMode;
    } else {
      maxSize = config->MaxWriteFastMode;
    }
    break;
  default:
    if (MEMIF_MODE_SLOW == context->mode) {
      maxSize = config->MaxReadNormalMode;
    } else {
      maxSize = config->MaxReadFastMode;
    }
    break;
  }

  return maxSize;
}

static void Fls_DoJobPostUpdate(Fls_AddressType address, uint8_t *data, Fls_LengthType length,
                                Fls_LengthType offset, Fls_StateType jobType, Std_ReturnType r) {
  Fls_ContextType *context = &Fls_Context;
  const Fls_ConfigType *config = FLS_CONFIG;

  if (FLS_PENDING == (FLS_STATE_MASK & context->state)) {
    if (jobType == (FLS_JOB_MASK & context->state)) {
      if ((context->address == address) && (context->data == data) && (context->length == length)) {
        if ((E_OK == r) || (E_FLS_PENDING == r)) {
          context->offset = offset;
          if (offset >= length) {
            /* @SWS_Fls_00262 */
            context->state = jobType | FLS_DONE;
            config->JobEndNotification();
          }
        } else if (E_FLS_INCONSISTENT == r) {
          /* @SWS_Fls_00200, @SWS_Fls_00263 */
          context->state = jobType | FLS_INCONSISTENT;
          config->JobErrorNotification();
        } else {
          context->state = jobType | FLS_FAIL;
          config->JobErrorNotification();
        }
      } else { /* maybe canceled and a new erase job requested */
        ASLOG(FLSI, ("Job %d changed from %p 0x%X@0x%X to %p 0x%X@0x%X\n", jobType, data, length,
                     address, context->data, context->length, context->address));
      }
    } else { /* maybe canceled */
      ASLOG(FLSI, ("Job type changed to 0x%02X during job %d\n", context->state, jobType));
    }
  } else { /* maybe canceled */
    ASLOG(FLSI, ("Job state changed to 0x%02X during job %d\n", context->state, jobType));
  }
}

static void Fls_DoJob(Fls_AddressType address, uint8_t *data, Fls_LengthType length,
                      Fls_LengthType offset, Fls_StateType jobType) {
  Std_ReturnType r = E_OK;
  Fls_LengthType maxSize;
  Fls_LengthType doSize;
  const Fls_ConfigType *config = FLS_CONFIG;
  Fls_ContextType *context = &Fls_Context;
  const Fls_SectorType *sector;

  maxSize = Fls_GetMaxWorkingSize(jobType);

  while ((offset < length) && (maxSize > 0) && (E_OK == r)) {
    sector = Fls_GetSectorByAddress(address + offset);
    if (NULL == sector) {
      /* Terminate as FATAL memory error */
      ASLOG(FLSE, ("Invalid Address 0x%X when do job %d\n", address + offset, jobType));
      config->JobErrorNotification();
      context->state = FLS_IDEL;
      r = E_NOT_OK;
    }

    doSize = length - offset;
    if (doSize > maxSize) {
      doSize = maxSize;
    }

    if (E_OK == r) {
      switch (jobType) {
      case FLS_JOB_ERASE:
        doSize = sector->SectorSize;
        r = Fls_AcErase(address + offset, doSize);
        break;
      case FLS_JOB_WRITE:
        doSize = sector->PageSize;
        r = Fls_AcWrite(address + offset, &data[offset], doSize);
        break;
      case FLS_JOB_READ:
        r = Fls_AcRead(address + offset, &data[offset], doSize);
        break;
      case FLS_JOB_COMPARE:
        r = Fls_AcCompare(address + offset, &data[offset], doSize);
        break;
      case FLS_JOB_BLANK_CHECK:
        r = Fls_AcBlankCheck(address + offset, doSize);
        break;
      default:
        break;
      }
    }

    if (E_OK == r) {
      offset += doSize;
      if (maxSize > doSize) {
        maxSize -= doSize;
      } else {
        maxSize = 0;
      }
    }
  }

  Fls_DoJobPostUpdate(address, data, length, offset, jobType, r);
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Fls_Init(const Fls_ConfigType *ConfigPtr) {
  (void)ConfigPtr;
  const Fls_ConfigType *config = FLS_CONFIG;
  Fls_ContextType *context = &Fls_Context;
  Fls_AcInit();
  context->state = FLS_IDEL;
  context->mode = config->defaultMode;
}

Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length) {
  Fls_ContextType *context = &Fls_Context;
  Std_ReturnType r;

  r = Fls_IsAddressLengthFine(TargetAddress, Length, FLS_ERASE_CHECK);
  if (E_OK == r) {
    EnterCritical();
    if (FLS_PENDING != (FLS_STATE_MASK & context->state)) {
      context->state = FLS_PENDING | FLS_JOB_ERASE;
    } else {
      r = E_NOT_OK;
    }
    ExitCritical();
  } else {
    ASLOG(FLSE, ("Address 0x%X Length=0x%X not okay for erase\n", TargetAddress, Length));
  }

  if (E_OK == r) {
    context->data = NULL;
    context->address = TargetAddress;
    context->length = Length;
    context->offset = 0;
  }

  return r;
}

Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8_t *SourceAddressPtr,
                         Fls_LengthType Length) {
  Fls_ContextType *context = &Fls_Context;
  Std_ReturnType r;

  r = Fls_IsAddressLengthFine(TargetAddress, Length, FLS_WRITE_CHECK);
  if (E_OK == r) {
    EnterCritical();
    if (FLS_PENDING != (FLS_STATE_MASK & context->state)) {
      context->state = FLS_PENDING | FLS_JOB_WRITE;
    } else {
      r = E_NOT_OK;
    }
    ExitCritical();
  } else {
    ASLOG(FLSE, ("Address 0x%X Length=0x%X not okay for write\n", TargetAddress, Length));
  }

  if (E_OK == r) {
    context->data = (uint8_t *)SourceAddressPtr;
    context->address = TargetAddress;
    context->length = Length;
    context->offset = 0;
  }

  return r;
}

void Fls_Cancel(void) {
  const Fls_ConfigType *config = FLS_CONFIG;
  Fls_ContextType *context = &Fls_Context;
  Fls_StateType state;

  EnterCritical();
  state = context->state;
  if (FLS_PENDING == (FLS_STATE_MASK & context->state)) {
    /* @SWS_Fls_00033 */
    context->state = FLS_CANCELED;
  } else {
    context->state = FLS_IDEL;
  }
  ExitCritical();

  if (FLS_PENDING == (FLS_STATE_MASK & state)) {
    config->JobErrorNotification();
  }
}

MemIf_StatusType Fls_GetStatus(void) {
  MemIf_StatusType status = MEMIF_IDLE;
  Fls_ContextType *context = &Fls_Context;

  if (FLS_PENDING == (FLS_STATE_MASK & context->state)) {
    status = MEMIF_BUSY;
  }

  return status;
}

MemIf_JobResultType Fls_GetJobResult(void) {
  MemIf_JobResultType result = MEMIF_JOB_FAILED;
  Fls_ContextType *context = &Fls_Context;

  switch (FLS_STATE_MASK & context->state) {
  case FLS_PENDING:
    result = MEMIF_JOB_PENDING;
    break;
  case FLS_CANCELED:
    result = MEMIF_JOB_CANCELED;
    break;
  case FLS_DONE:
    result = MEMIF_JOB_OK;
    break;
  case FLS_FAIL:
    result = MEMIF_JOB_FAILED;
    break;
  case FLS_INCONSISTENT:
    result = MEMIF_BLOCK_INCONSISTENT;
    break;
  default:
    break;
  }

  return result;
}

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8_t *TargetAddressPtr,
                        Fls_LengthType Length) {
  Fls_ContextType *context = &Fls_Context;
  Std_ReturnType r;

  r = Fls_IsAddressLengthFine(SourceAddress, Length, FLS_READ_CHECK);
  if (E_OK == r) {
    EnterCritical();
    if (FLS_PENDING != (FLS_STATE_MASK & context->state)) {
      context->state = FLS_PENDING | FLS_JOB_READ;
    } else {
      r = E_NOT_OK;
    }
    ExitCritical();
  } else {
    ASLOG(FLSE, ("Address 0x%X Length=0x%X not okay for read\n", SourceAddress, Length));
  }

  if (E_OK == r) {
    context->data = TargetAddressPtr;
    context->address = SourceAddress;
    context->length = Length;
    context->offset = 0;
  }

  return r;
}
Std_ReturnType Fls_Compare(Fls_AddressType SourceAddress, const uint8_t *TargetAddressPtr,
                           Fls_LengthType Length) {
  Fls_ContextType *context = &Fls_Context;
  Std_ReturnType r;

  r = Fls_IsAddressLengthFine(SourceAddress, Length, FLS_READ_CHECK);
  if (E_OK == r) {
    EnterCritical();
    if (FLS_PENDING != (FLS_STATE_MASK & context->state)) {
      context->state = FLS_PENDING | FLS_JOB_COMPARE;
    } else {
      r = E_NOT_OK;
    }
    ExitCritical();
  } else {
    ASLOG(FLSE, ("Address 0x%X Length=0x%X not okay for compare\n", SourceAddress, Length));
  }

  if (E_OK == r) {
    context->data = (uint8_t *)TargetAddressPtr;
    context->address = SourceAddress;
    context->length = Length;
    context->offset = 0;
  }

  return r;
}

void Fls_SetMode(MemIf_ModeType Mode) {
  Fls_ContextType *context = &Fls_Context;

  context->mode = Mode;
}

Std_ReturnType Fls_BlankCheck(Fls_AddressType TargetAddress, Fls_LengthType Length) {
  Fls_ContextType *context = &Fls_Context;
  Std_ReturnType r;

  r = Fls_IsAddressLengthFine(TargetAddress, Length, FLS_READ_CHECK);
  if (E_OK == r) {
    EnterCritical();
    if (FLS_PENDING != (FLS_STATE_MASK & context->state)) {
      context->state = FLS_PENDING | FLS_JOB_BLANK_CHECK;
    } else {
      r = E_NOT_OK;
    }
    ExitCritical();
  } else {
    ASLOG(FLSE, ("Address 0x%X Length=0x%X not okay for blank check\n", TargetAddress, Length));
  }

  if (E_OK == r) {
    context->data = NULL;
    context->address = TargetAddress;
    context->length = Length;
    context->offset = 0;
  }

  return r;
}

void Fls_MainFunction(void) {
  Fls_ContextType *context = &Fls_Context;
  uint8_t *data;
  Fls_AddressType address;
  Fls_LengthType length;
  Fls_LengthType offset;
  Fls_StateType jobType = FLS_JOB_NONE;

  EnterCritical();
  if (FLS_PENDING == (FLS_STATE_MASK & context->state)) {
    jobType = context->state & FLS_JOB_MASK;
    data = context->data;
    address = context->address;
    length = context->length;
    offset = context->offset;
  }
  ExitCritical();

  switch (jobType) {
  case FLS_JOB_ERASE:
  case FLS_JOB_WRITE:
  case FLS_JOB_READ:
  case FLS_JOB_COMPARE:
  case FLS_JOB_BLANK_CHECK:
    Fls_DoJob(address, data, length, offset, jobType);
    break;
  default:
    break;
  }
}
