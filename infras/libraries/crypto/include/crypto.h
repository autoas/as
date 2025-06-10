/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef _CRYPTO_H
#define _CRYPTO_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#ifdef USE_LTC
#include <tomcrypt.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define E_INVALID_SIGNATURE 80

#define CRYPTO_RSA_REVERSE 0x80000000ul
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int crypto_rsa_keygen(uint32_t bitsize, uint8_t *public_key, uint32_t *public_key_len,
                      uint8_t *private_key, uint32_t *private_key_len);

int crypto_rsa_sign(const uint8_t *data, uint32_t data_len, uint8_t *signature, uint32_t *sig_len,
                    const uint8_t *private_key, uint32_t key_len);

int crypto_rsa_verify(const uint8_t *data, uint32_t data_len, const uint8_t *signature,
                      uint32_t sig_len, const uint8_t *public_key, uint32_t key_len);

int crypto_rsa_encrypt(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t *out_len,
                       const uint8_t *public_key, uint32_t key_len);

int crypto_rsa_decrypt(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t *out_len,
                       const uint8_t *private_key, uint32_t key_len);

int crypto_base64_decode(const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t *outlen);

int crypto_base64_encode(const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t *outlen);
#ifdef __cplusplus
}
#endif
#endif /* _CRYPTO_H */
