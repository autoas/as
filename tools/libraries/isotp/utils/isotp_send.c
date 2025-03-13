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
#include "canlib.h"
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
#define rxDT rxid
#define txDT txid
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -d device -p port -r rxid -t txid -v AABBCCDDEEFF.. -w -l LL_DL -b baudrate "
         "-n N_TA -s delayUs\n"
         "\tdevice: protocol.device, for examples, \"CAN.simulator\", \"LIN.simulator\".\n"
         "\tfor J1939Tp: txid = txDT, rxid = rxDT, and follow options required\n"
         "\t-c txCM -f txFC -s txDirect -C rxCM -F rxFC -S rxDirect -j [CMDT|BAM]\n",
         prog);
}

static uint32_t toU32(const char *strV) {
  uint32_t u32V = 0;

  if ((0 == strncmp("0x", strV, 2)) || (0 == strncmp("0X", strV, 2))) {
    u32V = strtoul(strV, NULL, 16);
  } else {
    u32V = strtoul(strV, NULL, 10);
  }

  return u32V;
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
  int echo = 0;
  int quitRx = 0; /* whether do tx only */
  isotp_t *isotp;
  int baudrate = 500000;
  isotp_parameter_t params;
  static const uint8_t testerKeep[2] = {0x3E, 0x00 | 0x80};

  memset(&params, 0, sizeof(isotp_parameter_t));
  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:D:g:l:n:p:r:t:v:T:wec:C:f:F:s:S:j:q")) != -1) {
    switch (ch) {
    case 'b':
      baudrate = toU32(optarg);
      break;
    case 'd':
      device = optarg;
      break;
    case 'D':
      delayUs = (uint32_t)toU32(optarg);
      break;
    case 'l':
      ll_dl = toU32(optarg);
      break;
    case 'n':
      N_TA = toU32(optarg);
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'r':
      rxid = toU32(optarg);
      break;
    case 't':
      txid = toU32(optarg);
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
    case 'e':
      echo = 1;
      break;
    case 'q':
      quitRx = 1;
      break;
    case 'h':
      usage(argv[0]);
      return 0;
    default:
      break;
    }
  }

  if ((NULL == device) || (port < 0) || (length > sizeof(data)) || (baudrate < 0) ||
      (opterr != 0)) {
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
    params.U.CAN.STmin = delayUs;
  } else {
    printf("%s not supported\n", device);
    usage(argv[0]);
    return -1;
  }

  isotp = isotp_create(&params);

  if (NULL == isotp) {
    r = -2;
  }

  if (length > 0) {
    printf("TX: ");
    for (i = 0; i < length; i++) {
      printf("%02X ", data[i]);
    }
    printf("\n");
    if (quitRx) {
      r = isotp_transmit(isotp, data, length, NULL, 0);
    } else {
      r = isotp_transmit(isotp, data, length, data, sizeof(data));
    }
    do {
      if (quitRx) {
        break;
      }
      if (r > 0) {
        printf("RX: ");
        for (i = 0; i < r; i++) {
          printf("%02X ", data[i]);
        }
        printf("\n");
        if (wait) {
          isotp_transmit(isotp, testerKeep, 2, NULL, 0);
        }
      } else {
        printf("failed with error %d\n", r);
        break;
      }
      if (wait) {
        r = isotp_receive(isotp, data, sizeof(data));
      }
    } while (wait);
  } else {
    printf("going to receive TP data:\n");
    do {
      r = isotp_receive(isotp, data, sizeof(data));
      if (r > 0) {
        printf("RX: ");
        for (i = 0; i < r; i++) {
          printf("%02X ", data[i]);
        }
        printf("\n");
        if (echo) {
          r = isotp_transmit(isotp, data, r, NULL, 0);
          if (r < 0) {
            printf("echo back failed: %d\n", r);
          }
        }
      }
    } while (wait);
  }

  if (NULL != isotp) {
    isotp_destory(isotp);
  }

  return r;
}
