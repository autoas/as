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
/* ================================ [ DATAS     ] ============================================== */
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
/* ================================ [ FUNCTIONS ] ============================================== */
srec_t *srec_open(const char *path) {
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
      }
    } else {
      printf("no enough memory for %d bytes\n", (int)sz / 2);
    }
  } else {
    printf("srecord %s not exists\n", path);
  }

  return srec;
}

int srec_sign(const char *path, size_t total, srec_sign_type_t signType) {
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
  uint8_t checksum;
  char sline[SREC_MAX_ONE_LINE];
  static const char *signTypeName[] = {"CRC16", "CRC32"};

  if (signType >= SREC_SIGN_MAX) {
    printf("invalid sign type %d\n", signType);
    return -11;
  }

  if (SREC_SIGN_CRC32 == signType) {
    crcLen = 4;
  }

  memset(sline, 0, sizeof(sline));

  printf("sign %s by %s:", path, signTypeName[signType]);

  srec = srec_open(path);
  if (NULL == srec) {
    r = -1;
  } else {
    saddr = srec_range(srec, &length);
    if ((total - crcLen) < length) {
      printf(" range=0x%X not correct\n", (uint32_t)total);
      srec_print(srec);
      r = -2;
    }
  }

  if (0 == r) {
    buffer = malloc(total);
    if (NULL == buffer) {
      r = -3;
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
      r = -4;
    } else {
      fpO = fopen(path, "r");
      if (NULL == fpO) {
        r = -5;
      }
    }
  }

  if (0 == r) {
    saddr += total - crcLen;
    checksum = 5 + crcLen;
    checksum +=
      ((saddr >> 24) & 0xFF) + ((saddr >> 16) & 0xFF) + ((saddr >> 8) & 0xFF) + (saddr & 0xFF);
    if (SREC_SIGN_CRC32 == signType) {
      checksum += ((crc >> 24) & 0xFF) + ((crc >> 16) & 0xFF) + ((crc >> 8) & 0xFF) + (crc & 0xFF);
    } else {
      checksum += ((crc >> 8) & 0xFF) + (crc & 0xFF);
    }
    checksum = ~checksum;
    while ((0 == r) && fgets(sline, sizeof(sline), fpO)) {
      fputs(sline, fpS);
    }

    snprintf(sline, sizeof(sline), "S3%02X%08X%04X%02X\n", 5 + crcLen, saddr, crc, checksum);
    fputs(sline, fpS);
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

  return 0;
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
