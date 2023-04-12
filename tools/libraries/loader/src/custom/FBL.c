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
/* ================================ [ MACROS    ] ============================================== */
#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE 512
#endif

#define TO_BCD(v) (uint8_t)(((((v % 100)) / 10) << 4) | (v % 10))

#define TO_C(c) (char)(isprint(c) ? c : '.')

#define FL_ALIGN(length, sz) (((length + sz - 1) / sz) * sz)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
int check_programming_preconditions(loader_t *loader) {
  static const uint8_t data[] = {0x31, 0x01, 0x02, 0x03};
  static const int expected[] = {0x71, 0x01, 0x02, 0x03, 0x00};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static int routine_erase_flash(loader_t *loader) {
  static const uint8_t data[] = {0x31, 0x01, 0xFF, 0x01};
  static const int expected[] = {0x71, 0x01, 0xFF, 0x01};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  if (L_R_OK == r) {
    loader_add_progress(loader, 500, 0);  /* step 5% */
  }

  return r;
}

static int read_finger_print(loader_t *loader) {
  static const uint8_t data[3] = {0x22, 0xFE, 0x5E};
  static const int expected[] = {0x62, 0xFE, 0x5E, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  int r;
  uint8_t *res = loader_get_response(loader);

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  if (L_R_OK == r) {
    LDLOG(INFO, "\n  date: %02X-%02X-%02X\n", res[3], res[4], res[5]);
    LDLOG(INFO, "  serial = %02X%02X%02X%02X%02X%02X %c%c%c%c%c%c\n", // format
          res[6], res[7], res[8], res[9], res[10], res[11],           // serail
          TO_C(res[6]), TO_C(res[7]), TO_C(res[8]), TO_C(res[9]), TO_C(res[10]), TO_C(res[11]));
  }

  return r;
}

static int write_finger_print(loader_t *loader) {
  static uint8_t data[3 + 9] = {0x2E, 0xFE, 0x5E};
  static const int expected[] = {0x6E, 0xFE, 0x5E};
  int r;

  uint32_t year;
  uint8_t month;
  uint8_t day;
  const char serial[6] = "#ssas#";

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

  if (L_R_OK == r) {
    loader_add_progress(loader, 500, 0); /* step 5% */
  }

  return r;
}

static const loader_service_t lStdServices[] = {
  {"enter extended session", " okay\n", enter_extend_session},
  // {"control dtc setting off", " okay\n", control_dtc_setting_off},
  // {"communication disable", " okay\n", communicaiton_disable},
  {"level 1 security access", " okay\n", security_extds_access},
  {"enter program session", " okay\n", enter_program_session},
  {"level 2 security access", " okay\n", security_prgs_access},
  {NULL, NULL, download_flash_driver},
  {"read finger print", " okay\n", read_finger_print},
  {"erase flash", " okay\n", routine_erase_flash},
  {"write finger print", " okay\n", write_finger_print},
  {"download application", " okay\n", download_application},
  {"check integrity", " okay\n", routine_check_integrity},
  {"ecu reset", " okay\n", ecu_reset},
  // {"communication enable", " okay\n", communicaiton_enable},
  // {"control dtc setting on", " okay\n", control_dtc_setting_on},
};

static const loader_app_t lStdApp = {
  "FBL",
  lStdServices,
  ARRAY_SIZE(lStdServices),
};
/* ================================ [ FUNCTIONS ] ============================================== */
LOADER_APP_REGISTER(lStdApp)
