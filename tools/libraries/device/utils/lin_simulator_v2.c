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
#if defined(_WIN32)
#include <objbase.h>
#else
#include <uuid/uuid.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#define LIN_CAST_IP TCPIP_IPV4_ADDR(224, 244, 224, 245)
#define LIN_PORT_MIN 10000

#if defined(_WIN32)
#define LIN_UUID_LENGTH sizeof(GUID)
#else
#define LIN_UUID_LENGTH 16
#endif

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

struct lin_frame {
  Lin_FrameType frame;
  uint8_t uuid[LIN_UUID_LENGTH];
};

struct Lin_SocketHandleList_s {
  TcpIp_SocketIdType s; /* lin raw socket: listen */
  Lin_FrameType frame;
  Std_TimerType timer;
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
  Std_ReturnType ret;
  TcpIp_SocketIdType s;
  TcpIp_SockAddrType ipv4Addr;
  uint16_t u16Port;

  TcpIp_Init(NULL);

  s = TcpIp_Create(TCPIP_IPPROTO_UDP);
  if (s < 0) {
    printf("socket function failed\nn");
    return FALSE;
  }

  u16Port = LIN_PORT_MIN + port;
  ret = TcpIp_SetNonBlock(s, FALSE);
  if (E_OK == ret) {
    ret = TcpIp_SetTimeout(s, 10);
  }
  if (E_OK == ret) {
    ret = TcpIp_Bind(s, TCPIP_LOCALADDRID_ANY, &u16Port);
  }
  if (ret != E_OK) {
    printf("bind to port %d failed\n", port);
    TcpIp_Close(s, TRUE);
    return FALSE;
  }

  if (E_OK == ret) {
    TcpIp_SetupAddrFrom(&ipv4Addr, LIN_CAST_IP, u16Port);
    ret = TcpIp_AddToMulticast(s, &ipv4Addr);
  }

  if (ret != E_OK) {
    printf("add multicast failed\n");
    TcpIp_Close(s, TRUE);
    return FALSE;
  }

  printf("lin(%d) socket driver on-line!\n", port);

  socketH = malloc(sizeof(struct Lin_SocketHandleList_s));
  assert(socketH);
  socketH->s = s;
  socketH->frame.type = LIN_TYPE_INVALID;
  Std_TimerStop(&socketH->timer);

  return TRUE;
}

static void log_msg(Lin_FrameType *frame, double rtim) {
  static double preTime = 0.f;

  int bOut = FALSE;
  double deltaTime = 0.f;
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

    if (0.f != preTime) {
      deltaTime = (rtim - preTime) * 1000;
    }

    printf("] @ %f s %.1f ms \n", (float)rtim, (float)deltaTime);

    preTime = rtim;
  }
}

static void try_recv(void) {
  uint32_t len;
  Std_ReturnType ret;
  struct lin_frame lframe;
  TcpIp_SockAddrType RemoteAddr;
  Lin_FrameType frame;

  if (false == Std_IsTimerStarted(&timer0)) {
    Std_TimerStart(&timer0);
  }

  len = sizeof(lframe);
  ret = TcpIp_RecvFrom(socketH->s, &RemoteAddr, (void *)&lframe, &len);
  if ((E_OK == ret) && (sizeof(lframe) == len)) {
    double rtim = (double)Std_GetTimerElapsedTime(&timer0) / STD_TIMER_ONE_SECOND;
    frame = lframe.frame;
    if ((frame.type == LIN_TYPE_HEADER) || (frame.type == LIN_TYPE_EXT_HEADER)) {
      printf("%c: ", (char)frame.type);
      if (socketH->frame.type != LIN_TYPE_INVALID) {
        printf("Lin Error with header: type %c pid=0x%02X\n", (char)socketH->frame.type,
               socketH->frame.pid);
        printf("  %c: ", (char)frame.type);
        log_msg(&frame, rtim);
      } else {
        memcpy(&socketH->frame, &frame, LIN_MTU);
      }
      Std_TimerStart(&socketH->timer);
    } else if (frame.type == LIN_TYPE_DATA) {
      if ((socketH->frame.type != LIN_TYPE_HEADER) && (frame.type != LIN_TYPE_EXT_HEADER)) {
        printf("Lin Error with data: type %c pid=0x%02X\n", (char)socketH->frame.type,
               socketH->frame.pid);
        printf("  %c: ", (char)frame.type);
        log_msg(&frame, rtim);
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
  }
}
static void schedule(void) {
  try_recv();

  if (socketH->frame.type != LIN_TYPE_INVALID) {
    double rtim = (double)Std_GetTimerElapsedTime(&socketH->timer) / STD_TIMER_ONE_SECOND;
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
