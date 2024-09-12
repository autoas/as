/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Bit.h"
#ifdef _WIN32
#include <assert.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef STD_BIT_MAX_BYTES
#define STD_BIT_MAX_BYTES 256
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
  int i, j;
  for (i = 0; i < STD_BIT_MAX_BYTES; i++) {
    for (j = 0; j < 8; j++) {
      _bebm[i * 8 + j] = i * 8 + 7 - j;
    }
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
uint32_t Std_BitGetBigEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  /* calculate lsb bit index. */
  uint16_t lsbIndex = ((bitPos ^ 0x7) + bitSize - 1) ^ 7;
  uint8_t nBytes = (bitSize + 7) >> 3;
  const uint8_t *dataPtr = (const uint8_t *)ptr + (bitPos >> 3);
  uint32_t retV = 0;
  uint8_t bitShift = lsbIndex & 0x07;
  uint8_t bitsRead;
  uint32_t mask = 0xFFFFFFFFu >> (32 - bitSize);
  int i;

  for (i = 0; i < nBytes; i++) {
    retV = (retV << 8) | dataPtr[i];
  }

  bitsRead = 8 * nBytes - (7 - (bitPos & 0x07)); /* bits has been read for now */
  if (bitsRead < bitSize) {
    retV <<= (8 - bitShift);
    retV |= dataPtr[nBytes] >> bitShift;
  } else {
    retV >>= bitShift;
  }

  return retV & mask;
}

void Std_BitSetBigEndian(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
  /* calculate lsb bit index. */
  uint16_t lsbIndex = ((bitPos ^ 0x7) + bitSize - 1) ^ 7;
  uint8_t nBytes = (bitSize + 7) >> 3, nBytes2;
  uint8_t *dataPtr = (uint8_t *)ptr + (bitPos >> 3);
  uint8_t bitShift = lsbIndex & 0x07;
  uint8_t bitsWritten;
  uint32_t orgV, value2, newV;
  uint32_t mask, mask2;
  int i;

  mask = 0xFFFFFFFFu >> (32 - bitSize); /* calculate mask for value */
  value &= mask;                        /* mask value; */

  nBytes = (bitSize + 7) >> 3;
  if (bitShift > 0) {
    nBytes2 = (bitSize - (8 - bitShift) + 7) >> 3;
  } else {
    nBytes2 = nBytes;
  }
  orgV = 0;
  for (i = 0; i < nBytes2; i++) {
    orgV = (orgV << 8) | dataPtr[i];
  }

  if (bitShift > 0) {
    value2 = value >> (8 - bitShift);
    mask2 = mask >> (8 - bitShift);
  } else {
    value2 = value;
    mask2 = mask;
  }
  newV = (orgV & (~mask2)) | value2;

  for (i = nBytes2 - 1; i >= 0; i--) {
    dataPtr[i] = newV & 0xFF;
    newV >>= 8;
  }

  if (bitShift > 0) {
    value2 = value << bitShift;
    mask2 = mask << bitShift;
    newV = (dataPtr[nBytes2] & (~mask2)) | value2;
    dataPtr[nBytes2] = newV & 0xFF;
  }

  bitsWritten = 8 * nBytes - (7 - (bitPos & 0x07)); /* bits has been written for now */
  if (bitsWritten < bitSize) {
    orgV = dataPtr[nBytes];
    value2 = value << bitShift;
    mask2 = mask << bitShift;
    newV = (orgV & (~mask2)) | value2;
    dataPtr[nBytes] = newV;
  }
}

uint32_t Std_BitGetLittleEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize) {

  uint8_t bitShift, nBytes, bitsRead;
  const uint8_t *dataPtr = (const uint8_t *)ptr + (bitPos >> 3);
  uint32_t retV = 0;
  uint32_t mask = 0xFFFFFFFFu >> (32 - bitSize);
  int i;

  bitShift = bitPos & 0x07;
  nBytes = (bitSize + 7) >> 3;
  for (i = (int)nBytes - 1; i >= 0; i--) {
    retV = (retV << 8) | dataPtr[i];
  }
  retV >>= bitShift;
  bitsRead = (8 * nBytes - bitShift); /* bits has been read for now */
  if (bitsRead < bitSize) {
    retV |= (uint32_t)dataPtr[nBytes] << bitsRead;
  }

  return retV & mask;
}

void Std_BitSetLittleEndian(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
  uint8_t bitShift, nBytes, bitsWritten;
  uint8_t *dataPtr = (uint8_t *)ptr + (bitPos >> 3);
  uint32_t orgV;
  uint32_t mask;
  uint32_t value2, mask2, newV;
  int i;

  mask = 0xFFFFFFFFu >> (32 - bitSize); /* calculate mask for value */
  value &= mask;                        /* mask value */

  nBytes = (bitSize + 7) >> 3;
  orgV = 0;
  for (i = (int)nBytes - 1; i >= 0; i--) {
    orgV = (orgV << 8) | dataPtr[i];
  }

  bitShift = bitPos & 0x07;
  value2 = value << bitShift;
  mask2 = ~(mask << bitShift);
  newV = (orgV & mask2) | value2;

  for (i = 0; i < nBytes; i++) {
    dataPtr[i] = (uint8_t)newV;
    newV >>= 8;
  }
  bitsWritten = nBytes * 8 - bitShift; /* bits has been written for now */
  if (bitsWritten < bitSize) {
    orgV = (uint32_t)dataPtr[nBytes];
    mask2 = ~(mask >> bitsWritten);
    value2 = value >> bitsWritten;
    newV = (orgV & mask2) | value2;
    dataPtr[nBytes] = (uint8_t)newV & 0xFF;
  }
}

void Std_BitSet(void *ptr, uint16_t bitPos) {
  uint8_t *dataPtr = (uint8_t *)ptr;
  uint16_t bytePos = bitPos >> 3;
  uint8_t bitShift = bitPos & 0x07;
  dataPtr[bytePos] |= 1 << bitShift;
}

void Std_BitClear(void *ptr, uint16_t bitPos) {
  uint8_t *dataPtr = (uint8_t *)ptr;
  uint16_t bytePos = bitPos >> 3;
  uint8_t bitShift = bitPos & 0x07;
  dataPtr[bytePos] &= ~(1 << bitShift);
}

boolean Std_BitGet(const void *ptr, uint16_t bitPos) {
  boolean ret;
  uint8_t *dataPtr = (uint8_t *)ptr;
  uint16_t bytePos = bitPos >> 3;
  uint8_t bitShift = bitPos & 0x07;

  ret = (0 != (dataPtr[bytePos] & (1 << bitShift)));

  return ret;
}

#ifdef _WIN32
uint32_t Std_BitGetBEG(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  int nBit = -1, rBit, i;
  uint32_t value = 0;
  uint8_t rByte;
  const uint8_t *dataPtr = (const uint8_t *)ptr;
  /* nBit = _bebm.index(bitPos) */
  for (i = 0; i < ARRAY_SIZE(_bebm); i++) {
    if (_bebm[i] == bitPos) {
      nBit = i;
      break;
    }
  }
  assert(nBit >= 0);
  for (i = 0; i < bitSize; i++) {
    rBit = _bebm[nBit];
    rByte = rBit / 8;
    if ((dataPtr[rByte] & (1 << (rBit % 8))) != 0) {
      value = (value << 1) + 1;
    } else {
      value = (value << 1) + 0;
      nBit += 1;
    }
  }
  return value;
}

void Std_BitSetBEG(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
  int nBit = -1, wBit, rBit, i;
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
  for (i = 0; i < bitSize; i++) {
    wBit = _bebm[nBit];
    wByte = wBit / 8;
    wBit = wBit % 8;
    if ((value & (1 << rBit)) != 0) {
      dataPtr[wByte] |= 1 << wBit;
    } else {
      dataPtr[wByte] &= ~(1 << wBit);
    }
    nBit += 1;
    rBit -= 1;
  }
}

uint32_t Std_BitGetLEG(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  int nBit, rBit, i;
  uint32_t value = 0;
  uint8_t rByte;
  const uint8_t *dataPtr = (const uint8_t *)ptr;

  nBit = bitPos + bitSize - 1;
  value = 0;
  for (i = 0; i < bitSize; i++) {
    rBit = nBit;
    rByte = rBit / 8;
    if ((dataPtr[rByte] & (1 << (rBit % 8))) != 0) {
      value = (value << 1) + 1;
    } else {
      value = (value << 1) + 0;
    }
    nBit -= 1;
  }

  return value;
}

void Std_BitSetLEG(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
  int nBit, wBit, rBit, i;
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
    if ((value & (1 << rBit)) != 0) {
      dataPtr[wByte] |= 1 << wBit;
    } else {
      dataPtr[wByte] &= ~(1 << wBit);
    }
    nBit -= 1;
    rBit -= 1;
  }
}
#endif
