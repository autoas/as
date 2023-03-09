/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "./common.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define FL_MIN_ABILITY 128
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int enter_extend_session(loader_t *loader) {
  static const uint8_t data[] = {0x10, 0x03};
  static const int expected[] = {0x50, 0x03, -1, -1, -1, -1};
  return uds_broadcast_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int control_dtc_setting_off(loader_t *loader) {
  static const uint8_t data[] = {0x85, 0x02};
  static const int expected[] = {0xC5, 0x02};
  return uds_broadcast_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int control_dtc_setting_on(loader_t *loader) {
  static const uint8_t data[] = {0x85, 0x01};
  static const int expected[] = {0xC5, 0x01};
  return uds_broadcast_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int communicaiton_disable(loader_t *loader) {
  static const uint8_t data[] = {0x28, 0x03, 0x03};
  static const int expected[] = {0x68, 0x03};
  return uds_broadcast_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int communicaiton_enable(loader_t *loader) {
  static const uint8_t data[] = {0x28, 0x00, 0x03};
  static const int expected[] = {0x68, 0x00};
  return uds_broadcast_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int enter_program_session(loader_t *loader) {
  static const uint8_t data[] = {0x10, 0x02};
  static const int expected[] = {0x50, 0x02, -1, -1, -1, -1};
  return uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int request_download(loader_t *loader, uint32_t address, size_t length) {
  int r;
  uint8_t data[11];
  static const int expected[] = {0x74, 0x20};
  data[0] = 0x34;
  data[1] = 0x00;
  data[2] = 0x44;

  data[3] = (address >> 24) & 0xFF;
  data[4] = (address >> 16) & 0xFF;
  data[5] = (address >> 8) & 0xFF;
  data[6] = address & 0xFF;

  data[7] = (length >> 24) & 0xFF;
  data[8] = (length >> 16) & 0xFF;
  data[9] = (length >> 8) & 0xFF;
  data[10] = length & 0xFF;

  r = uds_request_service(loader, data, 11, expected, ARRAY_SIZE(expected));

  return r;
}

int transfer_data(loader_t *loader, uint32_t ability, uint8_t *data, size_t length) {
  int r = L_R_OK;
  uint8_t blockSequenceCounter = 1;
  size_t leftSize = length;
  size_t doSz;
  size_t curPos = 0;
  int expected[] = {0x76, 0};
  uint8_t *request = loader_get_request(loader);

  while ((leftSize > 0) && (L_R_OK == r) && (FALSE == loader_is_stopt(loader))) {
    request[0] = 0x36;
    request[1] = blockSequenceCounter;
    expected[1] = blockSequenceCounter;

    if (leftSize > ability) {
      doSz = ability;
      leftSize = leftSize - ability;
    } else {
      doSz = leftSize;
      leftSize = 0;
    }

    memcpy(&request[2], &data[curPos], doSz);

    r = uds_request_service(loader, request, 2 + doSz, expected, ARRAY_SIZE(expected));

    if (L_R_OK == r) {
      loader_add_progress(loader, 0, doSz);
    }

    blockSequenceCounter = (blockSequenceCounter + 1) & 0xFF;
    curPos += doSz;
  }

  return r;
}

int transfer_exit(loader_t *loader) {
  static const uint8_t data[] = {0x37};
  static const int expected[] = {0x77};
  return uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int download_one_section(loader_t *loader, sblk_t *blk) {
  int r;
  uint32_t ability = LOADER_MSG_SIZE;
  uint8_t *response = loader_get_response(loader);

  r = request_download(loader, blk->address, blk->length);

  if (L_R_OK == r) {
    ability = ((uint32_t)response[2] << 8) + response[3];
    if ((ability >= FL_MIN_ABILITY) && ((ability + 2) < LOADER_MSG_SIZE)) {
      r = transfer_data(loader, ability - 2, blk->data, blk->length);
    } else {
      LDLOG(DEBUG, "server ability error %d", ability);
      r = L_R_NOK;
    }
  }

  if (L_R_OK == r) {
    r = transfer_exit(loader);
  }

  return r;
}

int download_application(loader_t *loader) {
  int r = L_R_OK;
  size_t i;
  srec_t *appSRec = loader_get_app_srec(loader);

  for (i = 0; (L_R_OK == r) && (i < appSRec->numOfBlks); i++) {
    r = download_one_section(loader, &appSRec->blks[i]);
  }

  return r;
}

int download_flash_driver(loader_t *loader) {
  int r = L_R_OK;
  size_t i;
  srec_t *flsSRec = loader_get_flsdrv_srec(loader);

  if (NULL != flsSRec) {
    LDLOG(INFO, "download flash driver");
    for (i = 0; (L_R_OK == r) && (i < flsSRec->numOfBlks); i++) {
      r = download_one_section(loader, &flsSRec->blks[i]);
    }
    if (L_R_OK == r) {
      LDLOG(INFO, " okay\n");
    }
  }

  return r;
}

int ecu_reset(loader_t *loader) {
  static const uint8_t data[] = {0x11, 0x03};
  static const int expected[] = {0x51, 0x03};
  return uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int security_extds_access(loader_t *loader) {
  static const uint8_t data[] = {0x27, 0x01};
  uint8_t data2[6];
  static const int expected1[] = {0x67, 0x01, -1, -1, -1, -1};
  static const int expected2[] = {0x67, 0x02};
  uint32_t seed, key;
  int r;
  uint8_t *response = loader_get_response(loader);

  r = uds_request_service(loader, data, sizeof(data), expected1, ARRAY_SIZE(expected1));
  if (L_R_OK == r) {
    seed = ((uint32_t)response[2] << 24) + ((uint32_t)response[3] << 16) +
           ((uint32_t)response[4] << 8) + response[5];
    /* TODO: TBD */
    key = seed ^ 0x78934673;
    data2[0] = 0x27;
    data2[1] = 0x02;
    data2[2] = (key >> 24) & 0xFF;
    data2[3] = (key >> 16) & 0xFF;
    data2[4] = (key >> 8) & 0xFF;
    data2[5] = key & 0xFF;
    r = uds_request_service(loader, data2, 6, expected2, ARRAY_SIZE(expected2));
  }

  return r;
}

int security_prgs_access(loader_t *loader) {
  static const uint8_t data[] = {0x27, 0x03};
  uint8_t data2[6];
  static const int expected1[] = {0x67, 0x03, -1, -1, -1, -1};
  static const int expected2[] = {0x67, 0x04};
  uint32_t seed, key;
  int r;
  uint8_t *response = loader_get_response(loader);

  r = uds_request_service(loader, data, sizeof(data), expected1, ARRAY_SIZE(expected1));
  if (L_R_OK == r) {
    seed = ((uint32_t)response[2] << 24) + ((uint32_t)response[3] << 16) +
           ((uint32_t)response[4] << 8) + response[5];
    /* TODO: TBD */
    key = seed ^ 0x94586792;
    data2[0] = 0x27;
    data2[1] = 0x04;
    data2[2] = (key >> 24) & 0xFF;
    data2[3] = (key >> 16) & 0xFF;
    data2[4] = (key >> 8) & 0xFF;
    data2[5] = key & 0xFF;
    r = uds_request_service(loader, data2, 6, expected2, ARRAY_SIZE(expected2));
  }

  return r;
}