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
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
uint32_t Std_BitGetBigEndian(const void* ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetBigEndian(void* ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);
uint32_t Std_BitGetLittleEndian(const void* ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetLittleEndian(void* ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);

void Std_BitSet(void* ptr, uint16_t bitPos);
void Std_BitClear(void* ptr, uint16_t bitPos);
boolean Std_BitGet(const void* ptr, uint16_t bitPos);
#ifdef _WIN32
/* Golden algorithm to test above API */
uint32_t Std_BitGetBEG(const void* ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetBEG(void* ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);
uint32_t Std_BitGetLEG(const void* ptr, uint16_t bitPos, uint8_t bitSize);
void Std_BitSetLEG(void* ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize);
#endif
#ifdef __cplusplus
}
#endif
#endif /* _STD_BIT_H */