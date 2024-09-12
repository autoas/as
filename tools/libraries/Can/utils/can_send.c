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
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -d device -p port -b baudrate -i canid -v AABBCCDDEEFF..\n", prog);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  char *device = "simulator_v2";
  int port = 0;
  uint32_t canid = -1;
  int dlc = 0;
  uint8_t data[64];
  char bytes[3] = {0, 0, 0};
  int i;
  int rv = TRUE;
  int busid;
  int baudrate = 500000;

  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:i:p:v:")) != -1) {
    switch (ch) {
    case 'b':
      baudrate = atoi(optarg);
      break;
    case 'd':
      device = optarg;
      break;
    case 'i':
      canid = strtoul(optarg, NULL, 16);
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'v':
      dlc = strlen(optarg) / 2;
      for (i = 0; (i < dlc) && (i < sizeof(data)); i++) {
        bytes[0] = optarg[2 * i];
        bytes[1] = optarg[2 * i + 1];
        data[i] = strtoul(bytes, NULL, 16);
      }
      break;
    default:
      break;
    }
  }

  if ((NULL == device) || (port < 0) || (dlc > sizeof(data)) || (baudrate < 0) ||
      (opterr != 0)) {
    usage(argv[0]);
    return -1;
  }

  busid = can_open(device, (uint32_t)port, (uint32_t)baudrate);
  if (busid >= 0) {
    rv = can_write(busid, canid, dlc, data);
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
