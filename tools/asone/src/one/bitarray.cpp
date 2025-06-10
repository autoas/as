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

void BitArray::clear(void) {
  m_nBits = 0;
}

BitArray::BitArray(Endian endian) : m_Endian(endian) {
  m_Capability = 4096;
  m_Buffer.reserve(m_Capability);
  clear();
}

BitArray::BitArray(uint32_t nBits, Endian endian) : m_Endian(endian) {
  m_Capability = (nBits + 7) / 8;
  m_Buffer.reserve(m_Capability);
  clear();
}

BitArray::BitArray(std::vector<uint8_t> &data, Endian endian) : m_Endian(endian) {
  m_Capability = data.size();
  m_Buffer.resize(m_Capability);
  memcpy(m_Buffer.data(), data.data(), data.size());
  clear();
}

BitArray::BitArray(const uint8_t *data, uint32_t nBytes, Endian endian) : m_Endian(endian) {
  m_Capability = nBytes;
  m_Buffer.resize(m_Capability);
  memcpy(m_Buffer.data(), data, nBytes);
  clear();
}

uint32_t BitArray::pos() {
  uint32_t bitPos;
  if (Endian::BIG == m_Endian) {
    uint32_t byteI = m_nBits >> 3;
    uint32_t bitI = m_nBits & 0x07;
    bitPos = (byteI << 3) + 7 - bitI;
  } else {
    bitPos = m_nBits;
  }

  return bitPos;
}

void BitArray::put(uint32_t u32, uint8_t nBits) {
  size_t sBytes = m_Buffer.size();
  auto nBytes = (m_nBits + nBits + 7) / 8;
  if (nBytes != sBytes) {
    m_Buffer.resize(nBytes);
    for (size_t i = sBytes; i < nBytes; i++) {
      m_Buffer.data()[i] = 0;
    }
  }

  if (Endian::BIG == m_Endian) {
    Std_BitSetBigEndian(m_Buffer.data(), u32, (uint16_t)pos(), nBits);
  } else {
    Std_BitSetLittleEndian(m_Buffer.data(), u32, (uint16_t)pos(), nBits);
  }
  m_nBits += nBits;
}

uint32_t BitArray::get(uint8_t nBits) {
  uint32_t u32 = 0;

  if ((m_Buffer.size() * 8) >= (nBits + m_nBits)) {
    if (Endian::BIG == m_Endian) {
      u32 = Std_BitGetBigEndian(m_Buffer.data(), (uint16_t)pos(), nBits);
    } else {
      u32 = Std_BitGetLittleEndian(m_Buffer.data(), (uint16_t)pos(), nBits);
    }
    m_nBits += nBits;
  } else {
    throw std::runtime_error("bitarray: no enough bits left");
  }

  return u32;
}

uint32_t BitArray::left() {
  return m_Buffer.size() * 8 - m_nBits;
}

std::vector<uint8_t> BitArray::left_bytes(void) {
  std::vector<uint8_t> res;
  uint32_t curPos = m_nBits;
  size_t leftNBits = left();

  for (uint32_t i = 0; i < leftNBits; i += 8) {
    uint8_t nBits = 8;
    if (nBits > left()) {
      nBits = left();
    }
    uint8_t u8 = (uint8_t)get(nBits);
    res.push_back(u8);
  }
  m_nBits = curPos;

  return res;
}

void BitArray::drop(uint32_t nBits) {
  if ((m_Buffer.size() * 8) >= (nBits + m_nBits)) {
    m_nBits += nBits;
  } else {
    m_nBits = m_Buffer.size() * 8;
    throw std::runtime_error("bitarray: consume too much bits");
  }
}

std::vector<uint8_t> &BitArray::bytes(void) {
  return m_Buffer;
}

BitArray::~BitArray() {
}
} /* namespace as */
