/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#include "loader.h"
#include "loader/common.h"
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
/* ================================ [ MACROS    ] ============================================== */
#define TO_BCD(v) (uint8_t)(((((v % 100)) / 10) << 4) | (v % 10))

#define TO_C(c) (char)(isprint(c) ? c : '.')

#define FL_ALIGN(length, sz) (((length + sz - 1) / sz) * sz)

#define ERASE_FLASH_METHO 2
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static uint32_t FLASH_SECTOR_SIZE = 2048;
/* ================================ [ LOCALS    ] ============================================== */
INITIALIZER(_fbl_start) {
  char *str = getenv("FLASH_SECTOR_SIZE");
  if (str != NULL) {
    FLASH_SECTOR_SIZE = strtoul(str, NULL, 10);
  }
}

int check_programming_preconditions(loader_t *loader) {
  static const uint8_t data[] = {0x31, 0x01, 0x02, 0x03};
  static const int expected[] = {0x71, 0x01, 0x02, 0x03, 0x00};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}
#if 0 == ERASE_FLASH_METHO
static int routine_erase_flash(loader_t *loader) {
  static const int expected[] = {0x71, 0x01, 0xFF, 0x00};
  int r;

  data[0] = 0x31;
  data[1] = 0x01;
  data[2] = 0xFF;
  data[3] = 0x00;

  r = uds_request_service(loader, data, 4, expected, ARRAY_SIZE(expected));

  return r;
}
#elif 1 == ERASE_FLASH_METHO
static int routine_erase_flash(loader_t *loader) {
  uint8_t data[13];
  static const int expected[] = {0x71, 0x01, 0xFF, 0x00, 0x00};
  int r;
  srec_t *appSRec = loader_get_app_srec(loader);
  sblk_t *blk;
  size_t i;
  uint32_t start = UINT32_MAX;
  uint32_t end = 0;
  uint32_t size = 0;

  data[0] = 0x31;
  data[1] = 0x01;
  data[2] = 0xFF;
  data[3] = 0x00;
  data[4] = 0x44; /* addressAndLengthFormatIdentifier */

  for (i = 0; i < appSRec->numOfBlks; i++) {
    blk = &appSRec->blks[i];
    if (start > blk->address) {
      start = blk->address;
    }
    if ((blk->address + blk->length) > end) {
      end = blk->address + blk->length;
    }
  }
  size = FL_ALIGN(end - start, FLASH_SECTOR_SIZE);

  data[5] = (uint8_t)((start >> 24) & 0xFF);
  data[6] = (uint8_t)((start >> 16) & 0xFF);
  data[7] = (uint8_t)((start >> 8) & 0xFF);
  data[8] = (uint8_t)(start & 0xFF);

  data[9] = (uint8_t)((size >> 24) & 0xFF);
  data[10] = (uint8_t)((size >> 16) & 0xFF);
  data[11] = (uint8_t)((size >> 8) & 0xFF);
  data[12] = (uint8_t)(size & 0xFF);

  r = uds_request_service(loader, data, 13, expected, ARRAY_SIZE(expected));

  return r;
}
#elif 2 == ERASE_FLASH_METHO
static int routine_erase_flash(loader_t *loader) {
  uint8_t data[13];
  static const int expected[] = {0x71, 0x01, 0xFF, 0x00, 0x00};
  int r = L_R_OK;
  srec_t *appSRec = loader_get_app_srec(loader);
  sblk_t *blk;
  size_t i;
  uint32_t start = UINT32_MAX;
  uint32_t end = 0;
  uint32_t size = 0;

  data[0] = 0x31;
  data[1] = 0x01;
  data[2] = 0xFF;
  data[3] = 0x00;
  data[4] = 0x44; /* addressAndLengthFormatIdentifier */

  LDLOG(DEBUG, "Loader: FLASH_SECTOR_SIZE=%u\n", FLASH_SECTOR_SIZE);

  for (i = 0; (i < appSRec->numOfBlks) && (L_R_OK == r); i++) {
    blk = &appSRec->blks[i];
    start = (blk->address / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    end = blk->address + blk->length;
    size = FL_ALIGN(end - start, FLASH_SECTOR_SIZE);
    data[5] = (uint8_t)((start >> 24) & 0xFF);
    data[6] = (uint8_t)((start >> 16) & 0xFF);
    data[7] = (uint8_t)((start >> 8) & 0xFF);
    data[8] = (uint8_t)(start & 0xFF);

    data[9] = (uint8_t)((size >> 24) & 0xFF);
    data[10] = (uint8_t)((size >> 16) & 0xFF);
    data[11] = (uint8_t)((size >> 8) & 0xFF);
    data[12] = (uint8_t)(size & 0xFF);

    LDLOG(INFO, "\n\terase block 0x%x - 0x%x\n", start, size);
    r = uds_request_service(loader, data, 13, expected, ARRAY_SIZE(expected));
  }

  return r;
}
#endif

static int routine_erase_fee(loader_t *loader) {
  uint8_t data[13];
  static const int expected[] = {0x71, 0x01, 0xFF, 0x00, 0x00};
  int r = L_R_OK;
  uint32_t start = 0;
  uint32_t size = 0;

  char *str = getenv("FEE_BANK_ADDRESS");
  if (str != NULL) {
    start = strtoul(str, NULL, 16);
  } else {
    LDLOG(INFO, "\n\tskip fee erase as no env FEE_BANK_ADDRESS\n");
    return L_R_OK;
  }

  str = getenv("FEE_BANK_SIZE");
  if (str != NULL) {
    size = strtoul(str, NULL, 16);
  } else {
    LDLOG(INFO, "\n\tskip fee erase as no env FEE_BANK_SIZE\n");
    return L_R_OK;
  }

  data[0] = 0x31;
  data[1] = 0x01;
  data[2] = 0xFF;
  data[3] = 0x00;
  data[4] = 0x44; /* addressAndLengthFormatIdentifier */

  data[5] = (uint8_t)((start >> 24) & 0xFF);
  data[6] = (uint8_t)((start >> 16) & 0xFF);
  data[7] = (uint8_t)((start >> 8) & 0xFF);
  data[8] = (uint8_t)(start & 0xFF);

  data[9] = (uint8_t)((size >> 24) & 0xFF);
  data[10] = (uint8_t)((size >> 16) & 0xFF);
  data[11] = (uint8_t)((size >> 8) & 0xFF);
  data[12] = (uint8_t)(size & 0xFF);

  LDLOG(INFO, "\n\terase block 0x%x - 0x%x\n", start, size);
  r = uds_request_service(loader, data, 13, expected, ARRAY_SIZE(expected));

  return r;
}

static int check_flash_driver_integrity(loader_t *loader) {
  static uint8_t data[8] = {0x31, 0x01, 0x02, 0x02};
  static const int expected[] = {0x71, 0x01, 0x02, 0x02, 0x00};
  int r = L_R_OK;
  loader_crc_t Crc = loader_crc_init(loader);
  sblk_t *blk;
  uint32_t lenCrc;
  srec_t *flsSRec = loader_get_flsdrv_srec(loader);
  size_t i;

  if (NULL != flsSRec) {
    LDLOG(INFO, "check flash driver integrity");
    for (i = 0; i < flsSRec->numOfBlks; i++) {
      blk = &flsSRec->blks[i];
      Crc = loader_calulate_crc(loader, blk->data, blk->length, Crc, FALSE);
    }

    lenCrc = loader_set_crc(loader, &data[4], Crc);
    r = uds_request_service(loader, data, 4 + lenCrc, expected, ARRAY_SIZE(expected));
    if (L_R_OK == r) {
      LDLOG(INFO, " okay\n");
    }
  }

  return r;
}

static int check_program_integrity(loader_t *loader) {
  static uint8_t data[8] = {0x31, 0x01, 0x02, 0x02};
  static const int expected[] = {0x71, 0x01, 0x02, 0x02, 0x00};
  int r;
  loader_crc_t Crc = loader_crc_init(loader);
  sblk_t *blk;
  size_t i;
  srec_t *appSRec = loader_get_app_srec(loader);
  uint32_t lenCrc;

  for (i = 0; i < appSRec->numOfBlks; i++) {
    blk = &appSRec->blks[i];
    Crc = loader_calulate_crc(loader, blk->data, blk->length, Crc, FALSE);
  }

  lenCrc = loader_set_crc(loader, &data[4], Crc);
  r = uds_request_service(loader, data, 4 + lenCrc, expected, ARRAY_SIZE(expected));

  return r;
}

static int check_program_dependencies(loader_t *loader) {
  static const uint8_t data[] = {0x31, 0x01, 0xFF, 0x01};
  static const int expected[] = {0x71, 0x01, 0xFF, 0x01, 0x00};
  return uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

static int read_finger_print(loader_t *loader) {
  static const uint8_t data[3] = {0x22, 0xF1, 0x5B};
  static const int expected[] = {0x62, 0xF1, 0x5B, 0, // block id
                                 -1,   -1,   -1,   -1, -1, -1, -1, -1, -1};
  int r;
  uint8_t *res = loader_get_response(loader);

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  if (L_R_OK == r) {
    LDLOG(INFO, "\n  date: %02X-%02X-%02X\n", res[4], res[5], res[6]);
    LDLOG(INFO, "  serial = %02X%02X%02X%02X%02X%02X %c%c%c%c%c%c\n", // format
          res[7], res[8], res[9], res[10], res[11], res[12],          // serail
          TO_C(res[7]), TO_C(res[8]), TO_C(res[9]), TO_C(res[10]), TO_C(res[11]), TO_C(res[12]));
  }

  return r;
}

static int write_finger_print(loader_t *loader) {
  static uint8_t data[3 + 9] = {0x2E, 0xF1, 0x5A};
  static const int expected[] = {0x6E, 0xF1, 0x5A};
  int r;

  uint32_t year = 2023;
  uint8_t month = 2;
  uint8_t day = 24;
  const char serial[6] = "#byas#";

  time_t t = time(0);
  struct tm *lt = localtime(&t);
  year = (1900 + lt->tm_year);
  month = lt->tm_mon + 1;
  day = lt->tm_mday;

  data[3] = TO_BCD(year);
  data[4] = TO_BCD(month);
  data[5] = TO_BCD(day);
  memcpy(&data[6], serial, sizeof(serial));

  LDLOG(INFO, "\n  date: %02X-%02X-%02X\n", data[3], data[4], data[5]);
  LDLOG(INFO, "  serial = %02X%02X%02X%02X%02X%02X %c%c%c%c%c%c\n", // format
        data[6], data[7], data[8], data[9], data[10], data[11],     // serail
        TO_C(data[6]), TO_C(data[7]), TO_C(data[8]), TO_C(data[9]), TO_C(data[10]), TO_C(data[11]));
  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static int routine_check_integrity(loader_t *loader) {
  static const uint8_t data[4] = {0x31, 0x01, 0xFF, 0x02};
  static const int expected[] = {0x71, 0x01, 0xFF, 0x02};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static const loader_service_t lStdServices[] = {
  {"enter extended session", " okay\n", enter_extend_session, 100},
  {"control dtc setting off", " okay\n", control_dtc_setting_off, 100},
  {"communication disable", " okay\n", communicaiton_disable, 100},
  {"level 1 security access", " okay\n", security_extds_access, 100},
  // {"check programming preconditions", " okay\n", check_programming_preconditions, 100},
  {"enter program session", " okay\n", enter_program_session, 100},
  {"level 2 security access", " okay\n", security_prgs_access, 100},
  {NULL, NULL, download_flash_driver, 500},
  {NULL, NULL, check_flash_driver_integrity, 100},
  {"read finger print", " okay\n", read_finger_print, 100},
  {"erase flash", " okay\n", routine_erase_flash, 500},
  {"erase fee", " okay\n", routine_erase_fee, 100},
  {"write finger print", " okay\n", write_finger_print, 100},
  {"download application", " okay\n", download_application, 100},
  {"check program integrity", " okay\n", check_program_integrity, 100},
  {"check dependencies", " okay\n", check_program_dependencies, 100},
  {"check integrity", " okay\n", routine_check_integrity, 100},
  {"ecu reset", " okay\n", ecu_reset, 100},
  {"communication enable", " okay\n", communicaiton_enable, 100},
  {"control dtc setting on", " okay\n", control_dtc_setting_on, 100},
};

/* ================================ [ FUNCTIONS ] ============================================== */
LOADER_APP_REGISTER(lStdServices, FBL)
