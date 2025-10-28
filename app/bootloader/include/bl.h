/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef BL_H
#define BL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include "Std_Critical.h"
#include "Flash.h"
#include "Crc.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_BL -1
#define AS_LOG_BLI 1
#define AS_LOG_BLE 2

#define BL_FLASH_IDENTIFIER 0xFF
#define BL_FEE_IDENTIFIER 0xFE
#define BL_EEPROM_IDENTIFIER 0xEE
#define BL_FLSDRV_IDENTIFIER 0xFD

#define BL_VERSION_MAJOR 4
#define BL_VERSION_MINOR 0
#define BL_VERSION_PATCH 0

#if defined(_WIN32) || defined(__linux__)
#ifndef BL_USE_META
#define BL_USE_META
#endif
#endif

#ifndef BL_MAX_PROGRAMMING_COUNTER
#define BL_MAX_PROGRAMMING_COUNTER 10000
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
    FLASH_DRIVER_INIT(FLASH_DRIVER_START_ADDRESS, &blFlashParam);                                  \
    if (kFlashOk != blFlashParam.errorcode) {                                                      \
      ASLOG(BLE, ("Flash init failed: %d\n", blFlashParam.errorcode));                             \
    }                                                                                              \
    ExitCritical();                                                                                \
  } while (0)

#define BL_FLS_ERASE(addr, len)                                                                    \
  do {                                                                                             \
    blFlashParam.address = addr;                                                                   \
    blFlashParam.length = len;                                                                     \
    blFlashParam.data = NULL;                                                                      \
    EnterCritical();                                                                               \
    FLASH_DRIVER_ERASE(FLASH_DRIVER_START_ADDRESS, &blFlashParam);                                 \
    ExitCritical();                                                                                \
  } while (0)

#define BL_FLS_WRITE(addr, pData, len)                                                             \
  do {                                                                                             \
    blFlashParam.address = addr;                                                                   \
    blFlashParam.length = len;                                                                     \
    blFlashParam.data = (tData *)pData;                                                            \
    EnterCritical();                                                                               \
    FLASH_DRIVER_WRITE(FLASH_DRIVER_START_ADDRESS, &blFlashParam);                                 \
    ExitCritical();                                                                                \
  } while (0)

#if defined(_WIN32) || defined(__linux__)
#define BL_FLS_READ(addr, pData, len)                                                              \
  do {                                                                                             \
    blFlashParam.address = addr;                                                                   \
    blFlashParam.length = len;                                                                     \
    blFlashParam.data = (tData *)pData;                                                            \
    EnterCritical();                                                                               \
    FLASH_DRIVER_READ(FLASH_DRIVER_START_ADDRESS, &blFlashParam);                                  \
    ExitCritical();                                                                                \
  } while (0)
#elif defined(BL_USE_BUILTIN_FLS_READ)
#define BL_FLS_READ(addr, pData, len)                                                              \
  do {                                                                                             \
    blFlashParam.address = addr;                                                                   \
    blFlashParam.length = len;                                                                     \
    blFlashParam.data = (tData *)pData;                                                            \
    EnterCritical();                                                                               \
    FlashRead(&blFlashParam);                                                                      \
    ExitCritical();                                                                                \
  } while (0)
#else
#define BL_FLS_READ(addr, pData, len)                                                              \
  do {                                                                                             \
    /* For most of the MCU Flash, can read it directly */                                          \
    memcpy(pData, (void_ptr_t)(addr), len);                                                        \
    blFlashParam.errorcode = kFlashOk;                                                             \
  } while (0)
#endif

#define BL_FLS_DEINIT()                                                                            \
  do {                                                                                             \
    EnterCritical();                                                                               \
    FLASH_DRIVER_DEINIT(FLASH_DRIVER_START_ADDRESS, &blFlashParam);                                \
    ExitCritical();                                                                                \
  } while (0)

#define BL_META_MAGIC                                                                              \
  (((uint32_t)'M' << 24) | ((uint32_t)'E' << 16) | ((uint32_t)'T' << 8) | ((uint32_t)'A'))
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t addressLow;
  uint32_t addressHigh;
  uint8_t identifier;
} BL_MemoryInfoType;

#ifdef BL_USE_CRC_16
typedef uint16_t bl_crc_t;
#endif

#ifdef BL_USE_CRC_32
typedef uint32_t bl_crc_t;
#endif

typedef struct {
  uint32_t address;
  uint32_t length;
} BL_SegmentInfoType;

typedef struct {
  uint32_t magic;
  uint32_t programmingCounter; /* programming counter for current partition */
#ifdef BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER
  uint32_t rollingCounter; /* programming counter for all the partition */
#endif
} BL_Meta_t;

typedef struct {
  BL_Meta_t m;
  bl_crc_t crc;
  bl_crc_t crcInv;
} BL_MetaType;
/* ================================ [ DECLARES  ] ============================================== */
extern boolean BL_IsUpdateRequested(void);
extern void BL_JumpToApp(void);
extern Std_ReturnType BL_GetProgramCondition(Dcm_ProgConditionsType **cond);
extern Std_ReturnType BL_IsAppValid(void);
/* ================================ [ DATAS     ] ============================================== */
extern tFlashParam blFlashParam;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_Init(void);
void BL_ABInit(void);
void BL_ABSwitch(void);
void BL_ABSetActivePartition(char activePartition);
boolean BL_ABPrepareUpdateActive(void);
void BL_SessionReset(void);
Std_ReturnType BL_CheckAppIntegrity(void);
void BL_CheckAndJump(void);

void BL_FlushNvM(void);

void BL_MetaInit(void);
Std_ReturnType BL_MetaBackup(void);
Std_ReturnType BL_MetaUpdate(void);
uint32_t BL_MetaGetProgrammingCouner(void);
uint32_t BL_MetaGetRollingCouner(void);

void BL_MiscInit(void);
Std_ReturnType BL_MiscMetaInfoInitOnce(void);

Std_ReturnType BL_GetDownloadSegmentInfos(uint8_t *SegmentNum, BL_SegmentInfoType **SegmentInfos);
void BL_ResetDownloadSegmentInfos(void);
boolean BL_IsFlashDriverReady(void);

Std_ReturnType BL_CheckIntegrity(const uint8_t *dataIn, Dcm_OpStatusType OpStatus, uint8_t *dataOut,
                                 uint16_t *currentDataLength,
                                 Dcm_NegativeResponseCodeType *ErrorCode);

Std_ReturnType BL_WriteFingerPrint(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,
                                   Dcm_NegativeResponseCodeType *errorCode);

Std_ReturnType BL_CheckProgrammingDependencies(uint8_t *dataIn, Dcm_OpStatusType OpStatus,
                                               uint8_t *dataOut, uint16_t *currentDataLength,
                                               Dcm_NegativeResponseCodeType *ErrorCode);
#endif /* BL_H */
