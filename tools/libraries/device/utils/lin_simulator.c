/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "sys/queue.h"
#include <sys/time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include "TcpIp.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#define LIN_PORT_MIN 100
#define LIN_BUS_NODE_MAX 32 /* maximum node on the bus port */

#define in_range(c, lo, up) ((uint8_t)c >= lo && (uint8_t)c <= up)
#define isprint(c) in_range(c, 0x20, 0x7f)

#define LIN_MTU sizeof(Lin_FrameType)
#define LIN_MAX_DATA_SIZE 64

#define mLINID(frame) (frame->pid)
#define mLINDLC(frame) (frame->dlc)

#define LIN_TYPE_INVALID ((uint8_t)'I')
#define LIN_TYPE_BREAK ((uint8_t)'B')
#define LIN_TYPE_SYNC ((uint8_t)'S')
#define LIN_TYPE_HEADER ((uint8_t)'H')
#define LIN_TYPE_DATA ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA ((uint8_t)'F')

#define LIN_TYPE_EXT_HEADER ((uint8_t)'h')
#define LIN_TYPE_EXT_HEADER_AND_DATA ((uint8_t)'f')
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t type;
  uint32_t pid;
  uint8_t dlc;
  uint8_t data[LIN_MAX_DATA_SIZE];
  uint8_t checksum;
} Lin_FrameType;

struct Lin_SocketHandle_s {
  TcpIp_SocketIdType s; /* lin raw socket: accept */
  int error_counter;
  STAILQ_ENTRY(Lin_SocketHandle_s) entry;
};
struct Lin_SocketHandleList_s {
  TcpIp_SocketIdType s; /* lin raw socket: listen */
  Lin_FrameType frame;
  Std_TimerType timer;
  STAILQ_HEAD(, Lin_SocketHandle_s) head;
};

struct Lin_Filter_s {
  uint32_t mask;
  uint32_t code;
  STAILQ_ENTRY(Lin_Filter_s) entry;
};

struct Lin_FilterList_s {
  STAILQ_HEAD(, Lin_Filter_s) head;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static struct Lin_SocketHandleList_s *socketH = NULL;
static Std_TimerType timer0;
static struct Lin_FilterList_s *linFilterH = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static bool init_socket(int port) {
  Std_ReturnType ercd;
  TcpIp_SocketIdType s;
  uint16_t u16Port;

  TcpIp_Init(NULL);

  s = TcpIp_Create(TCPIP_IPPROTO_TCP);
  if (s < 0) {
    printf("socket function failed\nn");
    return FALSE;
  }

  u16Port = LIN_PORT_MIN + port;
  ercd = TcpIp_Bind(s, TCPIP_LOCALADDRID_ANY, &u16Port);
  if (ercd != E_OK) {
    printf("bind to port %d failed\n", port);
    TcpIp_Close(s, TRUE);
    return FALSE;
  }

  if (TcpIp_TcpListen(s, LIN_BUS_NODE_MAX) != E_OK) {
    printf("listen failed\n");
    TcpIp_Close(s, TRUE);
    return FALSE;
  }

  printf("lin(%d) socket driver on-line!\n", port);

  socketH = malloc(sizeof(struct Lin_SocketHandleList_s));
  assert(socketH);
  STAILQ_INIT(&socketH->head);
  socketH->s = s;
  socketH->frame.type = LIN_TYPE_INVALID;
  Std_TimerStop(&socketH->timer);

  return TRUE;
}

static void try_accept(void) {
  struct Lin_SocketHandle_s *handle;
  TcpIp_SocketIdType s = -1;
  TcpIp_SockAddrType RemoteAddr;
  Std_ReturnType ercd = TcpIp_TcpAccept(socketH->s, &s, &RemoteAddr);

  if (E_OK == ercd) {
    handle = malloc(sizeof(struct Lin_SocketHandle_s));
    assert(handle);
    handle->s = s;
    handle->error_counter = 0;
    STAILQ_INSERT_TAIL(&socketH->head, handle, entry);
    printf("lin socket %X on-line!\n", s);
  }
}

static void remove_socket(struct Lin_SocketHandle_s *h) {
  STAILQ_REMOVE(&socketH->head, h, Lin_SocketHandle_s, entry);
  TcpIp_Close(h->s, TRUE);
  free(h);
}
static void log_msg(Lin_FrameType *frame, float rtim) {
  int bOut = FALSE;

  struct Lin_Filter_s *filter;

  if (NULL == linFilterH) {
    bOut = TRUE;
  } else {
    STAILQ_FOREACH(filter, &linFilterH->head, entry) {
      if ((mLINID(frame) & filter->mask) == (filter->code & filter->mask)) {
        bOut = TRUE;
      }
    }
  }

  if (bOut) {
    int i;
    int dlc;
    printf("pid=%02X,dlc=%02d,data=[", mLINID(frame), mLINDLC(frame));
    dlc = mLINDLC(frame);
    if ((dlc < 8) || (dlc > 8)) {
      dlc = 8;
    }
    for (i = 0; i < dlc; i++) {
      printf("%02X,", frame->data[i]);
    }

    printf("] checksum=%02X [", frame->checksum);

    for (i = 0; i < dlc; i++) {
      if (isprint(frame->data[i])) {
        printf("%c", frame->data[i]);
      } else {
        printf(".");
      }
    }

    printf("] @ %f s\n", rtim);
  }
}

static void try_recv_forward(void) {
  uint32_t len;
  Std_ReturnType ercd;
  Lin_FrameType frame;
  struct Lin_SocketHandle_s *h;
  struct Lin_SocketHandle_s *h2;

  if (false == Std_IsTimerStarted(&timer0)) {
    Std_TimerStart(&timer0);
  }

  STAILQ_FOREACH(h, &socketH->head, entry) {
    len = LIN_MTU;
    ercd = TcpIp_Recv(h->s, (void *)&frame, &len);
    if ((E_OK == ercd) && (LIN_MTU == len)) {
      float rtim = (float)Std_GetTimerElapsedTime(&timer0) / STD_TIMER_ONE_SECOND;

      if ((frame.type == LIN_TYPE_HEADER) || (frame.type == LIN_TYPE_EXT_HEADER)) {
        printf("%c: ", (char)frame.type);
        if (socketH->frame.type != LIN_TYPE_INVALID) {
          printf("Lin Error: type %c pid=0x%02X\n", (char)socketH->frame.type, socketH->frame.pid);
        } else {
          memcpy(&socketH->frame, &frame, LIN_MTU);
        }
        Std_TimerStart(&socketH->timer);
      } else if (frame.type == LIN_TYPE_DATA) {
        if ((socketH->frame.type != LIN_TYPE_HEADER) || (frame.type != LIN_TYPE_EXT_HEADER)) {
          printf("Lin Error: type %c pid=0x%02X\n", (char)socketH->frame.type, socketH->frame.pid);
        } else {
          memcpy(&socketH->frame.dlc, &frame.dlc, LIN_MAX_DATA_SIZE + 2);
          log_msg(&socketH->frame, rtim);
        }
        socketH->frame.type = LIN_TYPE_INVALID;
      } else if ((frame.type == LIN_TYPE_HEADER_AND_DATA) ||
                 (frame.type == LIN_TYPE_EXT_HEADER_AND_DATA)) {
        printf("%c: ", (char)frame.type);
        log_msg(&frame, rtim);
        socketH->frame.type = LIN_TYPE_INVALID;
      } else {
        printf("Lin Error: type %c(%02X) is invalid\n", (char)frame.type, frame.type);
      }
      h->error_counter = 0;

      STAILQ_FOREACH(h2, &socketH->head, entry) {
        if (h != h2) {
          ercd = TcpIp_Send(h2->s, (uint8_t *)&frame, LIN_MTU);
          if (E_OK != ercd) {
            printf("send failed, remove this node %X!\n", h2->s);
            remove_socket(h2);
            break;
          }
        }
      }
    } else if (E_OK != ercd) {
      h->error_counter++;
      if (h->error_counter > 10) {
        printf("recv failed, remove this node %X!\n", h->s);
        remove_socket(h);
        break;
      }
    }
  }
}
static void schedule(void) {
  try_accept();
  try_recv_forward();

  if (socketH->frame.type != LIN_TYPE_INVALID) {
    float rtim = (float)Std_GetTimerElapsedTime(&socketH->timer) / STD_TIMER_ONE_SECOND;
    if (rtim > 1) {
      printf("Lin Error: timeout type %c pid=0x%02X\n", (char)socketH->frame.type,
             socketH->frame.pid);
      socketH->frame.type = LIN_TYPE_INVALID;
    }
  }
}

static void arg_filter(char *s) {
  char *code;
  struct Lin_Filter_s *filter = malloc(sizeof(struct Lin_Filter_s));
  assert(filter);

  code = strchr(s, '#');
  assert(code);
  code = &code[1];

  filter->mask = strtoul(s, NULL, 16);
  filter->code = strtoul(code, NULL, 16);

  if (NULL == linFilterH) {
    linFilterH = malloc(sizeof(struct Lin_FilterList_s));
    assert(linFilterH);
    STAILQ_INIT(&linFilterH->head);
  }

  STAILQ_INSERT_TAIL(&linFilterH->head, filter, entry);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage:%s <port> : 'port' is a number start from 0\n"
           "  -f <Mask>#<Code> : optional parameter for LIN log Mask and Code, in hex\n"
           "Example:\n"
           "  %s 0 -f 30#10",
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
