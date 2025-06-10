/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _STD_BIT_H
#define _STD_BIT_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef USE_STD_BIT64
#ifdef _WIN32
#define USE_STD_BIT64
#endif
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
uint32_t Std_BitGetBigEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetBigEndian(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);
uint32_t Std_BitGetLittleEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetLittleEndian(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);

#ifdef USE_STD_BIT64
uint64_t Std_Bit64GetBigEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_Bit64SetBigEndian(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize);
uint64_t Std_Bit64GetLittleEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_Bit64SetLittleEndian(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize);
#endif

void Std_BitSet(void *ptr, uint16_t bitPos);
void Std_BitClear(void *ptr, uint16_t bitPos);
boolean Std_BitGet(const void *ptr, uint16_t bitPos);

void Std_BitCopy(uint8_t *dst, uint16_t dstBitPos, uint8_t *src, uint16_t srcBitPos,
                 uint16_t bitSize);

#ifdef _WIN32
/* Golden algorithm to test above API */
uint32_t Std_BitGetBEG(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetBEG(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);
uint32_t Std_BitGetLEG(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetLEG(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);

uint64_t Std_Bit64GetBEG(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_Bit64SetBEG(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize);
uint64_t Std_Bit64GetLEG(const void *ptr, uint16_t bitPos, uint8_t bitSize);
void Std_Bit64SetLEG(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize);

void Std_BitCopyG(uint8_t *dst, uint16_t dstBitPos, uint8_t *src, uint16_t srcBitPos,
                  uint16_t bitSize);
#endif
#ifdef __cplusplus
}
#endif
#endif /* _STD_BIT_H */
