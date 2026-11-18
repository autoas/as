/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "srec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Crc.h"
/* ================================ [ MACROS    ] ============================================== */
#define SREC_MAX_ONE_LINE 512

#define SREC_ADD_LINE(fpS, saddr, data, len)                                                       \
  do {                                                                                             \
    if (srec->Type == SREC_SRECORD) {                                                              \
      Srec_AddLine(fpS, saddr, data, len);                                                         \
    } else {                                                                                       \
      IHex_AddLine(fpS, saddr, data, len);                                                         \
    }                                                                                              \
  } while (0)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void IHex_AddLine(FILE *fpS, uint32_t saddr, uint8_t *data, uint32_t len);
/* ================================ [ DATAS     ] ============================================== */
static const char *const srecSignTypeName[] = {
  "CRC16", "CRC32", "CRC16_V2", "CRC32_V2", "CRC16_V3", "CRC32_V3",
};
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t Srec_HexChar(char *str) {
  char hexs[3];
  hexs[0] = str[0];
  hexs[1] = str[1];
  hexs[2] = '\0';

  return (uint8_t)strtoul(hexs, NULL, 16);
}
/*
An s record looks like:

EXAMPLE
S<type><length><address><data><checksum>

DESCRIPTION
Where
o length
is the number of bytes following upto the checksum. Note that
this is not the number of chars following, since it takes two
chars to represent a byte.
o type
is one of:
0) header record
1) two byte address data record
2) three byte address data record
3) four byte address data record
7) four byte address termination record
8) three byte address termination record
9) two byte address termination record

o address
is the start address of the data following, or in the case of
a termination record, the start address of the image
o data
is the data.
o checksum
is the sum of all the raw byte data in the record, from the length
upwards, modulo 256 and subtracted from 255.

Command to padding s19 to align secion address by 32
srec_cat in.s19 -fill 0xFF -within in.s19 -range-padding 32 -o out.s19 -address-length=4
*/
static int Srec_Decode(Srec_FileType *srec, char *sline, int slen, int addrLen) {
  int err = 0;
  int dataLen = 0;
  int i;
  uint8_t checksum;
  uint8_t sum = 0;
  uint32_t address = 0;
  size_t offset;
  Srec_BlockType *curBlk = NULL;
  uint8_t v;
  uint32_t gap;
  uint32_t gapThresh = 8;

  char *str = getenv("FLASH_WRITE_SIZE");
  if (str != NULL) {
    gapThresh = strtoul(str, NULL, 10);
  }

  if (slen > 6) {
    dataLen = Srec_HexChar(&sline[2]);
  } else {
    err = -1;
  }

  if (0 == err) {
    if (((dataLen + 4) > slen) || (dataLen <= addrLen)) {
      err = -2;
    }
  }

  if (0 == err) {
    sum += dataLen;
    for (i = 0; i < addrLen; i++) {
      v = Srec_HexChar(&sline[4 + 2 * i]);
      address = (address << 8) + v;
      sum += v;
    }

    if (0 == srec->NumOfBlks) {
      curBlk = &srec->Blks[0];
      curBlk->Address = address;
      curBlk->Offset = 0;
      curBlk->Data = &srec->Data[curBlk->Offset];
      curBlk->Length = 0;
      srec->NumOfBlks++;
    } else {
      curBlk = &srec->Blks[srec->NumOfBlks - 1];
      gap = address - (curBlk->Address + curBlk->Length);
      if (gap < gapThresh) {
        memset(&curBlk->Data[curBlk->Length], 0xFF, gap);
        curBlk->Length += gap;
      }
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
          err = -3;
        }
      }
    }
  }

  if (0 == err) {
    for (i = 0; i < (dataLen - addrLen - 1); i++) {
      v = Srec_HexChar(&sline[4 + addrLen * 2 + 2 * i]);
      curBlk->Data[curBlk->Length++] = v;
      sum += v;
    }

    srec->TotalSize += dataLen - addrLen - 1;

    sum = (~sum) & 0xFF;
    checksum = Srec_HexChar(&sline[4 + 2 * (dataLen - 1)]);

    if (sum != checksum) {
      err = -4;
    }
  }

  return err;
}

static void Srec_Parse(Srec_FileType *srec, FILE *fp) {
  int err = 0;
  char sline[SREC_MAX_ONE_LINE];
  int slen;
  int linno = 0;

  while ((0 == err) && fgets(sline, sizeof(sline), fp)) {
    slen = strlen(sline);
    if (sline[0] != 'S') {
      err = -1;
      printf("invalid SRecord <%s>\n", sline);
    } else {
      switch (sline[1]) {
      case '1':
        err = Srec_Decode(srec, sline, slen, 2);
        break;
      case '2':
        err = Srec_Decode(srec, sline, slen, 3);
        break;
      case '3':
        err = Srec_Decode(srec, sline, slen, 4);
        break;
      default:
        break;
      }

      if (err) {
        printf("invalid SRecord at line %d, err=%d: <%s>\n", linno, err, sline);
      }
    }
    linno++;
  }
}

Srec_FileType *Srec_OpenImpl(const char *path) {
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
      Srec_Parse(srec, fp);
      if (0 == srec->NumOfBlks) {
        free(srec);
        srec = NULL;
      } else {
        srec->Type = SREC_SRECORD;
      }
    } else {
      printf("no enough memory for %d bytes\n", (int)sz / 2);
    }
  } else {
    printf("srecord %s not exists\n", path);
  }

  return srec;
}

static void Srec_AddLine(FILE *fpS, uint32_t saddr, uint8_t *data, uint32_t len) {
  uint8_t checksum;
  char sline[SREC_MAX_ONE_LINE];
  uint32_t i = 0;
  int ls;

  checksum = 5 + len;
  checksum +=
    ((saddr >> 24) & 0xFF) + ((saddr >> 16) & 0xFF) + ((saddr >> 8) & 0xFF) + (saddr & 0xFF);

  for (i = 0; i < len; i++) {
    checksum += data[i];
  }
  checksum = ~checksum;

  ls = snprintf(sline, sizeof(sline), "S3%02X%08X", 4 + len + 1, saddr);
  for (i = 0; i < len; i++) {
    ls += snprintf(&sline[ls], sizeof(sline) - ls, "%02X", data[i]);
  }
  ls += snprintf(&sline[ls], sizeof(sline) - ls, "%02X\n", checksum);

  fputs(sline, fpS);
}

static void Srec_AddSign(Srec_FileType *srec, FILE *fpS, Srec_SignType signType, uint32_t saddr,
                         uint32_t crc, uint32_t crcLen) {
  uint8_t data[4];
  uint32_t len;

  if (SREC_SIGN_CRC32 == signType) {
    data[0] = (crc >> 24) & 0xFF;
    data[1] = (crc >> 16) & 0xFF;
    data[2] = (crc >> 8) & 0xFF;
    data[3] = crc & 0xFF;
    len = 4;
  } else {
    data[0] = (crc >> 8) & 0xFF;
    data[1] = crc & 0xFF;
    len = 2;
  }

  SREC_ADD_LINE(fpS, saddr, data, len);
}

static int Srec_SignV1(const char *path, size_t total, Srec_SignType signType) {
  Srec_FileType *srec;
  int r = 0;
  uint8_t *buffer = NULL;
  FILE *fpO = NULL;
  FILE *fpS = NULL;
  FILE *fpB = NULL;
  uint32_t saddr, length;
  int i;
  uint32_t crc;
  uint32_t crcLen = 2;
  char sline[SREC_MAX_ONE_LINE];

  if (SREC_SIGN_CRC32 == signType) {
    crcLen = 4;
  }

  memset(sline, 0, sizeof(sline));

  printf("sign %s by %s:\n", path, srecSignTypeName[signType]);

  srec = Srec_Open(path);
  if (NULL == srec) {
    r = EEXIST;
  } else {
    Srec_Print(srec);
    saddr = Srec_Range(srec, &length);
    if ((total - crcLen) < length) {
      printf(" range=0x%X not correct\n", (uint32_t)total);
      r = EINVAL;
    }
  }

  if (0 == r) {
    buffer = malloc(total);
    if (NULL == buffer) {
      r = ENOMEM;
    }
  }

  if (0 == r) {
    memset(buffer, 0xFF, total - crcLen);
    for (i = 0; i < srec->NumOfBlks; i++) {
      memcpy(&buffer[srec->Blks[i].Address - saddr], srec->Blks[i].Data, srec->Blks[i].Length);
    }
    if (SREC_SIGN_CRC32 == signType) {
      crc = Crc_CalculateCRC32(buffer, total - crcLen, 0xFFFFFFFF, TRUE);
      buffer[total - 4] = (crc >> 24) & 0xFF;
      buffer[total - 3] = (crc >> 16) & 0xFF;
      buffer[total - 2] = (crc >> 8) & 0xFF;
      buffer[total - 1] = crc & 0xFF;
    } else {
      crc = Crc_CalculateCRC16(buffer, total - crcLen, 0xFFFF, TRUE);
      buffer[total - 2] = (crc >> 8) & 0xFF;
      buffer[total - 1] = crc & 0xFF;
    }
  }

  if (0 == r) {
    strncpy(sline, path, strlen(path) - 3);
    strcat(sline, "bin");
    fpB = fopen(sline, "wb");
    if (NULL != fpB) {
      fwrite(buffer, total, 1, fpB);
      fclose(fpB);
    } else {
      printf("\n\twarning: failed to create %s\n\n", sline);
    }
  }

  if (0 == r) {
    snprintf(sline, sizeof(sline), "%s.sign", path);
    fpS = fopen(sline, "w");
    if (NULL == fpS) {
      r = EACCES;
    } else {
      fpO = fopen(path, "r");
      if (NULL == fpO) {
        r = EACCES;
      }
    }
  }

  if (0 == r) {
    saddr += total - crcLen;
    while ((0 == r) && fgets(sline, sizeof(sline), fpO)) {
      fputs(sline, fpS);
    }

    Srec_AddSign(srec, fpS, signType, saddr, crc, crcLen);
  }

  if (0 != r) {
    printf(" failed with error %d\n", r);
  } else {
    printf(" okay\n");
  }

  if (buffer) {
    free(buffer);
  }

  if (fpS) {
    fclose(fpS);
  }

  if (fpO) {
    fclose(fpO);
  }

  return r;
}

static int Srec_SignV2(const char *path, size_t signAddr, Srec_SignType signType) {
  int r = 0;
  Srec_FileType *srec;
  int i;
  uint32_t saddr, length;
  uint32_t crc = 0xFFFFFFFFUL;
  FILE *fpO = NULL;
  FILE *fpS = NULL;
  char sline[SREC_MAX_ONE_LINE];
  uint8_t *data = (uint8_t *)sline;

  printf("sign %s by %s:\n", path, srecSignTypeName[signType]);

  srec = Srec_Open(path);
  if (NULL == srec) {
    r = EEXIST;
  } else {
    Srec_Print(srec);
    saddr = Srec_Range(srec, &length);
    if (signAddr < (saddr + length)) {
      printf(" sign address=0x%X not correct\n", (uint32_t)signAddr);
      r = EINVAL;
    }
  }

  if (0 == r) {
    for (i = 0; i < srec->NumOfBlks; i++) {
      if (SREC_SIGN_CRC32_V2 == signType) {
        crc = Crc_CalculateCRC32(srec->Blks[i].Data, srec->Blks[i].Length, crc, FALSE);
      } else {
        crc = Crc_CalculateCRC16(srec->Blks[i].Data, srec->Blks[i].Length,
                                 (uint16_t)(crc & 0xFFFFUL), FALSE);
      }
    }
  }

  if (0 == r) {
    snprintf(sline, sizeof(sline), "%s.sign", path);
    fpS = fopen(sline, "w");
    if (NULL == fpS) {
      r = EACCES;
    } else {
      fpO = fopen(path, "r");
      if (NULL == fpO) {
        r = EACCES;
      }
    }
  }

  if (0 == r) {
    while ((0 == r) && fgets(sline, sizeof(sline), fpO)) {
      fputs(sline, fpS);
    }

    saddr = signAddr;
    data[0] = (srec->NumOfBlks >> 24) & 0xFF;
    data[1] = (srec->NumOfBlks >> 16) & 0xFF;
    data[2] = (srec->NumOfBlks >> 8) & 0xFF;
    data[3] = srec->NumOfBlks & 0xFF;
    if (SREC_SIGN_CRC32_V2 == signType) {
      data[4] = (crc >> 24) & 0xFF;
      data[5] = (crc >> 16) & 0xFF;
      data[6] = (crc >> 8) & 0xFF;
      data[7] = crc & 0xFF;
    } else {
      data[4] = (crc >> 8) & 0xFF;
      data[5] = crc & 0xFF;
      data[6] = 0xFF;
      data[7] = 0xFF;
    }
    SREC_ADD_LINE(fpS, saddr, data, 8);
    saddr += 8;

    for (i = 0; i < srec->NumOfBlks; i++) {
      data[0] = (srec->Blks[i].Address >> 24) & 0xFF;
      data[1] = (srec->Blks[i].Address >> 16) & 0xFF;
      data[2] = (srec->Blks[i].Address >> 8) & 0xFF;
      data[3] = srec->Blks[i].Address & 0xFF;
      data[4] = (srec->Blks[i].Length >> 24) & 0xFF;
      data[5] = (srec->Blks[i].Length >> 16) & 0xFF;
      data[6] = (srec->Blks[i].Length >> 8) & 0xFF;
      data[7] = srec->Blks[i].Length & 0xFF;
      SREC_ADD_LINE(fpS, saddr, data, 8);
      saddr += 8;
    }
  }

  if (0 != r) {
    printf(" failed with error %d\n", r);
  } else {
    printf(" okay\n");
  }

  if (fpS) {
    fclose(fpS);
  }

  if (fpO) {
    fclose(fpO);
  }

  return r;
}

static int Srec_SignV3(const char *path, Srec_SignType signType) {
  int r = 0;
  Srec_FileType *srec;
  int i;
  uint32_t saddr, signAddr = 0, length;
  uint32_t crc = 0xFFFFFFFFUL;
  FILE *fpO = NULL;
  FILE *fpS = NULL;
  char sline[SREC_MAX_ONE_LINE];
  uint8_t *data = (uint8_t *)sline;

  printf("sign %s by %s:\n", path, srecSignTypeName[signType]);

  srec = Srec_Open(path);
  if (NULL == srec) {
    r = EEXIST;
  } else {
    Srec_Print(srec);
    saddr = Srec_Range(srec, &length);
    if (signAddr < (saddr + length)) {
      signAddr = saddr + length;
    }
  }

  if (0 == r) {
    for (i = 0; i < srec->NumOfBlks; i++) {
      if (SREC_SIGN_CRC32_V3 == signType) {
        crc = Crc_CalculateCRC32(srec->Blks[i].Data, srec->Blks[i].Length, crc, FALSE);
      } else {
        crc = Crc_CalculateCRC16(srec->Blks[i].Data, srec->Blks[i].Length,
                                 (uint16_t)(crc & 0xFFFFUL), FALSE);
      }
    }
  }

  if (0 == r) {
    snprintf(sline, sizeof(sline), "%s.sign", path);
    fpS = fopen(sline, "w");
    if (NULL == fpS) {
      r = EACCES;
    } else {
      fpO = fopen(path, "r");
      if (NULL == fpO) {
        r = EACCES;
      }
    }
  }

  if (0 == r) {
    while ((0 == r) && fgets(sline, sizeof(sline), fpO)) {
      fputs(sline, fpS);
    }

    saddr = signAddr;
    for (i = 0; i < srec->NumOfBlks; i++) {
      data[0] = (srec->Blks[i].Address >> 24) & 0xFF;
      data[1] = (srec->Blks[i].Address >> 16) & 0xFF;
      data[2] = (srec->Blks[i].Address >> 8) & 0xFF;
      data[3] = srec->Blks[i].Address & 0xFF;
      data[4] = (srec->Blks[i].Length >> 24) & 0xFF;
      data[5] = (srec->Blks[i].Length >> 16) & 0xFF;
      data[6] = (srec->Blks[i].Length >> 8) & 0xFF;
      data[7] = srec->Blks[i].Length & 0xFF;
      SREC_ADD_LINE(fpS, saddr, data, 8);
      saddr += 8;
    }
    data[0] = (srec->NumOfBlks >> 24) & 0xFF;
    data[1] = (srec->NumOfBlks >> 16) & 0xFF;
    data[2] = (srec->NumOfBlks >> 8) & 0xFF;
    data[3] = srec->NumOfBlks & 0xFF;
    if (SREC_SIGN_CRC32_V3 == signType) {
      data[4] = (crc >> 24) & 0xFF;
      data[5] = (crc >> 16) & 0xFF;
      data[6] = (crc >> 8) & 0xFF;
      data[7] = crc & 0xFF;
    } else {
      data[4] = (crc >> 8) & 0xFF;
      data[5] = crc & 0xFF;
      data[6] = 0xFF;
      data[7] = 0xFF;
    }
    memcpy(&data[8], "$BYASV3#", 8);
    SREC_ADD_LINE(fpS, saddr, data, 16);
    saddr += 8;
  }

  if (0 != r) {
    printf(" failed with error %d\n", r);
  } else {
    printf(" okay\n");
  }

  if (fpS) {
    fclose(fpS);
  }

  if (fpO) {
    fclose(fpO);
  }

  return r;
}

static uint32_t Srec_ToU32(const char *strV) {
  uint32_t u32V = 0;

  if ((0 == strncmp("0x", strV, 2)) || (0 == strncmp("0X", strV, 2))) {
    u32V = strtoul(strV, NULL, 16);
  } else {
    u32V = strtoul(strV, NULL, 10);
  }

  return u32V;
}

/* path: in pattern file_path.bin:address */
static Srec_FileType *Srec_BinOpenImpl(const char *path) {
  Srec_FileType *srec = NULL;
  FILE *fp;
  uint32_t addr;
  size_t sz;
  char filePath[256] = {0};
  char address[128] = {0};

  sscanf(path, "%255[^:]:%127s", filePath, address);

  addr = Srec_ToU32(address);
  fp = fopen(filePath, "rb");
  if (NULL != fp) {
    printf("binary: %s @ 0x%x\n", filePath, addr);
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    srec = (Srec_FileType *)malloc(sz + sizeof(Srec_FileType));
    if (NULL != srec) {
      srec->Data = (uint8_t *)&srec[1];
      srec->NumOfBlks = 1;
      srec->TotalSize = sz;
      srec->Type = SREC_BIN;
      srec->Blks[0].Data = srec->Data;
      srec->Blks[0].Address = addr;
      srec->Blks[0].Offset = 0;
      srec->Blks[0].Length = sz;
      fread(srec->Data, sz, 1, fp);
    } else {
      printf("no enough memory for %d bytes\n", (int)sz);
    }
  } else {
    printf("binary %s not exists\n", filePath);
  }

  return srec;
}
/* ================================ [ FUNCTIONS ] ============================================== */
Srec_FileType *Srec_Open(const char *path) {
  Srec_FileType *srec;

  if (NULL != strstr(path, ".s19")) {
    srec = Srec_OpenImpl(path);
  } else if (NULL != strstr(path, ".hex")) {
    srec = IHex_Open(path);
  } else if (NULL != strstr(path, ".bin")) {
    srec = Srec_BinOpenImpl(path);
  } else {
    srec = Srec_OpenImpl(path);
    if (NULL == srec) {
      srec = IHex_Open(path);
    }
    if (NULL == srec) {
      srec = Srec_BinOpenImpl(path);
    }
  }
  return srec;
}

int Srec_Sign(const char *path, size_t total, Srec_SignType signType) {
  int r = 0;

  switch (signType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC32:
    r = Srec_SignV1(path, total, signType);
    break;
  case SREC_SIGN_CRC16_V2:
  case SREC_SIGN_CRC32_V2:
    r = Srec_SignV2(path, total, signType);
    break;
  case SREC_SIGN_CRC16_V3:
  case SREC_SIGN_CRC32_V3:
    r = Srec_SignV3(path, signType);
    break;
  default:
    printf("invalid sign type %d\n", signType);
    r = EINVAL;
    break;
  }

  return r;
}

void Srec_Print(Srec_FileType *srec) {
  int i, j;
  uint16_t crc;
  for (i = 0; i < srec->NumOfBlks; i++) {
    printf("srec %d: address=0x%08X, length=0x%08X, offset=0x%08X, data=", i, srec->Blks[i].Address,
           (uint32_t)srec->Blks[i].Length, (uint32_t)srec->Blks[i].Offset);
    for (j = 0; (j < 8) && (j < srec->Blks[i].Length); j++) {
      printf("%02X", srec->Blks[i].Data[j]);
    }
    crc = Crc_CalculateCRC16(srec->Blks[i].Data, srec->Blks[i].Length, 0xFFFF, TRUE);
    printf(" crc16=%X\n", crc);
  }
}

uint32_t Srec_Range(Srec_FileType *srec, uint32_t *length) {
  int i;
  uint32_t saddr = 0xFFFFFFFFUL, eaddr = 0;
  for (i = 0; i < srec->NumOfBlks; i++) {
    if (srec->Blks[i].Address < saddr) {
      saddr = srec->Blks[i].Address;
    }
    if ((srec->Blks[i].Address + srec->Blks[i].Length) > eaddr) {
      eaddr = (uint32_t)(srec->Blks[i].Address + srec->Blks[i].Length);
    }
  }

  *length = eaddr - saddr;

  return saddr;
}

void Srec_Close(Srec_FileType *srec) {
  if (NULL != srec) {
    free(srec);
  }
}
