/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <Std_Types.h>
#include <Std_Timer.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/queue.h>
#include "TcpIp.h"
#include "doip_client.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DOIP 0
#define AS_LOG_DOIPI 2
#define AS_LOG_DOIPE 3

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
struct doip_node_s {
  doip_node_t node;
  uint8_t FA;
  uint8_t status;
  uint16_t SA;
  uint16_t LA;
  TcpIp_SockAddrType RemoteAddr;
  TcpIp_SocketIdType sock;
  bool connected;
  bool activated;
  pthread_t thread;
  pthread_mutex_t lock;
  sem_t sem;
  Std_TimerType alive_request_timer;
  Std_TimerType alive_timer;
  struct {
    int result;
    uint16_t payload_type;
  } R; /* for response */
  bool stopped;
  struct doip_client_s *client;
  STAILQ_ENTRY(doip_node_s) entry;
  uint8_t buffer[4096];
};

struct doip_client_s {
  TcpIp_SocketIdType discovery;
  TcpIp_SocketIdType test_equipment_request;
  char ip[32];
  int port;
  pthread_t thread;
  pthread_mutex_t lock;
  bool stopped;
  sem_t sem;
  struct {
    int result;
    uint16_t payload_type;
  } R; /* for response */
  STAILQ_HEAD(, doip_node_s) nodes;
  uint32_t numOfNodes;
  uint8_t buffer[4096];
};
/* ================================ [ DECLARES  ] ============================================== */
static void doip_handle_tcp_response(struct doip_node_s *node, uint8_t *data, uint16_t length);
/* ================================ [ DATAS     ] ============================================== */
static pthread_once_t l_initOnce;
/* ================================ [ LOCALS    ] ============================================== */
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
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_lock(&client->lock);
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
    sem_init(&node->sem, 0, 0);
    pthread_mutex_init(&node->lock, &attr);
    STAILQ_INSERT_TAIL(&client->nodes, node, entry);
    client->numOfNodes++;
    ASLOG(DOIPI, ("discovery node %d.%d.%d.%d:%d online\n", node->RemoteAddr.addr[0],
                  node->RemoteAddr.addr[1], node->RemoteAddr.addr[2], node->RemoteAddr.addr[3],
                  node->RemoteAddr.port));
  } else {
    free(node);
  }
  pthread_mutex_unlock(&client->lock);
}

static int doip_handle_VAN_VIN_response(doip_client_t *client, TcpIp_SockAddrType *RemoteAddr,
                                        uint8_t *payload, uint32_t length) {
  struct doip_node_s *node;
  int r = 0;

  ASLOG(DOIP, ("VAN message with length=%d\n", length));
  if (33 == length) {
    node = (struct doip_node_s *)malloc(sizeof(*node));
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

  ASLOG(DOIP,
        ("udp message from %d.%d.%d.%d:%d, length=%d\n", RemoteAddr->addr[0], RemoteAddr->addr[1],
         RemoteAddr->addr[2], RemoteAddr->addr[3], RemoteAddr->port, length));

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
    client->R.payload_type = payloadType;
    client->R.result = r;
    sem_post(&client->sem);
  }
}

static void *doip_daemon(void *arg) {
  Std_ReturnType ret;
  doip_client_t *client = (doip_client_t *)arg;
  uint32_t length;
  TcpIp_SockAddrType RemoteAddr;

  ASLOG(DOIPI, ("DoIP Client request on %s:%d\n", client->ip, client->port));
  while (false == client->stopped) {
    pthread_mutex_lock(&client->lock);
    length = sizeof(client->buffer);
    ret = TcpIp_RecvFrom(client->discovery, &RemoteAddr, client->buffer, &length);
    if ((E_OK == ret) && (length > 0)) {
      doip_handle_udp_message(client, &RemoteAddr, client->buffer, length);
    }
    length = sizeof(client->buffer);
    ret = TcpIp_RecvFrom(client->test_equipment_request, &RemoteAddr, client->buffer, &length);
    if ((E_OK == ret) && (length > 0)) {
      doip_handle_udp_message(client, &RemoteAddr, client->buffer, length);
    }
    pthread_mutex_unlock(&client->lock);
    usleep(1000);
  }

  ASLOG(DOIPI, ("DoIP Client offline\n"));

  TcpIp_Close(client->discovery, TRUE);

  return NULL;
}

static void doip_alive_check_request(struct doip_node_s *node) {
  Std_ReturnType ret;
  doip_fill_header(node->buffer, DOIP_ALIVE_CHECK_REQUEST, 0);
  ret = TcpIp_Send(node->sock, node->buffer, DOIP_HEADER_LENGTH);
  if (E_OK != ret) {
    ASLOG(DOIPE, ("send alive check request failed: %d\n", ret));
    node->stopped = true;
  }
}

static void *node_daemon(void *arg) {
  Std_ReturnType ret;
  struct doip_node_s *node = (struct doip_node_s *)arg;
  uint32_t length;
  ASLOG(DOIPI, ("DoIP node online\n"));
  node->connected = true;
  Std_TimerStart(&node->alive_request_timer);
  Std_TimerStart(&node->alive_timer);
  sem_post(&node->sem);
  while (false == node->stopped) {
    length = sizeof(node->buffer);
    pthread_mutex_lock(&node->lock);
    ret = TcpIp_Recv(node->sock, node->buffer, &length);
    if ((E_OK == ret) && (length > 0)) {
      doip_handle_tcp_response(node, node->buffer, length);
    }
    if (Std_GetTimerElapsedTime(&node->alive_request_timer) > DOIP_ALIVE_CHECK_TIME) {
      doip_alive_check_request(node);
      Std_TimerStart(&node->alive_request_timer);
    }
    if (Std_GetTimerElapsedTime(&node->alive_timer) > DOIP_ALIVE_TIMEOUT) {
      ASLOG(DOIPI, ("alive timer timeout, stop this node\n"));
      node->stopped = true;
    }
    pthread_mutex_unlock(&node->lock);
    usleep(1000);
  }

  pthread_mutex_lock(&node->lock);
  TcpIp_Close(node->sock, TRUE);
  node->connected = false;
  node->activated = false;
  sem_destroy(&node->sem);
  pthread_mutex_unlock(&node->lock);

  ASLOG(DOIPI, ("DoIP node offline\n"));
  return NULL;
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
  ASLOG(DOIP, ("Alive Check Request\n"));
  if (0 == length) {
    doip_fill_header(node->buffer, DOIP_ALIVE_CHECK_RESPONSE, 2);
    node->buffer[DOIP_HEADER_LENGTH] = (node->SA >> 8) & 0xFF;
    node->buffer[DOIP_HEADER_LENGTH + 1] = node->SA & 0xFF;
    ret = TcpIp_Send(node->sock, node->buffer, DOIP_HEADER_LENGTH + 2);
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

  if ((length >= DOIP_HEADER_LENGTH) && (DOIP_PROTOCOL_VERSION == data[0]) &&
      (data[0] = ((~data[1])) & 0xFF)) {
    payloadType = ((uint16_t)data[2] << 8) + data[3];
    payloadLength =
      ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
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
        } else {
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
    node->R.payload_type = payloadType;
    node->R.result = r;
    sem_post(&node->sem);
  }
}

static int doip_node_wait(struct doip_node_s *node) {
  int r;
  struct timespec ts;

  node->R.result = DOIP_E_TIMEOUT;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 5;
  pthread_mutex_unlock(&node->lock);
  sem_timedwait(&node->sem, &ts);
  r = node->R.result;
  pthread_mutex_lock(&node->lock);

  return r;
}

static void doip_node_clear(struct doip_node_s *node) {
  int r;
  do {
    r = sem_trywait(&node->sem);
  } while (0 == r);
}

static int doip_client_wait(doip_client_t *client) {
  int r;
  struct timespec ts;

  client->R.result = DOIP_E_TIMEOUT;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 5;
  pthread_mutex_unlock(&client->lock);
  sem_timedwait(&client->sem, &ts);
  r = client->R.result;
  pthread_mutex_lock(&client->lock);

  return r;
}

static void doip_client_clear(doip_client_t *client) {
  int r;
  do {
    r = sem_trywait(&client->sem);
  } while (0 == r);
}
/* ================================ [ FUNCTIONS ] ============================================== */
doip_client_t *doip_create_client(const char *ip, int port) {
  doip_client_t *client = NULL;
  TcpIp_SocketIdType discovery;
  TcpIp_SocketIdType test_equipment_request;
  Std_ReturnType ret = E_OK;
  uint16_t u16Port = port;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  TcpIp_SockAddrType ipv4Addr;
  uint32_t ipAddr = TcpIp_InetAddr(ip);

  pthread_once(&l_initOnce, doip_init);

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
          TcpIp_Bind(test_equipment_request, 0, &u16Port);
        }
      }
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    client = malloc(sizeof(doip_client_t));
    if (NULL != client) {
      client->discovery = discovery;
      client->test_equipment_request = test_equipment_request;
      client->stopped = false;
      strcpy(client->ip, ip);
      client->port = port;
      client->numOfNodes = 0;
      STAILQ_INIT(&client->nodes);
      sem_init(&client->sem, 0, 0);
      pthread_mutex_init(&client->lock, &attr);
    } else {
      ASLOG(DOIPE, ("Failed to allocate memory\n"));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    pthread_create(&client->thread, NULL, doip_daemon, client);
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
    usleep(1000);
  }

  num = client->numOfNodes;
  if (num > numOfNodes) {
    num = numOfNodes;
  }

  if (num > 0) {
    pthread_mutex_lock(&client->lock);
    STAILQ_FOREACH(n, &client->nodes, entry) {
      nodes[i] = &n->node;
      i++;
      if (i >= num) {
        break;
      }
    }
    pthread_mutex_unlock(&client->lock);
  }

  return client->numOfNodes;
}

doip_node_t *doip_request(doip_client_t *client) {
  Std_ReturnType ret;
  int r;
  doip_node_t *node = NULL;
  TcpIp_SockAddrType RemoteAddr;
  uint32_t ipAddr = TcpIp_InetAddr(client->ip);

  doip_client_clear(client);
  pthread_mutex_lock(&client->lock);
  doip_fill_header(client->buffer, DOIP_VID_REQUEST, 0);
  TcpIp_SetupAddrFrom(&RemoteAddr, ipAddr, client->port);
  ret =
    TcpIp_SendTo(client->test_equipment_request, &RemoteAddr, client->buffer, DOIP_HEADER_LENGTH);
  if (E_OK == ret) {
    r = doip_client_wait(client);
    if ((0 == r) && (DOIP_VAN_MSG_OR_VIN_RESPONCE == client->R.payload_type)) {
      node = (doip_node_t *)STAILQ_LAST(&client->nodes, doip_node_s, entry);
    }
  }
  pthread_mutex_unlock(&client->lock);

  return node;
}

int doip_connect(doip_node_t *node) {
  int r = 0;
  TcpIp_SocketIdType sockId;
  Std_ReturnType ret = E_OK;
  struct doip_node_s *n = (struct doip_node_s *)node;

  pthread_mutex_lock(&n->lock);
  if (n->connected) {
    r = DOIP_E_AGAIN;
  }
  pthread_mutex_unlock(&n->lock);

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
      pthread_create(&n->thread, NULL, node_daemon, n);
      sem_wait(&n->sem);
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

  if ((0 == oem_len) || ((4 == oem_len) && (NULL != oem))) {
  } else {
    r = DOIP_E_INVAL;
  }
  pthread_mutex_lock(&n->lock);
  if (n->activated) {
    r = DOIP_E_AGAIN;
  }
  pthread_mutex_unlock(&n->lock);

  if (0 == r) {
    doip_node_clear(n);
    pthread_mutex_lock(&n->lock);
    doip_fill_header(n->buffer, DOIP_ROUTING_ACTIVATION_REQUEST, 7 + oem_len);
    n->buffer[DOIP_HEADER_LENGTH] = (sa >> 8) & 0xFF;
    n->buffer[DOIP_HEADER_LENGTH + 1] = sa & 0xFF;
    n->buffer[DOIP_HEADER_LENGTH + 2] = at;
    if (oem_len > 0) {
      memcpy(&n->buffer[DOIP_HEADER_LENGTH + 3], oem, oem_len);
    }
    ret = TcpIp_Send(n->sock, n->buffer, DOIP_HEADER_LENGTH + 7 + oem_len);
    if (E_OK == ret) {
      r = doip_node_wait(n);
      if ((0 == r) && (DOIP_ROUTING_ACTIVATION_RESPONSE == n->R.payload_type)) {
        n->activated = true;
      } else {
        r = DOIP_E_NOT_OK;
      }
    } else {
      r = DOIP_E_ACCES;
    }
    pthread_mutex_unlock(&n->lock);
  }

  return r;
}

int doip_transmit(doip_node_t *node, uint16_t ta, const uint8_t *txBuffer, size_t txSize,
                  uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  Std_ReturnType ret;
  struct doip_node_s *n = (struct doip_node_s *)node;

  pthread_mutex_lock(&n->lock);
  if (false == n->activated) {
    r = DOIP_E_AGAIN;
  }
  pthread_mutex_unlock(&n->lock);

  if (0 == r) {
    doip_node_clear(n);
    pthread_mutex_lock(&n->lock);
    doip_fill_header(n->buffer, DOIP_DIAGNOSTIC_MESSAGE, 4 + txSize);
    n->buffer[DOIP_HEADER_LENGTH] = (n->SA >> 8) & 0xFF;
    n->buffer[DOIP_HEADER_LENGTH + 1] = n->SA & 0xFF;
    n->buffer[DOIP_HEADER_LENGTH + 2] = (ta >> 8) & 0xFF;
    n->buffer[DOIP_HEADER_LENGTH + 3] = ta & 0xFF;
    memcpy(&n->buffer[DOIP_HEADER_LENGTH + 4], txBuffer, txSize);
    ret = TcpIp_Send(n->sock, n->buffer, DOIP_HEADER_LENGTH + 4 + txSize);
    if (E_OK == ret) {
      r = doip_node_wait(n);
      if ((0 == r) && (DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK == n->R.payload_type)) {
      } else {
        r = DOIP_E_NOT_OK;
      }

      if (0 == r) {
        if (NULL != rxBuffer) {
          r = doip_node_wait(n);
          if ((r > 0) && (DOIP_DIAGNOSTIC_MESSAGE == n->R.payload_type)) {
            if (rxSize >= r) {
              memcpy(rxBuffer, &n->buffer[DOIP_HEADER_LENGTH + 4], r);
            } else {
              r = DOIP_E_NOSPC;
            }
          } else {
            r = DOIP_E_NOT_OK;
          }
        }
      }
    }
    pthread_mutex_unlock(&n->lock);
  }

  return r;
}

int doip_receive(doip_node_t *node, uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  struct doip_node_s *n = (struct doip_node_s *)node;

  pthread_mutex_lock(&n->lock);
  if (false == n->activated) {
    r = DOIP_E_AGAIN;
  }
  pthread_mutex_unlock(&n->lock);

  if (0 == r) {
    pthread_mutex_lock(&n->lock);

    r = doip_node_wait(n);
    if ((r > 0) && (DOIP_DIAGNOSTIC_MESSAGE == n->R.payload_type)) {
      if (rxSize >= r) {
        memcpy(rxBuffer, &n->buffer[DOIP_HEADER_LENGTH + 4], r);
      } else {
        r = DOIP_E_NOSPC;
      }
    } else {
      r = DOIP_E_NOT_OK;
    }

    pthread_mutex_unlock(&n->lock);
  }

  return r;
}

void doip_destory_client(doip_client_t *client) {
  struct doip_node_s *n;
  client->stopped = true;
  pthread_join(client->thread, NULL);
  pthread_mutex_lock(&client->lock);
  while (false == STAILQ_EMPTY(&client->nodes)) {
    n = STAILQ_FIRST(&client->nodes);
    STAILQ_REMOVE_HEAD(&client->nodes, entry);
    n->stopped = true;
    pthread_join(n->thread, NULL);
    free(n);
  }
  pthread_mutex_unlock(&client->lock);
  free(client);
}
