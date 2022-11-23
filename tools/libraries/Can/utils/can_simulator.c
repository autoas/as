/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <unistd.h>
#include "TcpIp.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#define CAN_MAX_DLEN 64 /* 64 for CANFD */
#define CAN_MTU sizeof(struct can_frame)
#define CAN_PORT_MIN 8000
#define CAN_BUS_NODE_MAX 32 /* maximum node on the bus port */

#define mCANID(frame)                                                                              \
  (((uint32_t)frame->data[CAN_MAX_DLEN + 0] << 24) +                                               \
   ((uint32_t)frame->data[CAN_MAX_DLEN + 1] << 16) +                                               \
   ((uint32_t)frame->data[CAN_MAX_DLEN + 2] << 8) + ((uint32_t)frame->data[CAN_MAX_DLEN + 3]))

#define mSetCANID(frame, canid)                                                                    \
  do {                                                                                             \
    frame->data[CAN_MAX_DLEN + 0] = (uint8_t)(canid >> 24);                                        \
    frame->data[CAN_MAX_DLEN + 1] = (uint8_t)(canid >> 16);                                        \
    frame->data[CAN_MAX_DLEN + 2] = (uint8_t)(canid >> 8);                                         \
    frame->data[CAN_MAX_DLEN + 3] = (uint8_t)(canid);                                              \
  } while (0)

#define mCANDLC(frame) ((uint8_t)frame->data[CAN_MAX_DLEN + 4])
#define mSetCANDLC(frame, dlc)                                                                     \
  do {                                                                                             \
    frame->data[CAN_MAX_DLEN + 4] = dlc;                                                           \
  } while (0)

#define in_range(c, lo, up) ((uint8_t)c >= lo && (uint8_t)c <= up)
#define isprint(c) in_range(c, 0x20, 0x7f)
/* ================================ [ TYPES     ] ============================================== */
/**
 * struct can_frame - basic CAN frame structure
 * @can_id:  CAN ID of the frame and CAN_*_FLAG flags, see canid_t definition
 * @can_dlc: frame payload length in byte (0 .. 8) aka data length code
 *           N.B. the DLC field from ISO 11898-1 Chapter 8.4.2.3 has a 1:1
 *           mapping of the 'data length code' to the real payload length
 * @data:    CAN frame payload (up to 8 byte)
 */
struct can_frame {
  uint8_t data[CAN_MAX_DLEN + 5];
};
struct Can_SocketHandle_s {
  TcpIp_SocketIdType s; /* can raw socket: accept */
  int error_counter;
  STAILQ_ENTRY(Can_SocketHandle_s) entry;
};

struct Can_SocketHandleList_s {
  TcpIp_SocketIdType s; /* can raw socket: listen */
  STAILQ_HEAD(, Can_SocketHandle_s) head;
};

struct Can_Filter_s {
  uint32_t mask;
  uint32_t code;
  STAILQ_ENTRY(Can_Filter_s) entry;
};

struct Can_FilterList_s {
  STAILQ_HEAD(, Can_Filter_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static struct Can_SocketHandleList_s *socketH = NULL;
static struct Can_FilterList_s *canFilterH = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static int init_socket(int port) {
  Std_ReturnType ercd;
  TcpIp_SocketIdType s;
  uint16_t u16Port;
  TcpIp_Init(NULL);

  s = TcpIp_Create(TCPIP_IPPROTO_TCP);
  if (s < 0) {
    printf("socket function failed with error: %d\n", s);
    return FALSE;
  }

  u16Port = CAN_PORT_MIN + port;
  ercd = TcpIp_Bind(s, TCPIP_LOCALADDRID_ANY, &u16Port);
  if (E_OK != ercd) {
    printf("bind to port %d failed with error: %d\n", port, ercd);
    TcpIp_Close(s, TRUE);
    return FALSE;
  }
  ercd = TcpIp_TcpListen(s, CAN_BUS_NODE_MAX);
  if (E_OK != ercd) {
    printf("listen failed with error: %d\n", ercd);
    TcpIp_Close(s, TRUE);
    return FALSE;
  }

  printf("can(%d) socket driver on-line!\n", port);

  socketH = malloc(sizeof(struct Can_SocketHandleList_s));
  assert(socketH);
  STAILQ_INIT(&socketH->head);
  socketH->s = s;

  return TRUE;
}

static void try_accept(void) {
  struct Can_SocketHandle_s *handle;
  Std_ReturnType ercd;
  TcpIp_SocketIdType s;
  TcpIp_SockAddrType RemoteAddr;

  ercd = TcpIp_TcpAccept(socketH->s, &s, &RemoteAddr);
  if (E_OK == ercd) {
    handle = malloc(sizeof(struct Can_SocketHandle_s));
    assert(handle);
    handle->s = s;
    handle->error_counter = 0;
    STAILQ_INSERT_TAIL(&socketH->head, handle, entry);
    printf("can socket %X on-line!\n", s);
  }
}

static void remove_socket(struct Can_SocketHandle_s *h) {
  STAILQ_REMOVE(&socketH->head, h, Can_SocketHandle_s, entry);
  TcpIp_Close(h->s, TRUE);
  free(h);
}

static void log_msg(struct can_frame *frame, float rtim) {
  int bOut = FALSE;
  static float lastTime = -1;
  int nSame = 0;

  if (-1 == lastTime) {
    lastTime = rtim;
  }

  struct Can_Filter_s *filter;

  if (NULL == canFilterH) {
    bOut = TRUE;
  } else {
    STAILQ_FOREACH(filter, &canFilterH->head, entry) {
      if ((mCANID(frame) & filter->mask) == (filter->code & filter->mask)) {
        bOut = TRUE;
      }
    }
  }

  if (bOut) {
    int i;
    int dlc;
    printf("canid=%08X,dlc=%02d,data=[", mCANID(frame), mCANDLC(frame));
    dlc = mCANDLC(frame);
    if (dlc < 8) {
      dlc = 8;
    }

    for (i = 0; i < dlc; i++) {
      if ((i >= 8) && (frame->data[i] == frame->data[i - 1])) {
        nSame++;
      } else {
        if (nSame > 0) {
          printf("`%d,", nSame);
        } else {
          printf("%02X,", frame->data[i]);
        }
        nSame = 0;
      }
    }

    if (nSame > 0) {
      printf("`%d,", nSame);
    }

    nSame = 0;

    printf("] [");

    for (i = 0; i < dlc; i++) {
      if ((i >= 8) && (frame->data[i] == frame->data[i - 1])) {
        nSame++;
        continue;
      } else {
        if (isprint(frame->data[i])) {
          printf("%c", frame->data[i]);
        } else {
          printf(".");
        }
        if (nSame > 0) {
          printf("`");
        }
        nSame = 0;
      }
    }
    if (nSame > 0) {
      printf("`");
    }

    printf("] @ %f s rel %.2f ms\n", rtim, (rtim - lastTime) * 1000);
    lastTime = rtim;
  }
}
static void try_recv_forward(void) {
  uint32_t len;
  Std_ReturnType ercd;
  struct can_frame frame;
  struct Can_SocketHandle_s *h;
  struct Can_SocketHandle_s *h2;
  static Std_TimerType timer;

  if (false == Std_IsTimerStarted(&timer)) {
    Std_TimerStart(&timer);
  }

  STAILQ_FOREACH(h, &socketH->head, entry) {
    len = CAN_MTU;
    ercd = TcpIp_Recv(h->s, (uint8_t *)&frame, &len);
    if ((E_OK == ercd) && (CAN_MTU == len)) {
      std_time_t elapsedTime = Std_GetTimerElapsedTime(&timer);
      float rtim = elapsedTime / 1000000.0;

      log_msg(&frame, rtim);
      h->error_counter = 0;

      STAILQ_FOREACH(h2, &socketH->head, entry) {
        if (h != h2) {
          ercd = TcpIp_Send(h2->s, (uint8_t *)&frame, CAN_MTU);
          if (E_OK != ercd) {
            printf("send failed with error: %d, remove this node %X!\n", ercd, h2->s);
            remove_socket(h2);
            break;
          }
        }
      }
    } else if (E_OK != ercd) {
      h->error_counter++;
      if (h->error_counter > 10) {
        printf("recv failed with error: %d, remove this node %X!\n", ercd, h->s);
        remove_socket(h);
        break;
      }
    }
  }
}

static void staus_monitor(void) {
  Std_ReturnType ercd = E_NOT_OK;
  struct Can_SocketHandle_s *h;

  while ((ercd != E_OK) && (FALSE == STAILQ_EMPTY(&socketH->head))) {
    STAILQ_FOREACH(h, &socketH->head, entry) {
      ercd = TcpIp_IsTcpStatusOK(h->s);
      if (E_OK != ercd) {
        printf("can socket %X off-line!\n", h->s);
        remove_socket(h);
        break;
      }
    }
  }
}

static void schedule(void) {
  try_accept();
  staus_monitor();
  try_recv_forward();
}

static void arg_filter(char *s) {
  char *code;
  struct Can_Filter_s *filter = malloc(sizeof(struct Can_Filter_s));
  assert(filter);

  code = strchr(s, '#');
  assert(code);
  code = &code[1];

  filter->mask = strtoul(s, NULL, 16);
  filter->code = strtoul(code, NULL, 16);

  if (NULL == canFilterH) {
    canFilterH = malloc(sizeof(struct Can_FilterList_s));
    assert(canFilterH);
    STAILQ_INIT(&canFilterH->head);
  }

  STAILQ_INSERT_TAIL(&canFilterH->head, filter, entry);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage:%s <port> : 'port' is a number start from 0\n"
           "  -f <Mask>#<Code> : optional parameter for CAN log Mask and Code, in hex\n"
           "Example:\n"
           "  %s 0 -f 700#300 | tee",
           argv[0], argv[0]);
    return -1;
  }

  if (FALSE == init_socket(atoi(argv[1]))) {
    return -1;
  }

  argc = argc - 2;
  argv = argv + 2;
  while (argc >= 2) {
    if (0 == strcmp(argv[0], "-f")) {
      arg_filter(argv[1]);
    }

    argc = argc - 2;
    argv = argv + 2;
  }

  for (;;) {
    schedule();
    usleep(1000);
  }

  return 0;
}
