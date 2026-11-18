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
  uint8_t *Data;
  uint32_t Address;
  size_t Offset;
  size_t Length;
} Srec_BlockType;

typedef struct {
  uint8_t *Data;
  Srec_BlockType Blks[SREC_MAX_BLK];
  size_t NumOfBlks;
  size_t TotalSize;
  enum {
    SREC_SRECORD,
    SREC_IHEX,
    SREC_BIN
  } Type;
} Srec_FileType;

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
} Srec_SignType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Srec_FileType *Srec_Open(const char *path);
int Srec_Sign(const char *path, size_t total, Srec_SignType signType);
void Srec_Print(Srec_FileType *srec);
uint32_t Srec_Range(Srec_FileType *srec, uint32_t *length);
void Srec_Close(Srec_FileType *srec);

Srec_FileType *IHex_Open(const char *path);
#ifdef __cplusplus
}
#endif
#endif /* SREC_H */
