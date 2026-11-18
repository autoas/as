/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "./common.h"
#include <string.h>
#include <chrono>
/* ================================ [ MACROS    ] ============================================== */
#define FL_MIN_ABILITY 128
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int Loader_EnterExtendSession(Loader_Type *loader) {
  static const uint8_t data[] = {0x10, 0x03 | SUPPRESS_POS_RESP_BIT};
  static const int expected[] = {0x50, 0x03, -1, -1, -1, -1};
  return Loader_BroadcastService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_ControlDtcSettingOff(Loader_Type *loader) {
  static const uint8_t data[] = {0x85, 0x02 | SUPPRESS_POS_RESP_BIT};
  static const int expected[] = {0xC5, 0x02};
  return Loader_BroadcastService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_ControlDtcSettingOn(Loader_Type *loader) {
  static const uint8_t data[] = {0x85, 0x01 | SUPPRESS_POS_RESP_BIT};
  static const int expected[] = {0xC5, 0x01};
  return Loader_BroadcastService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_CommunicationDisable(Loader_Type *loader) {
  static const uint8_t data[] = {0x28, 0x03 | SUPPRESS_POS_RESP_BIT, 0x03};
  static const int expected[] = {0x68, 0x03};
  return Loader_BroadcastService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_CommunicationEnable(Loader_Type *loader) {
  static const uint8_t data[] = {0x28, 0x00 | SUPPRESS_POS_RESP_BIT, 0x03};
  static const int expected[] = {0x68, 0x00};
  return Loader_BroadcastService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_EnterProgramSession(Loader_Type *loader) {
  static const uint8_t data[] = {0x10, 0x02};
  static const int expected[] = {0x50, 0x02, -1, -1, -1, -1};
  return Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_RequestDownload(Loader_Type *loader, uint32_t address, size_t length) {
  int r;
  uint8_t data[11];
  static const int expected[] = {0x74};
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

  r = Loader_RequestService(loader, data, 11, expected, ARRAY_SIZE(expected));

  return r;
}

int Loader_TransferData(Loader_Type *loader, uint32_t ability, uint8_t *data, size_t length) {
  int r = L_R_OK;
  uint8_t blockSequenceCounter = 1;
  size_t leftSize = length;
  size_t doSz;
  size_t curPos = 0;
  int expected[] = {0x76, 0};
  uint8_t *request = Loader_GetRequest(loader);

  while ((leftSize > 0) && (L_R_OK == r) && (FALSE == Loader_IsStopped(loader))) {
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

    r = Loader_RequestService(loader, request, 2 + doSz, expected, ARRAY_SIZE(expected));

    if (L_R_OK == r) {
      Loader_AddProgress(loader, doSz);
    }

    blockSequenceCounter = (blockSequenceCounter + 1) & 0xFF;
    curPos += doSz;
  }

  return r;
}

int Loader_TransferExit(Loader_Type *loader) {
  static const uint8_t data[] = {0x37};
  static const int expected[] = {0x77};
  return Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_DownloadOneSection(Loader_Type *loader, Srec_BlockType *blk) {
  int r;
  uint32_t ability = LOADER_MSG_SIZE;
  uint8_t *response = Loader_GetResponse(loader);
  uint32_t lengthFormatIdentifier;

  r = Loader_RequestDownload(loader, blk->Address, blk->Length);

  if (L_R_OK == r) {
    lengthFormatIdentifier = response[1] >> 4u;
    ability = 0;
    for (uint32_t i = 0; i < lengthFormatIdentifier; i++) {
      ability = (ability << 8) + response[2 + i];
    }
    if ((ability >= FL_MIN_ABILITY) && ((ability + 2) < LOADER_MSG_SIZE)) {
      LDLOG(DEBUG, "\n  ability %" PRIu32 "\n", ability);
      r = Loader_TransferData(loader, ability - 2, blk->Data, blk->Length);
    } else {
      LDLOG(ERROR, "server ability error %" PRIu32, ability);
      r = L_R_NOK;
    }
  }

  if (L_R_OK == r) {
    r = Loader_TransferExit(loader);
  }

  return r;
}

int Loader_DownloadApplication(Loader_Type *loader) {
  int r = L_R_OK;
  size_t i;
  Srec_FileType *appSRec = Loader_GetAppSrec(loader);
  auto begin = std::chrono::high_resolution_clock::now();
  for (i = 0; (L_R_OK == r) && (i < appSRec->NumOfBlks); i++) {
    r = Loader_DownloadOneSection(loader, &appSRec->Blks[i]);
  }
  float cost = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::high_resolution_clock::now() - begin)
                 .count() /
               1000.f;
  float speed = appSRec->TotalSize / 1024.0 / cost;
  LDLOG(INFO, "\n\taverage speed %.2f kbps cost %.2f seconds\n", speed, cost);
  return r;
}

int Loader_DownloadFlashDriver(Loader_Type *loader) {
  int r = L_R_OK;
  size_t i;
  Srec_FileType *flsSRec = Loader_GetFlsDrvSrec(loader);

  if (NULL != flsSRec) {
    LDLOG(INFO, "download flash driver");
    for (i = 0; (L_R_OK == r) && (i < flsSRec->NumOfBlks); i++) {
      r = Loader_DownloadOneSection(loader, &flsSRec->Blks[i]);
    }
    if (L_R_OK == r) {
      LDLOG(INFO, " okay\n");
    }
  }

  return r;
}

int Loader_EcuReset(Loader_Type *loader) {
  static const uint8_t data[] = {0x11, 0x03};
  static const int expected[] = {0x51, 0x03};
  return Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

int Loader_SecurityExtdsAccess(Loader_Type *loader) {
  static const uint8_t data[] = {0x27, 0x01};
  uint8_t data2[6];
  static const int expected1[] = {0x67, 0x01, -1, -1, -1, -1};
  static const int expected2[] = {0x67, 0x02};
  uint32_t seed, key;
  int r;
  uint8_t *response = Loader_GetResponse(loader);

  r = Loader_RequestService(loader, data, sizeof(data), expected1, ARRAY_SIZE(expected1));
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
    r = Loader_RequestService(loader, data2, 6, expected2, ARRAY_SIZE(expected2));
  }

  return r;
}

int Loader_SecurityPrgsAccess(Loader_Type *loader) {
  static const uint8_t data[] = {0x27, 0x03};
  uint8_t data2[6];
  static const int expected1[] = {0x67, 0x03, -1, -1, -1, -1};
  static const int expected2[] = {0x67, 0x04};
  uint32_t seed, key;
  int r;
  uint8_t *response = Loader_GetResponse(loader);

  r = Loader_RequestService(loader, data, sizeof(data), expected1, ARRAY_SIZE(expected1));
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
    r = Loader_RequestService(loader, data2, 6, expected2, ARRAY_SIZE(expected2));
  }

  return r;
}
