/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "TLS.h"
#include "TLS_Cfg.h"
#include "TLS_Priv.h"
#include "Std_Debug.h"
#include <string.h>
#include "NetMem.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_TLS 0
#define AS_LOG_TLSE 2
#define AS_LOG_MBEDTLS 0

#define MBEDTLS_DEBUG_LEVEL AS_LOG_LEVEL(MBEDTLS)

#ifdef TLS_USE_PB_CONFIG
#define TLS_CONFIG tlsConfig
#else
#define TLS_CONFIG (&TLS_Config)
#endif

#ifndef TLS_LOCAL_DATA_MAX_SIZE
#define TLS_LOCAL_DATA_MAX_SIZE 128
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const TLS_ConfigType TLS_Config;
/* ================================ [ DATAS     ] ============================================== */
#ifdef TLS_USE_PB_CONFIG
static const TLS_ConfigType *tlsConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void TLS_Debug(void *ctx, int level, const char *file, int line, const char *str) {
  ((void)level);

  ASPRINT(TLS, ("%s:%04d: %s", file, line, str));
}

static int TLS_NetSend(void *ctx, const unsigned char *buf, size_t len) {
  int rc = MBEDTLS_ERR_SSL_WANT_WRITE;
  Std_ReturnType ret;
  PduInfoType PduInfo;
  const TLS_ServerConfigType *server = (const TLS_ServerConfigType *)ctx;
  PduInfo.MetaDataPtr = NULL;
  PduInfo.SduDataPtr = (uint8_t *)buf;
  PduInfo.SduLength = (PduLengthType)len;

  ASHEXDUMP(TLS, ("%s send:", server->name), buf, len);
  ret = SoAd_IfTransmit(server->TxPduId, &PduInfo);
  if (E_OK == ret) {
    rc = (int)len;
  }
  ASPRINT(TLS, ("TLS_NetSend(%" PRIu64 ") = %d\n", len, rc));
  return rc;
}

static int TLS_NetRecv(void *ctx, unsigned char *buf, size_t len) {
  int rc = 0;
  Std_ReturnType ret;
  uint32_t length = (uint32_t)len;
  const TLS_ServerConfigType *server = (const TLS_ServerConfigType *)ctx;

  ret = SoAd_ControlRecv(server->SoConId, buf, &length);
  if (E_OK == ret) {
    rc = (int)length;
  }

  if (0 == rc) {
    rc = MBEDTLS_ERR_SSL_WANT_READ;
  } else {
    ASHEXDUMP(TLS, ("%s recv:", server->name), buf, rc);
  }

  return rc;
}

static Std_ReturnType TLS_ServerInit(const TLS_ServerConfigType *server) {
  Std_ReturnType ret = E_OK;
  int rc;
  mbedtls_ssl_init(&server->context->ssl);
  mbedtls_ssl_config_init(&server->context->conf);
#if defined(MBEDTLS_SSL_CACHE_C)
  mbedtls_ssl_cache_init(&server->context->cache);
#endif
  mbedtls_x509_crt_init(&server->context->srvcert);
  mbedtls_pk_init(&server->context->pkey);
  mbedtls_entropy_init(&server->context->entropy);
  mbedtls_ctr_drbg_init(&server->context->ctr_drbg);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
  {
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
      ASLOG(TLSE, ("Failed to initialize PSA Crypto implementation: %d\n", (int)status));
      ret = E_NOT_OK
    }
  }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_DEBUG_C)
  mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
#endif

  if (E_OK == ret) {
    ret = SoAd_TakeControl(server->SoConId);
  }

  ASLOG(TLS, ("Init TLS %s\n", server->name));
  /*1. Seed the RNG*/
  if (E_OK == ret) {
    ASPRINT(TLS, (". Seeding the random number generator..."));
    rc = mbedtls_ctr_drbg_seed(&server->context->ctr_drbg, mbedtls_entropy_func,
                               &server->context->entropy, (const unsigned char *)server->name,
                               strlen(server->name));
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", rc));
      ret = E_NOT_OK;
    } else {
      ASPRINT(TLS, (" ok\n"));
    }
  }

  /* 2. Load the certificates and private RSA key */
  if (E_OK == ret) {
    ASPRINT(TLS, (". Loading the server cert. and key..."));
    rc = mbedtls_x509_crt_parse(&server->context->srvcert,
                                (const unsigned char *)server->ServerCerts, server->ServerCertsLen);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_x509_crt_parse server cert returned %d\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    rc = mbedtls_x509_crt_parse(&server->context->srvcert, (const unsigned char *)server->CasCerts,
                                server->CasCertsLen);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_x509_crt_parse cas cert returned %d\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    rc = mbedtls_pk_parse_key(&server->context->pkey, (const unsigned char *)server->ServerKey,
                              server->ServerKeyLen, NULL, 0, mbedtls_ctr_drbg_random,
                              &server->context->ctr_drbg);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_pk_parse_key returned %d\n", rc));
      ret = E_NOT_OK;
    } else {
      ASPRINT(TLS, (" ok\n"));
    }
  }

  if (E_OK == ret) {
    ASPRINT(TLS, (". Setting up the SSL data...."));
    rc = mbedtls_ssl_config_defaults(&server->context->conf, MBEDTLS_SSL_IS_SERVER,
                                     MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_ssl_config_defaults returned %d\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    mbedtls_ssl_conf_rng(&server->context->conf, mbedtls_ctr_drbg_random,
                         &server->context->ctr_drbg);
    mbedtls_ssl_conf_dbg(&server->context->conf, TLS_Debug, NULL);

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_conf_session_cache(&server->context->conf, &server->context->cache,
                                   mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

    mbedtls_ssl_conf_ca_chain(&server->context->conf, server->context->srvcert.next, NULL);
  }
  if (E_OK == ret) {
    rc = mbedtls_ssl_conf_own_cert(&server->context->conf, &server->context->srvcert,
                                   &server->context->pkey);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    rc = mbedtls_ssl_setup(&server->context->ssl, &server->context->conf);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  ! mbedtls_ssl_setup returned %d\n\n", rc));
      ret = E_NOT_OK;
    } else {
      ASPRINT(TLS, (" ok\n"));
    }
  }

  if (E_OK == ret) {
    mbedtls_ssl_session_reset(&server->context->ssl);
    mbedtls_ssl_set_bio(&server->context->ssl, (void *)server, TLS_NetSend, TLS_NetRecv, NULL);
    server->context->data = NULL;
    server->context->length = 0;
  }

  return ret;
}

static void TLS_ServerSwitchOnline(const TLS_ServerConfigType *server) {
  Std_ReturnType ret;
  if (TLS_SERVER_IDLE == server->context->state) {
    ret = TLS_ServerInit(server);
    if (E_OK == ret) {
      ASLOG(TLS, ("Server %s connected\n", server->name));
      server->context->state = TLS_SERVER_HANDSHAKE;
    }
  } else {
    ASLOG(TLSE, ("Server %s in wrong state %u, reset\n", server->name, server->context->state));
    mbedtls_ssl_session_reset(&server->context->ssl);
    server->context->state = TLS_SERVER_HANDSHAKE;
  }
}

static void TLS_ServerDeinit(const TLS_ServerConfigType *server) {
  mbedtls_x509_crt_free(&server->context->srvcert);
  mbedtls_pk_free(&server->context->pkey);
  mbedtls_ssl_free(&server->context->ssl);
  mbedtls_ssl_config_free(&server->context->conf);
#if defined(MBEDTLS_SSL_CACHE_C)
  mbedtls_ssl_cache_free(&server->context->cache);
#endif
  mbedtls_ctr_drbg_free(&server->context->ctr_drbg);
  mbedtls_entropy_free(&server->context->entropy);
  server->context->state = TLS_SERVER_IDLE;
}

static void TLS_ServerSoConModeChg(const TLS_ServerConfigType *server, SoAd_SoConModeType Mode) {
  if (SOAD_SOCON_ONLINE == Mode) {
    TLS_ServerSwitchOnline(server);
  } else {
    TLS_ServerDeinit(server);
  }
}

static void TLS_ServerHandShake(const TLS_ServerConfigType *server) {
  int rc;

  rc = mbedtls_ssl_handshake(&server->context->ssl);
  if (0 == rc) {
    server->SoConModeChgNotification(server->SoConId, SOAD_SOCON_ONLINE);
    server->context->state = TLS_SERVER_READY;
  } else if ((rc != MBEDTLS_ERR_SSL_WANT_READ) && (rc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
    ASLOG(TLSE, ("mbedtls_ssl_handshake returned %d\n", rc));
    server->context->state = TLS_SERVER_DEAD;
    (void)SoAd_CloseSoCon(server->SoConId, TRUE);
    TLS_ServerDeinit(server);
  } else {
    // ASLOG(TLS, ("hand shake on going, rc=%d\n", rc));
  }
}

static void TLS_ServerRecvStart(const TLS_ServerConfigType *server) {
  Std_ReturnType ret = E_NOT_OK;
  int rc;
  uint8_t *data;
  PduInfoType PduInfo;
  uint32_t rxLen;
  uint32_t length = 0; /* length of left data */
  uint8_t header[TLS_HEADER_MAX_LEN];
  TcpIp_SockAddrType RemoteAddr;

  (void)mbedtls_ssl_read(&server->context->ssl, NULL, 0);
  rxLen = mbedtls_ssl_get_bytes_avail(&server->context->ssl);
  if (server->headerLen > 0) {
    if (rxLen >= server->headerLen) {
      rxLen = server->headerLen;
      data = header;
    } else {
      rxLen = 0; /* not enough data */
    }
  }

  if (0 == rxLen) {
    /* nothing there */
  } else if (NULL == data) { /* allocate data if no header TP control */
    data = Net_MemAlloc((uint32_t)rxLen);
    if (NULL != data) {
      ret = E_OK;
    } else {
      ASLOG(TLSE, ("%s: Failed to malloc for %d\n", server->name, rxLen));
    }
  } else {
    ret = E_OK;
  }

  if (E_OK == ret) {
    rc = mbedtls_ssl_read(&server->context->ssl, data, rxLen);
    if (rc >= 0) {
      if (rc > 0) {
        ASLOG(TLS, ("%s: read %d bytes\n", server->name, rc));
        PduInfo.SduDataPtr = data;
        PduInfo.SduLength = rc;
        PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
        (void)SoAd_GetRemoteAddr(server->SoConId, &RemoteAddr);
        if (server->headerLen > 0) {
          ret = server->IF->HeaderIndication(server->RxPduId, &PduInfo, &length);
          if (E_OK == ret) {
            if (length > 0) { /* the length of left bytes need to be recived */
              server->context->length = length;
              server->context->offset = server->headerLen;
              server->context->data = Net_MemAlloc((uint32_t)server->headerLen + length);
              /* allow allocation failed thus the further rx logic to drop data */
              if (NULL == server->context->data) {
                ASLOG(TLSE, ("%s: allocate %u rx buffer failed\n", server->name,
                             (uint32_t)server->headerLen + length));
              } else {
                (void)memcpy(server->context->data, data, rc);
              }
            } else {
              server->IF->RxIndication(server->RxPduId, &PduInfo);
            }
          }
        } else {
          server->IF->RxIndication(server->RxPduId, &PduInfo);
        }
      } else {
        ASLOG(TLSE, ("%s: recv with 0 bytes\n", server->name));
      }
    } else if ((rc != MBEDTLS_ERR_SSL_WANT_READ) && (rc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
      ASLOG(TLSE, ("mbedtls_ssl_read returned %d\n", rc));
      server->context->state = TLS_SERVER_DEAD;
      (void)SoAd_CloseSoCon(server->SoConId, TRUE);
      TLS_ServerDeinit(server);
      server->SoConModeChgNotification(server->SoConId, SOAD_SOCON_OFFLINE);
    } else {
      /* OK */
    }
    if (data != header) {
      Net_MemFree(data);
    }
  }
}

static void TLS_ServerRecvLeft(const TLS_ServerConfigType *server) {
  int rc;
  Std_ReturnType ret = E_NOT_OK;
  uint32_t rxLen;
  uint8_t *data = NULL;
  PduInfoType PduInfo;
  uint8_t cache[TLS_LOCAL_DATA_MAX_SIZE];
  TcpIp_SockAddrType RemoteAddr;

  rxLen = mbedtls_ssl_get_bytes_avail(&server->context->ssl);
  if (rxLen > 0) {
    if (rxLen > server->context->length) {
      rxLen = server->context->length;
    }
    if (NULL != server->context->data) {
      data = &server->context->data[server->context->offset];
    } else {
      data = cache;
      if (rxLen > TLS_LOCAL_DATA_MAX_SIZE) {
        rxLen = TLS_LOCAL_DATA_MAX_SIZE;
      }
      ASLOG(TLSE, ("%s: drop %u bytes\n", server->name, rxLen));
    }
    ret = E_OK;
  }
  if (E_OK == ret) {
    rc = mbedtls_ssl_read(&server->context->ssl, data, rxLen);
    if (rc >= 0) {
      if (rc > 0) {
        server->context->offset += rc;
        if (server->context->length > rc) {
          server->context->length -= rc;
        } else {
          server->context->length = 0;
        }
      }
      if ((0 == server->context->length) && (NULL != server->context->data)) {
        ASLOG(TLS, ("%s: read %d bytes\n", server->name, rxLen));
        PduInfo.SduDataPtr = server->context->data;
        PduInfo.SduLength = server->context->offset;
        PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
        (void)SoAd_GetRemoteAddr(server->SoConId, &RemoteAddr);
        server->IF->RxIndication(server->RxPduId, &PduInfo);
        if (NULL != server->context->data) {
          Net_MemFree(server->context->data);
          server->context->data = NULL;
        }
      }
    } else if ((rc != MBEDTLS_ERR_SSL_WANT_READ) && (rc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
      ASLOG(TLSE, ("mbedtls_ssl_read returned %d\n", rc));
      server->context->state = TLS_SERVER_DEAD;
      (void)SoAd_CloseSoCon(server->SoConId, TRUE);
      TLS_ServerDeinit(server);
      server->SoConModeChgNotification(server->SoConId, SOAD_SOCON_OFFLINE);
      if (NULL != server->context->data) {
        Net_MemFree(server->context->data);
        server->context->data = NULL;
      }
    } else {
      /* OK */
    }
  }
}

static void TLS_ServerReady(const TLS_ServerConfigType *server) {

  if (0 == server->context->length) {
    TLS_ServerRecvStart(server);
  } else {
    TLS_ServerRecvLeft(server);
  }
}

static void TLS_ServerMainFunction(const TLS_ServerConfigType *server) {
  switch (server->context->state) {
  case TLS_SERVER_HANDSHAKE:
    TLS_ServerHandShake(server);
    break;
  case TLS_SERVER_READY:
    TLS_ServerReady(server);
    break;
  default:
    break;
  }
}

static Std_ReturnType TLS_ServerTransmit(uint16_t serverIdx, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  int rc;
  const TLS_ServerConfigType *server = &TLS_CONFIG->servers[serverIdx];
  if (TLS_SERVER_READY == server->context->state) {
    rc = mbedtls_ssl_write(&server->context->ssl, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
    if (rc == PduInfoPtr->SduLength) {
      ret = E_OK;
    }
  }
  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void TLS_Init(const TLS_ConfigType *config) {
  uint16_t i;
#ifdef TLS_USE_PB_CONFIG
  if (NULL != CfgPtr) {
    TLS_CONFIG = config;
  } else {
    TLS_CONFIG = &TLS_Config;
  }
#else
  (void)config;
#endif

  for (i = 0; i < TLS_CONFIG->numOfServers; i++) {
    (void)memset(TLS_CONFIG->servers[i].context, 0, sizeof(TLS_ServerContextType));
  }
}

void TLS_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode) {
  uint16_t i;
  ASLOG(TLS, ("SoConId %u SoCon Mode %u\n", SoConId, Mode));
  for (i = 0; i < TLS_CONFIG->numOfServers; i++) {
    if (SoConId == TLS_CONFIG->servers[i].SoConId) {
      TLS_ServerSoConModeChg(&TLS_CONFIG->servers[i], Mode);
    }
  }
}

void TLS_MainFunction(void) {
  uint16_t i;
  for (i = 0; i < TLS_CONFIG->numOfServers; i++) {
    TLS_ServerMainFunction(&TLS_CONFIG->servers[i]);
  }
}

Std_ReturnType TLS_IfTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  if (TxPduId < TLS_CONFIG->numOfTxPduIds) {
    if (TRUE == TLS_CONFIG->TxPduIds[TxPduId].bServer) {
      ret = TLS_ServerTransmit(TLS_CONFIG->TxPduIds[TxPduId].idx, PduInfoPtr);
    }
  }
  return ret;
}
