/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
#ifndef TLS_PRIV_H
#define TLS_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "SoAd.h"
#include "../SoAd/SoAd_Priv.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define TLS_SERVER_IDLE ((TLS_ServerStateType)0)
#define TLS_SERVER_HANDSHAKE ((TLS_ServerStateType)1)
#define TLS_SERVER_READY ((TLS_ServerStateType)2)
#define TLS_SERVER_RESPONSE ((TLS_ServerStateType)3)
#define TLS_SERVER_DEAD ((TLS_ServerStateType)4)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t TLS_ServerStateType;

typedef struct {
  uint8_t *data; /* data allocated to recive a packet */
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt srvcert;
  mbedtls_pk_context pkey;
#if defined(MBEDTLS_SSL_CACHE_C)
  mbedtls_ssl_cache_context cache;
#endif
  TLS_ServerStateType state;
  PduLengthType length; /* length of the whole packet size */
  PduLengthType offset;
} TLS_ServerContextType;

typedef struct {
  TLS_ServerContextType *context;
  const char *name;
  const SoAd_InterfaceType *IF;
  const SoAd_SoConModeChgNotificationFncType SoConModeChgNotification;
  const uint8_t *ServerCerts;
  const uint8_t *CasCerts; /* Concatenation of all available CA certificates in PEM format */
  const uint8_t *ServerKey;
  uint32_t ServerCertsLen;
  uint32_t CasCertsLen;
  uint32_t ServerKeyLen;
  SoAd_SoConIdType SoConId;
  PduIdType TxPduId;  /* SoAd TxPduId */
  PduIdType RxPduId;  /* upper layer RxPduId */
  uint16_t headerLen; /* the length of the header of certain protocol such as DoIP and SOMEIP/SD */
} TLS_ServerConfigType;

typedef struct {
  uint16_t idx;    /* index to server or client */
  boolean bServer; /* server or cleint */
} TLS_TxPduIdType;

struct TLS_Config_s {
  const TLS_ServerConfigType *servers;
  const TLS_TxPduIdType *TxPduIds;
  uint16_t numOfServers;
  uint16_t numOfTxPduIds;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* TLS_PRIV_H */
