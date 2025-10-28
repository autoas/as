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
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -i srec_file -o binary\n", prog);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  char *srecF = nullptr;
  char *binaryF = nullptr;
  opterr = 0;
  while ((ch = getopt(argc, argv, "i:o:")) != -1) {
    switch (ch) {
    case 'i':
      srecF = optarg;
      break;
    case 'o':
      binaryF = optarg;
      break;
    case 'h':
      usage(argv[0]);
      return 0;
    default:
      break;
    }
  }

  if ((nullptr == srecF) || (nullptr == binaryF) || (opterr != 0)) {
    usage(argv[0]);
    return -1;
  }

  srec_t *srec = srec_open(srecF);
  if (nullptr != srec) {
    srec_print(srec);
    for (size_t i = 0; i < srec->numOfBlks; i++) {
      std::string binaryF_ = binaryF;
      if (i != 0) {
        binaryF_ += "." + std::to_string(i);
      }
      FILE *fp = fopen(binaryF_.c_str(), "wb");
      if (nullptr != fp) {
        fwrite(srec->blks[i].data, srec->blks[i].length, 1, fp);
        fclose(fp);
        printf("SAVE binary file: %s\n", binaryF_.c_str());
      } else {
        printf("Failed to create binary file: %s\n", binaryF_.c_str());
      }
    }
    srec_close(srec);
  }

  return 0;
}
