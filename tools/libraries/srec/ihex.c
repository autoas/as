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

#define IHEX_MINUMU_LENGTH 11

#define IHEX_TYPE_DATA_RECORD 0x00
#define IHEX_TYPE_END_RECORD 0x01
#define IHEX_TYPE_EXTEND_SEGMENT_ADDR 0x02
#define IHEX_TYPE_START_SEGMENT_ADDR 0x03
#define IHEX_TYPE_EXTEND_LINEAR_ADDR 0x04
#define IHEX_TYPE_START_LINEAR_ADDR 0x05
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t CC; /* Character Count */
  uint16_t addr;
  uint8_t type;
  uint8_t data[IHEX_MAX_ONE_LINE / 2];
} ihex_line_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t ihex_hex(const char *str) {
  char hexs[3];
  hexs[0] = str[0];
  hexs[1] = str[1];
  hexs[2] = '\0';

  return (uint8_t)strtoul(hexs, NULL, 16);
}

static int ihex_decode(const char *sline, int slen, ihex_line_t *iln) {
  int err = 0;
  int i;
  uint8_t checksum = 0;
  uint8_t sum = 0;

  iln->CC = ihex_hex(&sline[1]);
  if ((iln->CC * 2 + IHEX_MINUMU_LENGTH) > slen) {
    err = -__LINE__;
  }

  iln->addr = ihex_hex(&sline[3]);
  iln->addr = (iln->addr << 8) + ihex_hex(&sline[5]);
  iln->type = ihex_hex(&sline[7]);

  sum = iln->CC + ((iln->addr >> 8) & 0xFF) + (iln->addr & 0xFF) + iln->type;

  for (i = 0; i < iln->CC; i++) {
    iln->data[i] = ihex_hex(&sline[9 + i * 2]);
    sum += iln->data[i];
  }

  sum = 1 + (~sum);

  checksum = ihex_hex(&sline[9 + iln->CC * 2]);

  if (sum != checksum) {
    err = -__LINE__;
  }

  return err;
}

static int ihex_get_extend_segment_addr(ihex_line_t *iln, uint32_t *addr) {
  int err = 0;

  if (2 == iln->CC) {
    *addr = ((uint32_t)iln->data[0] << 12) + ((uint32_t)iln->data[1] << 4);
  } else {
    err = -__LINE__;
  }

  return err;
}

static int ihex_get_extend_linear_addr(ihex_line_t *iln, uint32_t *addr) {
  int err = 0;

  if (2 == iln->CC) {
    *addr = ((uint32_t)iln->data[0] << 24) + ((uint32_t)iln->data[1] << 16);
  } else {
    err = -__LINE__;
  }

  return err;
}

static int ihex_get_start_linear_addr(ihex_line_t *iln) {
  int err = 0;
  uint32_t addr;

  if (4 == iln->CC) {
    addr = ((uint32_t)iln->data[0] << 24) + ((uint32_t)iln->data[1] << 16) +
           ((uint32_t)iln->data[2] << 8) + iln->data[3];
    printf("  Start Linear Address: <0x%x>\n", addr);
  } else {
    err = -__LINE__;
  }

  return err;
}

static int ihex_add_data(ihex_line_t *iln, srec_t *srec, uint32_t base_addr) {
  int err = 0;
  sblk_t *curBlk = NULL;
  uint32_t address = base_addr + iln->addr;
  uint32_t offset;

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
        err = -__LINE__;
      }
    }
  }

  if (0 == err) {
    memcpy(&curBlk->data[curBlk->length], iln->data, iln->CC);
    curBlk->length += iln->CC;
    srec->totalSize += iln->CC;
  }
  return err;
}

static void ihex_parse(srec_t *srec, FILE *fp) {
  int err = 0;
  char sline[IHEX_MAX_ONE_LINE];
  uint32_t addr = 0;
  int slen;
  int linno = 0;
  ihex_line_t iln;

  while ((0 == err) && fgets(sline, sizeof(sline), fp)) {
    slen = strlen(sline);
    if (sline[0] == '\n') {
      /* do nothing */
    } else if (sline[0] != ':') {
      printf("  invalid intel hex <%s>, ignore\n", sline);
    } else {
      err = ihex_decode(sline, slen, &iln);
      if (0 == err) {
        switch (iln.type) {
        case IHEX_TYPE_DATA_RECORD:
          err = ihex_add_data(&iln, srec, addr);
          break;
        case IHEX_TYPE_END_RECORD:
          break;
        case IHEX_TYPE_EXTEND_SEGMENT_ADDR:
          err = ihex_get_extend_segment_addr(&iln, &addr);
          break;
        case IHEX_TYPE_START_SEGMENT_ADDR:
          printf(" type 03 not supported, ignore\n");
          break;
        case IHEX_TYPE_EXTEND_LINEAR_ADDR:
          err = ihex_get_extend_linear_addr(&iln, &addr);
          break;
        case IHEX_TYPE_START_LINEAR_ADDR:
          err = ihex_get_start_linear_addr(&iln);
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
srec_t *ihex_open(const char *path) {
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
      ihex_parse(srec, fp);
      if (0 == srec->numOfBlks) {
        free(srec);
        srec = NULL;
      } else {
        srec->type = SREC_IHEX;
      }
    } else {
      printf("no enough memory for %d bytes\n", (int)sz / 2);
    }
  } else {
    printf("ihex %s not exists\n", path);
  }

  return srec;
}

void ihex_add_sign(FILE *fpS, srec_sign_type_t signType, uint32_t saddr, uint32_t crc,
                   uint32_t crcLen) {
  uint16_t base_addr = (saddr >> 16) & 0xFFFF;
  uint16_t addr = saddr & 0xFFFF;
  uint8_t checksum;
  char sline[IHEX_MAX_ONE_LINE];
  checksum = 2 + 4 + ((base_addr >> 8) & 0xFF) + (base_addr & 0xFF);
  checksum = 1 + (~checksum);
  snprintf(sline, sizeof(sline), "\n:02000004%04X%02X\n", base_addr, checksum);
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

void ihex_add_line(FILE *fpS, uint32_t saddr, uint8_t *data, uint32_t len) {
  uint16_t base_addr = (saddr >> 16) & 0xFFFF;
  uint16_t addr = saddr & 0xFFFF;
  uint8_t checksum;
  uint32_t i = 0;
  int ls;
  char sline[IHEX_MAX_ONE_LINE];
  checksum = 2 + 4 + ((base_addr >> 8) & 0xFF) + (base_addr & 0xFF);
  checksum = 1 + (~checksum);
  snprintf(sline, sizeof(sline), ":02000004%04X%02X\n", base_addr, checksum);
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