/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#ifdef USE_DCM
#include "Dcm.h"
#ifdef USE_CRYPTO
#include "Std_Debug.h"
#include "Std_Timer.h"
#include <stdlib.h>
#include "crypto.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static uint8_t lPublicKey[512];
static uint16_t lPublicKeyLen;

#if defined(linux) || defined(_WIN32)
static uint8_t *lCAPublicKey = NULL;
static uint16_t lCAPublicKeyLen = 0;
#else
#error TODO: define the CA public key here
#endif

static uint8_t lChallenge[512];
static uint32_t lChallengeSeed = 0xabcdeffe;
/* ================================ [ LOCALS    ] ============================================== */
#if defined(linux) || defined(_WIN32)
static void __attribute__((constructor)) _auth_init(void) {
  FILE *fp;
  char *publicKeyTxt;
  size_t sz;
  unsigned long outlen;
  const char *publicKeyPath = getenv("CA_PUBLIC_KEY");
  if (NULL == publicKeyPath) {
    publicKeyPath = "ca_public_key.txt";
  }
  fp = fopen(publicKeyPath, "rb");
  if (NULL != fp) {
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    publicKeyTxt = (char *)malloc(sz + 1);
    publicKeyTxt[sz] = 0;
    lCAPublicKey = (uint8_t *)malloc(sz);
    if (publicKeyTxt && lCAPublicKey) {
      fseek(fp, 0, SEEK_SET);
      fread(publicKeyTxt, 1, sz, fp);
      ltc_mp = ltm_desc;
      base64_decode((uint8_t *)publicKeyTxt, sz, lCAPublicKey, &outlen);
      lCAPublicKeyLen = outlen;
      ASLOG(INFO, ("load CA public key <%s> length=%d OK\n", publicKeyTxt, (int)lCAPublicKeyLen));
      free(publicKeyTxt);
    } else {
      ASLOG(ERROR, ("OoM for CA public key <%s>\n", publicKeyPath));
    }
    fclose(fp);
  } else {
    ASLOG(ERROR,
          ("Authentication CA public key <%s> not found, use env CA_PUBLIC_KEY\n", publicKeyPath));
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Dcm_AuthenticationVerifyTesterCertificate(const uint8_t *publicKey,
                                                         uint16_t publicKeyLen,
                                                         const uint8_t *signature,
                                                         uint16_t signatureLen,
                                                         Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret = E_NOT_OK;
  int r;
  r = crypto_rsa_verify(publicKey, publicKeyLen, signature, signatureLen, lCAPublicKey,
                        lCAPublicKeyLen);
  if (E_INVALID_SIGNATURE == r) {
    *ErrorCode = DCM_E_INVALID_KEY;
  } else if (CRYPT_OK == r) {
    ret = E_OK;
  } else {
    *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
  }

  if (E_OK == ret) {
    if (sizeof(lPublicKey) >= publicKeyLen) {
      memcpy(lPublicKey, publicKey, publicKeyLen);
      lPublicKeyLen = publicKeyLen;
    } else {
      *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
      ret = E_NOT_OK;
    }
  }

  return ret;
}

Std_ReturnType Dcm_AuthenticationGetChallenge(uint8_t *challenge, uint16_t *length) {
  Std_ReturnType ret = E_NOT_OK;
  int i;

  if (sizeof(lChallenge) <= *length) {
    *length = sizeof(lChallenge);
    for (i = 0; i < sizeof(lChallenge) / 4; i++) {
      uint32_t u32Seed; /* intentional not initialized to use the stack random value */
      uint32_t u32Time = Std_GetTime();
      lChallengeSeed = lChallengeSeed ^ u32Seed ^ u32Time ^ 0xfeedbeef;
      lChallenge[i * 4 + 0] = (uint8_t)(lChallengeSeed >> 24);
      lChallenge[i * 4 + 1] = (uint8_t)(lChallengeSeed >> 16);
      lChallenge[i * 4 + 2] = (uint8_t)(lChallengeSeed >> 8);
      lChallenge[i * 4 + 3] = (uint8_t)(lChallengeSeed);
    }
    memcpy(challenge, lChallenge, *length);
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType Dcm_AuthenticationVerifyProofOfOwnership(const uint8_t *signature,
                                                        uint16_t signatureLen,
                                                        Dcm_NegativeResponseCodeType *ErrorCode) {
  Std_ReturnType ret = E_NOT_OK;
  int r;
  r = crypto_rsa_verify(lChallenge, sizeof(lChallenge), signature, signatureLen, lPublicKey,
                        lPublicKeyLen);
  if (E_INVALID_SIGNATURE == r) {
    *ErrorCode = DCM_E_INVALID_KEY;
  } else if (CRYPT_OK == r) {
    ret = E_OK;
  } else {
    *ErrorCode = DCM_E_CONDITIONS_NOT_CORRECT;
  }
  return ret;
}
#endif
#endif
