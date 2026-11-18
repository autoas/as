/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <Std_Types.h>
#include <Std_Timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/queue.h>
#include <assert.h>

#include "TcpIp.h"
#include "doip_client.h"
#include "Std_Debug.h"
#include "Std_Timer.h"

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

#include <mutex>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <queue>
#include <condition_variable>

#include "Semaphore.hpp"

using namespace as;
using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DOIP 0
#define AS_LOG_DOIPI 0
#define AS_LOG_DOIPE 3

#define AS_LOG_MBEDTLS 0

#define MBEDTLS_DEBUG_LEVEL AS_LOG_LEVEL(MBEDTLS)

#define SERVER_NAME "localhost"

#define DOIP_PROTOCOL_VERSION 2
#define DOIP_HEADER_LENGTH 8u

/* @SWS_DoIP_00008, @SWS_DoIP_00009 */
#define DOIP_GENERAL_HEADER_NEGATIVE_ACK 0x0000u                /* UDP/TCP*/
#define DOIP_VID_REQUEST 0x0001u                                /* UDP */
#define DOIP_VID_REQUEST_WITH_EID 0x0002u                       /* UDP */
#define DOIP_VID_REQUEST_WITH_VIN 0x0003u                       /* UDP */
#define DOIP_VAN_MSG_OR_VIN_RESPONCE 0x0004u                    /* UDP */
#define DOIP_ROUTING_ACTIVATION_REQUEST 0x0005u                 /* TCP */
#define DOIP_ROUTING_ACTIVATION_RESPONSE 0x0006u                /* TCP */
#define DOIP_ALIVE_CHECK_REQUEST 0x0007u                        /* TCP */
#define DOIP_ALIVE_CHECK_RESPONSE 0x0008u                       /* TCP */
#define DOIP_ENTITY_STATUS_REQUEST 0x4001u                      /* UDP */
#define DOIP_ENTITY_STATUS_RESPONSE 0x4002u                     /* UDP */
#define DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_REQUEST 0x4003u  /* UDP */
#define DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_RESPONSE 0x4004u /* UDP */
#define DOIP_DIAGNOSTIC_MESSAGE 0x8001                          /* TCP */
#define DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK 0x8002             /* TCP */
#define DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK 0x8003             /* TCP */

#define DOIP_ALIVE_TIMEOUT 5000000
#define DOIP_ALIVE_CHECK_TIME 3000000
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  DOIP_MSGQ_DEFAULT,
  DOIP_MSGQ_UDS_ACK,
  DOIP_MSGQ_UDS_REPLY,
  DOIP_MSGQ_MAX,
} DoIp_MsgQueueType;

typedef struct {
  int Result;
  uint16_t PayloadType;
  std::vector<uint8_t> Data;
} DoIp_MsgType;

struct DoIp_NodeContext_s {
  DoIp_NodeType Node;
  uint8_t LastSID = 0;
  uint8_t FA;
  uint8_t Status;
  uint16_t SA;
  uint16_t LA;
  TcpIp_SockAddrType RemoteAddr;
  TcpIp_SocketIdType Sock; /* The DoIP TCP socket */
  bool IsConnected;
  bool IsActivated;
  std::thread Thread;
  std::mutex Lock;
  Semaphore Sem;
  Std_TimerType AliveRequestTimer;
  Std_TimerType AliveTimer;

  std::queue<DoIp_MsgType> MsgQ[DOIP_MSGQ_MAX];
  std::condition_variable CondVarQ[DOIP_MSGQ_MAX];

  bool IsStopped;
  DoIp_ClientType *Client;
  STAILQ_ENTRY(DoIp_NodeContext_s) Entry;

  /* TLS related */
  mbedtls_entropy_context Entropy;
  mbedtls_ctr_drbg_context CtrDrbg;
  mbedtls_ssl_context Ssl;
  mbedtls_ssl_config Conf;
  mbedtls_x509_crt CaCert;
};

struct DoIp_Client_s {
  TcpIp_SocketIdType Discovery;
  TcpIp_SocketIdType TestEquipmentRequest;
  char Ip[32];
  int Port;
  std::thread Thread;
  std::mutex Lock;
  bool IsStopped;
  Semaphore Sem;

  std::queue<DoIp_MsgType> MsgQ;
  std::condition_variable CondVar;

  STAILQ_HEAD(, DoIp_NodeContext_s) Nodes;
  uint32_t NumOfNodes;
  /* TLS related */
  /* path to concatenation of all available CA certificates in PEM format */
  std::vector<char> CasPem;
};
/* ================================ [ DECLARES  ] ============================================== */
static void DoIp_HandleTcpResponse(struct DoIp_NodeContext_s *node, uint8_t *data, uint16_t length);
/* ================================ [ DATAS     ] ============================================== */
static boolean doipInitialized = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
static std::string DoIp_BuildHexString(const uint8_t *data, size_t length) {
  std::string s;
  char buf[4];
  size_t max = length;
  if (max > 32) {
    max = 32;
  }
  for (size_t i = 0; i < max; i++) {
    snprintf(buf, sizeof(buf), "%02X", data[i]);
    s += buf;
  }
  return s;
}

static Std_ReturnType TcpIp_TlsRecv(struct DoIp_NodeContext_s *node, uint8_t *buffer, uint32_t *length) {
  Std_ReturnType ret = E_OK;

  if (node->Client->CasPem.size() > 0) {
    int rc;
    rc = mbedtls_ssl_read(&node->Ssl, buffer, *length);
    *length = 0;
    if (rc >= 0) {
      *length = rc;
    } else if ((rc != MBEDTLS_ERR_SSL_WANT_READ) && (rc != MBEDTLS_ERR_SSL_WANT_WRITE)) {
      ASLOG(DOIPE, ("mbedtls_ssl_read returned %d\n", rc));
      ret = E_NOT_OK;
    } else {
      /* OK */
    }
  } else {
    ret = TcpIp_Recv(node->Sock, buffer, length);
  }

  return ret;
}

static Std_ReturnType TcpIp_TlsSend(struct DoIp_NodeContext_s *node, uint8_t *buffer, uint32_t length) {
  Std_ReturnType ret = E_NOT_OK;

  if (node->Client->CasPem.size() > 0) {
    int rc;
    rc = mbedtls_ssl_write(&node->Ssl, buffer, length);
    if (rc == (int)length) {
      ret = E_OK;
    }
  } else {
    ret = TcpIp_Send(node->Sock, buffer, length);
  }

  return ret;
}

static void DoIp_Init(void) {
  TcpIp_Init(NULL);
}

static void DoIp_FillHeader(uint8_t *header, uint16_t payloadType, uint32_t payloadLength) {
  header[0] = DOIP_PROTOCOL_VERSION;
  header[1] = ~DOIP_PROTOCOL_VERSION;
  header[2] = (payloadType >> 8) & 0xFF;
  header[3] = payloadType & 0xFF;
  header[4] = (payloadLength >> 24) & 0xFF;
  header[5] = (payloadLength >> 16) & 0xFF;
  header[6] = (payloadLength >> 8) & 0xFF;
  header[7] = payloadLength & 0xFF;
}

static void DoIp_AddNode(DoIp_ClientType *client, struct DoIp_NodeContext_s *node) {
  bool exist = false;
  struct DoIp_NodeContext_s *n;

  std::unique_lock<std::mutex> lck(client->Lock);
  STAILQ_FOREACH(n, &client->Nodes, Entry) {
    if ((0 == memcmp(n->Node.VIN, node->Node.VIN, 17)) &&
        (0 == memcmp(n->Node.EID, node->Node.EID, 6)) &&
        (0 == memcmp(n->Node.GID, node->Node.GID, 6)) && (n->Node.LA == node->Node.LA)) {
      exist = true;
      break;
    }
  }
  if (false == exist) {
    node->IsConnected = false;
    node->IsActivated = false;
    node->IsStopped = false;
    node->Client = client;
    STAILQ_INSERT_TAIL(&client->Nodes, node, Entry);
    client->NumOfNodes++;
    ASLOG(DOIPI, ("discovery node %d.%d.%d.%d:%d online\n", node->RemoteAddr.Addr[0],
                  node->RemoteAddr.Addr[1], node->RemoteAddr.Addr[2], node->RemoteAddr.Addr[3],
                  node->RemoteAddr.Port));
  } else {
    free(node);
  }
}

static int DoIp_HandleVinResponse(DoIp_ClientType *client, TcpIp_SockAddrType *RemoteAddr,
                                        uint8_t *payload, uint32_t length) {
  struct DoIp_NodeContext_s *node;
  int r = 0;

  ASLOG(DOIP, ("VAN message with length=%d\n", length));
  if (33 == length) {
    node = new struct DoIp_NodeContext_s;
    if (NULL != node) {
      memcpy(node->Node.VIN, &payload[0], 17);
      node->Node.LA = ((uint16_t)payload[17] << 8) + payload[18];
      memcpy(node->Node.EID, &payload[19], 6);
      memcpy(node->Node.GID, &payload[25], 6);
      node->FA = payload[31];
      node->Status = payload[32];
      node->RemoteAddr = *RemoteAddr;
      node->RemoteAddr.Port = client->Port;
      DoIp_AddNode(client, node);
    } else {
      ASLOG(DOIPE, ("OoM\n"));
      r = DOIP_E_NOMEM;
    }
  } else {
    r = DOIP_E_INVAL;
  }

  return r;
}

static void DoIp_HandleUdpMessage(DoIp_ClientType *client, TcpIp_SockAddrType *RemoteAddr,
                                    uint8_t *data, uint16_t length) {
  uint16_t payloadType = (uint16_t)-1;
  uint32_t payloadLength;
  int r = 0;

  ASLOG(DOIP, ("udp message from %d.%d.%d.%d:%d %s", RemoteAddr->Addr[0], RemoteAddr->Addr[1],
               RemoteAddr->Addr[2], RemoteAddr->Addr[3], RemoteAddr->Port,
               DoIp_BuildHexString(data, length).c_str()));

  if ((length >= DOIP_HEADER_LENGTH) && (DOIP_PROTOCOL_VERSION == data[0]) &&
      (data[0] = ((~data[1])) & 0xFF)) {
    payloadType = ((uint16_t)data[2] << 8) + data[3];
    payloadLength =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
    if ((payloadLength + DOIP_HEADER_LENGTH) <= length) {
      switch (payloadType) {
      case DOIP_VID_REQUEST:
        r = DOIP_E_OK_SILENT;
        break;
      case DOIP_VAN_MSG_OR_VIN_RESPONCE:
        r = DoIp_HandleVinResponse(client, RemoteAddr, &data[DOIP_HEADER_LENGTH],
                                         payloadLength);
        break;
      default:
        r = DOIP_E_INVAL;
        ASLOG(DOIPE, ("unsupported payload type 0x%X\n", payloadType));
        break;
      }
    } else {
      r = DOIP_E_TOO_LONG;
      ASLOG(DOIPE, ("message too long\n"));
    }
  } else {
    r = DOIP_E_INVAL;
    ASLOG(DOIPE, ("invalid udp message\n"));
  }

  if (DOIP_E_OK_SILENT != r) {
    std::unique_lock<std::mutex> lck(client->Lock);
    DoIp_MsgType msg;
    msg.PayloadType = payloadType;
    msg.Result = r;
    client->MsgQ.push(msg);
    client->CondVar.notify_one();
  }
}

static void DoIp_Daemon(void *arg) {
  Std_ReturnType ret;
  DoIp_ClientType *client = (DoIp_ClientType *)arg;
  uint32_t length;
  TcpIp_SockAddrType RemoteAddr;
  std::vector<uint8_t> buffer;
  buffer.resize(4096);

  ASLOG(DOIPI, ("DoIP Client request on %s:%d\n", client->Ip, client->Port));
  while (false == client->IsStopped) {
    length = buffer.size();
    ret = TcpIp_RecvFrom(client->Discovery, &RemoteAddr, buffer.data(), &length);
    if ((E_OK == ret) && (length > 0)) {
      DoIp_HandleUdpMessage(client, &RemoteAddr, buffer.data(), length);
    }
    length = buffer.size();
    ret = TcpIp_RecvFrom(client->TestEquipmentRequest, &RemoteAddr, buffer.data(), &length);
    if ((E_OK == ret) && (length > 0)) {
      DoIp_HandleUdpMessage(client, &RemoteAddr, buffer.data(), length);
    }
    std::this_thread::sleep_for(1ms);
  }

  ASLOG(DOIPI, ("DoIP Client offline\n"));

  TcpIp_Close(client->Discovery, TRUE);
}

static void DoIp_AliveCheckRequest(struct DoIp_NodeContext_s *node) {
  Std_ReturnType ret;
  uint8_t buffer[DOIP_HEADER_LENGTH];
  DoIp_FillHeader(buffer, DOIP_ALIVE_CHECK_REQUEST, 0);
  ret = TcpIp_TlsSend(node, buffer, DOIP_HEADER_LENGTH);
  if (E_OK != ret) {
    ASLOG(DOIPE, ("send alive check request failed: %d\n", ret));
    node->IsStopped = true;
  }
}

static void DoIp_TlsDebug(void *ctx, int level, const char *file, int line, const char *str) {
  ((void)level);

  ASPRINT(DOIP, ("%s:%04d: %s", file, line, str));
}

static int DoIp_TlsNetSend(void *ctx, const unsigned char *buf, size_t len) {
  int rc = MBEDTLS_ERR_SSL_WANT_WRITE;
  Std_ReturnType ret;
  struct DoIp_NodeContext_s *node = (struct DoIp_NodeContext_s *)ctx;

  ret = TcpIp_Send(node->Sock, buf, len);
  if (E_OK == ret) {
    rc = (int)len;
  }

  return rc;
}

static int DoIp_TlsNetRecv(void *ctx, unsigned char *buf, size_t len) {
  int rc = 0;
  Std_ReturnType ret;
  uint32_t length = (uint32_t)len;
  struct DoIp_NodeContext_s *node = (struct DoIp_NodeContext_s *)ctx;

  ret = TcpIp_Recv(node->Sock, buf, &length);
  if (E_OK == ret) {
    rc = (int)length;
  }

  if (0 == rc) {
    rc = MBEDTLS_ERR_SSL_WANT_READ;
  }

  return rc;
}

static void DoIp_NodeTlsSetup(struct DoIp_NodeContext_s *node) {
  int ret = 0;
  const char *pers = "doip_ssl_client";

  ASLOG(DOIPI, ("DoIP TLS setup\n"));

#if defined(MBEDTLS_DEBUG_C)
  mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
#endif

  mbedtls_ssl_init(&node->Ssl);
  mbedtls_ssl_config_init(&node->Conf);
  mbedtls_x509_crt_init(&node->CaCert);
  mbedtls_ctr_drbg_init(&node->CtrDrbg);
  mbedtls_entropy_init(&node->Entropy);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    ASLOG(DOIPE, ("Failed to initialize PSA Crypto implementation: %d\n", (int)status));
    ret = -1;
  }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

  if (0 == ret) {
    ret = mbedtls_ctr_drbg_seed(&node->CtrDrbg, mbedtls_entropy_func, &node->Entropy,
                                (const unsigned char *)pers, strlen(pers));
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ctr_drbg_seed returned %d\n", ret));
    }
  }

  if (0 == ret) {
    ret = mbedtls_x509_crt_parse(&node->CaCert, (const unsigned char *)node->Client->CasPem.data(),
                                 node->Client->CasPem.size());
    if (ret < 0) {
      ASLOG(DOIPE, ("mbedtls_x509_crt_parse returned -0x%x\n", (unsigned int)-ret));
    }
  }

  if (0 == ret) {
    ret = mbedtls_ssl_config_defaults(&node->Conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ssl_config_defaults returned %d\n\n", ret));
    }
  }

  if (0 == ret) {
    /* OPTIONAL is not optimal for security,
     * but makes interop easier in this simplified example */
    mbedtls_ssl_conf_authmode(&node->Conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&node->Conf, &node->CaCert, NULL);
    mbedtls_ssl_conf_rng(&node->Conf, mbedtls_ctr_drbg_random, &node->CtrDrbg);
    mbedtls_ssl_conf_dbg(&node->Conf, DoIp_TlsDebug, node);
  }

  if (0 == ret) {
    ret = mbedtls_ssl_setup(&node->Ssl, &node->Conf);
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ssl_setup returned %d\n\n", ret));
    }
  }

  if (0 == ret) {
    ret = mbedtls_ssl_set_hostname(&node->Ssl, SERVER_NAME);
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ssl_set_hostname returned %d\n", ret));
    }
  }

  if (0 == ret) {
    mbedtls_ssl_set_bio(&node->Ssl, node, DoIp_TlsNetSend, DoIp_TlsNetRecv, NULL);
  }

  while ((ret = mbedtls_ssl_handshake(&node->Ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      ASLOG(DOIPE, ("mbedtls_ssl_handshake returned -0x%x\n\n", (unsigned int)-ret));
      break;
    }
  }

  if (0 != ret) {
    node->IsStopped = true;
    ASLOG(DOIPE, ("DoIP TLS setup failed\n"));
  }
}

static void DoIp_NodeDaemon(void *arg) {
  Std_ReturnType ret;
  struct DoIp_NodeContext_s *node = (struct DoIp_NodeContext_s *)arg;
  uint32_t length = 0;
  uint32_t offset = 0;
  uint32_t payloadLength = UINT16_MAX;
  std::vector<uint8_t> buffer;
  buffer.resize(4096);

  if (node->Client->CasPem.size() > 0) {
    DoIp_NodeTlsSetup(node);
  }

  if (false == node->IsStopped) {
    ASLOG(DOIPI, ("DoIP node online\n"));
    node->IsConnected = true;
    Std_TimerStart(&node->AliveRequestTimer);
    Std_TimerStart(&node->AliveTimer);
  }
  node->Sem.Post();
  while (false == node->IsStopped) {
    if (offset < DOIP_HEADER_LENGTH) {
      length = DOIP_HEADER_LENGTH - offset;
    } else {
      length = payloadLength - (offset - DOIP_HEADER_LENGTH);
    }
    ret = TcpIp_TlsRecv(node, &buffer[offset], &length);
    if ((E_OK == ret) && (length > 0)) {
      ASLOG(DOIP, ("recv: %s\n", DoIp_BuildHexString(&buffer[offset], length).c_str()));
      offset += length;
      if (offset >= DOIP_HEADER_LENGTH) {
        if ((DOIP_PROTOCOL_VERSION == buffer[0]) && (buffer[0] = ((~buffer[1])) & 0xFF)) {
          payloadLength = ((uint32_t)buffer[4] << 24) + ((uint32_t)buffer[5] << 16) +
                          ((uint32_t)buffer[6] << 8) + buffer[7];
        } else {
          offset = 0;
          ASLOG(DOIPE, ("invalid tcp message\n"));
        }
      }
      if (offset >= (payloadLength + DOIP_HEADER_LENGTH)) {
        DoIp_HandleTcpResponse(node, buffer.data(), offset);
        offset = 0;
        payloadLength = UINT16_MAX;
        Std_TimerStart(&node->AliveTimer); /* as thre is response, restart alive timer */
      } else {
        /* wait a full DoIP message received */;
      }
    }
    if (Std_GetTimerElapsedTime(&node->AliveRequestTimer) > DOIP_ALIVE_CHECK_TIME) {
      DoIp_AliveCheckRequest(node);
      Std_TimerStart(&node->AliveRequestTimer);
    }
    if (Std_GetTimerElapsedTime(&node->AliveTimer) > DOIP_ALIVE_TIMEOUT) {
      ASLOG(DOIPI, ("alive timer timeout, stop this node\n"));
      node->IsStopped = true;
    }
    std::this_thread::sleep_for(1ms);
  }

  {
    std::unique_lock<std::mutex> lck(node->Lock);
    TcpIp_Close(node->Sock, TRUE);
    node->IsConnected = false;
    node->IsActivated = false;
  }

  ASLOG(DOIPI, ("DoIP node offline\n"));
}

static int DoIp_HandleActivateResponse(struct DoIp_NodeContext_s *node, uint8_t *payload,
                                         uint32_t length) {
  int r = 0;
  uint16_t sa, la;
  uint8_t resCode;
  if ((13 == length) || (9 == length)) {
    sa = ((uint16_t)payload[0] << 8) + payload[1];
    la = ((uint16_t)payload[2] << 8) + payload[3];
    resCode = payload[4];
    if (0x10 == resCode) {
      node->SA = sa;
      node->LA = la;
      ASLOG(DOIPI, ("activated okay with SA=%X, LA=%X\n", sa, la));
    } else {
      ASLOG(DOIPE, ("activate failed with response code 0x%x\n", resCode));
      r = DOIP_E_NEGATIVE;
      node->IsStopped = true;
    }
  } else {
    ASLOG(DOIPE, ("activate response with invalid length\n"));
    r = DOIP_E_INVAL;
  }
  return r;
}

static int DoIp_HandleAliveCheckRequest(struct DoIp_NodeContext_s *node, uint8_t *payload,
                                           uint32_t length) {
  int r = DOIP_E_OK_SILENT;
  Std_ReturnType ret;
  uint8_t buffer[DOIP_HEADER_LENGTH + 2];
  ASLOG(DOIP, ("Alive Check Request\n"));
  if (0 == length) {
    DoIp_FillHeader(buffer, DOIP_ALIVE_CHECK_RESPONSE, 2);
    buffer[DOIP_HEADER_LENGTH] = (node->SA >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 1] = node->SA & 0xFF;
    ret = TcpIp_TlsSend(node, buffer, DOIP_HEADER_LENGTH + 2);
    if (E_OK != ret) {
      ASLOG(DOIPE, ("send alive check response failed: %d\n", ret));
      node->IsStopped = true;
    }
  } else {
    ASLOG(DOIPE, ("alive check request with invalid length\n"));
    r = DOIP_E_INVAL;
  }
  return r;
}

static int DoIp_HandleAliveCheckResponse(struct DoIp_NodeContext_s *node, uint8_t *payload,
                                            uint32_t length) {
  int r = DOIP_E_OK_SILENT;
  uint16_t sa;

  if (2 == length) {
    sa = ((uint16_t)payload[0] << 8) + payload[1];
    if (sa != node->Node.LA) {
      ASLOG(DOIPE, ("alive check response with invalid SA(%X) != TA(%X)\n", sa, node->Node.LA));
      node->IsStopped = true;
    } else {
      Std_TimerStart(&node->AliveTimer);
    }
  } else {
    ASLOG(DOIPE, ("alive check response with invalid length\n"));
  }

  return r;
}

static int DoIp_HandleGeneralNegativeAck(struct DoIp_NodeContext_s *node, uint8_t *payload,
                                            uint32_t length) {
  int r = DOIP_E_NEGATIVE;

  if (1 == length) {
    ASLOG(DOIPE, ("Negative Ack 0x%X\n", payload[0]));
  } else {
    ASLOG(DOIPE, ("alive check request with invalid length\n"));
    r = DOIP_E_INVAL;
  }

  return r;
}

static void DoIp_HandleTcpResponse(struct DoIp_NodeContext_s *node, uint8_t *data, uint16_t length) {
  int r = 0;
  uint16_t payloadType = (uint16_t)-1;
  uint32_t payloadLength;
  // TODO: the data possible contains multiply response
  if ((length >= DOIP_HEADER_LENGTH) && (DOIP_PROTOCOL_VERSION == data[0]) &&
      (data[0] = ((~data[1])) & 0xFF)) {
    payloadType = ((uint16_t)data[2] << 8) + data[3];
    payloadLength =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
    ASLOG(DOIP, ("payload: %s\n", DoIp_BuildHexString(data, length).c_str()));
    if ((payloadLength + DOIP_HEADER_LENGTH) <= length) {
      switch (payloadType) {
      case DOIP_GENERAL_HEADER_NEGATIVE_ACK:
        r = DoIp_HandleGeneralNegativeAck(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_ROUTING_ACTIVATION_RESPONSE:
        r = DoIp_HandleActivateResponse(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_ALIVE_CHECK_REQUEST:
        r = DoIp_HandleAliveCheckRequest(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_ALIVE_CHECK_RESPONSE:
        r = DoIp_HandleAliveCheckResponse(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_DIAGNOSTIC_MESSAGE:
        if (payloadLength > 4) {
          r = payloadLength - 4;
          ASLOG(DOIPI,
                ("uds reply: %s\n", DoIp_BuildHexString(&data[DOIP_HEADER_LENGTH + 4], r).c_str()));
        } else {
          ASLOG(DOIPE, ("diagnostic message with invalid length\n"));
          r = DOIP_E_INVAL;
        }
        break;
      case DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK:
        r = 0;
        break;
      case DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK:
        ASLOG(DOIPE, ("diagnostic negative response code 0x%X\n", data[DOIP_HEADER_LENGTH + 4]));
        r = DOIP_E_NEGATIVE;
        break;
      default:
        r = DOIP_E_INVAL;
        ASLOG(DOIPE, ("unsupported payload type 0x%X\n", payloadType));
        break;
      }
    } else {
      r = DOIP_E_TOO_LONG;
      ASLOG(DOIPE, ("message too long\n"));
    }
  } else {
    r = DOIP_E_INVAL;
    ASLOG(DOIPE, ("invalid tcp message\n"));
  }

  if (DOIP_E_OK_SILENT != r) {
    ASLOG(DOIPI, ("post with payload type %x, result %d\n", payloadType, r));
    DoIp_MsgType msg;
    DoIp_MsgQueueType queType = DOIP_MSGQ_DEFAULT;
    switch (payloadType) {
    case DOIP_DIAGNOSTIC_MESSAGE:
      queType = DOIP_MSGQ_UDS_REPLY;
      break;
    case DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK:
    case DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK:
      queType = DOIP_MSGQ_UDS_ACK;
      break;
    default:
      break;
    }
    std::unique_lock<std::mutex> lck(node->Lock);
    msg.PayloadType = payloadType;
    msg.Result = r;
    msg.Data.resize(length);
    memcpy(msg.Data.data(), data, length);
    node->MsgQ[queType].push(msg);
    node->CondVarQ[queType].notify_one();
  }
}

static DoIp_MsgType DoIp_NodeWait(struct DoIp_NodeContext_s *node,
                                 DoIp_MsgQueueType queType = DOIP_MSGQ_DEFAULT) {
  DoIp_MsgType msg = {DOIP_E_TIMEOUT, 0, {}};

  std::unique_lock<std::mutex> lck(node->Lock);
  if (true == node->MsgQ[queType].empty()) {
    node->CondVarQ[queType].wait_for(lck, std::chrono::milliseconds(5000));
  }
  if (false == node->MsgQ[queType].empty()) {
    msg = node->MsgQ[queType].front();
    node->MsgQ[queType].pop();
  }

  return msg;
}

static DoIp_MsgType DoIp_ClientWait(DoIp_ClientType *client) {
  DoIp_MsgType msg = {DOIP_E_TIMEOUT, 0, {}};

  std::unique_lock<std::mutex> lck(client->Lock);
  if (true == client->MsgQ.empty()) {
    client->CondVar.wait_for(lck, std::chrono::milliseconds(5000));
  }
  if (false == client->MsgQ.empty()) {
    msg = client->MsgQ.front();
    client->MsgQ.pop();
  }

  return msg;
}

/* ================================ [ FUNCTIONS ] ============================================== */
DoIp_ClientType *DoIp_CreateClient(const char *ip, int port, const char *casPem) {
  DoIp_ClientType *client = NULL;
  TcpIp_SocketIdType discovery;
  TcpIp_SocketIdType test_equipment_request;
  Std_ReturnType ret = E_OK;
  uint16_t u16PortAny = TCPIP_PORT_ANY;
  uint16_t u16Port;
  TcpIp_SockAddrType ipv4Addr;
  uint32_t ipAddr = TcpIp_InetAddr(ip);
  boolean isMulticast;

  if (FALSE == doipInitialized) {
    DoIp_Init();
    doipInitialized = TRUE;
  }

  /* A multicast target expects the request on the well known port of the group, so that port has
   * to be bound. A unicast target answers to the source port of the request instead, and binding
   * the well known port may even fail when it is occupied on the host, e.g. by the port forward
   * of an emulated ECU: then the request goes out from any port and the answer comes back to it. */
  TcpIp_SetupAddrFrom(&ipv4Addr, ipAddr, (uint16_t)port);
  isMulticast = ((ipv4Addr.Addr[0] >= 224) && (ipv4Addr.Addr[0] <= 239)) ? TRUE : FALSE;
  u16Port = (TRUE == isMulticast) ? (uint16_t)port : TCPIP_PORT_ANY;

  discovery = TcpIp_Create(TCPIP_IPPROTO_UDP);
  if (discovery >= 0) {
    ret = TcpIp_Bind(discovery, 0, &u16Port);
    if (E_OK != ret) {
      ASLOG(DOIPE, ("Failed to bind\n"));
      TcpIp_Close(discovery, TRUE);
      ret = E_NOT_OK;
    } else {
      if (TRUE == isMulticast) {
        TcpIp_SetupAddrFrom(&ipv4Addr, ipAddr, u16Port);
        ret = TcpIp_AddToMulticast(discovery, &ipv4Addr);
      }
      if (E_OK == ret) {
        test_equipment_request = TcpIp_Create(TCPIP_IPPROTO_UDP);
        if (test_equipment_request < 0) {
          TcpIp_Close(discovery, TRUE);
          ret = E_NOT_OK;
        } else {
          TcpIp_Bind(test_equipment_request, 0, &u16PortAny);
          TcpIp_SetMulticastIF(test_equipment_request, 0);
        }
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    client = new DoIp_ClientType;
    if (NULL != client) {
      client->Discovery = discovery;
      client->TestEquipmentRequest = test_equipment_request;
      client->IsStopped = false;
      strcpy(client->Ip, ip);
      client->Port = port;
      client->NumOfNodes = 0;
      if (nullptr != casPem) {
        FILE *fp = fopen(casPem, "rb");
        assert(nullptr != fp);
        fseek(fp, 0, SEEK_END);
        size_t sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        client->CasPem.resize(sz + 1);
        fread(client->CasPem.data(), sz, 1, fp);
        client->CasPem[sz] = '\0';
        fclose(fp);
      }
      STAILQ_INIT(&client->Nodes);
    } else {
      ASLOG(DOIPE, ("Failed to allocate memory\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    client->Thread = std::thread(DoIp_Daemon, (void *)client);
  }

  return client;
}

int DoIp_AwaitVehicleAnnouncement(DoIp_ClientType *client, DoIp_NodeType **nodes, int numOfNodes,
                                    uint32_t timeout /* unit ms */) {
  int num = 0;
  int i = 0;
  struct DoIp_NodeContext_s *n;
  Std_TimerType timer;

  Std_TimerStart(&timer);
  while ((Std_GetTimerElapsedTime(&timer) < (timeout * 1000)) && (0 == client->NumOfNodes)) {
    std::this_thread::sleep_for(1ms);
  }

  num = client->NumOfNodes;
  if (num > numOfNodes) {
    num = numOfNodes;
  }

  if (num > 0) {
    std::unique_lock<std::mutex> lck(client->Lock);
    STAILQ_FOREACH(n, &client->Nodes, Entry) {
      nodes[i] = &n->Node;
      i++;
      if (i >= num) {
        break;
      }
    }
  }

  return client->NumOfNodes;
}

DoIp_NodeType *DoIp_Request(DoIp_ClientType *client) {
  Std_ReturnType ret;
  int r;
  DoIp_NodeType *node = NULL;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t ipAddr = TcpIp_InetAddr(client->Ip);
  uint8_t buffer[DOIP_HEADER_LENGTH];

  DoIp_FillHeader(buffer, DOIP_VID_REQUEST, 0);
  TcpIp_SetupAddrFrom(&RemoteAddr, ipAddr, client->Port);
  ret = TcpIp_SendTo(client->TestEquipmentRequest, &RemoteAddr, buffer, DOIP_HEADER_LENGTH);
  if (E_OK == ret) {
    DoIp_MsgType msg = DoIp_ClientWait(client);
    r = msg.Result;
    if ((0 == r) && (DOIP_VAN_MSG_OR_VIN_RESPONCE == msg.PayloadType)) {
      std::unique_lock<std::mutex> lck(client->Lock);
      node = (DoIp_NodeType *)STAILQ_LAST(&client->Nodes, DoIp_NodeContext_s, Entry);
    }
  }

  return node;
}

int DoIp_Connect(DoIp_NodeType *node) {
  int r = 0;
  TcpIp_SocketIdType sockId;
  Std_ReturnType ret = E_OK;
  struct DoIp_NodeContext_s *n = (struct DoIp_NodeContext_s *)node;

  if (n->IsConnected) {
    r = DOIP_E_AGAIN;
  }

  if (0 == r) {
    sockId = TcpIp_Create(TCPIP_IPPROTO_TCP);
    if (sockId >= 0) {
      ret = TcpIp_TcpConnect(sockId, &n->RemoteAddr);
      if (E_OK != ret) {
        ASLOG(DOIPE, ("Failed to connect\n"));
        TcpIp_Close(sockId, TRUE);
        ret = E_NOT_OK;
      }
    } else {
      ret = E_NOT_OK;
    }

    if (E_OK == ret) {
      n->Sock = sockId;
      n->Thread = std::thread(DoIp_NodeDaemon, (void *)n);
      n->Sem.Wait();
      if (true != n->IsConnected) {
        ASLOG(DOIPE, ("Failed to connect\n"));
        ret = E_NOT_OK;
      }
    } else {
      r = DOIP_E_NODEV;
    }
  }

  return r;
}

int DoIp_Activate(DoIp_NodeType *node, uint16_t sa, uint8_t at, uint8_t *oem, uint8_t oemLen) {
  int r = 0;
  Std_ReturnType ret;
  struct DoIp_NodeContext_s *n = (struct DoIp_NodeContext_s *)node;
  std::vector<uint8_t> buffer;
  buffer.resize(DOIP_HEADER_LENGTH + 7 + oemLen);

  if ((0 == oemLen) || ((4 == oemLen) && (NULL != oem))) {
  } else {
    r = DOIP_E_INVAL;
  }

  if (n->IsActivated) {
    r = DOIP_E_AGAIN;
  }

  if (0 == r) {
    DoIp_FillHeader(buffer.data(), DOIP_ROUTING_ACTIVATION_REQUEST, 7 + oemLen);
    buffer[DOIP_HEADER_LENGTH + 0] = (sa >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 1] = sa & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 2] = at;
    buffer[DOIP_HEADER_LENGTH + 3] = 0;
    buffer[DOIP_HEADER_LENGTH + 4] = 0;
    buffer[DOIP_HEADER_LENGTH + 5] = 0;
    buffer[DOIP_HEADER_LENGTH + 6] = 0;
    if (oemLen > 0) {
      memcpy(&buffer[DOIP_HEADER_LENGTH + 7], oem, oemLen);
    }
    ret = TcpIp_TlsSend(n, buffer.data(), DOIP_HEADER_LENGTH + 7 + oemLen);
    if (E_OK == ret) {
      DoIp_MsgType msg = DoIp_NodeWait(n);
      r = msg.Result;
      if ((0 == r) && (DOIP_ROUTING_ACTIVATION_RESPONSE == msg.PayloadType)) {
        n->IsActivated = true;
      } else {
        r = DOIP_E_NOT_OK;
      }
    } else {
      r = DOIP_E_ACCES;
    }
  }

  return r;
}

int DoIp_Transmit(DoIp_NodeType *node, uint16_t ta, const uint8_t *txBuffer, size_t txSize,
                  uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  Std_ReturnType ret;
  struct DoIp_NodeContext_s *n = (struct DoIp_NodeContext_s *)node;
  std::vector<uint8_t> buffer;
  buffer.resize(DOIP_HEADER_LENGTH + 4 + txSize);

  ASLOG(DOIPI, ("uds request to TA=%X: %s\n", ta, DoIp_BuildHexString(txBuffer, txSize).c_str()));

  if (false == n->IsActivated) {
    r = DOIP_E_AGAIN;
  }

  if (0 == r) {
    if ((2 == txSize) && (0x3E == txBuffer[0]) && (0x80 == txBuffer[1])) {
    } else {
      n->LastSID = txBuffer[0];
    }
    DoIp_FillHeader(buffer.data(), DOIP_DIAGNOSTIC_MESSAGE, 4 + txSize);
    buffer[DOIP_HEADER_LENGTH] = (n->SA >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 1] = n->SA & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 2] = (ta >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 3] = ta & 0xFF;
    memcpy(&buffer[DOIP_HEADER_LENGTH + 4], txBuffer, txSize);
    ret = TcpIp_TlsSend(n, buffer.data(), DOIP_HEADER_LENGTH + 4 + txSize);
    if (E_OK == ret) {
      DoIp_MsgType msg = DoIp_NodeWait(n, DOIP_MSGQ_UDS_ACK);
      r = msg.Result;
      if ((0 == r) && (DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK == msg.PayloadType)) {
      } else {
        ASLOG(DOIPE, ("no diagnostic positive ack received: r = %d, PayloadType = %x\n", r,
                      msg.PayloadType));
        r = DOIP_E_NOT_OK;
      }
      if (0 == r) {
        if (NULL != rxBuffer) {
          r = DoIp_Receive(node, rxBuffer, rxSize);
        }
      }
    }
  }

  return r;
}

int DoIp_Receive(DoIp_NodeType *node, uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  struct DoIp_NodeContext_s *n = (struct DoIp_NodeContext_s *)node;

  if (false == n->IsActivated) {
    r = DOIP_E_AGAIN;
  }

  if (0 == r) {
    bool bCheckNext = false;
    do {
      bCheckNext = false;
      DoIp_MsgType msg = DoIp_NodeWait(n, DOIP_MSGQ_UDS_REPLY);
      r = msg.Result;
      if ((r > 0) && (DOIP_DIAGNOSTIC_MESSAGE == msg.PayloadType)) {
        if (rxSize >= (uint32_t)r) {
          memcpy(rxBuffer, &msg.Data[DOIP_HEADER_LENGTH + 4], r);
        } else {
          r = DOIP_E_NOSPC;
        }
      } else {
        ASLOG(DOIPE,
              ("no diagnostic message received: r = %d, PayloadType = %x\n", r, msg.PayloadType));
        r = DOIP_E_NOT_OK;
      }
      if (r > 0) {
        if ((0x7F == rxBuffer[0]) && (n->LastSID != rxBuffer[1])) {
          ASLOG(DOIPI, ("negative response with different request SID, check next message\n"));
          bCheckNext = true;
        }
      }
    } while (true == bCheckNext);
  }

  return r;
}

void DoIp_DestroyClient(DoIp_ClientType *client) {
  struct DoIp_NodeContext_s *n;
  client->IsStopped = true;
  if (client->Thread.joinable()) {
    client->Thread.join();
  }
  std::unique_lock<std::mutex> lck(client->Lock);
  while (false == STAILQ_EMPTY(&client->Nodes)) {
    n = STAILQ_FIRST(&client->Nodes);
    STAILQ_REMOVE_HEAD(&client->Nodes, Entry);
    n->IsStopped = true;
    if (n->Thread.joinable()) {
      n->Thread.join();
    }
    delete n;
  }
  delete client;
}
