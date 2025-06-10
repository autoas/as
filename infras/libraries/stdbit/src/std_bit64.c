/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Bit.h"
#ifdef USE_STD_BIT64
#ifdef _WIN32
#include <assert.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef STD_BIT_MAX_BYTES
#define STD_BIT_MAX_BYTES 256u
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#ifdef _WIN32
static int _bebm[STD_BIT_MAX_BYTES * 8];
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
INITIALIZER(_init_bebm) {
  uint16_t i;
  uint16_t j;
  for (i = 0u; i < STD_BIT_MAX_BYTES; i++) {
    for (j = 0u; j < 8u; j++) {
      _bebm[(i * 8u) + j] = (i * 8u) + 7 - j;
    }
  }
}
#endif
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
uint64_t Std_Bit64GetBigEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  /* calculate lsb bit index. */
  uint16_t lsbIndex = ((bitPos ^ 0x7u) + bitSize - 1u) ^ 7u;
  uint8_t nBytes = (bitSize + 7u) >> 3;
  const uint8_t *dataPtr = (const uint8_t *)ptr + (bitPos >> 3);
  uint64_t retV = 0;
  uint8_t bitShift = lsbIndex & 0x07u;
  uint8_t bitsRead;
  uint64_t mask = 0xFFFFFFFFFFFFFFFFul >> (64u - bitSize);
  uint16_t i;

  for (i = 0u; i < nBytes; i++) {
    retV = (retV << 8) | dataPtr[i];
  }

  bitsRead = (8 * nBytes) - (7u - (bitPos & 0x07u)); /* bits has been read for now */
  if (bitsRead < bitSize) {
    retV <<= (8u - bitShift);
    retV |= dataPtr[nBytes] >> bitShift;
  } else {
    retV >>= bitShift;
  }

  return retV & mask;
}

void Std_Bit64SetBigEndian(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize) {
  /* calculate lsb bit index. */
  uint16_t lsbIndex = ((bitPos ^ 0x7u) + bitSize - 1u) ^ 7u;
  uint8_t nBytes = (bitSize + 7u) >> 3;
  uint8_t nBytes2;
  uint8_t *dataPtr = (uint8_t *)ptr + (bitPos >> 3);
  uint8_t bitShift = lsbIndex & 0x07u;
  uint8_t bitsWritten;
  uint64_t orgV;
  uint64_t value2;
  uint64_t newV;
  uint64_t mask;
  uint64_t mask2;
  int16_t i;
  uint64_t u64V = value;

  mask = 0xFFFFFFFFFFFFFFFFul >> (64u - bitSize); /* calculate mask for value */
  u64V &= mask;                                   /* mask value; */

  nBytes = (bitSize + 7u) >> 3u;
  if (bitShift > 0u) {
    nBytes2 = (bitSize - (8u - bitShift) + 7u) >> 3;
  } else {
    nBytes2 = nBytes;
  }
  orgV = 0u;
  for (i = 0; i < (int16_t)nBytes2; i++) {
    orgV = (orgV << 8) | dataPtr[i];
  }

  if (bitShift > 0u) {
    value2 = u64V >> (8u - bitShift);
    mask2 = mask >> (8u - bitShift);
  } else {
    value2 = u64V;
    mask2 = mask;
  }
  newV = (orgV & (~mask2)) | value2;

  for (i = (int16_t)nBytes2 - 1; i >= 0; i--) {
    dataPtr[i] = newV & 0xFFu;
    newV >>= 8;
  }

  if (bitShift > 0) {
    value2 = u64V << bitShift;
    mask2 = mask << bitShift;
    newV = (dataPtr[nBytes2] & (~mask2)) | value2;
    dataPtr[nBytes2] = newV & 0xFF;
  }

  bitsWritten = (8 * nBytes) - (7u - (bitPos & 0x07u)); /* bits has been written for now */
  if (bitsWritten < bitSize) {
    orgV = dataPtr[nBytes];
    value2 = u64V << bitShift;
    mask2 = mask << bitShift;
    newV = (orgV & (~mask2)) | value2;
    dataPtr[nBytes] = newV;
  }
}

uint64_t Std_Bit64GetLittleEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize) {

  uint8_t bitShift;
  uint8_t nBytes;
  uint8_t bitsRead;
  const uint8_t *dataPtr = (const uint8_t *)ptr + (bitPos >> 3);
  uint64_t retV = 0;
  uint64_t mask = 0xFFFFFFFFFFFFFFFFul >> (64u - bitSize);
  int16_t i;

  bitShift = bitPos & 0x07u;
  nBytes = (bitSize + 7u) >> 3;
  for (i = nBytes - 1u; i >= 0; i--) {
    retV = (retV << 8) | dataPtr[i];
  }
  retV >>= bitShift;
  bitsRead = (8 * nBytes) - bitShift; /* bits has been read for now */
  if (bitsRead < bitSize) {
    retV |= (uint64_t)dataPtr[nBytes] << bitsRead;
  }

  return retV & mask;
}

void Std_Bit64SetLittleEndian(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize) {
  uint8_t bitShift;
  uint8_t nBytes;
  uint8_t bitsWritten;
  uint8_t *dataPtr = (uint8_t *)ptr + (bitPos >> 3);
  uint64_t orgV;
  uint64_t mask;
  uint64_t value2;
  uint64_t mask2;
  uint64_t newV;
  uint64_t u32V = value;
  int16_t i;

  mask = 0xFFFFFFFFFFFFFFFFul >> (64u - bitSize); /* calculate mask for value */
  u32V &= mask;                                   /* mask value */

  nBytes = (bitSize + 7u) >> 3;
  orgV = 0;
  for (i = (int16_t)nBytes - 1; i >= 0; i--) {
    orgV = (orgV << 8) | dataPtr[i];
  }

  bitShift = bitPos & 0x07u;
  value2 = u32V << bitShift;
  mask2 = ~(mask << bitShift);
  newV = (orgV & mask2) | value2;

  for (i = 0; i < (int16_t)nBytes; i++) {
    dataPtr[i] = (uint8_t)newV;
    newV >>= 8;
  }
  bitsWritten = (nBytes * 8) - bitShift; /* bits has been written for now */
  if (bitsWritten < bitSize) {
    orgV = (uint64_t)dataPtr[nBytes];
    mask2 = ~(mask >> bitsWritten);
    value2 = u32V >> bitsWritten;
    newV = (orgV & mask2) | value2;
    dataPtr[nBytes] = (uint8_t)newV & 0xFFu;
  }
}

#ifdef _WIN32
uint64_t Std_Bit64GetBEG(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  int32_t nBit = -1;
  int32_t rBit;
  uint64_t i;
  uint64_t value = 0u;
  uint8_t rByte;
  const uint8_t *dataPtr = (const uint8_t *)ptr;
  /* nBit = _bebm.index(bitPos) */
  for (i = 0u; i < ARRAY_SIZE(_bebm); i++) {
    if (_bebm[i] == bitPos) {
      nBit = i;
      break;
    }
  }
  assert(nBit >= 0);
  for (i = 0u; i < bitSize; i++) {
    rBit = _bebm[nBit];
    rByte = rBit / 8u;
    if ((dataPtr[rByte] & (1u << (rBit % 8u))) != 0u) {
      value = (value << 1) + 1u;
    } else {
      value = (value << 1) + 0u;
      nBit += 1;
    }
  }
  return value;
}

void Std_Bit64SetBEG(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize) {
  int32_t nBit = -1;
  int32_t wBit;
  int32_t rBit;
  uint16_t i;
  uint8_t wByte;
  uint8_t *dataPtr = (uint8_t *)ptr;
  rBit = bitSize - 1;
  /* nBit = _bebm.index(bitPos) */
  for (i = 0; i < ARRAY_SIZE(_bebm); i++) {
    if (_bebm[i] == bitPos) {
      nBit = i;
      break;
    }
  }
  assert(nBit >= 0);
  wByte = 0;
  wBit = 0;
  for (i = 0u; i < bitSize; i++) {
    wBit = _bebm[nBit];
    wByte = wBit / 8;
    wBit = wBit % 8;
    if ((value & ((uint64_t)1 << rBit)) != 0u) {
      dataPtr[wByte] |= (uint64_t)1 << wBit;
    } else {
      dataPtr[wByte] &= ~((uint64_t)1 << wBit);
    }
    nBit += 1;
    rBit -= 1;
  }
}

uint64_t Std_Bit64GetLEG(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  int32_t nBit;
  int32_t rBit;
  uint64_t i;
  uint64_t value = 0;
  uint8_t rByte;
  const uint8_t *dataPtr = (const uint8_t *)ptr;

  nBit = bitPos + bitSize - 1;
  value = 0;
  for (i = 0; i < bitSize; i++) {
    rBit = nBit;
    rByte = rBit / 8u;
    if ((dataPtr[rByte] & ((uint64_t)1 << (rBit % 8u))) != 0u) {
      value = (value << 1) + 1;
    } else {
      value = (value << 1);
    }
    nBit -= 1;
  }

  return value;
}

void Std_Bit64SetLEG(void *ptr, uint64_t value, uint16_t bitPos, uint8_t bitSize) {
  int32_t nBit;
  int32_t wBit;
  int32_t rBit;
  uint64_t i;
  uint8_t wByte;
  uint8_t *dataPtr = (uint8_t *)ptr;
  rBit = bitSize - 1;
  nBit = bitPos + bitSize - 1;
  wByte = 0;
  wBit = 0;
  for (i = 0; i < bitSize; i++) {
    wBit = nBit;
    wByte = wBit / 8;
    wBit = wBit % 8;
    if ((value & ((uint64_t)1 << rBit)) != 0) {
      dataPtr[wByte] |= (uint64_t)1 << wBit;
    } else {
      dataPtr[wByte] &= ~((uint64_t)1 << wBit);
    }
    nBit -= 1;
    rBit -= 1;
  }
}
#endif
#endif /* USE_STD_BIT64 */
