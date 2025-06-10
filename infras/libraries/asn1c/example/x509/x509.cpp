/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "x509.hpp"
#include "Log.hpp"

namespace as {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
x509::x509(std::string der) {
  FILE *fp = fopen(der.c_str(), "rb");
  if (fp) {
    size_t sz;
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    m_DerData.resize(sz);
    fread(m_DerData.data(), 1, m_DerData.size(), fp);
    fclose(fp);
    asn_dec_rval_t rval = ber_decode(0, &asn_DEF_Certificate, (void **)&m_Certificate,
                                     m_DerData.data(), m_DerData.size());
    if ((rval.code == RC_OK) && (m_Certificate != nullptr)) {
      auto xer = der + ".xer";
      LOG(INFO, "load X509 certificate %s:\n", der.c_str());
      xer_fprint(Log::getFile(), &asn_DEF_Certificate, m_Certificate);
    } else {
      throw std::runtime_error("x509 failed to load der file: <" + der + ">, error is " +
                               std::to_string(rval.code));
    }
  } else {
    throw std::runtime_error("x509 no der file: <" + der + ">");
  }
}

std::vector<uint8_t> x509::public_key() {
  std::vector<uint8_t> key;
  uint8_t *ptr = m_Certificate->tbsCertificate.subjectPublicKeyInfo.subjectPublicKey.buf;
  size_t len = m_Certificate->tbsCertificate.subjectPublicKeyInfo.subjectPublicKey.size;
  key.reserve(len);
  for (size_t i = 0; i < len; i++) {
    key.push_back(ptr[i]);
  }
  return key;
}

std::vector<uint8_t> x509::signature() {
  std::vector<uint8_t> sig;
  sig.reserve(m_Certificate->signature.size);
  for (int i = 0; i < m_Certificate->signature.size; i++) {
    sig.push_back(m_Certificate->signature.buf[i]);
  }
  return sig;
}

std::vector<uint8_t> x509::tbs_certificate() {
  std::vector<uint8_t> tbs;
  uint8_t *der = m_DerData.data();
  size_t len;

  if ((0x30 == der[0]) && (0x82 == der[1])) {
    len = ((size_t)der[2] << 8) + der[3];
    if ((4 + len) == m_DerData.size()) {
      der = &der[4];
      if ((0x30 == der[0]) && (0x82 == der[1])) {
        len = ((size_t)der[2] << 8) + der[3] + 4;
        tbs.reserve(len);
        for (size_t i = 0; i < len; i++) {
          tbs.push_back(der[i]);
        }
      }
    }
  }

  return tbs;
}

x509::~x509() {
}

} /* namespace as */