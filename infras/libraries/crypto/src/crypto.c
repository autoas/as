/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "crypto.h"

#include "Std_Timer.h"

#ifdef USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#endif

#ifdef USE_MBEDTLS
#include "mbedtls/base64.h"
#include "mbedtls/rsa.h"
#include "mbedtls/pk.h"
#include "mbedtls/platform.h"
#include "mbedtls/rsa.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/sha256.h"
#include <string.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef USE_OPENSSL
// #define USE_OPENSSL_KEYGEN
// #define USE_OPENSSL_SIGN
// #define USE_OPENSSL_VERIFY
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(USE_MBEDTLS) && !defined(linux) && !defined(_WIN32)
void mbedtls_platform_zeroize(void *buf, size_t len) {
  memset(buf, 0, len);
}

void mbedtls_zeroize_and_free(void *buf, size_t len) {
  if (buf != NULL) {
    mbedtls_platform_zeroize(buf, len);
  }

  mbedtls_free(buf);
}

int mbedtls_platform_entropy_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
  size_t i;
  for (i = 0; i < len; i++) {
    uint8_t u8Seed;
    output[i] = Std_GetTime() & 0xFF;
    output[i] = output[i] ^ u8Seed;
    u8Seed = ~(u8Seed + output[i]);
  }
  *olen = len;
  return 0;
}

int mbedtls_hardclock_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
  unsigned long timer = Std_GetTime();
  ((void)data);
  *olen = 0;

  if (len < sizeof(unsigned long)) {
    return 0;
  }

  memcpy(output, &timer, sizeof(unsigned long));
  *olen = sizeof(unsigned long);

  return 0;
}
#endif

#ifdef USE_OPENSSL_KEYGEN
int crypto_rsa_keygen(uint32_t bitsize, uint8_t *public_key, uint32_t *public_key_len,
                      uint8_t *private_key, uint32_t *private_key_len) {
  int r = 0;
  int er = 0;
  EVP_PKEY_CTX *mdctx = NULL;
  EVP_PKEY *pkey = NULL;

  mdctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  if (NULL == mdctx) {
    r = -ENOMEM;
  }

  if (0 == r) {
    er = EVP_PKEY_keygen_init(mdctx);
    if (1 != er) {
      r = -__LINE__;
    }
  }

  if (0 == r) {
    er = EVP_PKEY_CTX_set_rsa_keygen_bits(mdctx, bitsize);
    if (er <= 0) {
      r = -__LINE__;
    }
  }

  if (0 == r) {
    er = EVP_PKEY_keygen(mdctx, &pkey);
    if ((er <= 0) || (NULL == pkey)) {
      r = -__LINE__;
    }
  }
#if 0
  if (0 == r) {
    FILE *fp = fopen("./public.pem", "w");
    if (NULL != fp) {
      PEM_write_RSAPublicKey(fp, EVP_PKEY_get0_RSA(pkey));
      fclose(fp);
    }
    fp = fopen("./private.pem", "w");
    if (NULL != fp) {
      PEM_write_RSAPrivateKey(fp, EVP_PKEY_get0_RSA(pkey), NULL, NULL, 0, NULL, NULL);
      fclose(fp);
    }
  }
#endif
  if (0 == r) {
    r = i2d_RSAPrivateKey(EVP_PKEY_get0_RSA(pkey), NULL);
    if (r <= (int)*private_key_len) {
      i2d_RSAPrivateKey(EVP_PKEY_get0_RSA(pkey), &private_key);
      *private_key_len = (uint32_t)r;
      r = 0;
    } else {
      r = -EINVAL;
    }
  }

  if (0 == r) {
    r = i2d_RSAPublicKey(EVP_PKEY_get0_RSA(pkey), NULL);
    if (r <= (int)*public_key_len) {
      i2d_RSAPublicKey(EVP_PKEY_get0_RSA(pkey), &public_key);
      *public_key_len = (uint32_t)r;
      r = 0;
    } else {
      r = -EINVAL;
    }
  }

  if (NULL != pkey) {
    EVP_PKEY_free(pkey);
  }

  if (NULL != mdctx) {
    EVP_PKEY_CTX_free(mdctx);
  }
  return r;
}
#elif defined(USE_LTC)
int crypto_rsa_keygen(uint32_t bitsize, uint8_t *public_key, uint32_t *public_key_len,
                      uint8_t *private_key, uint32_t *private_key_len) {
  int r;
  int prng_idx;
  rsa_key key;

  ltc_mp = ltm_desc;
  prng_idx = register_prng(&sprng_desc);
  if (prng_idx < 0) {
    r = CRYPT_ERROR;
  } else {
    r = rsa_make_key(NULL, prng_idx, bitsize / 8, 65537, &key);
    if (CRYPT_OK == r) {
      r = rsa_export(private_key, (unsigned long *)private_key_len, PK_PRIVATE, &key);
      if (CRYPT_OK == r) {
        r = rsa_export(public_key, (unsigned long *)public_key_len, PK_PUBLIC, &key);
      }
      rsa_free(&key);
    }
  }

  return r;
}
#elif defined(USE_MBEDTLS)
int crypto_rsa_keygen(uint32_t bitsize, uint8_t *public_key, uint32_t *public_key_len,
                      uint8_t *private_key, uint32_t *private_key_len) {
  int r = 0;
  mbedtls_pk_context key;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  static const char pers[] = "ssas";

  mbedtls_entropy_init(&entropy);
  mbedtls_pk_init(&key);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers,
                            sizeof(pers));
  if (0 == r) {
    r = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type((mbedtls_pk_type_t)MBEDTLS_PK_RSA));
  }

  if (0 == r) {
    r =
      mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, bitsize, 65537);
  }

  if (0 == r) {
    r = mbedtls_pk_write_pubkey_der(&key, public_key, *public_key_len);
    if (r > 0) {
      memmove(public_key, &public_key[*public_key_len - r], r);
      *public_key_len = r;
      r = 0;
    }
  }

  if (0 == r) {
    r = mbedtls_pk_write_key_der(&key, private_key, *private_key_len);
    if (r > 0) {
      memmove(private_key, &private_key[*private_key_len - r], r);
      *private_key_len = r;
      r = 0;
    }
  }

  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_pk_free(&key);
  mbedtls_entropy_free(&entropy);

  return r;
}
#endif

#ifdef USE_OPENSSL_SIGN
int crypto_rsa_sign(const uint8_t *data, uint32_t data_len, uint8_t *signature, uint32_t *sig_len,
                    const uint8_t *private_key, uint32_t key_len) {
  int r = 0;
  int er = 0;
  EVP_MD_CTX *mdctx = NULL;
  EVP_PKEY_CTX *pkctx = NULL;
  EVP_PKEY *pkey = NULL;
  size_t siglen = 0;

  mdctx = EVP_MD_CTX_new();
  if (NULL == mdctx) {
    r = -ENOMEM;
  }

  if (0 == r) {
    pkey = d2i_PrivateKey(EVP_PKEY_RSA, NULL, &private_key, key_len);
    if (NULL == pkey) {
      r = -EINVAL;
    }
  }

  if (0 == r) {
    pkctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (NULL == pkctx) {
      r = -ENOMEM;
    } else {
      er = EVP_PKEY_sign_init(pkctx);
      if (1 != er) {
        r = -__LINE__;
      }
    }
  }

  if (0 == r) {
    er = EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_PADDING);
    if (1 != er) {
      r = -__LINE__;
    } else {
      EVP_MD_CTX_set_pkey_ctx(mdctx, pkctx);
    }
  }

  if (0 == r) {
    er = EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, NULL);
    if (1 != er) {
      r = -__LINE__;
    }
  }

  if (0 == r) {
    er = EVP_DigestSignUpdate(mdctx, data, data_len);
    if (1 != er) {
      r = -__LINE__;
    }
  }
#if 0
  if (0 == r) {
    er = EVP_DigestSignFinal(mdctx, NULL, &siglen);
    if (1 != er) {
      r = -__LINE__;
    } else if (siglen > (size_t)*sig_len) {
      r = -EINVAL;
    } else {
      /* OK */
    }
  }
#endif
  if (0 == r) {
    siglen = *sig_len;
    er = EVP_DigestSignFinal(mdctx, signature, &siglen);
    if (1 != er) {
      r = -__LINE__;
    } else {
      *sig_len = (uint32_t)siglen;
    }
  }

  if (NULL != pkey) {
    EVP_PKEY_free(pkey);
  }

  if (NULL != pkctx) {
    EVP_PKEY_CTX_free(pkctx);
  }

  if (NULL != mdctx) {
    EVP_MD_CTX_free(mdctx);
  }
  return r;
}
#elif defined(USE_LTC)
int crypto_rsa_sign(const uint8_t *data, uint32_t data_len, uint8_t *signature, uint32_t *sig_len,
                    const uint8_t *private_key, uint32_t key_len) {
  int r;
  int hash_idx;
  int prng_idx;
  rsa_key key;
  const int padding = LTC_PKCS_1_V1_5;
  const unsigned long saltlen = 0;
  unsigned char hash[64];
  hash_state md;

  ltc_mp = ltm_desc;

  r = rsa_import(private_key, key_len, &key);
  if (CRYPT_OK == r) {
    hash_idx = register_hash(&sha512_desc);
    if (hash_idx < 0) {
      r = CRYPT_ERROR;
    } else {
      sha512_desc.init(&md);
      sha512_desc.process(&md, data, data_len);
      sha512_desc.done(&md, hash);
      prng_idx = padding == LTC_PKCS_1_PSS ? register_prng(&sprng_desc) : 0;
      if (prng_idx < 0) {
        r = CRYPT_ERROR;
      } else {
        r = rsa_sign_hash_ex(hash, sha512_desc.hashsize, signature, (unsigned long *)sig_len,
                             padding, NULL, prng_idx, hash_idx, saltlen, &key);
      }
    }
    rsa_free(&key);
  }

  return r;
}
#elif defined(USE_MBEDTLS)
int crypto_rsa_sign(const uint8_t *data, uint32_t data_len, uint8_t *signature, uint32_t *sig_len,
                    const uint8_t *private_key, uint32_t key_len) {
  int r = -1;
  mbedtls_pk_context pk;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  static const char pers[] = "ssas";
  uint8_t hash[64];
  size_t sigLen = 0;

  mbedtls_entropy_init(&entropy);
  mbedtls_pk_init(&pk);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers,
                            sizeof(pers));
  if (0 == r) {
#ifdef USE_MBEDTLS_V3
    r =
      mbedtls_pk_parse_key(&pk, private_key, key_len, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
#else
    r = mbedtls_pk_parse_key(&pk, private_key, key_len, NULL, 0);
#endif
  }
  if (0 == r) {
    r = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), data, data_len, hash);
  }
  if (0 == r) {
    mbedtls_rsa_set_padding(mbedtls_pk_rsa(pk), MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA512);
#ifdef USE_MBEDTLS_V3
    r = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA512, hash, sizeof(hash), signature, *sig_len, &sigLen,
                        mbedtls_ctr_drbg_random, &ctr_drbg);
#else
    sigLen = *sig_len;
    r = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA512, hash, sizeof(hash), signature, &sigLen,
                        mbedtls_ctr_drbg_random, &ctr_drbg);
#endif
    *sig_len = sigLen;
  }

  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_pk_free(&pk);
  mbedtls_entropy_free(&entropy);

  return r;
}
#endif

#ifdef USE_OPENSSL_VERIFY
int crypto_rsa_verify(const uint8_t *data, uint32_t data_len, const uint8_t *signature,
                      uint32_t sig_len, const uint8_t *public_key, uint32_t key_len) {
  int r = 0;
  int er = 0;
  EVP_MD_CTX *mdctx = NULL;
  EVP_PKEY_CTX *pkctx = NULL;
  EVP_PKEY *pkey = NULL;

  mdctx = EVP_MD_CTX_new();
  if (NULL == mdctx) {
    r = -ENOMEM;
  }

  if (0 == r) {
    pkey = d2i_PublicKey(EVP_PKEY_RSA, NULL, &public_key, key_len);
    if (NULL == pkey) {
      r = -EINVAL;
    }
  }

  if (0 == r) {
    pkctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (NULL == pkctx) {
      r = -ENOMEM;
    } else {
      er = EVP_PKEY_verify_init(pkctx);
      if (1 != er) {
        r = -__LINE__;
      }
    }
  }

  if (0 == r) {
    er = EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_PADDING);
    if (1 != er) {
      r = -__LINE__;
    } else {
      EVP_MD_CTX_set_pkey_ctx(mdctx, pkctx);
    }
  }

  if (0 == r) {
    er = EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, NULL);
    if (1 != er) {
      r = -__LINE__;
    }
  }

  if (0 == r) {
    er = EVP_DigestVerifyUpdate(mdctx, data, data_len);
    if (1 != er) {
      r = -__LINE__;
    }
  }

  if (0 == r) {
    er = EVP_DigestVerifyFinal(mdctx, signature, sig_len);
    if (1 == er) {
      /* signature matched */
    } else if (0 == er) {
      r = E_INVALID_SIGNATURE;
    } else {
      r = -__LINE__;
    }
  }

  if (NULL != pkey) {
    EVP_PKEY_free(pkey);
  }

  if (NULL != pkctx) {
    EVP_PKEY_CTX_free(pkctx);
  }

  if (NULL != mdctx) {
    EVP_MD_CTX_free(mdctx);
  }
  return r;
}
#elif defined(USE_LTC)
int crypto_rsa_verify(const uint8_t *data, uint32_t data_len, const uint8_t *signature,
                      uint32_t sig_len, const uint8_t *public_key, uint32_t key_len) {
  int r;
  int hash_idx;
  int stat = 0;
  rsa_key key;
  unsigned char hash[64];
  hash_state md;
  const int padding = LTC_PKCS_1_V1_5;
  const unsigned long saltlen = 0;

  ltc_mp = ltm_desc;

  r = rsa_import(public_key, key_len, &key);
  if (CRYPT_OK == r) {
    hash_idx = register_hash(&sha512_desc);
    if (hash_idx < 0) {
      r = CRYPT_ERROR;
    } else {
      sha512_desc.init(&md);
      sha512_desc.process(&md, data, data_len);
      sha512_desc.done(&md, hash);
      r = rsa_verify_hash_ex(signature, sig_len, hash, sha512_desc.hashsize, padding, hash_idx,
                             saltlen, &stat, &key);
      if (CRYPT_OK == r) {
        if (!stat) {
          r = E_INVALID_SIGNATURE;
        }
      } else if (CRYPT_INVALID_PACKET == r) {
        r = E_INVALID_SIGNATURE;
      }
    }
    rsa_free(&key);
  }

  return r;
}
#endif

#if defined(USE_MBEDTLS)
int crypto_rsa_verify(const uint8_t *data, uint32_t data_len, const uint8_t *signature,
                      uint32_t sig_len, const uint8_t *public_key, uint32_t key_len) {
  int r = -1;
  mbedtls_pk_context pk;
  uint8_t hash[64];

  mbedtls_pk_init(&pk);
  r = mbedtls_pk_parse_public_key(&pk, public_key, key_len);
  if (0 == r) {
    r = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), data, data_len, hash);
  }
  if (0 == r) {
    mbedtls_rsa_set_padding(mbedtls_pk_rsa(pk), MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA512);
    r = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA512, hash, sizeof(hash), signature, sig_len);
  }
  mbedtls_pk_free(&pk);

  return r;
}
#endif

#if defined(USE_LTC)
int crypto_rsa_encrypt(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t *out_len,
                       const uint8_t *public_key, uint32_t key_len) {
  int r;
  rsa_key key;
  int hash_idx;
  int prng_idx;
  const int padding = LTC_PKCS_1_OAEP;

  r = rsa_import(public_key, key_len, &key);
  if (CRYPT_OK == r) {
    hash_idx = register_hash(&sha512_desc);
    if (hash_idx < 0) {
      r = CRYPT_ERROR;
    } else {
      prng_idx = register_prng(&sprng_desc);
      if (prng_idx < 0) {
        r = CRYPT_ERROR;
      } else {
        r = rsa_encrypt_key_ex(in, in_len, out, (unsigned long *)out_len, NULL, 0, NULL, prng_idx,
                               hash_idx, padding, &key);
      }
    }
    rsa_free(&key);
  }

  return r;
}
#endif

#if defined(USE_MBEDTLS)
int crypto_rsa_encrypt(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t *out_len,
                       const uint8_t *public_key, uint32_t key_len) {
  int r = -1;
  mbedtls_pk_context pk;
  size_t olen = 0;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  static const char pers[] = "ssas";
  bool bReverse = false;

  if (0 != (CRYPTO_RSA_REVERSE & key_len)) {
    key_len &= ~CRYPTO_RSA_REVERSE;
    bReverse = true;
  }

  mbedtls_entropy_init(&entropy);
  mbedtls_pk_init(&pk);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers,
                            sizeof(pers));
  if (0 == r) {
    if (true == bReverse) {
#ifdef USE_MBEDTLS_V3
      r =
        mbedtls_pk_parse_key(&pk, public_key, key_len, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
#else
      r = mbedtls_pk_parse_key(&pk, public_key, key_len, NULL, 0);
#endif
    } else {
      r = mbedtls_pk_parse_public_key(&pk, public_key, key_len);
    }
  }
  if (0 == r) {
    if (true == bReverse) {
#ifdef USE_MBEDTLS_V3
      r = -1; /* not supproted */
#else
      r = mbedtls_rsa_pkcs1_encrypt(mbedtls_pk_rsa(pk), mbedtls_ctr_drbg_random, &ctr_drbg,
                                    MBEDTLS_RSA_PRIVATE, in_len, in, out);
      olen = mbedtls_rsa_get_len(mbedtls_pk_rsa(pk));
#endif
    } else {
      r = mbedtls_pk_encrypt(&pk, in, in_len, out, &olen, *out_len, mbedtls_ctr_drbg_random,
                             &ctr_drbg);
    }
    *out_len = olen;
  }
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_pk_free(&pk);
  mbedtls_entropy_free(&entropy);

  return r;
}
#endif

#if defined(USE_LTC)
int crypto_rsa_decrypt(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t *out_len,
                       const uint8_t *private_key, uint32_t key_len) {
  int r;
  rsa_key key;
  int hash_idx;
  int state;
  const int padding = LTC_PKCS_1_OAEP;

  r = rsa_import(private_key, key_len, &key);
  if (CRYPT_OK == r) {
    hash_idx = register_hash(&sha512_desc);
    if (hash_idx < 0) {
      r = CRYPT_ERROR;
    } else {
      r = rsa_decrypt_key_ex(in, in_len, out, (unsigned long *)out_len, NULL, 0, hash_idx, padding,
                             &state, &key);
      if (CRYPT_OK == r) {
        if (!state) {
          r = CRYPT_ERROR;
        }
      }
    }
    rsa_free(&key);
  }

  return r;
}
#endif

#if defined(USE_MBEDTLS)
int crypto_rsa_decrypt(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t *out_len,
                       const uint8_t *private_key, uint32_t key_len) {
  int r = -1;
  mbedtls_pk_context pk;
  size_t olen = 0;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  static const char pers[] = "ssas";
  bool bReverse = false;

  if (0 != (CRYPTO_RSA_REVERSE & key_len)) {
    key_len &= ~CRYPTO_RSA_REVERSE;
    bReverse = true;
  }

  mbedtls_entropy_init(&entropy);
  mbedtls_pk_init(&pk);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers,
                            sizeof(pers));
  if (0 == r) {
    if (true == bReverse) {
      r = mbedtls_pk_parse_public_key(&pk, private_key, key_len);
    } else {
#ifdef USE_MBEDTLS_V3
      r = mbedtls_pk_parse_key(&pk, private_key, key_len, NULL, 0, mbedtls_ctr_drbg_random,
                               &ctr_drbg);
#else
      r = mbedtls_pk_parse_key(&pk, private_key, key_len, NULL, 0);
#endif
    }
  }
  if (0 == r) {
    if (true == bReverse) {
#ifdef USE_MBEDTLS_V2
      olen = *out_len;
      r = mbedtls_rsa_pkcs1_decrypt(mbedtls_pk_rsa(pk), mbedtls_ctr_drbg_random, &ctr_drbg,
                                    MBEDTLS_RSA_PUBLIC, &olen, in, out, olen);
#else
      r = -1;
#endif
    } else {
      r = mbedtls_pk_decrypt(&pk, in, in_len, out, &olen, *out_len, mbedtls_ctr_drbg_random,
                             &ctr_drbg);
    }
    *out_len = olen;
  }
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_pk_free(&pk);
  mbedtls_entropy_free(&entropy);

  return r;
}
#endif

#if defined(USE_LTC)
int crypto_base64_decode(const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t *outlen) {
  int r;
  unsigned long olen = *outlen;
  r = base64_decode(in, inlen, out, &olen);
  *outlen = (uint32_t)olen;
  return r;
}
#endif

#if defined(USE_LTC)
int crypto_base64_encode(const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t *outlen) {
  int r;
  unsigned long olen = *outlen;
  r = base64_encode(in, inlen, out, &olen);
  *outlen = (uint32_t)olen;
  return r;
}
#endif

#if defined(USE_MBEDTLS)
int crypto_base64_decode(const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t *outlen) {
  size_t olen = 0;
  int ret;

  ret = mbedtls_base64_decode(out, *outlen, &olen, in, inlen);
  *outlen = (uint32_t)olen;

  return ret;
}
#endif

#if defined(USE_MBEDTLS)
int crypto_base64_encode(const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t *outlen) {
  size_t olen = 0;
  int ret;

  ret = mbedtls_base64_encode(out, *outlen, &olen, in, inlen);
  *outlen = (uint32_t)olen;

  return ret;
}
#endif
