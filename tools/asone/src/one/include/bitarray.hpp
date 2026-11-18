/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef BITARRAY_HPP
#define BITARRAY_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <vector>
#include <stdexcept>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class BitArray {

public:
  enum Endian {
    BIG,
    LITTLE,
  };

public:
  BitArray(Endian endian = Endian::BIG);
  BitArray(uint32_t nBits, Endian endian = Endian::BIG);
  BitArray(std::vector<uint8_t> &data, Endian endian = Endian::BIG);
  BitArray(const uint8_t *data, uint32_t nBytes, Endian endian = Endian::BIG);
  ~BitArray();

public:
  void Put(uint32_t u32, uint8_t nBits);
  uint32_t Get(uint8_t nBits);
  void Clear(void);

  std::vector<uint8_t> &Bytes(void);

  uint32_t Left(); /* bits left not get */
  std::vector<uint8_t> LeftBytes(void);
  void Drop(uint32_t nBits); /* consume nBits */

private:
  uint32_t Pos();

private:
  std::vector<uint8_t> m_Buffer;
  Endian m_Endian;
  uint32_t m_BitCount;
  uint32_t m_Capability;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* BITARRAY_HPP */
