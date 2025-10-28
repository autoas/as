/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
#include "RoD.h"

#ifdef BL_USE_AB
/* ================================ [ MACROS    ] ============================================== */
#define ROD_NUMBER_AppVersion 0
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} RoD_AppVersionType;
/* ================================ [ DECLARES  ] ============================================== */
extern const uint32_t blAppMemoryLowA;
extern const uint32_t blAppMemoryHighA;
extern const RoD_ConfigType *const RoD_AppConfigA;
extern const uint32_t blFingerPrintAddrA;
#ifdef BL_USE_APP_INFO
extern const uint32_t blAppInfoAddrA;
#endif
extern const uint32_t blAppValidFlagAddrA;
#ifdef BL_USE_META
extern const uint32_t blAppMetaAddrA;
extern const uint32_t blAppMetaBackupAddrA;
#endif

extern const uint32_t blAppMemoryLowB;
extern const uint32_t blAppMemoryHighB;
extern const RoD_ConfigType *const RoD_AppConfigB;
extern const uint32_t blFingerPrintAddrB;
#ifdef BL_USE_APP_INFO
extern const uint32_t blAppInfoAddrB;
#endif
extern const uint32_t blAppValidFlagAddrB;
#ifdef BL_USE_META
extern const uint32_t blAppMetaAddrB;
extern const uint32_t blAppMetaBackupAddrB;
#endif

extern const BL_MemoryInfoType blMemoryListA[];
extern const uint32_t blMemoryListASize;
extern const BL_MemoryInfoType blMemoryListB[];
extern const uint32_t blMemoryListBSize;
/* ================================ [ DATAS     ] ============================================== */
uint32_t blAppMemoryLow;
uint32_t blAppMemoryHigh;
uint32_t blFingerPrintAddr;
uint32_t blAppValidFlagAddr;
#ifdef BL_USE_APP_INFO
uint32_t blAppInfoAddr;
#endif
#ifdef BL_USE_META
uint32_t blAppMetaAddr;
uint32_t blAppMetaBackupAddr;
#endif
const BL_MemoryInfoType *blMemoryList;
uint32_t blMemoryListSize;

const RoD_ConfigType *RoD_AppConfig;

static char blActivePartition;

#ifdef BL_USE_AB_UPDATE_ACTIVE
/* With this to allow only once try to update current active partition */
static boolean blUpdateActive = FALSE;
#endif

static uint32_t blCurRollingCouner;
/* ================================ [ LOCALS    ] ============================================== */
static void BL_ABSetupEnv(void) {
  if ('A' == blActivePartition) {
    blAppMemoryLow = blAppMemoryLowA;
    blAppMemoryHigh = blAppMemoryHighA;
    blFingerPrintAddr = blFingerPrintAddrA;
    blAppValidFlagAddr = blAppValidFlagAddrA;
#ifdef BL_USE_APP_INFO
    blAppInfoAddr = blAppInfoAddrA;
#endif
    blMemoryList = blMemoryListA;
    blMemoryListSize = blMemoryListASize;
#ifdef BL_USE_META
    blAppMetaAddr = blAppMetaAddrA;
    blAppMetaBackupAddr = blAppMetaBackupAddrA;
#endif
    RoD_AppConfig = RoD_AppConfigA;
  } else {
    blAppMemoryLow = blAppMemoryLowB;
    blAppMemoryHigh = blAppMemoryHighB;
    blFingerPrintAddr = blFingerPrintAddrB;
    blAppValidFlagAddr = blAppValidFlagAddrB;
#ifdef BL_USE_APP_INFO
    blAppInfoAddr = blAppInfoAddrB;
#endif
    blMemoryList = blMemoryListB;
    blMemoryListSize = blMemoryListBSize;
#ifdef BL_USE_META
    blAppMetaAddr = blAppMetaAddrB;
    blAppMetaBackupAddr = blAppMetaBackupAddrB;
#endif
    RoD_AppConfig = RoD_AppConfigB;
  }
}

#ifdef BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER
static void BL_ABCheckActive(void) {
  BL_MetaType metaA;
  BL_MetaType metaB;
  bl_crc_t crc;

  BL_FLS_READ(blAppMetaAddrA, &metaA, sizeof(BL_MetaType));
  if (kFlashOk != blFlashParam.errorcode) {
    ASLOG(BLE, ("metaA read failed: %d\n", blFlashParam.errorcode));
    metaA.m.rollingCounter = 0;
  } else {
    crc = BL_CalculateCRC((const uint8_t *)&metaA.m, sizeof(BL_Meta_t), BL_CRC_START_VALUE, TRUE);
    if ((BL_META_MAGIC == metaA.m.magic) && (crc == metaA.crc) && (metaA.crc == (~metaA.crcInv))) {
      /* metaA valid, use it */
    } else {
      metaA.m.rollingCounter = 0;
    }
  }

  BL_FLS_READ(blAppMetaAddrB, &metaB, sizeof(BL_MetaType));
  if (kFlashOk != blFlashParam.errorcode) {
    ASLOG(BLE, ("metaB read failed: %d\n", blFlashParam.errorcode));
    metaB.m.rollingCounter = 0;
  } else {
    crc = BL_CalculateCRC((const uint8_t *)&metaB.m, sizeof(BL_Meta_t), BL_CRC_START_VALUE, TRUE);
    if ((BL_META_MAGIC == metaB.m.magic) && (crc == metaB.crc) && (metaB.crc == (~metaB.crcInv))) {
      /* metaB valid, use it */
    } else {
      metaB.m.rollingCounter = 0;
    }
  }

  if (metaB.m.rollingCounter > metaA.m.rollingCounter) {
    blActivePartition = 'B';
    blCurRollingCouner = metaB.m.rollingCounter;
  } else {
    blActivePartition = 'A';
    blCurRollingCouner = metaA.m.rollingCounter;
  }
}

#else
static void BL_ABCheckActive(void) {
  Std_ReturnType ret;
  uint16_t size = 0;
  const RoD_AppVersionType *pVersionA = NULL;
  const RoD_AppVersionType *pVersionB = NULL;

  ret = Rod_ReadData(RoD_AppConfigB, ROD_NUMBER_AppVersion, (const void **)&pVersionB, &size);
  if ((E_OK == ret) && (NULL != pVersionB) && (sizeof(RoD_AppVersionType) == size)) {
    /* current version B is valid */
    ret = Rod_ReadData(RoD_AppConfigA, ROD_NUMBER_AppVersion, (const void **)&pVersionA, &size);
    if ((E_OK == ret) && (NULL != pVersionA) && (sizeof(RoD_AppVersionType) == size)) {
      /* current version A valid */
      if ((pVersionA->year < pVersionB->year)) {
        blActivePartition = 'B'; /* A is old version */
      } else if ((pVersionA->year == pVersionB->year) && (pVersionA->month < pVersionB->month)) {
        blActivePartition = 'B';
      } else if ((pVersionA->year == pVersionB->year) && (pVersionA->month == pVersionB->month) &&
                 (pVersionA->day < pVersionB->day)) {
        blActivePartition = 'B';
      } else if ((pVersionA->year == pVersionB->year) && (pVersionA->month == pVersionB->month) &&
                 (pVersionA->day == pVersionB->day) && (pVersionA->hour < pVersionB->hour)) {
        blActivePartition = 'B';
      } else if ((pVersionA->year == pVersionB->year) && (pVersionA->month == pVersionB->month) &&
                 (pVersionA->day == pVersionB->day) && (pVersionA->hour == pVersionB->hour) &&
                 (pVersionA->minute < pVersionB->minute)) {
        blActivePartition = 'B';
      } else if ((pVersionA->year == pVersionB->year) && (pVersionA->month == pVersionB->month) &&
                 (pVersionA->day == pVersionB->day) && (pVersionA->hour == pVersionB->hour) &&
                 (pVersionA->minute == pVersionB->minute) &&
                 (pVersionA->second < pVersionB->second)) {
        blActivePartition = 'B';
      } else {
        /* B is old, use A as active */
      }
    } else { /* A is corrupted */
      blActivePartition = 'B';
    }
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_ABInit(void) {
  /* Init, get Active section */
  blActivePartition = 'A';
#ifdef BL_USE_AB_UPDATE_ACTIVE
  blUpdateActive = FALSE;
#endif

  BL_ABCheckActive();

  ASLOG(BLI, ("Partition %c is active\n", blActivePartition));
  BL_ABSetupEnv();
}

void BL_ABSwitch(void) {
  if ('A' == blActivePartition) {
    blActivePartition = 'B';
  } else {
    blActivePartition = 'A';
  }
  ASLOG(BLI, ("Going to update Partition %c\n", blActivePartition));
  BL_ABSetupEnv();
}

void BL_ABSetActivePartition(char activePartition) {
  blActivePartition = activePartition;
  BL_ABSetupEnv();
}

#ifdef BL_USE_AB_UPDATE_ACTIVE
boolean BL_ABPrepareUpdateActive(void) {
  int rv = FALSE;
  if (FALSE == blUpdateActive) {
    blUpdateActive = TRUE;
    BL_ABSwitch();
    rv = TRUE;
  }
  return rv;
}
#endif

uint32_t BL_MetaGetRollingCouner(void) {
  return blCurRollingCouner;
}
#endif /* BL_USE_AB */
