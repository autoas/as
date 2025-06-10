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
/* ================================ [ FUNCTIONS ] ============================================== */
uint32_t Std_BitGetBigEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  /* calculate lsb bit index. */
  uint16_t lsbIndex = ((bitPos ^ 0x7u) + bitSize - 1u) ^ 7u;
  uint8_t nBytes = (bitSize + 7u) >> 3;
  const uint8_t *dataPtr = (const uint8_t *)ptr + (bitPos >> 3);
  uint32_t retV = 0;
  uint8_t bitShift = lsbIndex & 0x07u;
  uint8_t bitsRead;
  uint32_t mask = 0xFFFFFFFFul >> (32u - bitSize);
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

void Std_BitSetBigEndian(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
  /* calculate lsb bit index. */
  uint16_t lsbIndex = ((bitPos ^ 0x7u) + bitSize - 1u) ^ 7u;
  uint8_t nBytes = (bitSize + 7u) >> 3;
  uint8_t nBytes2;
  uint8_t *dataPtr = (uint8_t *)ptr + (bitPos >> 3);
  uint8_t bitShift = lsbIndex & 0x07u;
  uint8_t bitsWritten;
  uint32_t orgV;
  uint32_t value2;
  uint32_t newV;
  uint32_t mask;
  uint32_t mask2;
  int16_t i;
  uint32_t u32V = value;

  mask = 0xFFFFFFFFul >> (32u - bitSize); /* calculate mask for value */
  u32V &= mask;                           /* mask value; */

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
    value2 = u32V >> (8u - bitShift);
    mask2 = mask >> (8u - bitShift);
  } else {
    value2 = u32V;
    mask2 = mask;
  }
  newV = (orgV & (~mask2)) | value2;

  for (i = (int16_t)nBytes2 - 1; i >= 0; i--) {
    dataPtr[i] = newV & 0xFFu;
    newV >>= 8;
  }

  if (bitShift > 0) {
    value2 = u32V << bitShift;
    mask2 = mask << bitShift;
    newV = (dataPtr[nBytes2] & (~mask2)) | value2;
    dataPtr[nBytes2] = newV & 0xFF;
  }

  bitsWritten = (8 * nBytes) - (7u - (bitPos & 0x07u)); /* bits has been written for now */
  if (bitsWritten < bitSize) {
    orgV = dataPtr[nBytes];
    value2 = u32V << bitShift;
    mask2 = mask << bitShift;
    newV = (orgV & (~mask2)) | value2;
    dataPtr[nBytes] = newV;
  }
}

uint32_t Std_BitGetLittleEndian(const void *ptr, uint16_t bitPos, uint8_t bitSize) {

  uint8_t bitShift;
  uint8_t nBytes;
  uint8_t bitsRead;
  const uint8_t *dataPtr = (const uint8_t *)ptr + (bitPos >> 3);
  uint32_t retV = 0;
  uint32_t mask = 0xFFFFFFFFul >> (32u - bitSize);
  int16_t i;

  bitShift = bitPos & 0x07u;
  nBytes = (bitSize + 7u) >> 3;
  for (i = nBytes - 1u; i >= 0; i--) {
    retV = (retV << 8) | dataPtr[i];
  }
  retV >>= bitShift;
  bitsRead = (8 * nBytes) - bitShift; /* bits has been read for now */
  if (bitsRead < bitSize) {
    retV |= (uint32_t)dataPtr[nBytes] << bitsRead;
  }

  return retV & mask;
}

void Std_BitSetLittleEndian(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
  uint8_t bitShift;
  uint8_t nBytes;
  uint8_t bitsWritten;
  uint8_t *dataPtr = (uint8_t *)ptr + (bitPos >> 3);
  uint32_t orgV;
  uint32_t mask;
  uint32_t value2;
  uint32_t mask2;
  uint32_t newV;
  uint32_t u32V = value;
  int16_t i;

  mask = 0xFFFFFFFFul >> (32u - bitSize); /* calculate mask for value */
  u32V &= mask;                           /* mask value */

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
    orgV = (uint32_t)dataPtr[nBytes];
    mask2 = ~(mask >> bitsWritten);
    value2 = u32V >> bitsWritten;
    newV = (orgV & mask2) | value2;
    dataPtr[nBytes] = (uint8_t)newV & 0xFFu;
  }
}

void Std_BitSet(void *ptr, uint16_t bitPos) {
  uint8_t *dataPtr = (uint8_t *)ptr;
  uint16_t bytePos = bitPos >> 3;
  uint8_t bitShift = bitPos & 0x07u;
  dataPtr[bytePos] |= 1u << bitShift;
}

void Std_BitClear(void *ptr, uint16_t bitPos) {
  uint8_t *dataPtr = (uint8_t *)ptr;
  uint16_t bytePos = bitPos >> 3;
  uint8_t bitShift = bitPos & 0x07u;
  dataPtr[bytePos] &= ~(1u << bitShift);
}

boolean Std_BitGet(const void *ptr, uint16_t bitPos) {
  boolean ret;
  const uint8_t *dataPtr = (const uint8_t *)ptr;
  uint16_t bytePos = bitPos >> 3;
  uint8_t bitShift = bitPos & 0x07u;

  ret = (0u != (dataPtr[bytePos] & (1u << bitShift)));

  return ret;
}

/* for example for a 32 bits, dstBitPos = 1
 * 0                 8                16                24            31 <-  bitPos
 * 7 6 5 4 3 2 1 0 - 7 6 5 4 3 2 1 0 - 7 6 5 4 3 2 1 0 - 7 6 5 4 3 2 1 0
 * & . . . a b c d   e f g h i j k l   m n o p q r s t   u v w x y z ! @
 * & a b c d e f g   h i j k l m n o   p q r s t u v w   x y z ! @ . . .
 * Initial: dstBP = 1, srcBP = 4, bs = 28
 * Loop 0: dstByte = 0, dstShift = 6, srcByte = 0, srcShift = 3, shift = 3, doSize = 4
 *         -> mask = 0x7f, dstBP = 5, srcBP = 8, bs = 24
 * Loop 1: dstByte = 0, dstShift = 2, srcByte = 1, srcShift = 7, shift = 5, doSize = 3
 *         -> dstBP = 8, srcBP = 12, bs = 21
 * Loop 2: dstByte = 1, dstShift = 7, srcByte = 1, srcShift = 4, shift = 3, doSize = 5
 *         -> dstBP = 13, srcBP = 17, bs = 16
 */
void Std_BitCopy(uint8_t *dst, uint16_t dstBitPos, uint8_t *src, uint16_t srcBitPos,
                 uint16_t bitSize) {

  uint16_t dstBP = dstBitPos;
  uint16_t srcBp = srcBitPos;
  uint16_t bs = bitSize;
  uint16_t dstByte;
  uint8_t dstShift;
  uint8_t mask;
  uint16_t srcByte;
  uint8_t srcShift;
  uint8_t doSize;
  uint8_t shift;
  while (bs > 0u) {
    dstByte = dstBP >> 3;
    dstShift = 7u - (dstBP & 0x7u);
    mask = (1u << (dstShift + 1u)) - 1u;
    srcByte = srcBp >> 3;
    srcShift = 7u - (srcBp & 0x7u);
    if (dstShift >= srcShift) { /* shift left */
      shift = dstShift - srcShift;
      doSize = srcShift + 1u;
      if (bs < doSize) {
        doSize = bs;
      }
      mask &= ~((1u << (dstShift + 1u - doSize)) - 1u);
      dst[dstByte] &= ~mask;
      dst[dstByte] |= (src[srcByte] << shift) & mask;
    } else { /* shift right  */
      shift = srcShift - dstShift;
      doSize = dstShift + 1u;
      if (bs < doSize) {
        doSize = bs;
      }
      mask &= ~((1u << (dstShift + 1u - doSize)) - 1u);
      dst[dstByte] &= ~mask;
      dst[dstByte] |= (src[srcByte] >> shift) & mask;
    }
    dstBP += doSize;
    srcBp += doSize;
    bs -= doSize;
  }
}

#ifdef _WIN32
uint32_t Std_BitGetBEG(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  int32_t nBit = -1;
  int32_t rBit;
  uint32_t i;
  uint32_t value = 0u;
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

void Std_BitSetBEG(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
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
    if ((value & (1u << rBit)) != 0u) {
      dataPtr[wByte] |= 1u << wBit;
    } else {
      dataPtr[wByte] &= ~(1u << wBit);
    }
    nBit += 1;
    rBit -= 1;
  }
}

uint32_t Std_BitGetLEG(const void *ptr, uint16_t bitPos, uint8_t bitSize) {
  int32_t nBit;
  int32_t rBit;
  uint32_t i;
  uint32_t value = 0;
  uint8_t rByte;
  const uint8_t *dataPtr = (const uint8_t *)ptr;

  nBit = bitPos + bitSize - 1;
  value = 0;
  for (i = 0; i < bitSize; i++) {
    rBit = nBit;
    rByte = rBit / 8u;
    if ((dataPtr[rByte] & (1 << (rBit % 8u))) != 0u) {
      value = (value << 1) + 1;
    } else {
      value = (value << 1);
    }
    nBit -= 1;
  }

  return value;
}

void Std_BitSetLEG(void *ptr, uint32_t value, uint16_t bitPos, uint8_t bitSize) {
  int32_t nBit;
  int32_t wBit;
  int32_t rBit;
  uint32_t i;
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

void Std_BitCopyG(uint8_t *dst, uint16_t dstBitPos, uint8_t *src, uint16_t srcBitPos,
                  uint16_t bitSize) {
  uint16_t dstBP = dstBitPos;
  uint16_t srcBp = srcBitPos;
  uint16_t i;
  uint16_t dstByte;
  uint8_t dstShift;
  uint16_t srcByte;
  uint8_t srcShift;
  for (i = 0; i < bitSize; i++) {
    dstByte = dstBP >> 3;
    dstShift = 7u - (dstBP & 0x7u);
    srcByte = srcBp >> 3;
    srcShift = 7u - (srcBp & 0x7u);
    if (0 != (src[srcByte] & (1u << srcShift))) { /* bit set */
      dst[dstByte] |= (1u << dstShift);
    } else { /* shift right  */
      dst[dstByte] &= ~(1u << dstShift);
    }
    dstBP += 1;
    srcBp += 1;
  }
}
#endif
