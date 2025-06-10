/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef _AS_X509_HPP_
#define _AS_X509_HPP_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

#include "Certificate.h"

namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class x509 {
public:
  x509(std::string der); /* load x509 from *.der file */
  ~x509();

public:
  std::vector<uint8_t> public_key();
  std::vector<uint8_t> signature();

  std::vector<uint8_t> tbs_certificate();

private:
  Certificate_t *m_Certificate = nullptr;
  std::vector<uint8_t> m_DerData;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* _AS_X509_HPP_ */
