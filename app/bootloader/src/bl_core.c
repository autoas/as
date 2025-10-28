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

#ifndef BL_APP_VALID_SAMPLE_SIZE
#define BL_APP_VALID_SAMPLE_SIZE FLASH_ALIGNED_READ_SIZE(32)
#endif

#ifndef BL_APP_VALID_SAMPLE_STRIDE
#define BL_APP_VALID_SAMPLE_STRIDE 1024
#endif

#ifndef BL_FLS_READ_SIZE
#define BL_FLS_READ_SIZE 256
#endif

#if defined(_WIN32) || defined(__linux__)
#define BL_SIM_ADDRESS_RE_MAPPING(MemoryAddress)                                                   \
  do {                                                                                             \
    /* address mapping to simulate memory */                                                       \
    if ((uint32_t)-1 == blBaseAddress) {                                                           \
      blBaseAddress = MemoryAddress;                                                               \
    }                                                                                              \
    if (MemoryAddress < blBaseAddress) {                                                           \
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
#define BL_SIM_ADDRESS_RE_MAPPING(addr)
#define BL_SIM_GET_FLASH_DRV_ADDRESS()
#define BL_SIM_ADDRESS_INIT()
#endif

#ifndef BL_MAX_SEGMENT
#define BL_MAX_SEGMENT 32
#endif

#ifndef BL_USE_FLS_READ
#if defined(_WIN32) || defined(__linux__) || defined(__HIWARE__)
#define BL_USE_FLS_READ
#endif
#endif

#if !defined(_WIN32) && !defined(__linux__)
#define FlashDriverRam ((uint8_t *)blFlsDriverMemoryLow)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
#if defined(_WIN32) || defined(__linux__)
extern uint8_t FlashDriverRam[];
#endif

#ifdef BL_USE_AB
extern const BL_MemoryInfoType *blMemoryList;
#else
extern const BL_MemoryInfoType blMemoryList[];
#endif
extern const uint32_t blMemoryListSize;
#ifndef BL_USE_BUILTIN_FLSDRV
extern const uint32_t blFlsDriverMemoryLow;
extern const uint32_t blFlsDriverMemoryHigh;
#endif
extern const uint32_t blAppMemoryLow;
extern const uint32_t blAppMemoryHigh;
extern const uint32_t blFingerPrintAddr;
extern const uint32_t blAppValidFlagAddr;
#ifdef BL_USE_APP_INFO
extern const uint32_t blAppInfoAddr;
#endif
#ifdef BL_USE_META
extern const uint32_t blAppMetaAddr;
extern const uint32_t blAppMetaBackupAddr;
#endif
/* ================================ [ DATAS     ] ============================================== */
tFlashParam blFlashParam = {
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

#ifndef BL_USE_BUILTIN_FLSDRV
static boolean bl_flashDriverReady = FALSE;
#endif

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
    ASLOG(BL, ("eraseFlash at %" PRIx32 "\n", (uint32_t)blOffset));
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
      ASLOG(BLE, ("erase failed: errorcode = %" PRIx32 "(addr=%" PRIx32 ",size=%" PRIx32 ")\n",
                  (uint32_t)blFlashParam.errorcode, (uint32_t)blFlashParam.address,
                  (uint32_t)blFlashParam.length));
      rv = DCM_ERASE_FAILED;
    }
    break;
  case DCM_CANCEL:
    ASLOG(BLE, ("erase canceled: addr %" PRIx32 " size %" PRIx32 " offset %" PRIx32 "\n",
                MemoryAddress, MemorySize, blOffset));
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
      ASLOG(BLE, ("FLS address mismatch: %" PRIx32 " %" PRIx32 " %" PRIx32 "\n", blWWBAddr,
                  blWWBOffset, MemoryAddress));
      rv = DCM_WRITE_FAILED;
      break;
    }
  case DCM_PENDING:
    ASLOG(BL, ("writeFlash at %" PRIx32 "\n", (uint32_t)blWWBAddr));
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
        ASLOG(BLE, ("write failed: errorcode = %" PRIx32 "(addr=%" PRIx32 ",size=%" PRIx32
                    ",data=%02" PRIx8 "%02" PRIx8 ")\n",
                    (uint32_t)blFlashParam.errorcode, (uint32_t)blFlashParam.address,
                    (uint32_t)blFlashParam.length, *((uint8_t *)blFlashParam.address),
                    *((uint8_t *)blFlashParam.address + 1)));
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
      ASLOG(BLE, ("flush failed: errorcode = %" PRIx32 "(addr=%" PRIx32 ",size=%" PRIx32
                  ",data=%02" PRIx8 "%02" PRIx8 ")\n",
                  (uint32_t)blFlashParam.errorcode, (uint32_t)blFlashParam.address,
                  (uint32_t)blFlashParam.length, *((uint8_t *)blFlashParam.address),
                  *((uint8_t *)blFlashParam.address + 1)));
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
    ASLOG(BL, ("writeFlash at %" PRIx32 "\n", (uint32_t)MemoryAddress + blOffset));
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
      ASLOG(BLE, ("write failed: errorcode = %" PRIx32 "(addr=%" PRIx32 ",size=%" PRIx32
                  ",data=%02" PRIx8 "%02" PRIx8 ")\n",
                  (uint32_t)blFlashParam.errorcode, (uint32_t)blFlashParam.address,
                  (uint32_t)blFlashParam.length, *((uint8_t *)blFlashParam.address),
                  *((uint8_t *)blFlashParam.address + 1)));
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
    ASLOG(BL, ("readFlash at %" PRIx32 "\n", (uint32_t)MemoryAddress + blOffset));
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
      ASLOG(BLE, ("read failed: errorcode = %" PRIx32 "(addr=%" PRIx32 ",size=%" PRIx32 ")\n",
                  (uint32_t)blFlashParam.errorcode, (uint32_t)blFlashParam.address,
                  (uint32_t)blFlashParam.length));
      rv = DCM_READ_FAILED;
    }
    break;
  default:
    rv = DCM_READ_FAILED;
    break;
  }

  return rv;
}

#ifndef BL_USE_BUILTIN_FLSDRV
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
      ASLOG(BLI, ("Flash Driver is ready @%x\n", FLASH_DRIVER_START_ADDRESS));
    } else {
      ASLOG(BLE, ("Flash Driver invalid, C %" PRIx32 " != R %" PRIx32 "\n", (uint32_t)calcCrc,
                  (uint32_t)flsCrc));
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
#endif /* BL_USE_BUILTIN_FLSDRV */

#ifdef BL_USE_APP_INFO
static bl_crc_t getAppNSampledCrc(void) {
  Dcm_ReturnReadMemoryType readRet = DCM_READ_OK;
  bl_crc_t sampledCrc = 0xdeadbeef;
#ifdef BL_USE_FLS_READ
  uint8_t data[BL_APP_VALID_SAMPLE_SIZE];
#else
  uint8_t *data;
#endif
  uint32_t numOfSections = 0;
  uint32_t address;
  uint32_t secLow = 0;
  uint32_t secHigh = 0;
  int i;

#ifdef BL_USE_FLS_READ
  readRet = readFlash(DCM_INITIAL, blAppInfoAddr, 4, data);
#else
  data = (uint8_t *)blAppInfoAddr;
#endif

  if (DCM_READ_OK == readRet) {
    numOfSections = (((uint32_t)(data)[0] << 24) + ((uint32_t)(data)[1] << 16) +
                     ((uint32_t)(data)[2] << 8) + (data)[3]);
  }

  for (i = 0; (i < numOfSections) && (DCM_READ_OK == readRet); i++) {
#ifdef BL_USE_FLS_READ
    readRet = readFlash(DCM_INITIAL, blAppInfoAddr + 8 + 8 * i, 8, data);
#else
    data = (uint8_t *)(blAppInfoAddr + 8 + 8 * i);
#endif
    if (DCM_READ_OK == readRet) {
      secLow = (((uint32_t)(data)[0] << 24) + ((uint32_t)(data)[1] << 16) +
                ((uint32_t)(data)[2] << 8) + (data)[3]);
      secHigh = secLow + (((uint32_t)(data)[4] << 24) + ((uint32_t)(data)[5] << 16) +
                          ((uint32_t)(data)[6] << 8) + (data)[7]);
      if ((secLow >= blAppMemoryLow) && (secHigh <= blAppMemoryHigh) && (secHigh > secLow)) {
        address = secLow;
      } else {
        readRet = DCM_READ_FAILED;
      }
    }
    while ((DCM_READ_OK == readRet) && ((address + BL_APP_VALID_SAMPLE_SIZE) <= secHigh)) {
#ifdef BL_USE_FLS_READ
      readRet = readFlash(DCM_INITIAL, address, BL_APP_VALID_SAMPLE_SIZE, data);
#else
      data = (uint8_t *)address;
#endif
      if (DCM_READ_OK == readRet) {
        sampledCrc = BL_CalculateCRC(data, BL_APP_VALID_SAMPLE_SIZE, sampledCrc, FALSE);
        address += BL_APP_VALID_SAMPLE_STRIDE;
      }
    }
  }

  return sampledCrc;
}
#else
static bl_crc_t getAppNSampledCrc(void) {
  bl_crc_t sampledCrc = BL_CRC_START_VALUE;
#ifdef BL_USE_FLS_READ
  uint8_t data[BL_APP_VALID_SAMPLE_SIZE];
#else
  uint8_t *data;
#endif
  uint32_t address;
  int i;

  for (i = 0; i < blMemoryListSize; i++) {
    if (BL_FLASH_IDENTIFIER == blMemoryList[i].identifier) {
      address = blMemoryList[i].addressLow;
      while (((address + BL_APP_VALID_SAMPLE_SIZE) <= blMemoryList[i].addressHigh) &&
             ((address + BL_APP_VALID_SAMPLE_SIZE) <= blAppValidFlagAddr) &&
             (address < blAppValidFlagAddr)) {
#ifdef BL_USE_FLS_READ
        readFlash(DCM_INITIAL, address, BL_APP_VALID_SAMPLE_SIZE, data);
#else
        data = (uint8_t *)address;
#endif
        sampledCrc = BL_CalculateCRC(data, BL_APP_VALID_SAMPLE_SIZE, sampledCrc, FALSE);
        address += BL_APP_VALID_SAMPLE_STRIDE;
      }
    }
  }
  return sampledCrc;
}
#endif

#if defined(BL_USE_APP_INFO_V2)
static Std_ReturnType BL_CopyAppInfoV2ForV1(void) {
  Std_ReturnType ret = E_OK;
  Dcm_ReturnReadMemoryType readRet;
  Dcm_ReturnWriteMemoryType writeRet;
  uint32_t numOfBlks;
  uint8_t signature[16];
  uint32_t addr;

  if (blSegmentNum > 0) {
    addr = blSegmentInfos[blSegmentNum - 1].address + blSegmentInfos[blSegmentNum - 1].length;
    ASLOG(BLI, ("V2 sinature at %" PRIx32 " app info at %" PRIx32 "\n", addr, blAppInfoAddr));
    readRet = readFlash(DCM_INITIAL, addr - 16, 16, (uint8_t *)signature);
  } else {
    readRet = DCM_READ_FAILED;
  }

  if (DCM_READ_OK == readRet) {
    if (0 == memcmp(&signature[8], "$BYASV3#", 8)) {
      numOfBlks = ((uint32_t)signature[0] << 24) + ((uint32_t)signature[1] << 16) +
                  ((uint32_t)signature[2] << 8) + signature[3];
      memcpy(blWriteWindowBuffer, signature, 8);
      readRet = readFlash(DCM_INITIAL, addr - 16 - 8 * numOfBlks, 8 * numOfBlks,
                          (uint8_t *)&blWriteWindowBuffer[8]);
      if (DCM_READ_OK == readRet) {
        blWWBAddr = blAppInfoAddr;
        blWWBOffset = 8 + 8 * numOfBlks;
        writeRet = flushFlash();
        if (DCM_WRITE_OK != writeRet) {
          ret = E_NOT_OK;
        }
      } else {
        ret = E_NOT_OK;
      }
    } else {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_SessionReset(void) {
  blMemoryIdentifier = 0;
  blOffset = 0;
#ifndef BL_USE_BUILTIN_FLSDRV
  bl_flashDriverReady = FALSE;
#endif

#ifndef BL_DISABLE_SEGMENT_INTEGRITY_CHECK
  blSegmentNum = 0;
#endif
#ifndef BL_USE_BUILTIN_FLSDRV
  memset(FlashDriverRam, 0xFF, blFlsDriverMemoryHigh - blFlsDriverMemoryLow);
#endif

  BL_SIM_ADDRESS_INIT();
}

uint8_t BL_GetMemoryIdentifier(uint32_t MemoryAddress, uint32_t MemorySize) {
  uint8_t MemoryIdentifier = 0;
  int i;
#if defined(BL_USE_AB) && defined(BL_USE_AB_UPDATE_ACTIVE)
  boolean bPrepareUpdateActiveOK = FALSE;
#endif

#if defined(BL_USE_AB) && defined(BL_USE_AB_UPDATE_ACTIVE)
  do {
    bPrepareUpdateActiveOK = FALSE;
#endif
    for (i = 0; i < blMemoryListSize; i++) {
      ASLOG(BL, ("%02" PRIx8 ": low=0x%" PRIx32 " high=%" PRIx32 "\n", blMemoryList[i].identifier,
                 blMemoryList[i].addressLow, blMemoryList[i].addressHigh));
      if ((MemoryAddress >= blMemoryList[i].addressLow) &&
          ((MemoryAddress + MemorySize) <= blMemoryList[i].addressHigh)) {
        MemoryIdentifier = blMemoryList[i].identifier;
        break;
      }
    }
#if defined(BL_USE_AB) && defined(BL_USE_AB_UPDATE_ACTIVE)
    if (0 == MemoryIdentifier) {
      bPrepareUpdateActiveOK = BL_ABPrepareUpdateActive();
    }
  } while (TRUE == bPrepareUpdateActiveOK);
#endif

  return MemoryIdentifier;
}

Std_ReturnType BL_StartEraseFlash(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                  uint8_t *dataOut, uint16_t *currentDataLength,
                                  Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnEraseMemoryType eraseRet;
  uint32_t MemoryAddress = blAppMemoryLow;
  uint32_t MemorySize = blAppMemoryHigh - blAppMemoryLow;
  uint8_t MemoryIdentifier;

  BL_SIM_ADDRESS_INIT();

  if (
#ifndef FLASH_ERASE_DISABLE_V0
    (0 == *currentDataLength) ||
#endif
    (9 == *currentDataLength)) {
    if (
#ifndef FLASH_ERASE_DISABLE_V0
      (9 == *currentDataLength) &&
#endif
      (dataIn[0] != 0x44)) {
      *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
#ifndef BL_USE_BUILTIN_FLSDRV
    else if (FALSE == bl_flashDriverReady) {
      ASLOG(BLE, ("Flash driver is not ready for erase\n"));
      *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
#endif
#ifdef BL_USE_META
    else if (BL_MetaGetProgrammingCouner() >= BL_MAX_PROGRAMMING_COUNTER) {
      ASLOG(BLE, ("Reach max programming counter\n"));
      *ErrorCode = DCM_E_EXCEED_NUMBER_OF_ATTEMPTS;
    }
#endif
    else {
      if (9 == *currentDataLength) {
        MemoryAddress = ((uint32_t)dataIn[1] << 24) + ((uint32_t)dataIn[2] << 16) +
                        ((uint32_t)dataIn[3] << 8) + dataIn[4];
        MemorySize = ((uint32_t)dataIn[5] << 24) + ((uint32_t)dataIn[6] << 16) +
                     ((uint32_t)dataIn[7] << 8) + dataIn[8];
        BL_SIM_ADDRESS_RE_MAPPING(MemoryAddress);
      }

      MemoryIdentifier = BL_GetMemoryIdentifier(MemoryAddress, MemorySize);
      if ((BL_FLASH_IDENTIFIER == MemoryIdentifier) || (BL_FEE_IDENTIFIER == MemoryIdentifier)) {
        /* anyway, before erase, do some misc bootloader relate specific jobs */
        r = BL_MiscMetaInfoInitOnce();
        if (E_OK == r) {
          eraseRet = eraseFlash(OpStatus, MemoryAddress, MemorySize);
        } else {
          eraseRet = DCM_ERASE_FAILED;
        }
        if (DCM_ERASE_OK == eraseRet) {
          r = E_OK;
#ifndef FLASH_ERASE_DISABLE_V0
          if (9 == *currentDataLength)
#endif
          {
            dataOut[0] = 0;
            *currentDataLength = 1;
          }
        } else if (DCM_ERASE_PENDING == eraseRet) {
          *ErrorCode = DCM_E_RESPONSE_PENDING;
        } else {
          *ErrorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
        }
      } else {
        *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
      }
    }
  } else {
    *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

#if 1
Std_ReturnType BL_CheckAppIntegrity(void) {
  Std_ReturnType r = E_NOT_OK;
  bl_crc_t Crc;
  bl_crc_t ValidCrc;
#ifdef BL_USE_FLS_READ
  uint8_t data[FLASH_ALIGNED_READ_SIZE(4)];
#else
  uint8_t *data;
#endif

#ifdef BL_USE_FLS_READ
  readFlash(DCM_INITIAL, blAppValidFlagAddr, FLASH_ALIGNED_READ_SIZE(4), data);
#else
  data = (uint8_t *)blAppValidFlagAddr;
#endif
  ValidCrc = BL_GetCRC(data);
  Crc = getAppNSampledCrc();

  if (ValidCrc == Crc) {
    r = E_OK;
  } else {
    ASLOG(BLE, ("CheckAppIntegrity Crc = %" PRIx32 " != ValidCrc %" PRIx32 "\n", (uint32_t)Crc,
                (uint32_t)ValidCrc));
  }
  return r;
}
#else
#if defined(_WIN32) || defined(__linux__)
Std_ReturnType BL_CheckAppIntegrity(void) {
  Std_ReturnType r = E_NOT_OK;
  uint32_t memoryAddress;
  uint32_t memorySize;
  uint32_t offset = 0;
  bl_crc_t appCrc = BL_CRC_START_VALUE;
  bl_crc_t calcCrc = BL_CRC_START_VALUE; /* initial value */
  static uint8_t dataIn[256];
  memoryAddress = blAppMemoryLow;

  FLASH_DRIVER_INIT(FLASH_DRIVER_START_ADDRESS, &blFlashParam);
  while (offset < (blFingerPrintAddr - blAppMemoryLow - BL_CRC_LENGTH)) {
    memorySize = sizeof(dataIn);
    if (memorySize > (blFingerPrintAddr - blAppMemoryLow - BL_CRC_LENGTH - offset)) {
      memorySize = (blFingerPrintAddr - blAppMemoryLow - BL_CRC_LENGTH - offset);
    }

    (void)readFlash(DCM_INITIAL, memoryAddress + offset, memorySize, (uint8_t *)dataIn);

    offset += memorySize;
    calcCrc = BL_CalculateCRC(dataIn, memorySize, calcCrc, FALSE);
  }

  (void)readFlash(DCM_INITIAL, blFingerPrintAddr - BL_CRC_LENGTH, BL_CRC_LENGTH, (uint8_t *)dataIn);
  appCrc = BL_GetCRC(dataIn);

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
  bl_crc_t appCrc = BL_CRC_START_VALUE;
  bl_crc_t calcCrc = BL_CRC_START_VALUE; /* initial value */
  memory = (uint8_t *)blAppMemoryLow;
  size = blFingerPrintAddr - blAppMemoryLow - BL_CRC_LENGTH;

  appCrc = BL_GetCRC(&memory[size]);
  calcCrc = BL_CalculateCRC(memory, size, calcCrc, FALSE);

  if (calcCrc == appCrc) {
    r = E_OK;
  }

  return r;
}
#endif
#endif

#if defined(BL_USE_APP_INFO) || defined(BL_USE_APP_INFO_V2)
Std_ReturnType BL_CheckIntegrity(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                 uint16_t *currentDataLength,
                                 Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_OK;
  Dcm_ReturnReadMemoryType readRet;
  static uint32_t numOfSections = 0;
  static uint32_t curSec = 0;
  static bl_crc_t appCrc = BL_CRC_START_VALUE;
  static bl_crc_t calcCrc = BL_CRC_START_VALUE; /* initial value */
  static uint32_t secLow = 0;
  static uint32_t secHigh = 0;
  uint32_t readLen = BL_FLS_READ_SIZE;
  /* NOTE: this size must be <= DCM_DEFAULT_RXBUF_SIZE-4.
   * and <= FL_READ_PER_CYCLE * FLASH_READ_SIZE */

#if defined(BL_USE_APP_INFO_V2)
  if (DCM_INITIAL == OpStatus) {
    r = BL_CopyAppInfoV2ForV1();
  }
  if (E_OK != r) {
    *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
    r = E_NOT_OK;
  } else
#endif
    if (0 == *currentDataLength) {
    if (DCM_INITIAL == OpStatus) {
      readRet = readFlash(DCM_INITIAL, blAppInfoAddr, 8, (uint8_t *)dataIn);
      if (DCM_READ_OK == readRet) {
        curSec = 0;
        numOfSections = (((uint32_t)(dataIn)[0] << 24) + ((uint32_t)(dataIn)[1] << 16) +
                         ((uint32_t)(dataIn)[2] << 8) + (dataIn)[3]);
        appCrc = BL_GetCRC(&dataIn[4]);
        calcCrc = BL_CRC_START_VALUE;
      } else {
        *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
        r = E_NOT_OK;
      }
      secLow = 0;
      secHigh = 0;
    }
  } else {
    *ErrorCode = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if ((E_OK == r) && (secLow >= secHigh)) {
    readRet = readFlash(DCM_INITIAL, blAppInfoAddr + 8 + 8 * curSec, 8, (uint8_t *)dataIn);
    if (DCM_READ_OK == readRet) {
      secLow = (((uint32_t)(dataIn)[0] << 24) + ((uint32_t)(dataIn)[1] << 16) +
                ((uint32_t)(dataIn)[2] << 8) + (dataIn)[3]);
      secHigh = secLow + (((uint32_t)(dataIn)[4] << 24) + ((uint32_t)(dataIn)[5] << 16) +
                          ((uint32_t)(dataIn)[6] << 8) + (dataIn)[7]);
      BL_SIM_ADDRESS_RE_MAPPING(secLow);
      BL_SIM_ADDRESS_RE_MAPPING(secHigh);
      if ((secLow >= blAppMemoryLow) && (secHigh <= blAppMemoryHigh) && (secHigh > secLow)) {
        ASLOG(BLI, ("check integrity on section %" PRIu32 ": %" PRIx32 "-%" PRIx32 "\n", curSec,
                    secLow, secHigh));
        curSec += 1; /* move to next section */
      } else {
        *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
        r = E_NOT_OK;
      }
    } else {
      *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    if ((secLow + readLen) > secHigh) {
      readLen = secHigh - secLow;
    }

    readRet = readFlash(DCM_INITIAL, secLow, readLen, (uint8_t *)dataIn);
    if (DCM_READ_OK == readRet) {
      secLow += readLen;
      calcCrc = BL_CalculateCRC(dataIn, readLen, calcCrc, FALSE);
      if (secLow >= secHigh) {         /* A section end */
        if (curSec >= numOfSections) { /* all section done */
          if (appCrc != calcCrc) {
            ASLOG(BLE, ("check integrity failed as %" PRIx32 " != %" PRIx32 "\n", calcCrc, appCrc));
            *ErrorCode = DCM_E_GENERAL_PROGRAMMING_FAILURE;
            r = E_NOT_OK;
          } else {
            /* pass check */
          }
        } else {
          *ErrorCode = DCM_E_RESPONSE_PENDING;
        }
      } else {
        *ErrorCode = DCM_E_RESPONSE_PENDING;
      }
    } else {
      *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
      r = E_NOT_OK;
    }
  }

  return r;
}
#else
Std_ReturnType BL_CheckIntegrity(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                 uint16_t *currentDataLength,
                                 Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnReadMemoryType readRet;
  static uint32_t offset = 0;
  bl_crc_t appCrc = BL_CRC_START_VALUE;
  static bl_crc_t calcCrc = BL_CRC_START_VALUE; /* initial value */
  uint32_t memoryAddress = blAppMemoryLow;
  uint32_t memorySize = BL_FLS_READ_SIZE;
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
            ASLOG(BLE, ("check integrity failed as %" PRIx32 " != %" PRIx32 "\n", calcCrc, appCrc));
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
#endif

Std_ReturnType BL_ProcessRequestDownload(Dcm_OpStatusType OpStatus, uint8_t DataFormatIdentifier,
                                         uint8_t MemoryIdentifier, uint32_t MemoryAddress,
                                         uint32_t MemorySize, uint32_t *BlockLength,
                                         Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType r = E_NOT_OK;
  (void)OpStatus;
  (void)BlockLength;

  BL_SIM_ADDRESS_RE_MAPPING(MemoryAddress);

  if (0x00 == DataFormatIdentifier) {
    r = E_OK;
  } else {
    ASLOG(BLE, ("invalid data format 0x%" PRIx32 "\n", DataFormatIdentifier));
    *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
  }

  if (E_OK == r) {
    MemoryIdentifier = BL_GetMemoryIdentifier(MemoryAddress, MemorySize);
    if (0 == MemoryIdentifier) {
      r = E_NOT_OK;
      ASLOG(BLE,
            ("invalid address(0x%" PRIx32 ") or size(%" PRIu32 ")\n", MemoryAddress, MemorySize));
      *ErrorCode = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  }

  if (E_OK == r) {
#ifndef FL_USE_WRITE_WINDOW_BUFFER
    if ((BL_FLASH_IDENTIFIER == MemoryIdentifier) &&
        ((FALSE == FLASH_IS_WRITE_ADDRESS_ALIGNED(MemoryAddress)) ||
         (FALSE == FLASH_IS_WRITE_ADDRESS_ALIGNED(MemorySize)))) {
      r = E_NOT_OK;
      ASLOG(BLE, ("address(0x%" PRIx32 ") or size(%u) not aligned\n", MemoryAddress, MemorySize));
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
        BL_FLS_READ(blWWBAddr, blWriteWindowBuffer, blWWBOffset);
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

  BL_SIM_ADDRESS_RE_MAPPING(MemoryAddress);

  switch (blMemoryIdentifier) {
  case BL_FLASH_IDENTIFIER:
#ifndef BL_USE_BUILTIN_FLSDRV
    if (FALSE == bl_flashDriverReady) {
      ASLOG(BLE, ("Flash driver is not ready for write\n"));
      *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
      ret = DCM_WRITE_FAILED;
    } else
#endif
    {
      ret = writeFlash(OpStatus, MemoryAddress, MemorySize, MemoryData);
    }
    break;
#ifndef BL_USE_BUILTIN_FLSDRV
  case BL_FLSDRV_IDENTIFIER:
    ret = writeFlashDriver(OpStatus, MemoryAddress, MemorySize, MemoryData);
    break;
#endif
  default:
    ASLOG(BLE, ("Invalid memory ID %" PRIx32 "\n", blMemoryIdentifier));
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

  BL_SIM_ADDRESS_RE_MAPPING(MemoryAddress);

  switch (blMemoryIdentifier) {
  case BL_FLASH_IDENTIFIER:
#ifndef BL_USE_BUILTIN_FLSDRV
    if (FALSE == bl_flashDriverReady) {
      ASLOG(BLE, ("Flash driver is not ready for read\n"));
      *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
      ret = DCM_READ_FAILED;
    } else
#endif
    {
      ret = readFlash(OpStatus, MemoryAddress, MemorySize, MemoryData);
    }
    break;
#ifndef BL_USE_BUILTIN_FLSDRV
  case BL_FLSDRV_IDENTIFIER:
    ret = readFlashDriver(OpStatus, MemoryAddress, MemorySize, MemoryData);
    break;
#endif
  default:
    ASLOG(BLE, ("Invalid memory ID %" PRIx32 "\n", blMemoryIdentifier));
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

#if !defined(__HIWARE__) && !defined(__CSP__)
FUNC(Std_ReturnType, __weak) BL_IsAppValid(void) {
  return E_NOT_OK;
}
#endif

void BL_CheckAndJump(void) {
#ifdef BL_USE_AB
  BL_ABInit();
#endif
#ifdef BL_RECOVERY_MODE
/* BL recovery mode that will not do booting, only do recovery - flashing new app image */
#else
  if (FALSE == BL_IsUpdateRequested()) {
    if (E_OK == BL_CheckAppIntegrity()) {
      ASLOG(INFO, ("application integrity is OK\n"));
      BL_JumpToApp();
    } else if (E_OK == BL_IsAppValid()) {
      ASLOG(INFO, ("application is valid\n"));
      BL_JumpToApp();
    } else {
#ifdef BL_USE_AB
      BL_ABSwitch(); /* as corrupted, witch to check if possible to boot from backup */
      if (E_OK == BL_CheckAppIntegrity()) {
        ASLOG(INFO, ("application integrity is OK\n"));
        BL_JumpToApp();
      } else if (E_OK == BL_IsAppValid()) {
        ASLOG(INFO, ("application is valid\n"));
        BL_JumpToApp();
      }
#endif
    }
  }
#endif
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

#ifdef BL_USE_AB
  BL_ABSwitch();
#endif
#ifdef BL_USE_META
  BL_MetaInit();
#endif
  BL_MiscInit();
  ASLOG(BLI,
        ("AS Bootloader version %d.%d.%d\n", BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_PATCH));
}

Std_ReturnType BL_ReadFingerPrint(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                  Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ret = E_OK;
  data[0] = 0;

  ret = readFlash(DCM_INITIAL, blFingerPrintAddr, FLASH_ALIGNED_READ_SIZE(length - 1), &data[1]);
  if (DCM_READ_OK != ret) {
    *errorCode = DCM_E_CONDITIONS_NOT_CORRECT;
    ret = DCM_WRITE_FAILED;
  }

  return ret;
}

Std_ReturnType BL_WriteFingerPrint(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                   Dcm_NegativeResponseCodeType *errorCode) {
  Std_ReturnType ret = E_OK;
#ifndef FL_USE_WRITE_WINDOW_BUFFER
  uint16_t alignedLen = FLASH_ALIGNED_WRITE_SIZE(length);
  int i;
#endif
#ifndef BL_USE_BUILTIN_FLSDRV
  if (FALSE == bl_flashDriverReady) {
    *errorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    ret = E_NOT_OK;
  }
#endif
  if (E_OK == ret) {
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

Std_ReturnType BL_CheckProgrammingDependencies(uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                               uint8_t *dataOut, uint16_t *currentDataLength,
                                               Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret = E_OK;
  bl_crc_t Crc;
#ifndef FL_USE_WRITE_WINDOW_BUFFER
  uint16_t alignedLen;
  int i;
#endif

/* TODO: */
#ifndef BL_USE_BUILTIN_FLSDRV
  if (FALSE == bl_flashDriverReady) {
    *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
    ret = E_NOT_OK;
  }
#endif
#ifdef BL_USE_META
  if (E_OK == ret) {
    ret = BL_MetaUpdate();
  }
#endif

  if (E_OK == ret) {
    /* This is the last step, set the app valid flag which is a sampled CRC */
    Crc = getAppNSampledCrc();
    ASLOG(BLI, ("app sampled CRC %" PRIx32 "\n", Crc));
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
    if (DCM_WRITE_OK != ret) {
      *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
    } else {
      *currentDataLength = 1;
      dataOut[0] = 0x00;
    }
    ret = E_OK;
  }
  return ret;
}

#ifndef BL_DISABLE_SEGMENT_INTEGRITY_CHECK
Std_ReturnType BL_CheckProgrammingIntegrity(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                            uint8_t *dataOut, uint16_t *currentDataLength,
                                            Dcm_NegativeResponseCodeType *ErrorCode) {

  Std_ReturnType r = E_NOT_OK;
  Dcm_ReturnReadMemoryType readRet;
  static int i;
  static uint32_t offset = 0;
  static bl_crc_t calcCrc; /* initial value */
  static bl_crc_t appCrc;
  uint32_t MemoryAddress;
  uint32_t MemorySize = 256;
  /* NOTE: this size must be <= DCM_DEFAULT_RXBUF_SIZE-4.
   * and <= FL_READ_PER_CYCLE * FLASH_READ_SIZE */
  uint8_t MemoryIdentifier;

  if (0 == blSegmentNum) {
    *ErrorCode = DCM_E_REQUEST_SEQUENCE_ERROR;
  } else if (blSegmentNum > ARRAY_SIZE(blSegmentInfos)) {
    *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
  } else if (BL_CRC_LENGTH == *currentDataLength) {
    r = E_OK;
    if (DCM_INITIAL == OpStatus) {
      i = 0;
      offset = 0;
      calcCrc = BL_CRC_START_VALUE;
      appCrc = BL_GetCRC(dataIn);
    }

    MemoryAddress = blSegmentInfos[i].address;
    if (MemorySize > (blSegmentInfos[i].length - offset)) {
      MemorySize = blSegmentInfos[i].length - offset;
    }

    MemoryIdentifier = BL_GetMemoryIdentifier(MemoryAddress, MemorySize);
    if (MemoryIdentifier == BL_FLASH_IDENTIFIER) {
      readRet = readFlash(DCM_INITIAL, MemoryAddress + offset, MemorySize, (uint8_t *)dataIn);
    } else {
#ifndef BL_USE_BUILTIN_FLSDRV
      BL_SIM_GET_FLASH_DRV_ADDRESS();
      readRet = readFlashDriver(DCM_INITIAL, MemoryAddress + offset, MemorySize, (uint8_t *)dataIn);
#else
      readRet = DCM_READ_FAILED;
#endif
    }
    if (DCM_READ_OK == readRet) {
      offset += MemorySize;
      calcCrc = BL_CalculateCRC(dataIn, MemorySize, calcCrc, FALSE);

      if (offset >= blSegmentInfos[i].length) {
        i++;
        offset = 0;
        if (i >= blSegmentNum) {
          if (appCrc != calcCrc) {
            ASLOG(BLE, ("check segment integrity failed as %" PRIx32 " != %" PRIx32 "\n", calcCrc,
                        appCrc));
            dataOut[0] = 1; /* incorrect */
            *currentDataLength = 1;
          } else {
            dataOut[0] = 0; /* correct result */
            *currentDataLength = 1;
          }
          blSegmentNum = 0; /* drop segment infos */
        } else {
          *ErrorCode = DCM_E_RESPONSE_PENDING;
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
#endif

Std_ReturnType BL_CheckProgrammingPreconditions(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                                uint8_t *dataOut, uint16_t *currentDataLength,
                                                Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret = E_OK;
  *currentDataLength = 1;
  dataOut[0] = 0x00;
  return ret;
}

#ifndef BL_DISABLE_SEGMENT_INTEGRITY_CHECK
Std_ReturnType BL_GetDownloadSegmentInfos(uint8_t *SegmentNum, BL_SegmentInfoType **SegmentInfos) {
  *SegmentNum = blSegmentNum;
  *SegmentInfos = blSegmentInfos;
  return E_OK;
}

void BL_ResetDownloadSegmentInfos(void) {
  blSegmentNum = 0;
}
#endif

boolean BL_IsFlashDriverReady(void) {
#ifndef BL_USE_BUILTIN_FLSDRV
  return bl_flashDriverReady;
#else
  return TRUE;
#endif
}
