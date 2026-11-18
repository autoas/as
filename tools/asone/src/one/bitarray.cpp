/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#include "bitarray.hpp"
#include "Std_Bit.h"
#include <stdio.h>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

void BitArray::Clear(void) {
  m_BitCount = 0;
}

BitArray::BitArray(Endian endian) : m_Endian(endian) {
  m_Capability = 4096;
  m_Buffer.reserve(m_Capability);
  Clear();
}

BitArray::BitArray(uint32_t nBits, Endian endian) : m_Endian(endian) {
  m_Capability = (nBits + 7) / 8;
  m_Buffer.reserve(m_Capability);
  Clear();
}

BitArray::BitArray(std::vector<uint8_t> &data, Endian endian) : m_Endian(endian) {
  m_Capability = data.size();
  m_Buffer.resize(m_Capability);
  memcpy(m_Buffer.data(), data.data(), data.size());
  Clear();
}

BitArray::BitArray(const uint8_t *data, uint32_t nBytes, Endian endian) : m_Endian(endian) {
  m_Capability = nBytes;
  m_Buffer.resize(m_Capability);
  memcpy(m_Buffer.data(), data, nBytes);
  Clear();
}

uint32_t BitArray::Pos() {
  uint32_t bitPos;
  if (Endian::BIG == m_Endian) {
    uint32_t byteI = m_BitCount >> 3;
    uint32_t bitI = m_BitCount & 0x07;
    bitPos = (byteI << 3) + 7 - bitI;
  } else {
    bitPos = m_BitCount;
  }

  return bitPos;
}

void BitArray::Put(uint32_t u32, uint8_t nBits) {
  size_t sBytes = m_Buffer.size();
  auto nBytes = (m_BitCount + nBits + 7) / 8;
  if (nBytes != sBytes) {
    m_Buffer.resize(nBytes);
    for (size_t i = sBytes; i < nBytes; i++) {
      m_Buffer.data()[i] = 0;
    }
  }

  if (Endian::BIG == m_Endian) {
    Std_BitSetBigEndian(m_Buffer.data(), u32, (uint16_t)Pos(), nBits);
  } else {
    Std_BitSetLittleEndian(m_Buffer.data(), u32, (uint16_t)Pos(), nBits);
  }
  m_BitCount += nBits;
}

uint32_t BitArray::Get(uint8_t nBits) {
  uint32_t u32 = 0;

  if ((m_Buffer.size() * 8) >= (nBits + m_BitCount)) {
    if (Endian::BIG == m_Endian) {
      u32 = Std_BitGetBigEndian(m_Buffer.data(), (uint16_t)Pos(), nBits);
    } else {
      u32 = Std_BitGetLittleEndian(m_Buffer.data(), (uint16_t)Pos(), nBits);
    }
    m_BitCount += nBits;
  } else {
    throw std::runtime_error("bitarray: no enough bits left");
  }

  return u32;
}

uint32_t BitArray::Left() {
  return m_Buffer.size() * 8 - m_BitCount;
}

std::vector<uint8_t> BitArray::LeftBytes(void) {
  std::vector<uint8_t> res;
  uint32_t curPos = m_BitCount;
  size_t leftNBits = Left();

  for (uint32_t i = 0; i < leftNBits; i += 8) {
    uint8_t nBits = 8;
    if (nBits > Left()) {
      nBits = Left();
    }
    uint8_t u8 = (uint8_t)Get(nBits);
    res.push_back(u8);
  }
  m_BitCount = curPos;

  return res;
}

void BitArray::Drop(uint32_t nBits) {
  if ((m_Buffer.size() * 8) >= (nBits + m_BitCount)) {
    m_BitCount += nBits;
  } else {
    m_BitCount = m_Buffer.size() * 8;
    throw std::runtime_error("bitarray: consume too much bits");
  }
}

std::vector<uint8_t> &BitArray::Bytes(void) {
  return m_Buffer;
}

BitArray::~BitArray() {
}
} /* namespace as */
