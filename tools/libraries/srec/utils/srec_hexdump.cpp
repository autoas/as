/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>

#include "srec.h"
#include "Log.hpp"

using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -i srec_file -a address -l length\n", prog);
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
  char *srecF = nullptr;
  uint32_t address = UINT32_MAX;
  uint32_t length = 32;
  char prefix[32];
  opterr = 0;
  while ((ch = getopt(argc, argv, "i:a:l:")) != -1) {
    switch (ch) {
    case 'i':
      srecF = optarg;
      break;
    case 'a':
      address = toU32(optarg);
      break;
    case 'l':
      length = toU32(optarg);
      break;
    case 'h':
      usage(argv[0]);
      return 0;
    default:
      break;
    }
  }

  if ((nullptr == srecF) || (opterr != 0)) {
    usage(argv[0]);
    return -1;
  }

  srec_t *srec = srec_open(srecF);
  if (nullptr != srec) {
    srec_print(srec);
    for (size_t i = 0; i < srec->numOfBlks; i++) {
      if (address == UINT32_MAX) {
        Log::hexdump(Logger::INFO, "", srec->blks[i].data, srec->blks[i].length, 16);
      } else {
        if (address >= srec->blks[i].address) {
          uint32_t offset = address - srec->blks[i].address;
          if (offset < srec->blks[i].length) {
            uint32_t avail = srec->blks[i].length - offset;
            if (avail > length) {
              length = avail;
            }
            snprintf(prefix, sizeof(prefix), "%08X", address);
            Log::hexdump(Logger::INFO, prefix, srec->blks[i].data + offset, length, 16);
          }
        }
      }
    }
    srec_close(srec);
  } else {
    printf("Failed to open srec file: %s\n", srecF);
  }

  return 0;
}
