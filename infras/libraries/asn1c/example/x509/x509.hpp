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
class X509 {
public:
  X509(std::string der); /* load x509 from *.der file */
  ~X509();

public:
  std::vector<uint8_t> PublicKey();
  std::vector<uint8_t> Signature();

  std::vector<uint8_t> TbsCertificate();

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
