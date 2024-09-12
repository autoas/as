/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "canlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "Std_Types.h"
#include "osal.h"
#ifdef _WIN32
#include <direct.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
int busid;
uint32_t rxid = 0x7FF;
uint32_t txid = 0x7FE;
uint32_t trid = 0;
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -d device -p port -b baudrate -r rxid -t txid -T trid\n", prog);
}

static void can_stdin(void *args) {
  uint8_t dlc = 0;
  uint8_t data[64];
  uint32_t canid;
  bool rv;
  int i;
  while (TRUE) {
    canid = rxid;
    dlc = sizeof(data);
    rv = can_read(busid, &canid, &dlc, data);
    if (TRUE == rv) {
      for (i = 0; i < dlc; i++) {
        printf("%c", data[i]);
      }
    } else {
      OSAL_SleepUs(1000);
    }
  }
}

static void can_trace(void *args) {
  uint8_t dlc = 0;
  uint8_t data[64];
  uint32_t canid;
  bool rv;
#ifdef _WIN32
  _mkdir("log");
#else
  mkdir("log", 0777);
#endif
  FILE *fp = fopen("log/trace.bin", "wb");
  if (NULL == fp) {
    printf("can't create log/trace.bin\n");
    return;
  }
  while (TRUE) {
    canid = trid;
    dlc = sizeof(data);
    rv = can_read(busid, &canid, &dlc, data);
    if (TRUE == rv) {
      fwrite(data, dlc, 1, fp);
    } else {
      fflush(fp);
      OSAL_SleepUs(1000);
    }
  }
}

static void can_stdout(void) {
  int i;
  size_t len;
  char inputs[512];
  while (TRUE) {
    if (fgets(inputs, sizeof(inputs) - 1, stdin) != NULL) {
      len = strlen(inputs);
      inputs[len++] = '\n';
      for (i = 0; i < len; i += 8) {
        uint8_t dlc = len - i;
        if (dlc > 8) {
          dlc = 8;
        }
        (void)can_write(busid, txid, dlc, (uint8_t *)&(inputs[i]));
      }
    } else {
      OSAL_SleepUs(1000);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  char *device = "simulator_v2";
  int port = 0;
  int rv = TRUE;
  int baudrate = 500000;

  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:p:r:t:T:")) != -1) {
    switch (ch) {
    case 'b':
      baudrate = atoi(optarg);
      break;
    case 'd':
      device = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'r':
      rxid = strtoul(optarg, NULL, 16);
      break;
    case 't':
      rxid = strtoul(optarg, NULL, 16);
      break;
    case 'T':
      trid = strtoul(optarg, NULL, 16);
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
  if (busid >= 0) {
    OSAL_ThreadCreate(can_stdin, NULL);
    if (0 != trid) {
      OSAL_ThreadCreate(can_trace, NULL);
    }
    can_stdout();
  } else {
    rv = FALSE;
  }

  if (busid >= 0) {
    (void)can_close(busid);
  }

  if (FALSE == rv) {
    return -2;
  }
  return 0;
}
