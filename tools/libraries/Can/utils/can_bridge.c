/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "canlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "Std_Types.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -d device [-p port] [-b baudrate] -d device [-p port] [-b baudrate]\n", prog);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  char *device[2] = {NULL, NULL};
  int baudrate[2] = {500000, 500000};
  int port[2] = {0, 0};
  int busid[2] = {-1, -1};
  int deviceIndex = 0;
  int baudrateIndex = 0;
  int portIndex = 0;

  bool rv;
  bool bHasMsg;
  uint32_t canid = -1;
  uint8_t dlc = 0;
  uint8_t data[64];

  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:p:")) != -1) {
    switch (ch) {
    case 'b':
      if (baudrateIndex < 2) {
        baudrate[baudrateIndex++] = atoi(optarg);
      }
      break;
    case 'd':
      if (deviceIndex < 2) {
        device[deviceIndex++] = optarg;
      }
      break;
    case 'p':
      if (portIndex < 2) {
        port[portIndex++] = atoi(optarg);
      }
      break;
    default:
      break;
    }
  }

  if ((NULL == device[0]) || (NULL == device[1]) || (port[0] < 0) || (port[1] < 0) ||
      (baudrate[0] < 0) || (baudrate[1] < 0) || (opterr != 0)) {
    usage(argv[0]);
    return -1;
  }

  printf("device 0: %s port=%d, baudrare=%d\n", device[0], port[0], baudrate[0]);
  printf("device 1: %s port=%d, baudrare=%d\n", device[1], port[1], baudrate[1]);

  busid[0] = can_open(device[0], (uint32_t)port[0], (uint32_t)baudrate[0]);
  if (busid[0] >= 0) {
    busid[1] = can_open(device[1], (uint32_t)port[1], (uint32_t)baudrate[1]);
  }

  if ((busid[0] >= 0) && (busid[1] >= 0)) {
    while (TRUE) {
      bHasMsg = FALSE;
      canid = -1;
      dlc = sizeof(data);
      rv = can_read(busid[0], &canid, &dlc, data);
      if (rv) {
        can_write(busid[1], canid, dlc, data);
        bHasMsg = TRUE;
      }
      canid = -1;
      dlc = sizeof(data);
      rv = can_read(busid[1], &canid, &dlc, data);
      if (rv) {
        can_write(busid[0], canid, dlc, data);
        bHasMsg = TRUE;
      }
      if (FALSE == bHasMsg) {
        Std_Sleep(1000); /* sleep to wait a message to be ready */
      }
    }
  }

  if (busid[0] >= 0) {
    (void)can_close(busid[0]);
  }

  if (busid[1] >= 0) {
    (void)can_close(busid[1]);
  }

  return 0;
}
