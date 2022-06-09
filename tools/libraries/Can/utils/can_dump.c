/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "canlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "Std_Types.h"
#include "Std_Timer.h"
#include <sys/queue.h>
#include <assert.h>
#include <ctype.h>
/* ================================ [ MACROS    ] ============================================== */
#define CAN_MAX_DLEN 64 /* 64 for CANFD */
/* ================================ [ TYPES     ] ============================================== */
struct can_frame {
  uint32_t can_id;
  uint8_t can_dlc;
  uint8_t data[CAN_MAX_DLEN];
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
static struct Can_FilterList_s *canFilterH = NULL;
/* ================================ [ LOCALS    ] ============================================== */
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
      if ((frame->can_id & filter->mask) == (filter->code & filter->mask)) {
        bOut = TRUE;
      }
    }
  }

  if (bOut) {
    int i;
    int dlc;
    printf("canid=%08X,dlc=%02d,data=[", frame->can_id, frame->can_dlc);
    dlc = frame->can_dlc;
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
static void usage(char *prog) {
  printf("usage: %s -d device -p port -b baudrate -f <Mask>#<Code>\n"
         "  -f <Mask>#<Code> : optional parameter for CAN log Mask and Code, in hex\n"
         "Example:\n"
         "  %s -d simulator_v2 -p 0 -f 700#300\n",
         prog, prog);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  char *device = "simulator";
  int busid;
  int port = 0;
  int baudrate = 500000;
  struct can_frame frame;
  bool rv;
  Std_TimerType timer;

  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:f:hp:")) != -1) {
    switch (ch) {
    case 'b':
      baudrate = atoi(optarg);
      break;
    case 'd':
      device = optarg;
      break;
    case 'f':
      arg_filter(optarg);
      break;
    case 'h':
      usage(argv[0]);
      return 0;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    default:
      break;
    }
  }

  if ((NULL == device) || (port < 0) || (baudrate < 0) || (opterr != 0)) {
    usage(argv[0]);
    return -1;
  }

  busid = can_open(device, (uint32_t)port, (uint32_t)baudrate);
  if (busid < 0) {
    return -2;
  }

  Std_TimerStart(&timer);

  while (true) {
    do {
      frame.can_dlc = sizeof(frame.data);
      frame.can_id = -1;
      rv = can_read(busid, &frame.can_id, &frame.can_dlc, frame.data);
      if (true == rv) {
        std_time_t elapsedTime = Std_GetTimerElapsedTime(&timer);
        float rtim = elapsedTime / 1000000.0;
        log_msg(&frame, rtim);
      }
    } while (true == rv);
    Std_Sleep(1000);
  }

  return 0;
}
