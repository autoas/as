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
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void ihex_add_sign(FILE *fpS, srec_sign_type_t signType, uint32_t saddr, uint32_t crc,
                   uint32_t crcLen);
/* ================================ [ DATAS     ] ============================================== */
static const char *const signTypeName[] = {
  "CRC16", "CRC32", "CRC16_V2", "CRC32_V2", "CRC16_V3", "CRC32_V3",
};
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t srec_hex(char *str) {
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
static int srec_decode(srec_t *srec, char *sline, int slen, int addrLen) {
  int err = 0;
  int dataLen = 0;
  int i;
  uint8_t checksum;
  uint8_t sum = 0;
  uint32_t address = 0;
  size_t offset;
  sblk_t *curBlk = NULL;
  uint8_t v;
  uint32_t gap;
  uint32_t gapThresh = 8;

  char *str = getenv("FLASH_WRITE_SIZE");
  if (str != NULL) {
    gapThresh = strtoul(str, NULL, 10);
  }

  if (slen > 6) {
    dataLen = srec_hex(&sline[2]);
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
      v = srec_hex(&sline[4 + 2 * i]);
      address = (address << 8) + v;
      sum += v;
    }

    if (0 == srec->numOfBlks) {
      curBlk = &srec->blks[0];
      curBlk->address = address;
      curBlk->offset = 0;
      curBlk->data = &srec->data[curBlk->offset];
      curBlk->length = 0;
      srec->numOfBlks++;
    } else {
      curBlk = &srec->blks[srec->numOfBlks - 1];
      gap = address - (curBlk->address + curBlk->length);
      if (gap < gapThresh) {
        memset(&curBlk->data[curBlk->length], 0xFF, gap);
        curBlk->length += gap;
      }
      if ((curBlk->address + curBlk->length) != address) {
        offset = curBlk->offset + curBlk->length;
        srec->numOfBlks++;
        if (srec->numOfBlks < SREC_MAX_BLK) {
          curBlk = &srec->blks[srec->numOfBlks - 1];
          curBlk->address = address;
          curBlk->offset = offset;
          curBlk->data = &srec->data[curBlk->offset];
          curBlk->length = 0;
        } else {
          srec->numOfBlks--;
          err = -3;
        }
      }
    }
  }

  if (0 == err) {
    for (i = 0; i < (dataLen - addrLen - 1); i++) {
      v = srec_hex(&sline[4 + addrLen * 2 + 2 * i]);
      curBlk->data[curBlk->length++] = v;
      sum += v;
    }

    srec->totalSize += dataLen - addrLen - 1;

    sum = (~sum) & 0xFF;
    checksum = srec_hex(&sline[4 + 2 * (dataLen - 1)]);

    if (sum != checksum) {
      err = -4;
    }
  }

  return err;
}

static void srec_parse(srec_t *srec, FILE *fp) {
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
        err = srec_decode(srec, sline, slen, 2);
        break;
      case '2':
        err = srec_decode(srec, sline, slen, 3);
        break;
      case '3':
        err = srec_decode(srec, sline, slen, 4);
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

srec_t *srec_open_impl(const char *path) {
  srec_t *srec = NULL;
  FILE *fp;
  size_t sz;

  fp = fopen(path, "r");
  if (NULL != fp) {
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    srec = (srec_t *)malloc(sz / 2 + sizeof(srec_t));
    if (NULL != srec) {
      srec->data = (uint8_t *)&srec[1];
      srec->numOfBlks = 0;
      srec->totalSize = 0;
      memset(srec->blks, 0, sizeof(srec->blks));
      srec_parse(srec, fp);
      if (0 == srec->numOfBlks) {
        free(srec);
        srec = NULL;
      } else {
        srec->type = SREC_SRECORD;
      }
    } else {
      printf("no enough memory for %d bytes\n", (int)sz / 2);
    }
  } else {
    printf("srecord %s not exists\n", path);
  }

  return srec;
}

static void srec_add_line(FILE *fpS, uint32_t saddr, uint8_t *data, uint32_t len) {
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

static void srec_add_sign(FILE *fpS, srec_sign_type_t signType, uint32_t saddr, uint32_t crc,
                          uint32_t crcLen) {
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

  srec_add_line(fpS, saddr, data, len);
}

static int srec_sign_v1(const char *path, size_t total, srec_sign_type_t signType) {
  srec_t *srec;
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

  printf("sign %s by %s:\n", path, signTypeName[signType]);

  srec = srec_open(path);
  if (NULL == srec) {
    r = EEXIST;
  } else {
    srec_print(srec);
    saddr = srec_range(srec, &length);
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
    for (i = 0; i < srec->numOfBlks; i++) {
      memcpy(&buffer[srec->blks[i].address - saddr], srec->blks[i].data, srec->blks[i].length);
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

    if (srec->type == SREC_SRECORD) {
      srec_add_sign(fpS, signType, saddr, crc, crcLen);
    } else {
      ihex_add_sign(fpS, signType, saddr, crc, crcLen);
    }
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

static int srec_sign_v2(const char *path, size_t signAddr, srec_sign_type_t signType) {
  int r = 0;
  srec_t *srec;
  int i;
  uint32_t saddr, length;
  uint32_t crc = 0xFFFFFFFFUL;
  FILE *fpO = NULL;
  FILE *fpS = NULL;
  char sline[SREC_MAX_ONE_LINE];
  uint8_t *data = (uint8_t *)sline;

  printf("sign %s by %s:\n", path, signTypeName[signType]);

  srec = srec_open(path);
  if (NULL == srec) {
    r = EEXIST;
  } else {
    srec_print(srec);
    saddr = srec_range(srec, &length);
    if (signAddr < (saddr + length)) {
      printf(" sign address=0x%X not correct\n", (uint32_t)signAddr);
      r = EINVAL;
    }
  }

  if (0 == r) {
    for (i = 0; i < srec->numOfBlks; i++) {
      if (SREC_SIGN_CRC32_V2 == signType) {
        crc = Crc_CalculateCRC32(srec->blks[i].data, srec->blks[i].length, crc, FALSE);
      } else {
        crc = Crc_CalculateCRC16(srec->blks[i].data, srec->blks[i].length,
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

    if (srec->type == SREC_SRECORD) {
      saddr = signAddr;
      data[0] = (srec->numOfBlks >> 24) & 0xFF;
      data[1] = (srec->numOfBlks >> 16) & 0xFF;
      data[2] = (srec->numOfBlks >> 8) & 0xFF;
      data[3] = srec->numOfBlks & 0xFF;
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
      srec_add_line(fpS, saddr, data, 8);
      saddr += 8;

      for (i = 0; i < srec->numOfBlks; i++) {
        data[0] = (srec->blks[i].address >> 24) & 0xFF;
        data[1] = (srec->blks[i].address >> 16) & 0xFF;
        data[2] = (srec->blks[i].address >> 8) & 0xFF;
        data[3] = srec->blks[i].address & 0xFF;
        data[4] = (srec->blks[i].length >> 24) & 0xFF;
        data[5] = (srec->blks[i].length >> 16) & 0xFF;
        data[6] = (srec->blks[i].length >> 8) & 0xFF;
        data[7] = srec->blks[i].length & 0xFF;
        srec_add_line(fpS, saddr, data, 8);
        saddr += 8;
      }
    } else {
      r = ENOTSUP;
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

static int srec_sign_v3(const char *path, srec_sign_type_t signType) {
  int r = 0;
  srec_t *srec;
  int i;
  uint32_t saddr, signAddr = 0, length;
  uint32_t crc = 0xFFFFFFFFUL;
  FILE *fpO = NULL;
  FILE *fpS = NULL;
  char sline[SREC_MAX_ONE_LINE];
  uint8_t *data = (uint8_t *)sline;

  printf("sign %s by %s:\n", path, signTypeName[signType]);

  srec = srec_open(path);
  if (NULL == srec) {
    r = EEXIST;
  } else {
    srec_print(srec);
    saddr = srec_range(srec, &length);
    if (signAddr < (saddr + length)) {
      signAddr = saddr + length;
    }
  }

  if (0 == r) {
    for (i = 0; i < srec->numOfBlks; i++) {
      if (SREC_SIGN_CRC32_V3 == signType) {
        crc = Crc_CalculateCRC32(srec->blks[i].data, srec->blks[i].length, crc, FALSE);
      } else {
        crc = Crc_CalculateCRC16(srec->blks[i].data, srec->blks[i].length,
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

    if (srec->type == SREC_SRECORD) {
      saddr = signAddr;
      for (i = 0; i < srec->numOfBlks; i++) {
        data[0] = (srec->blks[i].address >> 24) & 0xFF;
        data[1] = (srec->blks[i].address >> 16) & 0xFF;
        data[2] = (srec->blks[i].address >> 8) & 0xFF;
        data[3] = srec->blks[i].address & 0xFF;
        data[4] = (srec->blks[i].length >> 24) & 0xFF;
        data[5] = (srec->blks[i].length >> 16) & 0xFF;
        data[6] = (srec->blks[i].length >> 8) & 0xFF;
        data[7] = srec->blks[i].length & 0xFF;
        srec_add_line(fpS, saddr, data, 8);
        saddr += 8;
      }
      data[0] = (srec->numOfBlks >> 24) & 0xFF;
      data[1] = (srec->numOfBlks >> 16) & 0xFF;
      data[2] = (srec->numOfBlks >> 8) & 0xFF;
      data[3] = srec->numOfBlks & 0xFF;
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
      srec_add_line(fpS, saddr, data, 16);
      saddr += 8;
    } else {
      r = ENOTSUP;
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

static uint32_t toU32(const char *strV) {
  uint32_t u32V = 0;

  if ((0 == strncmp("0x", strV, 2)) || (0 == strncmp("0X", strV, 2))) {
    u32V = strtoul(strV, NULL, 16);
  } else {
    u32V = strtoul(strV, NULL, 10);
  }

  return u32V;
}

/* path: in pattern file_path.bin:address */
static srec_t *bin_open_impl(const char *path) {
  srec_t *srec = NULL;
  FILE *fp;
  uint32_t addr;
  size_t sz;
  char filePath[256] = {0};
  char address[128] = {0};

  sscanf(path, "%255[^:]:%127s", filePath, address);

  addr = toU32(address);
  fp = fopen(filePath, "rb");
  if (NULL != fp) {
    printf("binary: %s @ 0x%x\n", filePath, addr);
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    srec = (srec_t *)malloc(sz + sizeof(srec_t));
    if (NULL != srec) {
      srec->data = (uint8_t *)&srec[1];
      srec->numOfBlks = 1;
      srec->totalSize = sz;
      srec->type = SREAC_BIN;
      srec->blks[0].data = srec->data;
      srec->blks[0].address = addr;
      srec->blks[0].offset = 0;
      srec->blks[0].length = sz;
      fread(srec->data, sz, 1, fp);
    } else {
      printf("no enough memory for %d bytes\n", (int)sz);
    }
  } else {
    printf("binary %s not exists\n", filePath);
  }

  return srec;
}
/* ================================ [ FUNCTIONS ] ============================================== */
srec_t *srec_open(const char *path) {
  srec_t *srec;

  if (NULL != strstr(path, ".s19")) {
    srec = srec_open_impl(path);
  } else if (NULL != strstr(path, ".hex")) {
    srec = ihex_open(path);
  } else if (NULL != strstr(path, ".bin")) {
    srec = bin_open_impl(path);
  } else {
    srec = srec_open_impl(path);
    if (NULL == srec) {
      srec = ihex_open(path);
    }
    if (NULL == srec) {
      srec = bin_open_impl(path);
    }
  }
  return srec;
}

int srec_sign(const char *path, size_t total, srec_sign_type_t signType) {
  int r = 0;

  switch (signType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC32:
    r = srec_sign_v1(path, total, signType);
    break;
  case SREC_SIGN_CRC16_V2:
  case SREC_SIGN_CRC32_V2:
    r = srec_sign_v2(path, total, signType);
    break;
  case SREC_SIGN_CRC16_V3:
  case SREC_SIGN_CRC32_V3:
    r = srec_sign_v3(path, signType);
    break;
  default:
    printf("invalid sign type %d\n", signType);
    r = EINVAL;
    break;
  }

  return r;
}

void srec_print(srec_t *srec) {
  int i, j;
  uint16_t crc;
  for (i = 0; i < srec->numOfBlks; i++) {
    printf("srec %d: address=0x%08X, length=0x%08X, offset=0x%08X, data=", i, srec->blks[i].address,
           (uint32_t)srec->blks[i].length, (uint32_t)srec->blks[i].offset);
    for (j = 0; (j < 8) && (j < srec->blks[i].length); j++) {
      printf("%02X", srec->blks[i].data[j]);
    }
    crc = Crc_CalculateCRC16(srec->blks[i].data, srec->blks[i].length, 0xFFFF, TRUE);
    printf(" crc16=%X\n", crc);
  }
}

uint32_t srec_range(srec_t *srec, uint32_t *length) {
  int i;
  uint32_t saddr = 0xFFFFFFFFUL, eaddr = 0;
  for (i = 0; i < srec->numOfBlks; i++) {
    if (srec->blks[i].address < saddr) {
      saddr = srec->blks[i].address;
    }
    if ((srec->blks[i].address + srec->blks[i].length) > eaddr) {
      eaddr = (uint32_t)(srec->blks[i].address + srec->blks[i].length);
    }
  }

  *length = eaddr - saddr;

  return saddr;
}

void srec_close(srec_t *srec) {
  if (NULL != srec) {
    free(srec);
  }
}
