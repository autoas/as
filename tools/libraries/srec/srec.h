/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef SREC_H
#define SREC_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define SREC_MAX_BLK 32
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t *data;
  uint32_t address;
  size_t offset;
  size_t length;
} sblk_t;

typedef struct {
  uint8_t *data;
  sblk_t blks[SREC_MAX_BLK];
  size_t numOfBlks;
  size_t totalSize;
  enum {
    SREC_SRECORD,
    SREC_IHEX,
    SREAC_BIN
  } type;
} srec_t;

typedef enum {
  /* sign method v1: all sections conbined as 1 section with padding 0xFF and calculate the crc of
   * this 1 section, the crc is stored at the end of this 1 section */
  SREC_SIGN_CRC16,
  SREC_SIGN_CRC32,
  /* sign method v2: loop each section to calclulate the crc, and save the section address&size
   * information and the crc to the end */
  SREC_SIGN_CRC16_V2,
  SREC_SIGN_CRC32_V2,
  /* sign method v3: just append the sign info at the end of last section */
  SREC_SIGN_CRC16_V3,
  SREC_SIGN_CRC32_V3,
  SREC_SIGN_MAX
} srec_sign_type_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
srec_t *srec_open(const char *path);
int srec_sign(const char *path, size_t total, srec_sign_type_t signType);
void srec_print(srec_t *srec);
uint32_t srec_range(srec_t *srec, uint32_t *length);
void srec_close(srec_t *srec);

srec_t *ihex_open(const char *path);
#ifdef __cplusplus
}
#endif
#endif /* SREC_H */
