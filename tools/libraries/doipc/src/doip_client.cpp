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
#define AS_LOG_DOIPI 2
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
} doip_msg_queue_type_t;

typedef struct {
  int result;
  uint16_t payload_type;
  std::vector<uint8_t> data;
} doip_msg_t;

struct doip_node_s {
  doip_node_t node;
  uint8_t lastSID = 0;
  uint8_t FA;
  uint8_t status;
  uint16_t SA;
  uint16_t LA;
  TcpIp_SockAddrType RemoteAddr;
  TcpIp_SocketIdType sock; /* The DoIP TCP socket */
  bool connected;
  bool activated;
  std::thread thread;
  std::mutex lock;
  Semaphore sem;
  Std_TimerType alive_request_timer;
  Std_TimerType alive_timer;

  std::queue<doip_msg_t> msgQ[DOIP_MSGQ_MAX];
  std::condition_variable condVarQ[DOIP_MSGQ_MAX];

  bool stopped;
  struct doip_client_s *client;
  STAILQ_ENTRY(doip_node_s) entry;

  /* TLS related */
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;
};

struct doip_client_s {
  TcpIp_SocketIdType discovery;
  TcpIp_SocketIdType test_equipment_request;
  char ip[32];
  int port;
  std::thread thread;
  std::mutex lock;
  bool stopped;
  Semaphore sem;

  std::queue<doip_msg_t> msgQ;
  std::condition_variable condVar;

  STAILQ_HEAD(, doip_node_s) nodes;
  uint32_t numOfNodes;
  /* TLS related */
  /* path to concatenation of all available CA certificates in PEM format */
  std::vector<char> casPem;
};
/* ================================ [ DECLARES  ] ============================================== */
static void doip_handle_tcp_response(struct doip_node_s *node, uint8_t *data, uint16_t length);
/* ================================ [ DATAS     ] ============================================== */
static boolean l_initialized = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
static std::string build_hexstring(const uint8_t *data, size_t length) {
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

static Std_ReturnType TcpIp_TlsRecv(struct doip_node_s *node, uint8_t *buffer, uint32_t *length) {
  Std_ReturnType ret = E_OK;

  if (node->client->casPem.size() > 0) {
    int rc;
    rc = mbedtls_ssl_read(&node->ssl, buffer, *length);
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
    ret = TcpIp_Recv(node->sock, buffer, length);
  }

  return ret;
}

static Std_ReturnType TcpIp_TlsSend(struct doip_node_s *node, uint8_t *buffer, uint32_t length) {
  Std_ReturnType ret = E_NOT_OK;

  if (node->client->casPem.size() > 0) {
    int rc;
    rc = mbedtls_ssl_write(&node->ssl, buffer, length);
    if (rc == (int)length) {
      ret = E_OK;
    }
  } else {
    ret = TcpIp_Send(node->sock, buffer, length);
  }

  return ret;
}

static void doip_init(void) {
  TcpIp_Init(NULL);
}

static void doip_fill_header(uint8_t *header, uint16_t payloadType, uint32_t payloadLength) {
  header[0] = DOIP_PROTOCOL_VERSION;
  header[1] = ~DOIP_PROTOCOL_VERSION;
  header[2] = (payloadType >> 8) & 0xFF;
  header[3] = payloadType & 0xFF;
  header[4] = (payloadLength >> 24) & 0xFF;
  header[5] = (payloadLength >> 16) & 0xFF;
  header[6] = (payloadLength >> 8) & 0xFF;
  header[7] = payloadLength & 0xFF;
}

static void doip_add_node(doip_client_t *client, struct doip_node_s *node) {
  bool exist = false;
  struct doip_node_s *n;

  std::unique_lock<std::mutex> lck(client->lock);
  STAILQ_FOREACH(n, &client->nodes, entry) {
    if ((0 == memcmp(n->node.VIN, node->node.VIN, 17)) &&
        (0 == memcmp(n->node.EID, node->node.EID, 6)) &&
        (0 == memcmp(n->node.GID, node->node.GID, 6)) && (n->node.LA == node->node.LA)) {
      exist = true;
      break;
    }
  }
  if (false == exist) {
    node->connected = false;
    node->activated = false;
    node->stopped = false;
    node->client = client;
    STAILQ_INSERT_TAIL(&client->nodes, node, entry);
    client->numOfNodes++;
    ASLOG(DOIPI, ("discovery node %d.%d.%d.%d:%d online\n", node->RemoteAddr.addr[0],
                  node->RemoteAddr.addr[1], node->RemoteAddr.addr[2], node->RemoteAddr.addr[3],
                  node->RemoteAddr.port));
  } else {
    free(node);
  }
}

static int doip_handle_VAN_VIN_response(doip_client_t *client, TcpIp_SockAddrType *RemoteAddr,
                                        uint8_t *payload, uint32_t length) {
  struct doip_node_s *node;
  int r = 0;

  ASLOG(DOIP, ("VAN message with length=%d\n", length));
  if (33 == length) {
    node = new struct doip_node_s;
    if (NULL != node) {
      memcpy(node->node.VIN, &payload[0], 17);
      node->node.LA = ((uint16_t)payload[17] << 8) + payload[18];
      memcpy(node->node.EID, &payload[19], 6);
      memcpy(node->node.GID, &payload[25], 6);
      node->FA = payload[31];
      node->status = payload[32];
      node->RemoteAddr = *RemoteAddr;
      node->RemoteAddr.port = client->port;
      doip_add_node(client, node);
    } else {
      ASLOG(DOIPE, ("OoM\n"));
      r = DOIP_E_NOMEM;
    }
  } else {
    r = DOIP_E_INVAL;
  }

  return r;
}

static void doip_handle_udp_message(doip_client_t *client, TcpIp_SockAddrType *RemoteAddr,
                                    uint8_t *data, uint16_t length) {
  uint16_t payloadType = (uint16_t)-1;
  uint32_t payloadLength;
  int r = 0;

  ASLOG(DOIP, ("udp message from %d.%d.%d.%d:%d %s", RemoteAddr->addr[0], RemoteAddr->addr[1],
               RemoteAddr->addr[2], RemoteAddr->addr[3], RemoteAddr->port,
               build_hexstring(data, length).c_str()));

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
        r = doip_handle_VAN_VIN_response(client, RemoteAddr, &data[DOIP_HEADER_LENGTH],
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
    std::unique_lock<std::mutex> lck(client->lock);
    doip_msg_t msg;
    msg.payload_type = payloadType;
    msg.result = r;
    client->msgQ.push(msg);
    client->condVar.notify_one();
  }
}

static void doip_daemon(void *arg) {
  Std_ReturnType ret;
  doip_client_t *client = (doip_client_t *)arg;
  uint32_t length;
  TcpIp_SockAddrType RemoteAddr;
  std::vector<uint8_t> buffer;
  buffer.resize(4096);

  ASLOG(DOIPI, ("DoIP Client request on %s:%d\n", client->ip, client->port));
  while (false == client->stopped) {
    length = buffer.size();
    ret = TcpIp_RecvFrom(client->discovery, &RemoteAddr, buffer.data(), &length);
    if ((E_OK == ret) && (length > 0)) {
      doip_handle_udp_message(client, &RemoteAddr, buffer.data(), length);
    }
    length = buffer.size();
    ret = TcpIp_RecvFrom(client->test_equipment_request, &RemoteAddr, buffer.data(), &length);
    if ((E_OK == ret) && (length > 0)) {
      doip_handle_udp_message(client, &RemoteAddr, buffer.data(), length);
    }
    std::this_thread::sleep_for(1ms);
  }

  ASLOG(DOIPI, ("DoIP Client offline\n"));

  TcpIp_Close(client->discovery, TRUE);
}

static void doip_alive_check_request(struct doip_node_s *node) {
  Std_ReturnType ret;
  uint8_t buffer[DOIP_HEADER_LENGTH];
  doip_fill_header(buffer, DOIP_ALIVE_CHECK_REQUEST, 0);
  ret = TcpIp_TlsSend(node, buffer, DOIP_HEADER_LENGTH);
  if (E_OK != ret) {
    ASLOG(DOIPE, ("send alive check request failed: %d\n", ret));
    node->stopped = true;
  }
}

static void TLS_Debug(void *ctx, int level, const char *file, int line, const char *str) {
  ((void)level);

  ASPRINT(DOIP, ("%s:%04d: %s", file, line, str));
}

static int TLS_NetSend(void *ctx, const unsigned char *buf, size_t len) {
  int rc = MBEDTLS_ERR_SSL_WANT_WRITE;
  Std_ReturnType ret;
  struct doip_node_s *node = (struct doip_node_s *)ctx;

  ret = TcpIp_Send(node->sock, buf, len);
  if (E_OK == ret) {
    rc = (int)len;
  }

  return rc;
}

static int TLS_NetRecv(void *ctx, unsigned char *buf, size_t len) {
  int rc = 0;
  Std_ReturnType ret;
  uint32_t length = (uint32_t)len;
  struct doip_node_s *node = (struct doip_node_s *)ctx;

  ret = TcpIp_Recv(node->sock, buf, &length);
  if (E_OK == ret) {
    rc = (int)length;
  }

  if (0 == rc) {
    rc = MBEDTLS_ERR_SSL_WANT_READ;
  }

  return rc;
}

static void node_tls_setup(struct doip_node_s *node) {
  int ret = 0;
  const char *pers = "doip_ssl_client";

  ASLOG(DOIPI, ("DoIP TLS setup\n"));

#if defined(MBEDTLS_DEBUG_C)
  mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
#endif

  mbedtls_ssl_init(&node->ssl);
  mbedtls_ssl_config_init(&node->conf);
  mbedtls_x509_crt_init(&node->cacert);
  mbedtls_ctr_drbg_init(&node->ctr_drbg);
  mbedtls_entropy_init(&node->entropy);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    ASLOG(DOIPE, ("Failed to initialize PSA Crypto implementation: %d\n", (int)status));
    ret = -1;
  }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

  if (0 == ret) {
    ret = mbedtls_ctr_drbg_seed(&node->ctr_drbg, mbedtls_entropy_func, &node->entropy,
                                (const unsigned char *)pers, strlen(pers));
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ctr_drbg_seed returned %d\n", ret));
    }
  }

  if (0 == ret) {
    ret = mbedtls_x509_crt_parse(&node->cacert, (const unsigned char *)node->client->casPem.data(),
                                 node->client->casPem.size());
    if (ret < 0) {
      ASLOG(DOIPE, ("mbedtls_x509_crt_parse returned -0x%x\n", (unsigned int)-ret));
    }
  }

  if (0 == ret) {
    ret = mbedtls_ssl_config_defaults(&node->conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ssl_config_defaults returned %d\n\n", ret));
    }
  }

  if (0 == ret) {
    /* OPTIONAL is not optimal for security,
     * but makes interop easier in this simplified example */
    mbedtls_ssl_conf_authmode(&node->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&node->conf, &node->cacert, NULL);
    mbedtls_ssl_conf_rng(&node->conf, mbedtls_ctr_drbg_random, &node->ctr_drbg);
    mbedtls_ssl_conf_dbg(&node->conf, TLS_Debug, node);
  }

  if (0 == ret) {
    ret = mbedtls_ssl_setup(&node->ssl, &node->conf);
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ssl_setup returned %d\n\n", ret));
    }
  }

  if (0 == ret) {
    ret = mbedtls_ssl_set_hostname(&node->ssl, SERVER_NAME);
    if (0 != ret) {
      ASLOG(DOIPE, ("mbedtls_ssl_set_hostname returned %d\n", ret));
    }
  }

  if (0 == ret) {
    mbedtls_ssl_set_bio(&node->ssl, node, TLS_NetSend, TLS_NetRecv, NULL);
  }

  while ((ret = mbedtls_ssl_handshake(&node->ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      ASLOG(DOIPE, ("mbedtls_ssl_handshake returned -0x%x\n\n", (unsigned int)-ret));
      break;
    }
  }

  if (0 != ret) {
    node->stopped = true;
    ASLOG(DOIPE, ("DoIP TLS setup failed\n"));
  }
}

static void node_daemon(void *arg) {
  Std_ReturnType ret;
  struct doip_node_s *node = (struct doip_node_s *)arg;
  uint32_t length = 0;
  uint32_t offset = 0;
  uint32_t payloadLength = UINT16_MAX;
  std::vector<uint8_t> buffer;
  buffer.resize(4096);

  if (node->client->casPem.size() > 0) {
    node_tls_setup(node);
  }

  if (false == node->stopped) {
    ASLOG(DOIPI, ("DoIP node online\n"));
    node->connected = true;
    Std_TimerStart(&node->alive_request_timer);
    Std_TimerStart(&node->alive_timer);
  }
  node->sem.post();
  while (false == node->stopped) {
    if (offset < DOIP_HEADER_LENGTH) {
      length = DOIP_HEADER_LENGTH - offset;
    } else {
      length = payloadLength - (offset - DOIP_HEADER_LENGTH);
    }
    ret = TcpIp_TlsRecv(node, &buffer[offset], &length);
    if ((E_OK == ret) && (length > 0)) {
      ASLOG(DOIP, ("recv: %s\n", build_hexstring(&buffer[offset], length).c_str()));
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
        doip_handle_tcp_response(node, buffer.data(), offset);
        offset = 0;
        payloadLength = UINT16_MAX;
        Std_TimerStart(&node->alive_timer); /* as thre is response, restart alive timer */
      } else {
        /* wait a full DoIP message received */;
      }
    }
    if (Std_GetTimerElapsedTime(&node->alive_request_timer) > DOIP_ALIVE_CHECK_TIME) {
      doip_alive_check_request(node);
      Std_TimerStart(&node->alive_request_timer);
    }
    if (Std_GetTimerElapsedTime(&node->alive_timer) > DOIP_ALIVE_TIMEOUT) {
      ASLOG(DOIPI, ("alive timer timeout, stop this node\n"));
      node->stopped = true;
    }
    std::this_thread::sleep_for(1ms);
  }

  {
    std::unique_lock<std::mutex> lck(node->lock);
    TcpIp_Close(node->sock, TRUE);
    node->connected = false;
    node->activated = false;
  }

  ASLOG(DOIPI, ("DoIP node offline\n"));
}

static int doip_handle_activate_response(struct doip_node_s *node, uint8_t *payload,
                                         uint32_t length) {
  int r = 0;
  uint16_t sa, la;
  uint8_t resCode;
  if (13 == length) {
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
      node->stopped = true;
    }
  } else {
    ASLOG(DOIPE, ("activate response with invalid length\n"));
    r = DOIP_E_INVAL;
  }
  return r;
}

static int doip_handle_alive_check_request(struct doip_node_s *node, uint8_t *payload,
                                           uint32_t length) {
  int r = DOIP_E_OK_SILENT;
  Std_ReturnType ret;
  uint8_t buffer[DOIP_HEADER_LENGTH + 2];
  ASLOG(DOIP, ("Alive Check Request\n"));
  if (0 == length) {
    doip_fill_header(buffer, DOIP_ALIVE_CHECK_RESPONSE, 2);
    buffer[DOIP_HEADER_LENGTH] = (node->SA >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 1] = node->SA & 0xFF;
    ret = TcpIp_TlsSend(node, buffer, DOIP_HEADER_LENGTH + 2);
    if (E_OK != ret) {
      ASLOG(DOIPE, ("send alive check response failed: %d\n", ret));
      node->stopped = true;
    }
  } else {
    ASLOG(DOIPE, ("alive check request with invalid length\n"));
    r = DOIP_E_INVAL;
  }
  return r;
}

static int doip_handle_alive_check_response(struct doip_node_s *node, uint8_t *payload,
                                            uint32_t length) {
  int r = DOIP_E_OK_SILENT;
  uint16_t sa;

  if (2 == length) {
    sa = ((uint16_t)payload[0] << 8) + payload[1];
    if (sa != node->node.LA) {
      ASLOG(DOIPE, ("alive check response with invalid SA(%X) != TA(%X)\n", sa, node->node.LA));
      node->stopped = true;
    } else {
      Std_TimerStart(&node->alive_timer);
    }
  } else {
    ASLOG(DOIPE, ("alive check response with invalid length\n"));
  }

  return r;
}

static int doip_handle_general_negative_ack(struct doip_node_s *node, uint8_t *payload,
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

static void doip_handle_tcp_response(struct doip_node_s *node, uint8_t *data, uint16_t length) {
  int r = 0;
  uint16_t payloadType = (uint16_t)-1;
  uint32_t payloadLength;
  // TODO: the data possible contains multiply response
  if ((length >= DOIP_HEADER_LENGTH) && (DOIP_PROTOCOL_VERSION == data[0]) &&
      (data[0] = ((~data[1])) & 0xFF)) {
    payloadType = ((uint16_t)data[2] << 8) + data[3];
    payloadLength =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
    ASLOG(DOIP, ("payload: %s\n", build_hexstring(data, length).c_str()));
    if ((payloadLength + DOIP_HEADER_LENGTH) <= length) {
      switch (payloadType) {
      case DOIP_GENERAL_HEADER_NEGATIVE_ACK:
        r = doip_handle_general_negative_ack(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_ROUTING_ACTIVATION_RESPONSE:
        r = doip_handle_activate_response(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_ALIVE_CHECK_REQUEST:
        r = doip_handle_alive_check_request(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_ALIVE_CHECK_RESPONSE:
        r = doip_handle_alive_check_response(node, &data[DOIP_HEADER_LENGTH], payloadLength);
        break;
      case DOIP_DIAGNOSTIC_MESSAGE:
        if (payloadLength > 4) {
          r = payloadLength - 4;
          ASLOG(DOIPI,
                ("uds reply: %s\n", build_hexstring(&data[DOIP_HEADER_LENGTH + 4], r).c_str()));
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
    doip_msg_t msg;
    doip_msg_queue_type_t queType = DOIP_MSGQ_DEFAULT;
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
    std::unique_lock<std::mutex> lck(node->lock);
    msg.payload_type = payloadType;
    msg.result = r;
    msg.data.resize(length);
    memcpy(msg.data.data(), data, length);
    node->msgQ[queType].push(msg);
    node->condVarQ[queType].notify_one();
  }
}

static doip_msg_t doip_node_wait(struct doip_node_s *node,
                                 doip_msg_queue_type_t queType = DOIP_MSGQ_DEFAULT) {
  doip_msg_t msg = {DOIP_E_TIMEOUT, 0, {}};

  std::unique_lock<std::mutex> lck(node->lock);
  if (true == node->msgQ[queType].empty()) {
    node->condVarQ[queType].wait_for(lck, std::chrono::milliseconds(5000));
  }
  if (false == node->msgQ[queType].empty()) {
    msg = node->msgQ[queType].front();
    node->msgQ[queType].pop();
  }

  return msg;
}

static doip_msg_t doip_client_wait(doip_client_t *client) {
  doip_msg_t msg = {DOIP_E_TIMEOUT, 0, {}};

  std::unique_lock<std::mutex> lck(client->lock);
  if (true == client->msgQ.empty()) {
    client->condVar.wait_for(lck, std::chrono::milliseconds(5000));
  }
  if (false == client->msgQ.empty()) {
    msg = client->msgQ.front();
    client->msgQ.pop();
  }

  return msg;
}

/* ================================ [ FUNCTIONS ] ============================================== */
doip_client_t *doip_create_client(const char *ip, int port, const char *casPem) {
  doip_client_t *client = NULL;
  TcpIp_SocketIdType discovery;
  TcpIp_SocketIdType test_equipment_request;
  Std_ReturnType ret = E_OK;
  uint16_t u16PortAny = TCPIP_PORT_ANY;
  uint16_t u16Port = port;
  TcpIp_SockAddrType ipv4Addr;
  uint32_t ipAddr = TcpIp_InetAddr(ip);

  if (FALSE == l_initialized) {
    doip_init();
    l_initialized = TRUE;
  }

  discovery = TcpIp_Create(TCPIP_IPPROTO_UDP);
  if (discovery >= 0) {
    ret = TcpIp_Bind(discovery, 0, &u16Port);
    if (E_OK != ret) {
      ASLOG(DOIPE, ("Failed to bind\n"));
      TcpIp_Close(discovery, TRUE);
      ret = E_NOT_OK;
    } else {
      TcpIp_SetupAddrFrom(&ipv4Addr, ipAddr, u16Port);
      ret = TcpIp_AddToMulticast(discovery, &ipv4Addr);
      if (E_OK == ret) {
        test_equipment_request = TcpIp_Create(TCPIP_IPPROTO_UDP);
        if (test_equipment_request < 0) {
          TcpIp_Close(discovery, TRUE);
          ret = E_NOT_OK;
        } else {
          TcpIp_Bind(test_equipment_request, TCPIP_LOCALADDRID_ANY, &u16PortAny);
          TcpIp_SetMulticastIF(test_equipment_request, TCPIP_LOCALADDRID_ANY);
        }
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    client = new doip_client_t;
    if (NULL != client) {
      client->discovery = discovery;
      client->test_equipment_request = test_equipment_request;
      client->stopped = false;
      strcpy(client->ip, ip);
      client->port = port;
      client->numOfNodes = 0;
      if (nullptr != casPem) {
        FILE *fp = fopen(casPem, "rb");
        assert(nullptr != fp);
        fseek(fp, 0, SEEK_END);
        size_t sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        client->casPem.resize(sz + 1);
        fread(client->casPem.data(), sz, 1, fp);
        client->casPem[sz] = '\0';
        fclose(fp);
      }
      STAILQ_INIT(&client->nodes);
    } else {
      ASLOG(DOIPE, ("Failed to allocate memory\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    client->thread = std::thread(doip_daemon, (void *)client);
  }

  return client;
}

int doip_await_vehicle_announcement(doip_client_t *client, doip_node_t **nodes, int numOfNodes,
                                    uint32_t timeout /* unit ms */) {
  int num = 0;
  int i = 0;
  struct doip_node_s *n;
  Std_TimerType timer;

  Std_TimerStart(&timer);
  while ((Std_GetTimerElapsedTime(&timer) < (timeout * 1000)) && (0 == client->numOfNodes)) {
    std::this_thread::sleep_for(1ms);
  }

  num = client->numOfNodes;
  if (num > numOfNodes) {
    num = numOfNodes;
  }

  if (num > 0) {
    std::unique_lock<std::mutex> lck(client->lock);
    STAILQ_FOREACH(n, &client->nodes, entry) {
      nodes[i] = &n->node;
      i++;
      if (i >= num) {
        break;
      }
    }
  }

  return client->numOfNodes;
}

doip_node_t *doip_request(doip_client_t *client) {
  Std_ReturnType ret;
  int r;
  doip_node_t *node = NULL;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t ipAddr = TcpIp_InetAddr(client->ip);
  uint8_t buffer[DOIP_HEADER_LENGTH];

  doip_fill_header(buffer, DOIP_VID_REQUEST, 0);
  TcpIp_SetupAddrFrom(&RemoteAddr, ipAddr, client->port);
  ret = TcpIp_SendTo(client->test_equipment_request, &RemoteAddr, buffer, DOIP_HEADER_LENGTH);
  if (E_OK == ret) {
    doip_msg_t msg = doip_client_wait(client);
    r = msg.result;
    if ((0 == r) && (DOIP_VAN_MSG_OR_VIN_RESPONCE == msg.payload_type)) {
      std::unique_lock<std::mutex> lck(client->lock);
      node = (doip_node_t *)STAILQ_LAST(&client->nodes, doip_node_s, entry);
    }
  }

  return node;
}

int doip_connect(doip_node_t *node) {
  int r = 0;
  TcpIp_SocketIdType sockId;
  Std_ReturnType ret = E_OK;
  struct doip_node_s *n = (struct doip_node_s *)node;

  if (n->connected) {
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
      n->sock = sockId;
      n->thread = std::thread(node_daemon, (void *)n);
      n->sem.wait();
      if (true != n->connected) {
        ASLOG(DOIPE, ("Failed to connect\n"));
        ret = E_NOT_OK;
      }
    } else {
      r = DOIP_E_NODEV;
    }
  }

  return r;
}

int doip_activate(doip_node_t *node, uint16_t sa, uint8_t at, uint8_t *oem, uint8_t oem_len) {
  int r = 0;
  Std_ReturnType ret;
  struct doip_node_s *n = (struct doip_node_s *)node;
  std::vector<uint8_t> buffer;
  buffer.resize(DOIP_HEADER_LENGTH + 7 + oem_len);

  if ((0 == oem_len) || ((4 == oem_len) && (NULL != oem))) {
  } else {
    r = DOIP_E_INVAL;
  }

  if (n->activated) {
    r = DOIP_E_AGAIN;
  }

  if (0 == r) {
    doip_fill_header(buffer.data(), DOIP_ROUTING_ACTIVATION_REQUEST, 7 + oem_len);
    buffer[DOIP_HEADER_LENGTH + 0] = (sa >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 1] = sa & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 2] = at;
    buffer[DOIP_HEADER_LENGTH + 3] = 0;
    buffer[DOIP_HEADER_LENGTH + 4] = 0;
    buffer[DOIP_HEADER_LENGTH + 5] = 0;
    buffer[DOIP_HEADER_LENGTH + 6] = 0;
    if (oem_len > 0) {
      memcpy(&buffer[DOIP_HEADER_LENGTH + 7], oem, oem_len);
    }
    ret = TcpIp_TlsSend(n, buffer.data(), DOIP_HEADER_LENGTH + 7 + oem_len);
    if (E_OK == ret) {
      doip_msg_t msg = doip_node_wait(n);
      r = msg.result;
      if ((0 == r) && (DOIP_ROUTING_ACTIVATION_RESPONSE == msg.payload_type)) {
        n->activated = true;
      } else {
        r = DOIP_E_NOT_OK;
      }
    } else {
      r = DOIP_E_ACCES;
    }
  }

  return r;
}

int doip_transmit(doip_node_t *node, uint16_t ta, const uint8_t *txBuffer, size_t txSize,
                  uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  Std_ReturnType ret;
  struct doip_node_s *n = (struct doip_node_s *)node;
  std::vector<uint8_t> buffer;
  buffer.resize(DOIP_HEADER_LENGTH + 4 + txSize);

  ASLOG(DOIPI, ("uds request to TA=%X: %s\n", ta, build_hexstring(txBuffer, txSize).c_str()));

  if (false == n->activated) {
    r = DOIP_E_AGAIN;
  }

  if (0 == r) {
    if ((2 == txSize) && (0x3E == txBuffer[0]) && (0x80 == txBuffer[1])) {
    } else {
      n->lastSID = txBuffer[0];
    }
    doip_fill_header(buffer.data(), DOIP_DIAGNOSTIC_MESSAGE, 4 + txSize);
    buffer[DOIP_HEADER_LENGTH] = (n->SA >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 1] = n->SA & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 2] = (ta >> 8) & 0xFF;
    buffer[DOIP_HEADER_LENGTH + 3] = ta & 0xFF;
    memcpy(&buffer[DOIP_HEADER_LENGTH + 4], txBuffer, txSize);
    ret = TcpIp_TlsSend(n, buffer.data(), DOIP_HEADER_LENGTH + 4 + txSize);
    if (E_OK == ret) {
      doip_msg_t msg = doip_node_wait(n, DOIP_MSGQ_UDS_ACK);
      r = msg.result;
      if ((0 == r) && (DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK == msg.payload_type)) {
      } else {
        ASLOG(DOIPE, ("no diagnostic positive ack received: r = %d, payload_type = %x\n", r,
                      msg.payload_type));
        r = DOIP_E_NOT_OK;
      }
      if (0 == r) {
        if (NULL != rxBuffer) {
          r = doip_receive(node, rxBuffer, rxSize);
        }
      }
    }
  }

  return r;
}

int doip_receive(doip_node_t *node, uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  struct doip_node_s *n = (struct doip_node_s *)node;

  if (false == n->activated) {
    r = DOIP_E_AGAIN;
  }

  if (0 == r) {
    bool bCheckNext = false;
    do {
      bCheckNext = false;
      doip_msg_t msg = doip_node_wait(n, DOIP_MSGQ_UDS_REPLY);
      r = msg.result;
      if ((r > 0) && (DOIP_DIAGNOSTIC_MESSAGE == msg.payload_type)) {
        if (rxSize >= (uint32_t)r) {
          memcpy(rxBuffer, &msg.data[DOIP_HEADER_LENGTH + 4], r);
        } else {
          r = DOIP_E_NOSPC;
        }
      } else {
        ASLOG(DOIPE,
              ("no diagnostic message received: r = %d, payload_type = %x\n", r, msg.payload_type));
        r = DOIP_E_NOT_OK;
      }
      if (r > 0) {
        if ((0x7F == rxBuffer[0]) && (n->lastSID != rxBuffer[1])) {
          ASLOG(DOIPI, ("negative response with different request SID, check next message\n"));
          bCheckNext = true;
        }
      }
    } while (true == bCheckNext);
  }

  return r;
}

void doip_destory_client(doip_client_t *client) {
  struct doip_node_s *n;
  client->stopped = true;
  if (client->thread.joinable()) {
    client->thread.join();
  }
  std::unique_lock<std::mutex> lck(client->lock);
  while (false == STAILQ_EMPTY(&client->nodes)) {
    n = STAILQ_FIRST(&client->nodes);
    STAILQ_REMOVE_HEAD(&client->nodes, entry);
    n->stopped = true;
    if (n->thread.joinable()) {
      n->thread.join();
    }
    delete n;
  }
  delete client;
}
