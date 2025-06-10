/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __BIT_ARRAY_HPP__
#define __BIT_ARRAY_HPP__
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
  void put(uint32_t u32, uint8_t nBits);
  uint32_t get(uint8_t nBits);
  void clear(void);

  std::vector<uint8_t> &bytes(void);

  uint32_t left(); /* bits left not get */
  std::vector<uint8_t> left_bytes(void);
  void drop(uint32_t nBits); /* consume nBits */

private:
  uint32_t pos();

private:
  std::vector<uint8_t> m_Buffer;
  Endian m_Endian;
  uint32_t m_nBits;
  uint32_t m_Capability;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* __BIT_ARRAY_HPP__ */
