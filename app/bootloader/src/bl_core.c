/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(__linux__)
#define FL_ERASE_PER_CYCLE 32
#ifndef FL_USE_WRITE_WINDOW_BUFFER
#define FL_USE_WRITE_WINDOW_BUFFER
#endif
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

#ifdef FL_USE_WRITE_WINDOW_BUFFER
#ifndef FL_WRITE_WINDOW_SIZE
#define FL_WRITE_WINDOW_SIZE (8 * FLASH_WRITE_SIZE)
#endif

#if (FL_WRITE_WINDOW_SIZE % FLASH_WRITE_SIZE) != 0
#error flash write window buffer size should be N times of FLASH_WRITE_SIZE
#endif
#endif /* FL_USE_WRITE_WINDOW_BUFFER */

#ifndef BL_APP_VALID_SAMPLE_NUM
#if defined(_WIN32) || defined(__linux__)
#define BL_APP_VALID_SAMPLE_NUM ((blFingerPrintAddr - blAppMemoryLow) / FLASH_ERASE_SIZE)
#else
#define BL_APP_VALID_SAMPLE_NUM 32
#endif
#endif

#ifndef BL_APP_VALID_SAMPLE_STRIDE
#define BL_APP_VALID_SAMPLE_STRIDE                                                                 \
  FLASH_ALIGNED_ERASE_SIZE((blFingerPrintAddr - blAppMemoryLow) / BL_APP_VALID_SAMPLE_NUM)
#endif

#ifndef BL_APP_VALID_SAMPLE_SIZE
#define BL_APP_VALID_SAMPLE_SIZE FLASH_ALIGNED_READ_SIZE(32)
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

#define BL_SIM_GET_FLASH_DRV_ADDRESS()                                                             \
  do {                                                                                             \
    MemoryAddress += blFlsDriverMemoryLow;                                                         \
  } while (0)

#define BL_SIM_ADDRESS_INIT()                                                                      \
  do {                                                                                             \
    blBaseAddress = (uint32_t)-1;                                                                  \
  } while (0)

#else
#define BL_SIM_ADDRESS_RE_MAPPING()
#define BL_SIM_GET_FLASH_DRV_ADDRESS()
#define BL_SIM_ADDRESS_INIT()
#endif

#ifndef BL_MAX_SEGMENT
#define BL_MAX_SEGMENT 8
#endif

#if defined(BL_USE_CRC_32) && defined(BL_USE_CRC_16)
#error only one CRC method can be used
#endif

#if !defined(BL_USE_CRC_32) && !defined(BL_USE_CRC_16)
#define BL_USE_CRC_16
#endif

#define BL_CRC_LENGTH sizeof(bl_crc_t)
#ifdef BL_USE_CRC_16
#define BL_CRC_START_VALUE 0xFFFF
#define BL_CalculateCRC(DataPtr, Length, StartValue, IsFirstCall)                                  \
  Crc_CalculateCRC16(DataPtr, Length, StartValue, IsFirstCall)
#define BL_GetCRC(ptr) (((bl_crc_t)(ptr)[0] << 8) + (ptr)[1])
#define BL_SetCRC(ptr, crc)                                                                        \
  do {                                                                                             \
    ptr[0] = (crc >> 8) & 0xFF;                                                                    \
    ptr[1] = crc & 0xFF;                                                                           \
  } while (0)
#endif

#ifdef BL_USE_CRC_32
#define BL_CRC_START_VALUE 0xFFFFFFFF
#define BL_CalculateCRC(DataPtr, Length, StartValue, IsFirstCall)                                  \
  Crc_CalculateCRC32(DataPtr, Length, StartValue, IsFirstCall)
#define BL_GetCRC(ptr)                                                                             \
  (((bl_crc_t)(ptr)[0] << 24) + ((bl_crc_t)(ptr)[1] << 16) + ((bl_crc_t)(ptr)[2] << 8) + (ptr)[3])
#define BL_SetCRC(ptr, crc)                                                                        \
  do {                                                                                             \
    ptr[0] = (crc >> 24) & 0xFF;                                                                   \
    ptr[1] = (crc >> 16) & 0xFF;                                                                   \
    ptr[2] = (crc >> 8) & 0xFF;                                                                    \
    ptr[3] = crc & 0xFF;                                                                           \
  } while (0)
#endif

#define BL_FLS_INIT()                                                                              \
  do {                                                                                             \
    EnterCritical();                                                                               \
    FLASH_DRIVER_INIT(FLASH_DRIVER_STARTADDRESS, &blFlashParam);                                   \
    ExitCritical();                                                                                \
  } while (0)

#define BL_FLS_ERASE(addr, len)                                                                    \
  do {                                                                                             \
    blFlashParam.address = addr;                                                                   \
    blFlashParam.length = len;                                                                     \
    blFlashParam.data = NULL;                                                                      \
    EnterCritical();                                                                               \
    FLASH_DRIVER_ERASE(FLASH_DRIVER_STARTADDRESS, &blFlashParam);                                  \
    ExitCritical();                                                                                \
  } while (0)

#define BL_FLS_WRITE(addr, pData, len)                                                             \
  do {                                                                                             \
    blFlashParam.address = addr;                                                                   \
    blFlashParam.length = len;                                                                     \
    blFlashParam.data = (tData *)pData;                                                            \
    EnterCritical();                                                                               \
    FLASH_DRIVER_WRITE(FLASH_DRIVER_STARTADDRESS, &blFlashParam);                                  \
    ExitCritical();                                                                                \
  } while (0)

#if defined(_WIN32) || defined(__linux__)
#define BL_FLS_READ(addr, pData, len)                                                              \
  do {                                                                                             \
    blFlashParam.address = addr;                                                                   \
    blFlashParam.length = len;                                                                     \
    blFlashParam.data = (tData *)pData;                                                            \
    EnterCritical();                                                                               \
    FLASH_DRIVER_READ(FLASH_DRIVER_STARTADDRESS, &blFlashParam);                                   \
    ExitCritical();                                                                                \
  } while (0)
#else
#define BL_FLS_READ(addr, pData, len)                                                              \
  do {                                                                                             \
    /* For most of the MCU Flash, can read it directly */                                          \
    memcpy(pData, (void *)addr, len);                                                              \
    blFlashParam.errorcode = kFlashOk;                                                             \
  } while (0)
#endif

#define BL_FLS_DEINIT()                                                                            \
  do {                                                                                             \
    EnterCritical();                                                                               \
    FLASH_DRIVER_DEINIT(FLASH_DRIVER_STARTADDRESS, &blFlashParam);                                 \
    ExitCritical();                                                                                \
  } while (0)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t address;
  uint32_t length;
} BL_SegmentInfoType;

#ifdef BL_USE_CRC_16
typedef uint16_t bl_crc_t;
#endif

#ifdef BL_USE_CRC_32
typedef uint32_t bl_crc_t;
#endif
/* ================================ [ DECLARES  ] ============================================== */
extern uint8_t FlashDriverRam[];
extern const BL_MemoryInfoType blMemoryList[];
extern const uint32_t blMemoryListSize;
extern const uint32_t blFlsDriverMemoryLow;
extern const uint32_t blFlsDriverMemoryHigh;
extern const uint32_t blAppMemoryLow;
extern const uint32_t blAppMemoryHigh;
extern const uint32_t blFingerPrintAddr;
extern const uint32_t blAppValidFlagAddr;
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

#ifndef BL_DISABLE_SEGMENT_INTEGRITY_CHECK
static BL_SegmentInfoType blSegmentInfos[BL_MAX_SEGMENT];
static uint8_t blSegmentNum = 0;
#endif

#ifdef FL_USE_WRITE_WINDOW_BUFFER
static uint8_t blWriteWindowBuffer[FL_WRITE_WINDOW_SIZE];
static uint32_t blWWBOffset;
static uint32_t blWWBAddr;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static Dcm_ReturnEraseMemoryType eraseFlash(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                            uint32_t MemorySize) {
  Dcm_ReturnEraseMemoryType rv = DCM_ERASE_FAILED;

  uint32_t length;
  switch (OpStatus) {
  case DCM_INITIAL:
    BL_FLS_INIT();
    if (kFlashOk == blFlashParam.errorcode) {
      blOffset = 0;
      rv = DCM_ERASE_PENDING;
    } else {
      rv = DCM_ERASE_FAILED;
      break;
    }
    /* no break here intentionally */
  case DCM_PENDING:
    ASLOG(BL, ("eraseFlash at %X\n", (uint32_t)blOffset));
    length = MemorySize - blOffset;
    if (length > (FL_ERASE_PER_CYCLE * FLASH_ERASE_SIZE)) {
      length = FL_ERASE_PER_CYCLE * FLASH_ERASE_SIZE;
    }

    BL_FLS_ERASE(MemoryAddress + blOffset, length);
    blOffset += length;
    if (kFlashOk == blFlashParam.errorcode) {
      if (blOffset >= MemorySize) {
        rv = DCM_ERASE_OK;
      } else {
        rv = DCM_ERASE_PENDING;
      }
    } else {
      ASLOG(BLE,
            ("erase failed: errorcode = %X(addr=%X,size=%X)\n", (uint32_t)blFlashParam.errorcode,
             (uint32_t)blFlashParam.address, (uint32_t)blFlashParam.length));
      rv = DCM_ERASE_FAILED;
    }
    break;
  case DCM_CANCEL:
    ASLOG(BLE,
          ("erase canceled: addr %X size %X offset %X\n", MemoryAddress, MemorySize, blOffset));
    break;
  default:
    rv = DCM_ERASE_FAILED;
    break;
  }

  return rv;
}
#ifdef FL_USE_WRITE_WINDOW_BUFFER
static Dcm_ReturnWriteMemoryType writeFlash(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                            uint32_t MemorySize, uint8_t *MemoryData) {
  Dcm_ReturnWriteMemoryType rv = DCM_WRITE_FAILED;
  uint32_t length;

  switch (OpStatus) {
  case DCM_INITIAL:
    blOffset = 0;
    if ((blWWBAddr + blWWBOffset) != MemoryAddress) {
      ASLOG(BLE, ("FLS address mismatch: %X %X %X\n", blWWBAddr, blWWBOffset, MemoryAddress));
      rv = DCM_WRITE_FAILED;
      break;
    }
  case DCM_PENDING:
    ASLOG(BL, ("writeFlash at %X\n", (uint32_t)blWWBAddr));
    length = MemorySize - blOffset;
    if (length >= (FL_WRITE_WINDOW_SIZE - blWWBOffset)) {
      /* we have enough to fill the window buffer */
      length = FL_WRITE_WINDOW_SIZE - blWWBOffset;
      memcpy(&blWriteWindowBuffer[blWWBOffset], &MemoryData[blOffset], length);
      BL_FLS_WRITE(blWWBAddr, blWriteWindowBuffer, FL_WRITE_WINDOW_SIZE);
      blOffset += length;
      blWWBOffset = 0;
      blWWBAddr += FL_WRITE_WINDOW_SIZE;
      if (kFlashOk == blFlashParam.errorcode) {
        if (blOffset >= MemorySize) {
          rv = DCM_WRITE_OK;
        } else {
          rv = DCM_WRITE_PENDING;
        }
      } else {
        ASLOG(BLE,
              ("write failed: errorcode = %X(addr=%X,size=%X)\n", (uint32_t)blFlashParam.errorcode,
               (uint32_t)blFlashParam.address, (uint32_t)blFlashParam.length));
        rv = DCM_WRITE_FAILED;
      }
    } else {
      memcpy(&blWriteWindowBuffer[blWWBOffset], &MemoryData[blOffset], length);
      blOffset += length;
      blWWBOffset += length;
      rv = DCM_WRITE_OK;
    }
    break;
  default:
    rv = DCM_WRITE_FAILED;
    break;
  }

  return rv;
}

static Dcm_ReturnWriteMemoryType flushFlash(void) {
  Dcm_ReturnWriteMemoryType ret = DCM_WRITE_OK;
  tLength length;
  if (blWWBOffset > 0) {
    length = FLASH_ALIGNED_WRITE_SIZE(blWWBOffset);
    memset(&blWriteWindowBuffer[blWWBOffset], 0xFF, length - blWWBOffset);
    BL_FLS_WRITE(blWWBAddr, blWriteWindowBuffer, length);
    if (kFlashOk != blFlashParam.errorcode) {
      ASLOG(BLE,
            ("write failed: errorcode = %X(addr=%X,size=%X)\n", (uint32_t)blFlashParam.errorcode,
             (uint32_t)blFlashParam.address, (uint32_t)blFlashParam.length));
      ret = DCM_WRITE_FAILED;
    }
  }
  blWWBAddr = 0;
  blWWBOffset = 0;
  return ret;
}
#else
static Dcm_ReturnWriteMemoryType writeFlash(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                            uint32_t MemorySize, uint8_t *MemoryData) {
  Dcm_ReturnWriteMemoryType rv = DCM_WRITE_FAILED;
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
    BL_FLS_WRITE(MemoryAddress + blOffset, &MemoryData[blOffset], length);
    blOffset += length;
    if (kFlashOk == blFlashParam.errorcode) {
      if (blOffset >= MemorySize) {
        rv = DCM_WRITE_OK;
      } else {
        rv = DCM_WRITE_PENDING;
      }
    } else {
      ASLOG(BLE,
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
#endif /* FL_USE_WRITE_WINDOW_BUFFER */
static Dcm_ReturnReadMemoryType readFlash(Dcm_OpStatusType OpStatus, uint32_t MemoryAddress,
                                          uint32_t MemorySize, uint8_t *MemoryData) {
  Dcm_ReturnReadMemoryType rv = DCM_READ_FAILED;
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
    BL_FLS_READ(MemoryAddress + blOffset, &MemoryData[blOffset], length);
    blOffset += length;
    if (kFlashOk == blFlashParam.errorcode) {
      if (blOffset >= MemorySize) {
        rv = DCM_READ_OK;
      } else {
        rv = DCM_READ_PENDING;
      }
    } else {
      ASLOG(BLE,
            ("read failed: errorcode = %X(addr=%X,size=%X)\n", (uint32_t)blFlashParam.errorcode,
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
  bl_crc_t flsCrc;
  bl_crc_t calcCrc;
  uint32_t flsSz = blFlsDriverMemoryHigh - blFlsDriverMemoryLow;
  uint32_t offset = MemoryAddress - blFlsDriverMemoryLow;

  memcpy((void *)&(FlashDriverRam[offset]), (void *)MemoryData, MemorySize);
  if (blFlsDriverMemoryHigh == (MemoryAddress + MemorySize)) {
    flsCrc = BL_GetCRC(&FlashDriverRam[flsSz - BL_CRC_LENGTH]);
    calcCrc = BL_CalculateCRC(FlashDriverRam, flsSz - BL_CRC_LENGTH, BL_CRC_START_VALUE, TRUE);

    if (flsCrc == calcCrc) {
      bl_flashDriverReady = TRUE;
      ASLOG(BLI, ("Flash Driver is ready\n"));
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

static bl_crc_t getAppNSampledCrc(void) {
  bl_crc_t sampledCrc = BL_CRC_START_VALUE;
#if defined(_WIN32) || defined(__linux__)
  uint8_t data[BL_APP_VALID_SAMPLE_SIZE];
#else
  uint8_t *data;
#endif
  uint32_t address;
  int i;

  for (i = 0; i < BL_APP_VALID_SAMPLE_NUM; i++) {
    address = blAppMemoryLow + BL_APP_VALID_SAMPLE_STRIDE * i;
#if defined(_WIN32) || defined(__linux__)
    readFlash(DCM_INITIAL, address, BL_APP_VALID_SAMPLE_SIZE, data);
#else
    data = (uint8_t *)address;
#endif
    sampledCrc = BL_CalculateCRC(data, BL_APP_VALID_SAMPLE_SIZE, sampledCrc, FALSE);
  }
  return sampledCrc;
}

static Std_ReturnType setAppValidFlag(Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret = E_OK;
  bl_crc_t Crc;
#ifndef FL_USE_WRITE_WINDOW_BUFFER
  uint16_t alignedLen;
  int i;
#endif

  if (FALSE == bl_flashDriverReady) {
    *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    /* This is the last step, set the app valid flag which is a sampled CRC */
    Crc = getAppNSampledCrc();
#ifndef FL_USE_WRITE_WINDOW_BUFFER
    alignedLen = FLASH_ALIGNED_WRITE_SIZE(4);
    BL_SetCRC(dataIn, Crc);
    for (i = BL_CRC_LENGTH; i < alignedLen; i++) {
      dataIn[i] = 0xFF;
    }
    ret = writeFlash(DCM_INITIAL, blAppValidFlagAddr, alignedLen, dataIn);
#else
    BL_SetCRC(blWriteWindowBuffer, Crc);
    blWWBAddr = blAppValidFlagAddr;
    blWWBOffset = BL_CRC_LENGTH;
    ret = flushFlash();
#endif
    if (E_OK != ret) {
      *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
    }
  }
  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_SessionReset(void) {
  blMemoryIdentifier = 0;
  blOffset = 0;

  bl_flashDriverReady = FALSE;

#ifndef BL_DISABLE_SEGMENT_INTEGRITY_CHECK
  blSegmentNum = 0;
#endif

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

Std_ReturnType BL_CheckAppIntegrity(void) {
  Std_ReturnType r = E_NOT_OK;
  bl_crc_t Crc;
  bl_crc_t ValidCrc;
#if defined(_WIN32) || defined(__linux__)
  uint8_t data[FLASH_ALIGNED_READ_SIZE(4)];
#else
  uint8_t *data;
#endif

#if defined(_WIN32) || defined(__linux__)
  readFlash(DCM_INITIAL, blAppValidFlagAddr, FLASH_ALIGNED_READ_SIZE(4), data);
#else
  data = (uint8_t *)blAppValidFlagAddr;
#endif
  ValidCrc = BL_GetCRC(data);
  Crc = getAppNSampledCrc();

  if (ValidCrc == Crc) {
    r = E_OK;
  }
  return r;
}

Std_ReturnType BL_CheckIntegrity(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                 uint16_t *currentDataLength,
                                 Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnReadMemoryType readRet;
  static uint32_t offset = 0;
  bl_crc_t appCrc = BL_CRC_START_VALUE;
  static bl_crc_t calcCrc = BL_CRC_START_VALUE; /* initial value */
  uint32_t memoryAddress = blAppMemoryLow;
  uint32_t memorySize = 256;
  /* NOTE: this size must be <= DCM_DEFAULT_RXBUF_SIZE-4.
   * and <= FL_READ_PER_CYCLE * FLASH_READ_SIZE */

  if (0 == *currentDataLength) {
    r = E_OK;
    if (DCM_INITIAL == OpStatus) {
      offset = 0;
      calcCrc = BL_CRC_START_VALUE;
    }

    if (memorySize > (blFingerPrintAddr - blAppMemoryLow - BL_CRC_LENGTH - offset)) {
      memorySize = (blFingerPrintAddr - blAppMemoryLow - BL_CRC_LENGTH - offset);
    }

    readRet = readFlash(DCM_INITIAL, memoryAddress + offset, memorySize, (uint8_t *)dataIn);
    if (DCM_READ_OK == readRet) {
      offset += memorySize;
      calcCrc = BL_CalculateCRC(dataIn, memorySize, calcCrc, FALSE);

      if (offset >= (blFingerPrintAddr - blAppMemoryLow - BL_CRC_LENGTH)) {
        readRet = readFlash(DCM_INITIAL, blFingerPrintAddr - BL_CRC_LENGTH, BL_CRC_LENGTH,
                            (uint8_t *)dataIn);
        if (DCM_READ_OK == readRet) {
          appCrc = BL_GetCRC(dataIn);
          if (appCrc != calcCrc) {
            ASLOG(BLE, ("check integrity failed as %X != %X\n", calcCrc, appCrc));
            *ErrorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
            r = E_NOT_OK;
          } else {
            r = setAppValidFlag(ErrorCode);
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
#ifndef FL_USE_WRITE_WINDOW_BUFFER
    if ((BL_FLASH_IDENTIFIER == MemoryIdentifier) &&
        ((FALSE == FLASH_IS_WRITE_ADDRESS_ALIGNED(MemoryAddress)) ||
         (FALSE == FLASH_IS_WRITE_ADDRESS_ALIGNED(MemorySize)))) {
      r = E_NOT_OK;
      ASLOG(BLE, ("address(0x%x) or size(%u) not aligned\n", MemoryAddress, MemorySize));
      *ErrorCode = DCM_E_UPLOAD_DOWNLOAD_NOT_ACCEPTED;
    }
#endif

    if (E_OK == r) {
      blMemoryIdentifier = MemoryIdentifier;
      blOffset = 0;
#ifdef FL_USE_WRITE_WINDOW_BUFFER
      if (BL_FLASH_IDENTIFIER == MemoryIdentifier) {
        blWWBOffset = MemoryAddress % FLASH_WRITE_SIZE;
        blWWBAddr = MemoryAddress - blWWBOffset;
        memset(blWriteWindowBuffer, 0xFF, blWWBOffset);
      }
#endif
#ifndef BL_DISABLE_SEGMENT_INTEGRITY_CHECK
      if (blSegmentNum < ARRAY_SIZE(blSegmentInfos)) {
        blSegmentInfos[blSegmentNum].address = MemoryAddress;
        blSegmentInfos[blSegmentNum].length = MemorySize;
        blSegmentNum++;
      } else {
        r = E_NOT_OK;
        ASLOG(BLE, ("too much segments\n"));
        *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
      }
#endif
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
      ASLOG(BLE, ("Flash driver is not ready for write\n"));
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
    ASLOG(BLE, ("Invalid memory ID %X\n", blMemoryIdentifier));
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
      ASLOG(BLE, ("Flash driver is not ready for read\n"));
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
    ASLOG(BLE, ("Invalid memory ID %X\n", blMemoryIdentifier));
    *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    ret = DCM_READ_FAILED;
    break;
  }
  return ret;
}

Std_ReturnType BL_ProcessRequestTransferExit(Dcm_OpStatusType OpStatus,
                                             Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_OK;

#ifdef FL_USE_WRITE_WINDOW_BUFFER
  if (BL_FLASH_IDENTIFIER == blMemoryIdentifier) {
    r = flushFlash();
    if (DCM_WRITE_OK != r) {
      *ErrorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
    }
  }
#endif

  blMemoryIdentifier = 0;
  blOffset = 0;

  return r;
}

Std_ReturnType __weak BL_IsAppValid(void) {
  return E_NOT_OK;
}

void BL_CheckAndJump(void) {
  if (FALSE == BL_IsUpdateRequested()) {
    if (E_OK == BL_CheckAppIntegrity()) {
      ASLOG(INFO, ("application integrity is OK\n"));
      BL_JumpToApp();
    } else if (E_OK == BL_IsAppValid()) {
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

Std_ReturnType BL_ReadFingerPrint(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                  Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ret = E_OK;
#if defined(_WIN32) || defined(__linux__)
  ret = readFlash(DCM_INITIAL, blFingerPrintAddr, FLASH_ALIGNED_READ_SIZE(length), data);
  if (DCM_READ_OK != ret) {
    *errorCode = DCM_E_CONDITIONS_NOT_CORRECT;
    ret = DCM_WRITE_FAILED;
  }
#else
  memcpy(data, (uint8_t *)blFingerPrintAddr, length);
#endif

  return ret;
}

Std_ReturnType BL_WriteFingerPrint(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                   Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ret;
#ifndef FL_USE_WRITE_WINDOW_BUFFER
  uint16_t alignedLen = FLASH_ALIGNED_WRITE_SIZE(length);
  int i;
#endif

  if (FALSE == bl_flashDriverReady) {
    *errorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    ret = E_NOT_OK;
  } else {
#ifndef FL_USE_WRITE_WINDOW_BUFFER
    for (i = length; i < alignedLen; i++) {
      data[i] = 0xFF;
    }
    ret = writeFlash(DCM_INITIAL, blFingerPrintAddr, alignedLen, data);
    if (DCM_WRITE_OK != ret) {
      ret = E_NOT_OK;
      *errorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
    }
#else
    if (length <= FL_WRITE_WINDOW_SIZE) {
      memcpy(blWriteWindowBuffer, data, length);
      blWWBAddr = blFingerPrintAddr;
      blWWBOffset = length;
      ret = flushFlash();
      if (DCM_WRITE_OK != ret) {
        *errorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
      }
    } else {
      ret = E_NOT_OK;
      *errorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
#endif
  }
  return ret;
}
