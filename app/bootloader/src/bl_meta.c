/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "bl.h"
#ifdef BL_USE_META
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const uint32_t blAppMetaAddr;
extern const uint32_t blAppMetaBackupAddr;
/* ================================ [ DATAS     ] ============================================== */
static uint32_t blCurProgrammingCouner = UINT32_MAX;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_MetaInit(void) {
  blCurProgrammingCouner = UINT32_MAX;
}

Std_ReturnType BL_MetaBackup(void) {
  Std_ReturnType ret = E_OK;
  BL_MetaType meta;
  uint8_t data[FLASH_ALIGNED_WRITE_SIZE(sizeof(meta))];
  bl_crc_t crc;

  BL_FLS_READ(blAppMetaAddr, &meta, sizeof(BL_MetaType));
  if (kFlashOk != blFlashParam.errorcode) {
    ASLOG(BLE, ("meta read failed: %d\n", blFlashParam.errorcode));
    ret = E_NOT_OK;
  } else {
    crc = BL_CalculateCRC((const uint8_t *)&meta.m, sizeof(BL_Meta_t), BL_CRC_START_VALUE, TRUE);
    if ((BL_META_MAGIC == meta.m.magic) && (crc == meta.crc) && (meta.crc == (~meta.crcInv))) {
      /* meta valid, back up */
      ASLOG(BLI, ("programming Counter: %" PRIu32 "\n", meta.m.programmingCounter));
      blCurProgrammingCouner = meta.m.programmingCounter;
      memcpy(data, &meta, sizeof(meta));
      if (sizeof(data) > sizeof(meta)) {
        memset(&data[sizeof(meta)], 0xFF, sizeof(data) - sizeof(meta));
      }
      BL_FLS_INIT();
      BL_FLS_ERASE(blAppMetaBackupAddr, FLASH_ERASE_SIZE);
      if (kFlashOk == blFlashParam.errorcode) {
        BL_FLS_WRITE(blAppMetaBackupAddr, &meta, sizeof(data));
        if (kFlashOk != blFlashParam.errorcode) {
          ASLOG(BLE, ("backup meta write failed: %d\n", blFlashParam.errorcode));
          ret = E_NOT_OK;
        }
      } else {
        ASLOG(BLE, ("erase backup meta failed: %d\n", blFlashParam.errorcode));
        ret = E_NOT_OK;
      }
    }
  }

  return ret;
}

Std_ReturnType BL_MetaUpdate(void) {
  Std_ReturnType ret = E_OK;
  BL_MetaType meta;
  uint8_t data[FLASH_ALIGNED_WRITE_SIZE(sizeof(meta))];
  bl_crc_t crc;

  BL_FLS_READ(blAppMetaBackupAddr, &meta, sizeof(BL_MetaType));
  if (kFlashOk != blFlashParam.errorcode) {
    ASLOG(BLE, ("backup meta read failed: %d\n", blFlashParam.errorcode));
    ret = E_NOT_OK;
  } else {
    crc = BL_CalculateCRC((const uint8_t *)&meta.m, sizeof(BL_Meta_t), BL_CRC_START_VALUE, TRUE);
    if ((BL_META_MAGIC == meta.m.magic) && (crc == meta.crc) && (meta.crc == (~meta.crcInv))) {
      /* backup meta valid, update it */
      if (meta.m.programmingCounter < UINT32_MAX) {
        meta.m.programmingCounter++;
      }
#if defined(BL_USE_AB) && defined(BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER)
      meta.m.rollingCounter = BL_MetaGetRollingCouner();
      if (meta.m.rollingCounter < UINT32_MAX) {
        meta.m.rollingCounter++;
      }
#endif
    } else { /* first time update */
      meta.m.magic = BL_META_MAGIC;
      meta.m.programmingCounter = 1;
#if defined(BL_USE_AB) && defined(BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER)
      meta.m.rollingCounter = BL_MetaGetRollingCouner();
      if (meta.m.rollingCounter < UINT32_MAX) {
        meta.m.rollingCounter++;
      }
#endif
      ASLOG(BLI, ("First Time or Backup Meta corrupted\n"));
    }
    ASLOG(BLI, ("programming Counter: %" PRIu32 "\n", meta.m.programmingCounter));
#if defined(BL_USE_AB) && defined(BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER)
    ASLOG(BLI, ("Rolling Couner: %" PRIu32 "\n", meta.m.rollingCounter));
#endif
    blCurProgrammingCouner = meta.m.programmingCounter;
    meta.crc =
      BL_CalculateCRC((const uint8_t *)&meta.m, sizeof(BL_Meta_t), BL_CRC_START_VALUE, TRUE);
    meta.crcInv = ~meta.crc;
    memcpy(data, &meta, sizeof(meta));
    if (sizeof(data) > sizeof(meta)) {
      memset(&data[sizeof(meta)], 0xFF, sizeof(data) - sizeof(meta));
    }
    BL_FLS_WRITE(blAppMetaAddr, &meta, sizeof(data));
    if (kFlashOk != blFlashParam.errorcode) {
      ASLOG(BLE, ("update meta write failed: %d\n", blFlashParam.errorcode));
      ret = E_NOT_OK;
    }
  }

  return ret;
}

uint32_t BL_MetaGetProgrammingCouner(void) {
  BL_MetaType meta;
  bl_crc_t crc;

  if (UINT32_MAX == blCurProgrammingCouner) {
    blCurProgrammingCouner = 0; /* assume start from 0 if can't find one */
    BL_FLS_READ(blAppMetaAddr, &meta, sizeof(BL_MetaType));
    if (kFlashOk != blFlashParam.errorcode) {
      ASLOG(BLE, ("meta main read failed: %d\n", blFlashParam.errorcode));
    } else {
      crc = BL_CalculateCRC((const uint8_t *)&meta.m, sizeof(BL_Meta_t), BL_CRC_START_VALUE, TRUE);
      if ((BL_META_MAGIC == meta.m.magic) && (crc == meta.crc) && (meta.crc == (~meta.crcInv))) {
        blCurProgrammingCouner = meta.m.programmingCounter;
      } else {
        BL_FLS_READ(blAppMetaBackupAddr, &meta, sizeof(BL_MetaType));
        if (kFlashOk != blFlashParam.errorcode) {
          ASLOG(BLE, ("meta backup read failed: %d\n", blFlashParam.errorcode));
        } else {
          crc =
            BL_CalculateCRC((const uint8_t *)&meta.m, sizeof(BL_Meta_t), BL_CRC_START_VALUE, TRUE);
          if ((BL_META_MAGIC == meta.m.magic) && (crc == meta.crc) &&
              (meta.crc == (~meta.crcInv))) {
            blCurProgrammingCouner = meta.m.programmingCounter;
          }
        }
      }
    }
  }

  return blCurProgrammingCouner;
}
#endif /* BL_USE_META */
