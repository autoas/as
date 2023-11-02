/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <isotp.h>
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
  printf("usage: %s -d device -p port -r rxid -t txid -v AABBCCDDEEFF.. -w -l LL_DL -b baudrate "
         "-n N_TA -s delayUs\n"
         "\tdevice: protocol.device, for examples, \"CAN.simulator\", \"LIN.simulator\".\n",
         prog);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  char *device = "CAN.simulator_v2";
  int port = 0;
  uint32_t rxid = 0x732, txid = 0x731;
  uint16_t N_TA = 0xFFFF;
  uint32_t delayUs = 0;
  int ll_dl = 8;
  int length = 0;
  uint8_t data[4095];
  char bytes[3] = {0, 0, 0};
  int i;
  int r = 0;
  int wait = 0;
  isotp_t *isotp;
  int baudrate = 500000;
  uint32_t timeout = 100; /* ms */
  isotp_parameter_t params;
  static const uint8_t testerKeep[2] = {0x3E, 0x00 | 0x80};

  memset(&params, 0, sizeof(isotp_parameter_t));
  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:D:l:n:p:r:t:v:T:w")) != -1) {
    switch (ch) {
    case 'b':
      baudrate = atoi(optarg);
      break;
    case 'd':
      device = optarg;
      break;
    case 'D':
      delayUs = (uint32_t)atoi(optarg);
      break;
    case 'l':
      ll_dl = atoi(optarg);
      break;
    case 'n':
      N_TA = strtoul(optarg, NULL, 16);
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'r':
      rxid = strtoul(optarg, NULL, 16);
      break;
    case 't':
      txid = strtoul(optarg, NULL, 16);
      break;
    case 'T':
      timeout = strtoul(optarg, NULL, 10);
      break;
    case 'v':
      length = strlen(optarg) / 2;
      for (i = 0; (i < length) && (i < sizeof(data)); i++) {
        bytes[0] = optarg[2 * i];
        bytes[1] = optarg[2 * i + 1];
        data[i] = strtoul(bytes, NULL, 16);
      }
      break;
    case 'w':
      wait = 1;
      break;
    default:
      break;
    }
  }

  if ((NULL == device) || (port < 0) || (rxid < 0) || (txid < 0) || (length > sizeof(data)) ||
      (length <= 0) || (baudrate < 0) || (opterr != 0) || ((ll_dl != 8) && (ll_dl != 64))) {
    usage(argv[0]);
    return -1;
  }

  params.baudrate = (uint32_t)baudrate;
  params.port = port;
  params.ll_dl = ll_dl;
  params.N_TA = N_TA;
  if (0 == strncmp("CAN", device, 3)) {
    strcpy(params.device, &device[4]);
    params.protocol = ISOTP_OVER_CAN;
    params.U.CAN.RxCanId = (uint32_t)rxid;
    params.U.CAN.TxCanId = (uint32_t)txid;
    params.U.CAN.BlockSize = 8;
    params.U.CAN.STmin = 0;
  } else if (0 == strncmp("LIN", device, 3)) {
    if (0x731 == txid) {
      txid = 0x3c;
    }
    if (0x732 == rxid) {
      rxid = 0x3d;
    }
    strcpy(params.device, &device[4]);
    params.protocol = ISOTP_OVER_LIN;
    params.U.LIN.RxId = (uint32_t)rxid;
    params.U.LIN.TxId = (uint32_t)txid;
    params.U.LIN.timeout = timeout;
    params.U.LIN.delayUs = delayUs;
  } else {
    printf("%s not supported\n", device);
    usage(argv[0]);
    return -1;
  }

  isotp = isotp_create(&params);

  if (NULL == isotp) {
    r = -2;
  }

  printf("TX: ");
  for (i = 0; i < length; i++) {
    printf("%02X ", data[i]);
  }
  printf("\n");
  r = isotp_transmit(isotp, data, length, data, sizeof(data));

  do {
    if (r > 0) {
      printf("RX: ");
      for (i = 0; i < r; i++) {
        printf("%02X ", data[i]);
      }
      printf("\n");
      isotp_transmit(isotp, testerKeep, 2, NULL, 0);
    } else {
      printf("failed with error %d\n", r);
      break;
    }
    if (wait) {
      r = isotp_receive(isotp, data, sizeof(data));
    }
  } while (wait);

  if (NULL != isotp) {
    isotp_destory(isotp);
  }

  return r;
}
