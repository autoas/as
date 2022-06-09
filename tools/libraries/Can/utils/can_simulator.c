/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef _WIN32
#include <Ws2tcpip.h>
#include <windows.h>
#include <winsock2.h>
#include <mmsystem.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif
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

#ifdef _WIN32
/* Link with ws2_32.lib */
#ifndef __GNUC__
#pragma comment(lib, "Ws2_32.lib")
#else
/* -lwsock32 */
#endif
#endif
/* ================================ [ MACROS    ] ============================================== */
#define CAN_MAX_DLEN 64 /* 64 for CANFD */
#define CAN_MTU sizeof(struct can_frame)
#define CAN_PORT_MIN 8000
#define CAN_BUS_NODE_MAX 32 /* maximum node on the bus port */

#define CAN_FRAME_TYPE_RAW 0
#define CAN_FRAME_TYPE_MTU 1
#define CAN_FRAME_TYPE CAN_FRAME_TYPE_RAW
#if (CAN_FRAME_TYPE == CAN_FRAME_TYPE_RAW)
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
#else
#define mCANID(frame) frame->can_id

#define mSetCANID(frame, canid)                                                                    \
  do {                                                                                             \
    frame->can_id = canid;                                                                         \
  } while (0)

#define mCANDLC(frame) (frame->can_dlc)
#define mSetCANDLC(frame, dlc)                                                                     \
  do {                                                                                             \
    frame->can_dlc = dlc;                                                                          \
  } while (0)
#endif
#define in_range(c, lo, up) ((uint8_t)c >= lo && (uint8_t)c <= up)
#define isprint(c) in_range(c, 0x20, 0x7f)

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

//#define USE_RX_DAEMON
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
#if (CAN_FRAME_TYPE == CAN_FRAME_TYPE_RAW)
  uint8_t data[CAN_MAX_DLEN + 5];
#else
  uint32_t can_id; /* 32 bit CAN_ID + EFF/RTR/ERR flags */
  uint8_t can_dlc; /* frame payload length in byte (0 .. CAN_MAX_DLEN) */
  uint8_t data[CAN_MAX_DLEN] __attribute__((aligned(8)));
#endif
};
struct Can_SocketHandle_s {
  int s; /* can raw socket: accept */
  int error_counter;
  STAILQ_ENTRY(Can_SocketHandle_s) entry;
};
struct Can_SocketHandleList_s {
  int s; /* can raw socket: listen */
#ifdef USE_RX_DAEMON
  pthread_t rx_thread;
#endif
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
#ifdef USE_RX_DAEMON
static pthread_mutex_t socketLock = PTHREAD_MUTEX_INITIALIZER;
#endif
static struct timeval m0;
static struct Can_FilterList_s *canFilterH = NULL;
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void __deinit(void) {
  TIMECAPS xTimeCaps;

  if (timeGetDevCaps(&xTimeCaps, sizeof(xTimeCaps)) == MMSYSERR_NOERROR) {
    /* Match the call to timeBeginPeriod( xTimeCaps.wPeriodMin ) made when
    the process started with a timeEndPeriod() as the process exits. */
    timeEndPeriod(xTimeCaps.wPeriodMin);
  }
}

static void __attribute__((constructor)) __init(void) {
  TIMECAPS xTimeCaps;
  if (timeGetDevCaps(&xTimeCaps, sizeof(xTimeCaps)) == MMSYSERR_NOERROR) {
    timeBeginPeriod(xTimeCaps.wPeriodMin);
    atexit(__deinit);
  }
}
#else
static int WSAGetLastError(void) {
  perror("");
  return errno;
}
static int closesocket(int s) {
  return close(s);
}
#endif
static int init_socket(int port) {
  int ercd;
  int s;
  struct sockaddr_in service;
  /* struct timeval tv; */

#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    printf("socket function failed with error: %d\n", WSAGetLastError());
    return FALSE;
  }

  service.sin_family = AF_INET;
  service.sin_addr.s_addr = inet_addr("127.0.0.1");
  service.sin_port = (u_short)htons(CAN_PORT_MIN + port);
  ercd = bind(s, (struct sockaddr *)&(service), sizeof(struct sockaddr));
  if (ercd < 0) {
    printf("bind to port %d failed with error: %d\n", port, WSAGetLastError());
    closesocket(s);
    return FALSE;
  }

  if (listen(s, CAN_BUS_NODE_MAX) < 0) {
    printf("listen failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    return FALSE;
  }

#ifdef _WIN32
  /* Set Timeout for recv call */
  /*
  tv.tv_sec  = 0;
  tv.tv_usec = 0;
  if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval)) == SOCKET_ERROR)
  {
    wprintf(L"setsockopt failed with error: %ld\n", WSAGetLastError());
    closesocket(s);
    return FALSE;
  }
  */
  /* set to non blocking mode */
  u_long iMode = 1;
  ioctlsocket(s, FIONBIO, &iMode);
#else
  int iMode = 1;
  ioctl(s, FIONBIO, (char *)&iMode);
#endif

  printf("can(%d) socket driver on-line!\n", port);

  socketH = malloc(sizeof(struct Can_SocketHandleList_s));
  assert(socketH);
  STAILQ_INIT(&socketH->head);
  socketH->s = s;

  return TRUE;
}
static void try_accept(void) {
  struct Can_SocketHandle_s *handle;
  /* struct timeval tv; */
  int s = accept(socketH->s, NULL, NULL);

  if (s >= 0) {
    /* tv.tv_sec  = 0;
    tv.tv_usec = 0; */
    handle = malloc(sizeof(struct Can_SocketHandle_s));
    assert(handle);
    handle->s = s;
    handle->error_counter = 0;
#ifdef _WIN32
    /* Set Timeout for recv call */
    /*
    if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval)) == SOCKET_ERROR)
    {
      wprintf(L"setsockopt failed with error: %ld\n", WSAGetLastError());
      closesocket(s);
      return;
    }
    */
    /* set to non blocking mode */
    u_long iMode = 1;
    ioctlsocket(s, FIONBIO, &iMode);
#else
    int iMode = 1;
    ioctl(s, FIONBIO, (char *)&iMode);
#endif
#ifdef USE_RX_DAEMON
    pthread_mutex_lock(&socketLock);
#endif
    STAILQ_INSERT_TAIL(&socketH->head, handle, entry);
#ifdef USE_RX_DAEMON
    pthread_mutex_unlock(&socketLock);
#endif
    printf("can socket %X on-line!\n", s);
  } else {
    // wprintf(L"accept failed with error: %ld\n", WSAGetLastError());
  }
}
#ifdef USE_RX_DAEMON
static void *rx_daemon(void *param) {
  (void)param;
  while (TRUE) {
    try_accept();
  }

  return NULL;
}
#endif
static void remove_socket(struct Can_SocketHandle_s *h) {
  STAILQ_REMOVE(&socketH->head, h, Can_SocketHandle_s, entry);
  closesocket(h->s);
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
  int len;
  struct can_frame frame;
  struct Can_SocketHandle_s *h;
  struct Can_SocketHandle_s *h2;
#ifdef USE_RX_DAEMON
  pthread_mutex_lock(&socketLock);
#endif
  STAILQ_FOREACH(h, &socketH->head, entry) {
    len = recv(h->s, (void *)&frame, CAN_MTU, 0);
    if (CAN_MTU == len) {
      struct timeval m1;

      gettimeofday(&m1, NULL);
      float rtim = m1.tv_sec - m0.tv_sec;

      if (m1.tv_usec > m0.tv_usec) {
        rtim += (float)(m1.tv_usec - m0.tv_usec) / 1000000.0;
      } else {
        rtim = rtim - 1 + (float)(1000000.0 + m1.tv_usec - m0.tv_usec) / 1000000.0;
      }

      log_msg(&frame, rtim);
      h->error_counter = 0;

      STAILQ_FOREACH(h2, &socketH->head, entry) {
        if (h != h2) {
          if (send(h2->s, (const char *)&frame, CAN_MTU, 0) != CAN_MTU) {
            printf("send failed with error: %d, remove this node %X!\n", WSAGetLastError(), h2->s);
            remove_socket(h2);
            break;
          }
        }
      }
    } else if (-1 == len) {
#ifdef _WIN32
      if (10035 != WSAGetLastError())
#else
      if (EAGAIN != errno)
#endif
      {
        printf("recv failed with error: %d, remove this node %X!\n", WSAGetLastError(), h->s);
        remove_socket(h);
        break;
      } else {
        /* Resource temporarily unavailable. */
      }
    } else {
#ifdef __LINUX__
      printf("recv failed with error: %d, remove this node %X!\n", WSAGetLastError(), h->s);
      remove_socket(h);
      break;
#else
      h->error_counter++;
      if (h->error_counter > 10) {
        printf("recv failed with error: %d, remove this node %X!\n", WSAGetLastError(), h->s);
        remove_socket(h);
        break;
      }
#endif
    }
  }
#ifdef USE_RX_DAEMON
  pthread_mutex_unlock(&socketLock);
#endif
}
static void schedule(void) {
#ifndef USE_RX_DAEMON
  try_accept();
#endif
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
           "  %s 0 -f 700#300",
           argv[0], argv[0]);
    return -1;
  }
  gettimeofday(&m0, NULL);
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

#ifdef USE_RX_DAEMON
  if (0 == pthread_create(&(socketH->rx_thread), NULL, rx_daemon, NULL)) {
  } else {
    return -1;
  }
#endif
  for (;;) {
    schedule();
    usleep(1000);
  }

  return 0;
}
