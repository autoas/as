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
         "-n N_TA -s delayUs [-V version]\n"
         "\tdevice: protocol.device, for examples, \"CAN.simulator\", \"LIN.simulator\".\n"
         "\tfor J1939Tp: txid = txDT, rxid = rxDT, and follow options required\n"
         "\t-c txCM -f txFC -s txDirect -C rxCM -F rxFC -S rxDirect -j [CMDT|BAM]\n"
         "\t-V version: 1 for CAN ISO-TP v1, 2 for CAN ISO-TP v2 (default)\n",
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
  isotp_j1939tp_protocal_t jprotocol = ISOTP_J1939TP_PROTOCOL_CMDT;
  uint32_t rxCM = 0x18ECFFE8, rxDirect = 0x18FECAE8, rxFC = 0x18ECFFE9;
  uint32_t txCM = 0x17ECFFE8, txDirect = 0x17FECAE8, txFC = 0x17ECFFE9;
  uint16_t N_TA = 0xFFFF;
  uint32_t delayUs = 0;
  uint32_t PgPGN = 0;
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
  uint32_t timeout = 100; /* ms */
  isotp_parameter_t params;
  static const uint8_t testerKeep[2] = {0x3E, 0x00 | 0x80};
  isotp_can_version_t version = ISOTP_CAN_V2;

  memset(&params, 0, sizeof(isotp_parameter_t));
  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:D:g:l:n:p:r:t:v:V:T:wec:C:f:F:s:S:j:q")) != -1) {
    switch (ch) {
    case 'b':
      baudrate = toU32(optarg);
      break;
    case 'd':
      device = optarg;
      if (0 == strncmp("J1939TP", device, 7)) {
        txDT = 0x17EBFFE8;
        rxDT = 0x18EBFFE8;
      }
      break;
    case 'D':
      delayUs = (uint32_t)toU32(optarg);
      break;
    case 'g':
      PgPGN = (uint32_t)toU32(optarg);
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
    case 'T':
      timeout = toU32(optarg);
      break;
    case 'v':
      length = strlen(optarg) / 2;
      for (i = 0; (i < length) && (i < sizeof(data)); i++) {
        bytes[0] = optarg[2 * i];
        bytes[1] = optarg[2 * i + 1];
        data[i] = strtoul(bytes, NULL, 16);
      }
      break;
    case 'V':
      version = (isotp_can_version_t)toU32(optarg);
      break;
    case 'w':
      wait = 1;
      break;
    case 'e':
      echo = 1;
      break;
    case 'c':
      txCM = toU32(optarg);
      break;
    case 'f':
      txFC = toU32(optarg);
      break;
    case 's':
      txDirect = toU32(optarg);
      break;
    case 'C':
      rxCM = toU32(optarg);
      break;
    case 'F':
      rxFC = toU32(optarg);
      break;
    case 'S':
      rxDirect = toU32(optarg);
      break;
    case 'j':
      if (0 == strncmp("BAM", optarg, 3)) {
        jprotocol = ISOTP_J1939TP_PROTOCOL_BAM;
      } else if (0 == strncmp("CMDT", optarg, 4)) {
        jprotocol = ISOTP_J1939TP_PROTOCOL_CMDT;
      } else {
        usage(argv[0]);
        return -1;
      }
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
    params.U.CAN.version = version;
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
  } else if (0 == strncmp("J1939TP", device, 7)) {
    strcpy(params.device, &device[8]);
    params.protocol = ISOTP_OVER_J1939TP;
    params.U.J1939TP.TX.CM = txCM | CAN_ID_EXTENDED;
    params.U.J1939TP.TX.Direct = txDirect | CAN_ID_EXTENDED;
    params.U.J1939TP.TX.DT = txDT | CAN_ID_EXTENDED;
    params.U.J1939TP.TX.FC = txFC;
    params.U.J1939TP.RX.CM = rxCM;
    params.U.J1939TP.RX.Direct = rxDirect;
    params.U.J1939TP.RX.DT = rxDT;
    params.U.J1939TP.RX.FC = rxFC | CAN_ID_EXTENDED;
    params.U.J1939TP.protocol = jprotocol;
    params.U.J1939TP.PgPGN = PgPGN;

    printf("J1939TP protocol %s\n", ISOTP_J1939TP_PROTOCOL_CMDT == jprotocol ? "CMDT" : "BAM");
    printf("  TX: CM=%08X DT=%08X Direct=%08X FC=%08X\n", txCM, txid, txDirect, txFC);
    printf("  RX: CM=%08X DT=%08X Direct=%08X FC=%08X\n", rxCM, rxid, rxDirect, rxFC);
    printf("  reverse:\n\t%s -d %s"
           " -c 0x%08X -t 0x%08X -f 0x%08X -s 0x%08X"
           " -C 0x%08X -r 0x%08X -F 0x%08X -S 0x%08X"
           " -j %s\n",
           argv[0], device, rxCM, rxDT, rxFC, rxDirect, txCM, txDT, txFC, txDirect,
           ISOTP_J1939TP_PROTOCOL_CMDT == jprotocol ? "CMDT" : "BAM");

    params.U.J1939TP.STMin = 10;
    params.U.J1939TP.Tr = 200;
    params.U.J1939TP.T1 = 750;
    params.U.J1939TP.T2 = 1250;
    params.U.J1939TP.T3 = 1250;
    params.U.J1939TP.T4 = 1050;
    params.U.J1939TP.TxMaxPacketsPerBlock = 8;
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
        if (ISOTP_OVER_J1939TP == params.protocol) {
          (void)isotp_ioctl(isotp, ISOTP_IOCTL_J1939TP_GET_PGN, &PgPGN, sizeof(PgPGN));
          printf("RX PgPGN %" PRIx32 ": ", PgPGN);
        } else {
          printf("RX: ");
        }
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
