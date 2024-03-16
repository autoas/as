/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vdds.hpp"
#include <unistd.h>
#include <signal.h>
#include "Std_Debug.h"

using namespace as::vdds;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  char string[128];
} HelloWorld_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static bool lStopped = false;
/* ================================ [ LOCALS    ] ============================================== */
static void signalHandler(int sig) {
  lStopped = true;
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int r = 0;
  uint32_t sessionId = 0;
  int periodMs = 1000;

  int opt;
  while ((opt = getopt(argc, argv, "p:")) != -1) {
    switch (opt) {
    case 'p':
      periodMs = atoi(optarg);
      break;
    default:
      break;
    }
  }

  signal(SIGINT, signalHandler);

  Publisher<HelloWorld_t> pub("/hello_wrold/xx");
  r = pub.init();
  while ((0 == r) && (false == lStopped)) {
    HelloWorld_t *sample = nullptr;
    r = pub.load(sample);
    if (0 == r) {
      int len = snprintf(sample->string, sizeof(sample->string), "hello world: %u", sessionId);
      ASLOG(INFO, ("publish: %s, idx = %u\n", sample->string, pub.idx(sample)));
      r = pub.publish(sample, len);
      sessionId++;
    } else if ((ETIMEDOUT == r) || (ENODATA == r)) {
      r = 0;
    } else {
      ASLOG(ERROR, ("exit as error %d\n", r));
    }
    usleep(periodMs * 1000);
  }

  return r;
}