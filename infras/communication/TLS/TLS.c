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

  ASHEXDUMP(TLS, ("%s send:", server->Name), buf, len);
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
    ASHEXDUMP(TLS, ("%s recv:", server->Name), buf, rc);
  }

  return rc;
}

static Std_ReturnType TLS_ServerInit(const TLS_ServerConfigType *server) {
  Std_ReturnType ret = E_OK;
  int rc;
  mbedtls_ssl_init(&server->Context->Ssl);
  mbedtls_ssl_config_init(&server->Context->Conf);
#if defined(MBEDTLS_SSL_CACHE_C)
  mbedtls_ssl_cache_init(&server->Context->Cache);
#endif
  mbedtls_x509_crt_init(&server->Context->Srvcert);
  mbedtls_pk_init(&server->Context->Pkey);
  mbedtls_entropy_init(&server->Context->Entropy);
  mbedtls_ctr_drbg_init(&server->Context->Ctr_drbg);

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

  ASLOG(TLS, ("Init TLS %s\n", server->Name));
  /*1. Seed the RNG*/
  if (E_OK == ret) {
    ASPRINT(TLS, (". Seeding the random number generator..."));
    rc = mbedtls_ctr_drbg_seed(&server->Context->Ctr_drbg, mbedtls_entropy_func,
                               &server->Context->Entropy, (const unsigned char *)server->Name,
                               strlen(server->Name));
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
    rc = mbedtls_x509_crt_parse(&server->Context->Srvcert,
                                (const unsigned char *)server->ServerCerts, server->ServerCertsLen);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_x509_crt_parse server cert returned %d\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    rc = mbedtls_x509_crt_parse(&server->Context->Srvcert, (const unsigned char *)server->CasCerts,
                                server->CasCertsLen);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_x509_crt_parse cas cert returned %d\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    rc = mbedtls_pk_parse_key(&server->Context->Pkey, (const unsigned char *)server->ServerKey,
                              server->ServerKeyLen, NULL, 0, mbedtls_ctr_drbg_random,
                              &server->Context->Ctr_drbg);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_pk_parse_key returned %d\n", rc));
      ret = E_NOT_OK;
    } else {
      ASPRINT(TLS, (" ok\n"));
    }
  }

  if (E_OK == ret) {
    ASPRINT(TLS, (". Setting up the SSL data...."));
    rc = mbedtls_ssl_config_defaults(&server->Context->Conf, MBEDTLS_SSL_IS_SERVER,
                                     MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  !  mbedtls_ssl_config_defaults returned %d\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    mbedtls_ssl_conf_rng(&server->Context->Conf, mbedtls_ctr_drbg_random,
                         &server->Context->Ctr_drbg);
    mbedtls_ssl_conf_dbg(&server->Context->Conf, TLS_Debug, NULL);

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_conf_session_cache(&server->Context->Conf, &server->Context->Cache,
                                   mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

    mbedtls_ssl_conf_ca_chain(&server->Context->Conf, server->Context->Srvcert.next, NULL);
  }
  if (E_OK == ret) {
    rc = mbedtls_ssl_conf_own_cert(&server->Context->Conf, &server->Context->Srvcert,
                                   &server->Context->Pkey);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", rc));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    rc = mbedtls_ssl_setup(&server->Context->Ssl, &server->Context->Conf);
    if (rc != 0) {
      ASPRINT(TLS, (" failed\n  ! mbedtls_ssl_setup returned %d\n\n", rc));
      ret = E_NOT_OK;
    } else {
      ASPRINT(TLS, (" ok\n"));
    }
  }

  if (E_OK == ret) {
    mbedtls_ssl_session_reset(&server->Context->Ssl);
    mbedtls_ssl_set_bio(&server->Context->Ssl, (void *)server, TLS_NetSend, TLS_NetRecv, NULL);
    server->Context->Data = NULL;
    server->Context->Length = 0;
  }

  return ret;
}

static void TLS_ServerSwitchOnline(const TLS_ServerConfigType *server) {
  Std_ReturnType ret;
  if (TLS_SERVER_IDLE == server->Context->State) {
    ret = TLS_ServerInit(server);
    if (E_OK == ret) {
      ASLOG(TLS, ("Server %s connected\n", server->Name));
      server->Context->State = TLS_SERVER_HANDSHAKE;
    }
  } else {
    ASLOG(TLSE, ("Server %s in wrong state %u, reset\n", server->Name, server->Context->State));
    mbedtls_ssl_session_reset(&server->Context->Ssl);
    server->Context->State = TLS_SERVER_HANDSHAKE;
  }
}

static void TLS_ServerDeinit(const TLS_ServerConfigType *server) {
  mbedtls_x509_crt_free(&server->Context->Srvcert);
  mbedtls_pk_free(&server->Context->Pkey);
  mbedtls_ssl_free(&server->Context->Ssl);
  mbedtls_ssl_config_free(&server->Context->Conf);
#if defined(MBEDTLS_SSL_CACHE_C)
  mbedtls_ssl_cache_free(&server->Context->Cache);
#endif
  mbedtls_ctr_drbg_free(&server->Context->Ctr_drbg);
  mbedtls_entropy_free(&server->Context->Entropy);
  server->Context->State = TLS_SERVER_IDLE;
}

static void TLS_ServerSoConModeChg(const TLS_ServerConfigType *server, SoAd_SoConModeType mode) {
  if (SOAD_SOCON_ONLINE == mode) {
    TLS_ServerSwitchOnline(server);
  } else {
    TLS_ServerDeinit(server);
  }
}

static void TLS_ServerHandShake(const TLS_ServerConfigType *server) {
  int rc;

  rc = mbedtls_ssl_handshake(&server->Context->Ssl);
  if (0 == rc) {
    server->SoConModeChgNotification(server->SoConId, SOAD_SOCON_ONLINE);
    server->Context->State = TLS_SERVER_READY;
  } else if ((rc != MBEDTLS_ERR_SSL_WANT_READ) && (rc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
    ASLOG(TLSE, ("mbedtls_ssl_handshake returned %d\n", rc));
    server->Context->State = TLS_SERVER_DEAD;
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

  (void)mbedtls_ssl_read(&server->Context->Ssl, NULL, 0);
  rxLen = mbedtls_ssl_get_bytes_avail(&server->Context->Ssl);
  if (server->HeaderLen > 0) {
    if (rxLen >= server->HeaderLen) {
      rxLen = server->HeaderLen;
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
      ASLOG(TLSE, ("%s: Failed to malloc for %d\n", server->Name, rxLen));
    }
  } else {
    ret = E_OK;
  }

  if (E_OK == ret) {
    rc = mbedtls_ssl_read(&server->Context->Ssl, data, rxLen);
    if (rc >= 0) {
      if (rc > 0) {
        ASLOG(TLS, ("%s: read %d bytes\n", server->Name, rc));
        PduInfo.SduDataPtr = data;
        PduInfo.SduLength = rc;
        PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
        (void)SoAd_GetRemoteAddr(server->SoConId, &RemoteAddr);
        if (server->HeaderLen > 0) {
          ret = server->IF->HeaderIndication(server->RxPduId, &PduInfo, &length);
          if (E_OK == ret) {
            if (length > 0) { /* the length of left bytes need to be recived */
              server->Context->Length = length;
              server->Context->Offset = server->HeaderLen;
              server->Context->Data = Net_MemAlloc((uint32_t)server->HeaderLen + length);
              /* allow allocation failed thus the further rx logic to drop data */
              if (NULL == server->Context->Data) {
                ASLOG(TLSE, ("%s: allocate %u rx buffer failed\n", server->Name,
                             (uint32_t)server->HeaderLen + length));
              } else {
                (void)memcpy(server->Context->Data, data, rc);
              }
            } else {
              server->IF->RxIndication(server->RxPduId, &PduInfo);
            }
          }
        } else {
          server->IF->RxIndication(server->RxPduId, &PduInfo);
        }
      } else {
        ASLOG(TLSE, ("%s: recv with 0 bytes\n", server->Name));
      }
    } else if ((rc != MBEDTLS_ERR_SSL_WANT_READ) && (rc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
      ASLOG(TLSE, ("mbedtls_ssl_read returned %d\n", rc));
      server->Context->State = TLS_SERVER_DEAD;
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

  rxLen = mbedtls_ssl_get_bytes_avail(&server->Context->Ssl);
  if (rxLen > 0) {
    if (rxLen > server->Context->Length) {
      rxLen = server->Context->Length;
    }
    if (NULL != server->Context->Data) {
      data = &server->Context->Data[server->Context->Offset];
    } else {
      data = cache;
      if (rxLen > TLS_LOCAL_DATA_MAX_SIZE) {
        rxLen = TLS_LOCAL_DATA_MAX_SIZE;
      }
      ASLOG(TLSE, ("%s: drop %u bytes\n", server->Name, rxLen));
    }
    ret = E_OK;
  }
  if (E_OK == ret) {
    rc = mbedtls_ssl_read(&server->Context->Ssl, data, rxLen);
    if (rc >= 0) {
      if (rc > 0) {
        server->Context->Offset += rc;
        if (server->Context->Length > rc) {
          server->Context->Length -= rc;
        } else {
          server->Context->Length = 0;
        }
      }
      if ((0 == server->Context->Length) && (NULL != server->Context->Data)) {
        ASLOG(TLS, ("%s: read %d bytes\n", server->Name, rxLen));
        PduInfo.SduDataPtr = server->Context->Data;
        PduInfo.SduLength = server->Context->Offset;
        PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
        (void)SoAd_GetRemoteAddr(server->SoConId, &RemoteAddr);
        server->IF->RxIndication(server->RxPduId, &PduInfo);
        if (NULL != server->Context->Data) {
          Net_MemFree(server->Context->Data);
          server->Context->Data = NULL;
        }
      }
    } else if ((rc != MBEDTLS_ERR_SSL_WANT_READ) && (rc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
      ASLOG(TLSE, ("mbedtls_ssl_read returned %d\n", rc));
      server->Context->State = TLS_SERVER_DEAD;
      (void)SoAd_CloseSoCon(server->SoConId, TRUE);
      TLS_ServerDeinit(server);
      server->SoConModeChgNotification(server->SoConId, SOAD_SOCON_OFFLINE);
      if (NULL != server->Context->Data) {
        Net_MemFree(server->Context->Data);
        server->Context->Data = NULL;
      }
    } else {
      /* OK */
    }
  }
}

static void TLS_ServerReady(const TLS_ServerConfigType *server) {

  if (0 == server->Context->Length) {
    TLS_ServerRecvStart(server);
  } else {
    TLS_ServerRecvLeft(server);
  }
}

static void TLS_ServerMainFunction(const TLS_ServerConfigType *server) {
  switch (server->Context->State) {
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

static Std_ReturnType TLS_ServerTransmit(uint16_t serverIdx, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  int rc;
  const TLS_ServerConfigType *server = &TLS_CONFIG->Servers[serverIdx];
  if (TLS_SERVER_READY == server->Context->State) {
    rc = mbedtls_ssl_write(&server->Context->Ssl, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
    if (rc == pduInfoPtr->SduLength) {
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

  for (i = 0; i < TLS_CONFIG->NumOfServers; i++) {
    (void)memset(TLS_CONFIG->Servers[i].Context, 0, sizeof(TLS_ServerContextType));
  }
}

void TLS_SoConModeChg(SoAd_SoConIdType soConId, SoAd_SoConModeType mode) {
  uint16_t i;
  ASLOG(TLS, ("SoConId %u SoCon mode %u\n", soConId, mode));
  for (i = 0; i < TLS_CONFIG->NumOfServers; i++) {
    if (soConId == TLS_CONFIG->Servers[i].SoConId) {
      TLS_ServerSoConModeChg(&TLS_CONFIG->Servers[i], mode);
    }
  }
}

void TLS_MainFunction(void) {
  uint16_t i;
  for (i = 0; i < TLS_CONFIG->NumOfServers; i++) {
    TLS_ServerMainFunction(&TLS_CONFIG->Servers[i]);
  }
}

Std_ReturnType TLS_IfTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  if (txPduId < TLS_CONFIG->NumOfTxPduIds) {
    if (TRUE == TLS_CONFIG->TxPduIds[txPduId].Server) {
      ret = TLS_ServerTransmit(TLS_CONFIG->TxPduIds[txPduId].Idx, pduInfoPtr);
    }
  }
  return ret;
}
