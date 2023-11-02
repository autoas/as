/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include "isotp.h"
#include "srec.h"
#include "loader.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void std_set_log_name(const char *path);
void std_set_log_level(int level);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -a app_srecord_file [-f flash_driver_srecord_file] [-l 8|64 ] [-s range]"
         "[-S crc16|crc32] [-c choice] [-F funcAddr] [-n N_TA] [-s delayUs]\n"
         "[-d device] [-p port] [-r rxid] [-t txid] [-b baudrate]\n",
         prog);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  char *device = "CAN.simulator_v2";
  int port = 0;
  int baudrate = 500000;
  uint32_t rxid = 0x732, txid = 0x731;
  uint16_t N_TA = 0xFFFF;
  uint32_t delayUs = 0;
  int funcAddr = 0x7DF;
  int ll_dl = 8;
  char *appSRecPath = NULL;
  char *flsSRecPath = NULL;
  srec_t *appSRec = NULL;
  srec_t *flsSRec = NULL;
  size_t total = 0; /* for sign */
  srec_sign_type_t signType = SREC_SIGN_CRC16;
  uint32_t timeout = 100; /* ms */
  int r = 0;
  int verbose = 0;
  const char *choice = "FBL";
  loader_args_t args;

  int progress = 0;
  int lastProgress = 0;
  char *log = NULL;

  isotp_t *isotp = NULL;
  loader_t *loader = NULL;

  isotp_parameter_t params;

  opterr = 0;
  while ((ch = getopt(argc, argv, "a:b:c:d:D:f:l:n:p:r:s:S:t:T:v")) != -1) {
    switch (ch) {
    case 'a':
      appSRecPath = optarg;
      break;
    case 'b':
      baudrate = atoi(optarg);
      break;
    case 'c':
      choice = optarg;
      break;
    case 'd':
      device = optarg;
      break;
    case 'D':
      delayUs = (uint32_t)atoi(optarg);
      break;
    case 'f':
      flsSRecPath = optarg;
      break;
    case 'F':
      funcAddr = atoi(optarg);
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
    case 's':
      total = strtoul(optarg, NULL, 10);
      break;
    case 'S':
      if (0 == strcmp("crc32", optarg)) {
        signType = SREC_SIGN_CRC32;
      } else if (0 == strcmp("crc16", optarg)) {
        signType = SREC_SIGN_CRC16;
      } else {
        usage(argv[0]);
        return -1;
      }
      break;
    case 't':
      txid = strtoul(optarg, NULL, 16);
      break;
    case 'T':
      timeout = strtoul(optarg, NULL, 10);
      break;
    case 'v':
      verbose = 1;
      break;
    default:
      break;
    }
  }

  if (total > 0) {
    if ((NULL != appSRecPath) && (NULL != flsSRecPath)) {
      printf("only can sign one srecord, but 2 specified\n");
      r = -1;
    } else {
      if (NULL != appSRecPath) {
        r = srec_sign(appSRecPath, total, signType);
      } else {
        r = srec_sign(flsSRecPath, total, signType);
      }
      return r;
    }
  }

  if ((NULL == device) || (port < 0) || (rxid < 0) || (txid < 0) || (baudrate < 0) ||
      (opterr != 0) || ((ll_dl != 8) && (ll_dl != 64)) || (funcAddr < 0)) {
    usage(argv[0]);
    return -1;
  }

  if (NULL == appSRecPath) {
    usage(argv[0]);
    r = -2;
  }

  if (0 == r) {
    appSRec = srec_open(appSRecPath);
    if (NULL == appSRec) {
      r = -3;
      printf("failed to load srecord file %s\n", appSRecPath);
    } else {
      printf("block information of %s:\n", appSRecPath);
      srec_print(appSRec);
    }
  }

  if ((0 == r) && (NULL != flsSRecPath)) {
    flsSRec = srec_open(flsSRecPath);
    if (NULL == flsSRecPath) {
      r = -4;
      printf("failed to load srecord file %s\n", flsSRecPath);
    } else {
      printf("block information of %s:\n", flsSRecPath);
      srec_print(flsSRec);
    }
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
    funcAddr = 0; /* This is not avaiable for LIN */
    params.U.LIN.delayUs = delayUs;
  } else {
    printf("%s not supported\n", device);
    usage(argv[0]);
    r = -5;
  }

  if (0 == r) {
    std_set_log_name("Loader");
  }

  if (0 == r) {
    isotp = isotp_create(&params);
    if (NULL == isotp) {
      r = -6;
    }
  }

  if (0 == r) {
    args.isotp = isotp;
    args.appSRec = appSRec;
    args.flsSRec = flsSRec;
    args.choice = choice;
    args.signType = signType;
    args.funcAddr = (uint32_t)funcAddr;
    loader = loader_create(&args);
    if (NULL == loader) {
      printf("failed to create loader\n");
      r = -7;
    } else {
      if (verbose) {
        loader_set_log_level(loader, L_LOG_DEBUG);
        std_set_log_level(0);
      }
    }
  }

  while ((0 == r) && (progress < 10000)) {
    r = loader_poll(loader, &progress, &log);
    if (NULL != log) {
      printf(log);
      free(log);
    }
    if (lastProgress != progress) {
      printf("\r\t\t\t\tprogress %2d.%02d%% ", (int)progress / 100, (int)progress % 100);
      fflush(stdout);
      lastProgress = progress;
    }
  }

  if (NULL != loader) {
    loader_destory(loader);
  }

  if (NULL != isotp) {
    isotp_destory(isotp);
  }

  return r;
}
