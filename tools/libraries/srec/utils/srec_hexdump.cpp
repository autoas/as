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

  Srec_FileType *srec = Srec_Open(srecF);
  if (nullptr != srec) {
    Srec_Print(srec);
    for (size_t i = 0; i < srec->NumOfBlks; i++) {
      if (address == UINT32_MAX) {
        Log::HexDump(Logger::INFO, "", srec->Blks[i].Data, srec->Blks[i].Length, 16);
      } else {
        if (address >= srec->Blks[i].Address) {
          uint32_t offset = address - srec->Blks[i].Address;
          if (offset < srec->Blks[i].Length) {
            uint32_t avail = srec->Blks[i].Length - offset;
            if (avail > length) {
              length = avail;
            }
            snprintf(prefix, sizeof(prefix), "%08X", address);
            Log::HexDump(Logger::INFO, prefix, srec->Blks[i].Data + offset, length, 16);
          }
        }
      }
    }
    Srec_Close(srec);
  } else {
    printf("Failed to open srec file: %s\n", srecF);
  }

  return 0;
}
