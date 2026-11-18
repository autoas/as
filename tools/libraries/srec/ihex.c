/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 * ref: https://www.engineersgarage.com/hex-file-format/
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "srec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define IHEX_MAX_ONE_LINE 512

#define IHEX_MIN_LENGTH 11

#define IHEX_TYPE_DATA_RECORD 0x00
#define IHEX_TYPE_END_RECORD 0x01
#define IHEX_TYPE_EXTEND_SEGMENT_ADDR 0x02
#define IHEX_TYPE_START_SEGMENT_ADDR 0x03
#define IHEX_TYPE_EXTEND_LINEAR_ADDR 0x04
#define IHEX_TYPE_START_LINEAR_ADDR 0x05
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t CharCount;
  uint16_t Address;
  uint8_t Type;
  uint8_t Data[IHEX_MAX_ONE_LINE / 2];
} IHex_LineType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t IHex_Char(const char *str) {
  char hexs[3];
  hexs[0] = str[0];
  hexs[1] = str[1];
  hexs[2] = '\0';

  return (uint8_t)strtoul(hexs, NULL, 16);
}

static int IHex_Decode(const char *sline, int slen, IHex_LineType *iln) {
  int err = 0;
  int i;
  uint8_t checksum = 0;
  uint8_t sum = 0;

  iln->CharCount = IHex_Char(&sline[1]);
  if ((iln->CharCount * 2 + IHEX_MIN_LENGTH) > slen) {
    err = -__LINE__;
  }

  iln->Address = IHex_Char(&sline[3]);
  iln->Address = (iln->Address << 8) + IHex_Char(&sline[5]);
  iln->Type = IHex_Char(&sline[7]);

  sum = iln->CharCount + ((iln->Address >> 8) & 0xFF) + (iln->Address & 0xFF) + iln->Type;

  for (i = 0; i < iln->CharCount; i++) {
    iln->Data[i] = IHex_Char(&sline[9 + i * 2]);
    sum += iln->Data[i];
  }

  sum = 1 + (~sum);

  checksum = IHex_Char(&sline[9 + iln->CharCount * 2]);

  if (sum != checksum) {
    err = -__LINE__;
  }

  return err;
}

static int IHex_GetExtendSegmentAddr(IHex_LineType *iln, uint32_t *addr) {
  int err = 0;

  if (2 == iln->CharCount) {
    *addr = ((uint32_t)iln->Data[0] << 12) + ((uint32_t)iln->Data[1] << 4);
  } else {
    err = -__LINE__;
  }

  return err;
}

static int IHex_GetExtendLinearAddr(IHex_LineType *iln, uint32_t *addr) {
  int err = 0;

  if (2 == iln->CharCount) {
    *addr = ((uint32_t)iln->Data[0] << 24) + ((uint32_t)iln->Data[1] << 16);
  } else {
    err = -__LINE__;
  }

  return err;
}

static int IHex_GetStartLinearAddr(IHex_LineType *iln) {
  int err = 0;
  uint32_t addr;

  if (4 == iln->CharCount) {
    addr = ((uint32_t)iln->Data[0] << 24) + ((uint32_t)iln->Data[1] << 16) +
           ((uint32_t)iln->Data[2] << 8) + iln->Data[3];
    printf("  Start Linear Address: <0x%x>\n", addr);
  } else {
    err = -__LINE__;
  }

  return err;
}

static int IHex_AddData(IHex_LineType *iln, Srec_FileType *srec, uint32_t baseAddress) {
  int err = 0;
  Srec_BlockType *curBlk = NULL;
  uint32_t address = baseAddress + iln->Address;
  uint32_t offset;

  if (0 == srec->NumOfBlks) {
    curBlk = &srec->Blks[0];
    curBlk->Address = address;
    curBlk->Offset = 0;
    curBlk->Data = &srec->Data[curBlk->Offset];
    curBlk->Length = 0;
    srec->NumOfBlks++;
  } else {
    curBlk = &srec->Blks[srec->NumOfBlks - 1];
    if ((curBlk->Address + curBlk->Length) != address) {
      offset = curBlk->Offset + curBlk->Length;
      srec->NumOfBlks++;
      if (srec->NumOfBlks < SREC_MAX_BLK) {
        curBlk = &srec->Blks[srec->NumOfBlks - 1];
        curBlk->Address = address;
        curBlk->Offset = offset;
        curBlk->Data = &srec->Data[curBlk->Offset];
        curBlk->Length = 0;
      } else {
        srec->NumOfBlks--;
        err = -__LINE__;
      }
    }
  }

  if (0 == err) {
    memcpy(&curBlk->Data[curBlk->Length], iln->Data, iln->CharCount);
    curBlk->Length += iln->CharCount;
    srec->TotalSize += iln->CharCount;
  }
  return err;
}

static void IHex_Parse(Srec_FileType *srec, FILE *fp) {
  int err = 0;
  char sline[IHEX_MAX_ONE_LINE];
  uint32_t addr = 0;
  int slen;
  int linno = 0;
  IHex_LineType iln;

  while ((0 == err) && fgets(sline, sizeof(sline), fp)) {
    slen = strlen(sline);
    if (sline[0] == '\n') {
      /* do nothing */
    } else if (sline[0] != ':') {
      printf("  invalid intel hex <%s>, ignore\n", sline);
    } else {
      err = IHex_Decode(sline, slen, &iln);
      if (0 == err) {
        switch (iln.Type) {
        case IHEX_TYPE_DATA_RECORD:
          err = IHex_AddData(&iln, srec, addr);
          break;
        case IHEX_TYPE_END_RECORD:
          break;
        case IHEX_TYPE_EXTEND_SEGMENT_ADDR:
          err = IHex_GetExtendSegmentAddr(&iln, &addr);
          break;
        case IHEX_TYPE_START_SEGMENT_ADDR:
          printf(" type 03 not supported, ignore\n");
          break;
        case IHEX_TYPE_EXTEND_LINEAR_ADDR:
          err = IHex_GetExtendLinearAddr(&iln, &addr);
          break;
        case IHEX_TYPE_START_LINEAR_ADDR:
          err = IHex_GetStartLinearAddr(&iln);
          break;
        default:
          err = -__LINE__;
          break;
        }
      }
      if (err) {
        printf("invalid ihex at line %d, err=%d: <%s>\n", linno, err, sline);
      }
    }
    linno++;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
Srec_FileType *IHex_Open(const char *path) {
  Srec_FileType *srec = NULL;
  FILE *fp;
  size_t sz;

  fp = fopen(path, "r");
  if (NULL != fp) {
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    srec = (Srec_FileType *)malloc(sz / 2 + sizeof(Srec_FileType));
    if (NULL != srec) {
      srec->Data = (uint8_t *)&srec[1];
      srec->NumOfBlks = 0;
      srec->TotalSize = 0;
      memset(srec->Blks, 0, sizeof(srec->Blks));
      IHex_Parse(srec, fp);
      if (0 == srec->NumOfBlks) {
        free(srec);
        srec = NULL;
      } else {
        srec->Type = SREC_IHEX;
      }
    } else {
      printf("no enough memory for %d bytes\n", (int)sz / 2);
    }
  } else {
    printf("ihex %s not exists\n", path);
  }

  return srec;
}

void IHex_AddSign(FILE *fpS, Srec_SignType signType, uint32_t saddr, uint32_t crc,
                  uint32_t crcLen) {
  uint16_t baseAddress = (saddr >> 16) & 0xFFFF;
  uint16_t addr = saddr & 0xFFFF;
  uint8_t checksum;
  char sline[IHEX_MAX_ONE_LINE];
  checksum = 2 + 4 + ((baseAddress >> 8) & 0xFF) + (baseAddress & 0xFF);
  checksum = 1 + (~checksum);
  snprintf(sline, sizeof(sline), "\n:02000004%04X%02X\n", baseAddress, checksum);
  fputs(sline, fpS);
  if (SREC_SIGN_CRC32 == signType) {
    checksum = 4 + ((addr >> 8) & 0xFF) + (addr & 0xFF) + ((crc >> 24) & 0xFF) +
               ((crc >> 16) & 0xFF) + ((crc >> 8) & 0xFF) + (crc & 0xFF);
    checksum = 1 + (~checksum);
    snprintf(sline, sizeof(sline), ":04%04X00%04X%02X\n", addr, crc, checksum);
  } else {
    checksum = 2 + ((addr >> 8) & 0xFF) + (addr & 0xFF) + ((crc >> 8) & 0xFF) + (crc & 0xFF);
    checksum = 1 + (~checksum);
    snprintf(sline, sizeof(sline), ":02%04X00%04X%02X\n", addr, crc, checksum);
  }
  fputs(sline, fpS);
}

void IHex_AddLine(FILE *fpS, uint32_t saddr, uint8_t *data, uint32_t len) {
  uint16_t baseAddress = (saddr >> 16) & 0xFFFF;
  uint16_t addr = saddr & 0xFFFF;
  uint8_t checksum;
  uint32_t i = 0;
  int ls;
  char sline[IHEX_MAX_ONE_LINE];
  checksum = 2 + 4 + ((baseAddress >> 8) & 0xFF) + (baseAddress & 0xFF);
  checksum = 1 + (~checksum);
  snprintf(sline, sizeof(sline), ":02000004%04X%02X\n", baseAddress, checksum);
  fputs(sline, fpS);
  checksum = len + ((addr >> 8) & 0xFF) + (addr & 0xFF);
  ls = snprintf(sline, sizeof(sline), ":%02X%04X00", len, addr);
  for (i = 0; i < len; i++) {
    ls += snprintf(&sline[ls], sizeof(sline) - ls, "%02X", data[i]);
    checksum += data[i];
  }
  checksum = 1 + (~checksum);
  snprintf(&sline[ls], sizeof(sline) - ls, "%02X\n", checksum);

  fputs(sline, fpS);
}
