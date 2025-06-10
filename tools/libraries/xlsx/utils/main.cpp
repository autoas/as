/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include "XLTester.hpp"

using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
void std_set_log_name(const char *path);
void std_set_log_level(int level);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -t path/to/test_spec.xlsx\n", prog);
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int r = 0;
  int ch;
  char *spec = nullptr;

  opterr = 0;
  while ((ch = getopt(argc, argv, "t:")) != -1) {
    switch (ch) {
    case 't':
      spec = optarg;
      break;
    default:
      break;
    }
  }

  if (nullptr == spec) {
    usage(argv[0]);
    return -1;
  }

  XLTester tester;

  r = tester.start(spec);

  if (0 != r) {
    printf("%s", tester.getMsg().c_str());
  } else {
    int progress = 0;
    int lastProgress = 0;
    do {
      progress = tester.getProgress();
      auto msg = tester.getMsg();
      if (msg.size() > 0) {
        printf("%s", msg.c_str());
      }

      if (lastProgress != progress) {
        printf("\r%2d.%02d%% ", (int)progress / (XLTESTER_PROGRESS_DONE / 100),
               (int)progress % (XLTESTER_PROGRESS_DONE / 100));
        fflush(stdout);
        lastProgress = progress;
      }
      usleep(1000);
    } while ((progress >= 0) && (progress < XLTESTER_PROGRESS_DONE));
  }

  return r;
}
