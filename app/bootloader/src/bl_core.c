/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(__linux__)
#define FL_ERASE_PER_CYCLE 32
#else
#ifndef FL_ERASE_PER_CYCLE
#define FL_ERASE_PER_CYCLE 1
#endif
#endif
#ifndef FL_WRITE_PER_CYCLE
#define FL_WRITE_PER_CYCLE (4096 / FLASH_WRITE_SIZE)
#endif
#ifndef FL_READ_PER_CYCLE
#define FL_READ_PER_CYCLE (4096 / FLASH_READ_SIZE)
#endif

#if defined(_WIN32) || defined(__linux__)
#define BL_SIM_ADDRESS_RE_MAPPING()                                                                \
  do {                                                                                             \
    /* address mapping to simulate memory */                                                       \
    if ((uint32_t)-1 == blBaseAddress) {                                                           \
      blBaseAddress = MemoryAddress;                                                               \
    }                                                                                              \
    MemoryAddress = MemoryAddress - blBaseAddress;                                                 \
    if (TRUE == bl_flashDriverReady) {                                                             \
      MemoryAddress += blAppMemoryLow;                                                             \
    } else {                                                                                       \
      MemoryAddress += blFlsDriverMemoryLow;                                                       \
    }                                                                                              \
  } while (0)

#define BL_SIM_ADDRESS_INIT()                                                                      \
  do {                                                                                             \
    blBaseAddress = (uint32_t)-1;                                                                  \
  } while (0)

#else
#define BL_SIM_ADDRESS_RE_MAPPING()
#define BL_SIM_ADDRESS_INIT()
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern uint8_t FlashDriverRam[];
extern const BL_MemoryInfoType blMemoryList[];
extern const uint32_t blMemoryListSize;
extern const uint32_t blFlsDriverMemoryLow;
extern const uint32_t blFlsDriverMemoryHigh;
extern const uint32_t blAppMemoryLow;
extern const uint32_t blAppMemoryHigh;
/* ================================ [ DATAS     ] ============================================== */
static tFlashParam blFlashParam = {
  /* .patchlevel  = */ FLASH_DRIVER_VERSION_PATCH,
  /* .minornumber = */ FLASH_DRIVER_VERSION_MINOR,
  /* .majornumber = */ FLASH_DRIVER_VERSION_MAJOR,
  /* .reserved1   = */ 0,
  /* .errorcode   = */ kFlashOk,
  /* .reserved2   = */ 0,
  /* .address     = */ 0,
  /* .length      = */ 0,
  /* .data        = */ NULL,
  /* .wdTriggerFct =*/(tWDTriggerFct)NULL,
};

static uint8_t blMemoryIdentifier;
static uint32_t blOffset;

#if defined(_WIN32) || defined(__linux__)
static uint32_t blBaseAddress;
#endif

static boolean bl_flashDriverReady = FALSE;
static boolean bl_flashErased = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
static Dcm_ReturnEraseMemoryType eraseFlash(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                            uint32_t MemorySize) {
  Dcm_ReturnEraseMemoryType rv;

  uint32_t length;
  switch (OpStatus) {
  case DCM_INITIAL:
    FLASH_DRIVER_INIT(FLASH_DRIVER_STARTADDRESS, &blFlashParam);
    if (kFlashOk == blFlashParam.errorcode) {
      blOffset = 0;
      rv = DCM_ERASE_PENDING;
    } else {
      rv = DCM_ERASE_FAILED;
      break;
    }
    /* no break here intentionally */
  case DCM_PENDING:
    ASLOG(BL, ("eraseFlast at %X\n", (uint32_t)blOffset));
    length = MemorySize - blOffset;
    if (length > (FL_ERASE_PER_CYCLE * FLASH_ERASE_SIZE)) {
      length = FL_ERASE_PER_CYCLE * FLASH_ERASE_SIZE;
    }

    blFlashParam.address = MemoryAddress + blOffset;
    blFlashParam.length = length;
    blFlashParam.data = NULL;
    FLASH_DRIVER_ERASE(FLASH_DRIVER_STARTADDRESS, &blFlashParam);
    blOffset += length;
    if (kFlashOk == blFlashParam.errorcode) {
      if (blOffset >= MemorySize) {
        rv = DCM_ERASE_OK;
      } else {
        rv = DCM_ERASE_PENDING;
      }
    } else {
      ASLOG(BL,
            ("erase failed: errorcode = %X(addr=%X,size=%X)\n", (uint32_t)blFlashParam.errorcode,
             (uint32_t)blFlashParam.address, (uint32_t)blFlashParam.length));
      rv = DCM_ERASE_FAILED;
    }
    break;
  default:
    rv = DCM_ERASE_FAILED;
    break;
  }

  return rv;
}

static Dcm_ReturnWriteMemoryType writeFlash(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                            uint32_t MemorySize, uint8_t *MemoryData) {
  Dcm_ReturnWriteMemoryType rv;
  uint32_t length;

  switch (OpStatus) {
  case DCM_INITIAL:
    blOffset = 0;
  case DCM_PENDING:
    ASLOG(BL, ("writeFlash at %X\n", (uint32_t)MemoryAddress + blOffset));
    length = MemorySize - blOffset;
    if (length > (FL_WRITE_PER_CYCLE * FLASH_WRITE_SIZE)) {
      length = FL_WRITE_PER_CYCLE * FLASH_WRITE_SIZE;
    }
    blFlashParam.address = MemoryAddress + blOffset;
    blFlashParam.length = length;
    blFlashParam.data = (tData *)&MemoryData[blOffset];
    FLASH_DRIVER_WRITE(FLASH_DRIVER_STARTADDRESS, &blFlashParam);
    blOffset += length;
    if (kFlashOk == blFlashParam.errorcode) {
      if (blOffset >= MemorySize) {
        rv = DCM_WRITE_OK;
      } else {
        rv = DCM_WRITE_PENDING;
      }
    } else {
      ASLOG(BL,
            ("write failed: errorcode = %X(addr=%X,size=%X)\n", (uint32_t)blFlashParam.errorcode,
             (uint32_t)blFlashParam.address, (uint32_t)blFlashParam.length));
      rv = DCM_WRITE_FAILED;
    }
    break;
  default:
    rv = DCM_WRITE_FAILED;
    break;
  }

  return rv;
}

static Dcm_ReturnReadMemoryType readFlash(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                          uint32_t MemorySize, uint8_t *MemoryData) {
  Dcm_ReturnReadMemoryType rv;
  uint32_t length;
  switch (OpStatus) {
  case DCM_INITIAL:
    blOffset = 0;
  case DCM_PENDING:
    ASLOG(BL, ("readFlash at %X\n", (uint32_t)MemoryAddress + blOffset));
    length = MemorySize - blOffset;
    if (length > (FL_READ_PER_CYCLE * FLASH_READ_SIZE)) {
      length = FL_READ_PER_CYCLE * FLASH_READ_SIZE;
    }
    blFlashParam.address = MemoryAddress + blOffset;
    blFlashParam.length = length;
    blFlashParam.data = (tData *)&MemoryData[blOffset];
    FLASH_DRIVER_READ(FLASH_DRIVER_STARTADDRESS, &blFlashParam);
    blOffset += length;
    if (kFlashOk == blFlashParam.errorcode) {
      if (blOffset >= MemorySize) {
        rv = DCM_READ_OK;
      } else {
        rv = DCM_READ_PENDING;
      }
    } else {
      ASLOG(BL, ("read failed: errorcode = %X(addr=%X,size=%X)\n", (uint32_t)blFlashParam.errorcode,
                 (uint32_t)blFlashParam.address, (uint32_t)blFlashParam.length));
      rv = DCM_READ_FAILED;
    }
    break;
  default:
    rv = DCM_READ_FAILED;
    break;
  }

  return rv;
}

static Dcm_ReturnWriteMemoryType writeFlashDriver(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                                  uint32_t MemorySize, uint8_t *MemoryData) {
  uint16_t flsCrc;
  uint16_t calcCrc;
  uint32_t flsSz = blFlsDriverMemoryHigh - blFlsDriverMemoryLow;
  uint32_t offset = MemoryAddress - blFlsDriverMemoryLow;

  memcpy((void *)&(FlashDriverRam[offset]), (void *)MemoryData, MemorySize);
  if (blFlsDriverMemoryHigh == (MemoryAddress + MemorySize)) {
    flsCrc = ((uint16_t)FlashDriverRam[flsSz - 2] << 8) + FlashDriverRam[flsSz - 1];
    calcCrc = Crc_CalculateCRC16(FlashDriverRam, flsSz - 2, 0xFFFF, TRUE);

    if (flsCrc == calcCrc) {
      bl_flashDriverReady = TRUE;
    } else {
      ASLOG(BLE, ("Flash Driver invalid, C %X != R %X\n", calcCrc, flsCrc));
    }
  }
  return DCM_WRITE_OK;
}

static Dcm_ReturnReadMemoryType readFlashDriver(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                                uint32_t MemorySize, uint8_t *MemoryData) {
  uint32_t offset = MemoryAddress - blFlsDriverMemoryLow;
  memcpy(MemoryData, &FlashDriverRam[offset], MemorySize);
  return DCM_READ_OK;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_SessionReset(void) {
  blMemoryIdentifier = 0;
  blOffset = 0;

  bl_flashDriverReady = FALSE;
  bl_flashErased = FALSE;

  memset(FlashDriverRam, 0xFF, blFlsDriverMemoryHigh - blFlsDriverMemoryLow);

  BL_SIM_ADDRESS_INIT();
}

uint8_t BL_GetMemoryIdentifier(uint32_t MemoryAddress, uint32_t MemorySize) {
  uint8_t MemoryIdentifier = 0;
  int i;

  for (i = 0; i < blMemoryListSize; i++) {
    if ((MemoryAddress >= blMemoryList[i].addressLow) &&
        ((MemoryAddress + MemorySize) <= blMemoryList[i].addressHigh)) {
      MemoryIdentifier = blMemoryList[i].identifier;
      break;
    }
  }

  return MemoryIdentifier;
}

Std_ReturnType BL_StartEraseFlash(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                  uint8_t *dataOut, uint16_t *currentDataLength,
                                  Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnEraseMemoryType eraseRet;

  BL_SIM_ADDRESS_INIT();

  if (0 == *currentDataLength) {
    if (FALSE == bl_flashDriverReady) {
      *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    } else {
      eraseRet = eraseFlash(OpStatus, blAppMemoryLow, blAppMemoryHigh - blAppMemoryLow);
      if (DCM_ERASE_OK == eraseRet) {
        r = E_OK;
      } else if (DCM_ERASE_PENDING == eraseRet) {
        *ErrorCode = DCM_E_RESPONSE_PENDING;
      } else {
        *ErrorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
      }
    }
  } else {
    *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}
#if defined(_WIN32) || defined(__linux__)
Std_ReturnType BL_CheckAppIntegrity(void) {
  Std_ReturnType r = E_NOT_OK;
  uint32_t memoryAddress;
  uint32_t memorySize;
  uint32_t offset = 0;
  uint16_t appCrc = 0xFFFF;
  uint16_t calcCrc = 0xFFFF; /* initial value */
  static uint8_t dataIn[256];
  memoryAddress = blAppMemoryLow;

  FLASH_DRIVER_INIT(FLASH_DRIVER_STARTADDRESS, &blFlashParam);
  while (offset < (blAppMemoryHigh - blAppMemoryLow - 2)) {
    memorySize = sizeof(dataIn);
    if (memorySize > (blAppMemoryHigh - blAppMemoryLow - 2 - offset)) {
      memorySize = (blAppMemoryHigh - blAppMemoryLow - 2 - offset);
    }

    (void)readFlash(DCM_INITIAL, memoryAddress + offset, memorySize, (uint8_t *)dataIn);

    offset += memorySize;
    calcCrc = Crc_CalculateCRC16(dataIn, memorySize, calcCrc, FALSE);
  }

  (void)readFlash(DCM_INITIAL, blAppMemoryHigh - 2, 2, (uint8_t *)dataIn);
  appCrc = ((uint16_t)dataIn[0] << 8) + dataIn[1];

  if (calcCrc == appCrc) {
    r = E_OK;
  }

  return r;
}
#else
Std_ReturnType BL_CheckAppIntegrity(void) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t *memory;
  uint32_t size;
  uint16_t appCrc = 0xFFFF;
  uint16_t calcCrc = 0xFFFF; /* initial value */
  memory = (uint8_t *)blAppMemoryLow;
  size = blAppMemoryHigh - blAppMemoryLow - 2;

  appCrc = ((uint16_t)memory[size] << 8) + memory[size + 1];
  calcCrc = Crc_CalculateCRC16(memory, size, calcCrc, FALSE);

  if (calcCrc == appCrc) {
    r = E_OK;
  }

  return r;
}
#endif

Std_ReturnType BL_CheckIntegrity(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                 uint16_t *currentDataLength,
                                 Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnReadMemoryType readRet;
  static uint32_t offset = 0;
  uint16_t appCrc = 0xFFFF;
  static uint16_t calcCrc = 0xFFFF; /* initial value */
  uint32_t memoryAddress = blAppMemoryLow;
  uint32_t memorySize = 256;
  /* NOTE: this size must be <= DCM_DEFAULT_RXBUF_SIZE-4.
   * and <= FL_READ_PER_CYCLE * FLASH_READ_SIZE */

  if (0 == *currentDataLength) {
    r = E_OK;
    if (DCM_INITIAL == OpStatus) {
      offset = 0;
      calcCrc = 0xFFFF;
    }

    if (memorySize > (blAppMemoryHigh - blAppMemoryLow - 2 - offset)) {
      memorySize = (blAppMemoryHigh - blAppMemoryLow - 2 - offset);
    }

    readRet = readFlash(DCM_INITIAL, memoryAddress + offset, memorySize, (uint8_t *)dataIn);
    if (DCM_READ_OK == readRet) {
      offset += memorySize;
      calcCrc = Crc_CalculateCRC16(dataIn, memorySize, calcCrc, FALSE);

      if (offset >= (blAppMemoryHigh - blAppMemoryLow - 2)) {
        readRet = readFlash(DCM_INITIAL, blAppMemoryHigh - 2, 2, (uint8_t *)dataIn);
        if (DCM_READ_OK == readRet) {
          appCrc = ((uint16_t)dataIn[0] << 8) + dataIn[1];
          if (appCrc != calcCrc) {
            ASLOG(BLE, ("check integrity failed as %X != %X\n", calcCrc, appCrc));
            *ErrorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
            r = E_NOT_OK;
          } else {
            /* pass check */
          }
        } else {
          *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
          r = E_NOT_OK;
        }
      } else {
        *ErrorCode = DCM_E_RESPONSE_PENDING;
      }
    } else {
      *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
      r = E_NOT_OK;
    }
  } else {
    *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType BL_ProcessRequestDownload(Dcm_OpStatusType OpStatus, uint8_t DataFormatIdentifier,
                                         uint8_t MemoryIdentifier, uint32_t MemoryAddress,
                                         uint32_t MemorySize, uint32_t *BlockLength,
                                         Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  (void)OpStatus;
  (void)BlockLength;

  BL_SIM_ADDRESS_RE_MAPPING();

  if (0x00 == DataFormatIdentifier) {
    r = E_OK;
  } else {
    ASLOG(BLE, ("invalid data format 0x%x\n", DataFormatIdentifier));
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
  }

  if (E_OK == r) {
    MemoryIdentifier = BL_GetMemoryIdentifier(MemoryAddress, MemorySize);
    if (0 == MemoryIdentifier) {
      r = E_NOT_OK;
      ASLOG(BLE, ("invalid address(0x%x) or size(%u)\n", MemoryAddress, MemorySize));
      *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  }

  if (E_OK == r) {
    if (FLASH_IS_WRITE_ADDRESS_ALIGNED(MemoryAddress) &&
        FLASH_IS_WRITE_ADDRESS_ALIGNED(MemorySize)) {
      blMemoryIdentifier = MemoryIdentifier;
      blOffset = 0;
    } else {
      r = E_NOT_OK;
      ASLOG(BLE, ("address(0x%x) or size(%u) not aligned\n", MemoryAddress, MemorySize));
      *ErrorCode = DCM_E_UPLOAD_DOWNLOAD_NOT_ACCEPTED;
    }
  }

  return r;
}

Dcm_ReturnWriteMemoryType BL_ProcessTransferDataWrite(Dcm_OpStatusType OpStatus,
                                                      uint8_t MemoryIdentifier,
                                                      uint32_t MemoryAddress, uint32_t MemorySize,
                                                      const Dcm_RequestDataArrayType MemoryData,
                                                      Dcm_NegativeResponseCodeType *ErrorCode) {
  Dcm_ReturnWriteMemoryType ret = DCM_WRITE_OK;
  (void)MemoryIdentifier;

  BL_SIM_ADDRESS_RE_MAPPING();

  switch (blMemoryIdentifier) {
  case BL_FLASH_IDENTIFIER:
    if (FALSE == bl_flashDriverReady) {
      *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
      ret = DCM_WRITE_FAILED;
    } else {
      ret = writeFlash(OpStatus, MemoryAddress, MemorySize, MemoryData);
    }
    break;
  case BL_FLSDRV_IDENTIFIER:
    ret = writeFlashDriver(OpStatus, MemoryAddress, MemorySize, MemoryData);
    break;
  default:
    *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    ret = DCM_WRITE_FAILED;
    break;
  }
  return ret;
}

Dcm_ReturnReadMemoryType BL_ProcessTransferDataRead(Dcm_OpStatusType OpStatus,
                                                    uint8_t MemoryIdentifier,
                                                    uint32_t MemoryAddress, uint32_t MemorySize,
                                                    Dcm_RequestDataArrayType MemoryData,
                                                    Dcm_NegativeResponseCodeType *ErrorCode) {
  Dcm_ReturnReadMemoryType ret = DCM_READ_OK;
  (void)MemoryIdentifier;

  BL_SIM_ADDRESS_RE_MAPPING();

  switch (blMemoryIdentifier) {
  case BL_FLASH_IDENTIFIER:
    if (FALSE == bl_flashDriverReady) {
      *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
      ret = DCM_READ_FAILED;
    } else {
      ret = readFlash(OpStatus, MemoryAddress, MemorySize, MemoryData);
    }
    break;
  case BL_FLSDRV_IDENTIFIER:
    ret = readFlashDriver(OpStatus, MemoryAddress, MemorySize, MemoryData);
    break;
  default:
    *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    ret = DCM_READ_FAILED;
    break;
  }
  return ret;
}

Std_ReturnType BL_ProcessRequestTransferExit(Dcm_OpStatusType OpStatus,
                                             Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_OK;

  blMemoryIdentifier = 0;
  blOffset = 0;

  return r;
}

void BL_CheckAndJump(void) {
  if (FALSE == BL_IsUpdateRequested()) {
    if (E_OK == BL_CheckAppIntegrity()) {
      ASLOG(INFO, ("application is valid\n"));
      BL_JumpToApp();
    }
  }
}

void BL_Init(void) {
  Std_ReturnType r;
  Dcm_ProgConditionsType *cond = NULL;
  static const uint8_t response[] = {0x50, 0x02, 0x13, 0x88, 0x00, 0x32};

  r = BL_GetProgramCondition(&cond);
  if ((E_OK == r) && (cond != NULL)) {
    Dcm_SetSesCtrlType(DCM_PROGRAMMING_SESSION);
    if (cond->ResponseRequired) {
      Dcm_Transmit(response, sizeof(response), 0);
    }
  }
  BL_SessionReset();
}