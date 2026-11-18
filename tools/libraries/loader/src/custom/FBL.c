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
static uint32_t loaderFlashSectorSize = 2048;
/* ================================ [ LOCALS    ] ============================================== */
INITIALIZER(_fbl_start) {
  char *str = getenv("loaderFlashSectorSize");
  if (str != NULL) {
    loaderFlashSectorSize = strtoul(str, NULL, 10);
  }
}

int Loader_CheckProgrammingPreconditions(Loader_Type *loader) {
  static const uint8_t data[] = {0x31, 0x01, 0x02, 0x03};
  static const int expected[] = {0x71, 0x01, 0x02, 0x03, 0x00};
  int r;

  r = Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}
#if 0 == ERASE_FLASH_METHO
static int Loader_RoutineEraseFlash(Loader_Type *loader) {
  static const int expected[] = {0x71, 0x01, 0xFF, 0x00};
  int r;

  data[0] = 0x31;
  data[1] = 0x01;
  data[2] = 0xFF;
  data[3] = 0x00;

  r = Loader_RequestService(loader, data, 4, expected, ARRAY_SIZE(expected));

  return r;
}
#elif 1 == ERASE_FLASH_METHO
static int Loader_RoutineEraseFlash(Loader_Type *loader) {
  uint8_t data[13];
  static const int expected[] = {0x71, 0x01, 0xFF, 0x00, 0x00};
  int r;
  Srec_FileType *appSRec = Loader_GetAppSrec(loader);
  Srec_BlockType *blk;
  size_t i;
  uint32_t start = UINT32_MAX;
  uint32_t end = 0;
  uint32_t size = 0;

  data[0] = 0x31;
  data[1] = 0x01;
  data[2] = 0xFF;
  data[3] = 0x00;
  data[4] = 0x44; /* addressAndLengthFormatIdentifier */

  for (i = 0; i < appSRec->NumOfBlks; i++) {
    blk = &appSRec->Blks[i];
    if (start > blk->Address) {
      start = blk->Address;
    }
    if ((blk->Address + blk->Length) > end) {
      end = blk->Address + blk->Length;
    }
  }
  size = FL_ALIGN(end - start, loaderFlashSectorSize);

  data[5] = (uint8_t)((start >> 24) & 0xFF);
  data[6] = (uint8_t)((start >> 16) & 0xFF);
  data[7] = (uint8_t)((start >> 8) & 0xFF);
  data[8] = (uint8_t)(start & 0xFF);

  data[9] = (uint8_t)((size >> 24) & 0xFF);
  data[10] = (uint8_t)((size >> 16) & 0xFF);
  data[11] = (uint8_t)((size >> 8) & 0xFF);
  data[12] = (uint8_t)(size & 0xFF);

  r = Loader_RequestService(loader, data, 13, expected, ARRAY_SIZE(expected));

  return r;
}
#elif 2 == ERASE_FLASH_METHO
static int Loader_RoutineEraseFlash(Loader_Type *loader) {
  uint8_t data[13];
  static const int expected[] = {0x71, 0x01, 0xFF, 0x00, 0x00};
  int r = L_R_OK;
  Srec_FileType *appSRec = Loader_GetAppSrec(loader);
  Srec_BlockType *blk;
  size_t i;
  uint32_t start = UINT32_MAX;
  uint32_t end = 0;
  uint32_t size = 0;

  data[0] = 0x31;
  data[1] = 0x01;
  data[2] = 0xFF;
  data[3] = 0x00;
  data[4] = 0x44; /* addressAndLengthFormatIdentifier */

  LDLOG(DEBUG, "Loader: loaderFlashSectorSize=%u\n", loaderFlashSectorSize);

  for (i = 0; (i < appSRec->NumOfBlks) && (L_R_OK == r); i++) {
    blk = &appSRec->Blks[i];
    start = (blk->Address / loaderFlashSectorSize) * loaderFlashSectorSize;
    end = blk->Address + blk->Length;
    size = FL_ALIGN(end - start, loaderFlashSectorSize);
    data[5] = (uint8_t)((start >> 24) & 0xFF);
    data[6] = (uint8_t)((start >> 16) & 0xFF);
    data[7] = (uint8_t)((start >> 8) & 0xFF);
    data[8] = (uint8_t)(start & 0xFF);

    data[9] = (uint8_t)((size >> 24) & 0xFF);
    data[10] = (uint8_t)((size >> 16) & 0xFF);
    data[11] = (uint8_t)((size >> 8) & 0xFF);
    data[12] = (uint8_t)(size & 0xFF);

    LDLOG(INFO, "\n\terase block 0x%x - 0x%x\n", start, size);
    r = Loader_RequestService(loader, data, 13, expected, ARRAY_SIZE(expected));
  }

  return r;
}
#endif

static int Loader_RoutineEraseFee(Loader_Type *loader) {
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
  r = Loader_RequestService(loader, data, 13, expected, ARRAY_SIZE(expected));

  return r;
}

static int Loader_CheckFlashDriverIntegrity(Loader_Type *loader) {
  static uint8_t data[8] = {0x31, 0x01, 0x02, 0x02};
  static const int expected[] = {0x71, 0x01, 0x02, 0x02, 0x00};
  int r = L_R_OK;
  Loader_CrcType Crc = Loader_CrcInit(loader);
  Srec_BlockType *blk;
  uint32_t lenCrc;
  Srec_FileType *flsSRec = Loader_GetFlsDrvSrec(loader);
  size_t i;

  if (NULL != flsSRec) {
    LDLOG(INFO, "check flash driver integrity");
    for (i = 0; i < flsSRec->NumOfBlks; i++) {
      blk = &flsSRec->Blks[i];
      Crc = Loader_CalculateCrc(loader, blk->Data, blk->Length, Crc, FALSE);
    }

    lenCrc = Loader_SetCrc(loader, &data[4], Crc);
    r = Loader_RequestService(loader, data, 4 + lenCrc, expected, ARRAY_SIZE(expected));
    if (L_R_OK == r) {
      LDLOG(INFO, " okay\n");
    }
  }

  return r;
}

static int Loader_CheckProgramIntegrity(Loader_Type *loader) {
  static uint8_t data[8] = {0x31, 0x01, 0x02, 0x02};
  static const int expected[] = {0x71, 0x01, 0x02, 0x02, 0x00};
  int r;
  Loader_CrcType Crc = Loader_CrcInit(loader);
  Srec_BlockType *blk;
  size_t i;
  Srec_FileType *appSRec = Loader_GetAppSrec(loader);
  uint32_t lenCrc;

  for (i = 0; i < appSRec->NumOfBlks; i++) {
    blk = &appSRec->Blks[i];
    Crc = Loader_CalculateCrc(loader, blk->Data, blk->Length, Crc, FALSE);
  }

  lenCrc = Loader_SetCrc(loader, &data[4], Crc);
  r = Loader_RequestService(loader, data, 4 + lenCrc, expected, ARRAY_SIZE(expected));

  return r;
}

static int Loader_CheckProgramDependencies(Loader_Type *loader) {
  static const uint8_t data[] = {0x31, 0x01, 0xFF, 0x01};
  static const int expected[] = {0x71, 0x01, 0xFF, 0x01, 0x00};
  return Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));
}

static int Loader_ReadFingerPrint(Loader_Type *loader) {
  static const uint8_t data[3] = {0x22, 0xF1, 0x5B};
  static const int expected[] = {0x62, 0xF1, 0x5B, 0, // block id
                                 -1,   -1,   -1,   -1, -1, -1, -1, -1, -1};
  int r;
  uint8_t *res = Loader_GetResponse(loader);

  r = Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  if (L_R_OK == r) {
    LDLOG(INFO, "\n  date: %02X-%02X-%02X\n", res[4], res[5], res[6]);
    LDLOG(INFO, "  serial = %02X%02X%02X%02X%02X%02X %c%c%c%c%c%c\n", // format
          res[7], res[8], res[9], res[10], res[11], res[12],          // serail
          TO_C(res[7]), TO_C(res[8]), TO_C(res[9]), TO_C(res[10]), TO_C(res[11]), TO_C(res[12]));
  }

  return r;
}

static int Loader_WriteFingerPrint(Loader_Type *loader) {
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
  r = Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static int Loader_RoutineCheckIntegrity(Loader_Type *loader) {
  static const uint8_t data[4] = {0x31, 0x01, 0xFF, 0x02};
  static const int expected[] = {0x71, 0x01, 0xFF, 0x02};
  int r;

  r = Loader_RequestService(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static const Loader_ServiceType lStdServices[] = {
  {"enter extended session", " okay\n", Loader_EnterExtendSession, 100},
  {"control dtc setting off", " okay\n", Loader_ControlDtcSettingOff, 100},
  {"communication disable", " okay\n", Loader_CommunicationDisable, 100},
  {"level 1 security access", " okay\n", Loader_SecurityExtdsAccess, 100},
  // {"check programming preconditions", " okay\n", Loader_CheckProgrammingPreconditions, 100},
  {"enter program session", " okay\n", Loader_EnterProgramSession, 100},
  {"level 2 security access", " okay\n", Loader_SecurityPrgsAccess, 100},
  {NULL, NULL, Loader_DownloadFlashDriver, 500},
  {NULL, NULL, Loader_CheckFlashDriverIntegrity, 100},
  {"read finger print", " okay\n", Loader_ReadFingerPrint, 100},
  {"erase flash", " okay\n", Loader_RoutineEraseFlash, 500},
  {"erase fee", " okay\n", Loader_RoutineEraseFee, 100},
  {"write finger print", " okay\n", Loader_WriteFingerPrint, 100},
  {"download application", " okay\n", Loader_DownloadApplication, 100},
  {"check program integrity", " okay\n", Loader_CheckProgramIntegrity, 100},
  {"check dependencies", " okay\n", Loader_CheckProgramDependencies, 100},
  {"check integrity", " okay\n", Loader_RoutineCheckIntegrity, 100},
  {"ecu reset", " okay\n", Loader_EcuReset, 100},
  {"communication enable", " okay\n", Loader_CommunicationEnable, 100},
  {"control dtc setting on", " okay\n", Loader_ControlDtcSettingOn, 100},
};

/* ================================ [ FUNCTIONS ] ============================================== */
LOADER_APP_REGISTER(lStdServices, FBL)
