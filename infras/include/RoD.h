/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ROD_H
#define ROD_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define ROD_MAGIC_NUMBER                                                                           \
  (((uint32_t)'D' << 24) | ((uint32_t)'A' << 16) | ((uint32_t)'d' << 8) | ((uint32_t)'a'))

#if defined(__CC_ARM) || defined(__GNUC__) || defined(__ARMCC_VERSION)
#define ROD_CONST_ENTRY __attribute__((section("ROD_ENTRY")))
#define ROD_CONST __attribute__((section("ROD_CONST")))
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  const void *Data;
  uint16_t Size;
  uint16_t Crc;
  uint16_t InvCrc;
} RoD_DataType;

typedef struct {
  const RoD_DataType *Datas;
  uint32_t Magic;
  uint32_t InvMagic;
  uint16_t NumOfData;
  uint16_t InvNumOfData;
} RoD_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Rod_ReadData(const RoD_ConfigType *config, uint16_t dataId, const void **pData,
                            uint16_t *size);
#endif /* ROD_H */
