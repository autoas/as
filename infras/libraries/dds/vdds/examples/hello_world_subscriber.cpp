/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vdds.hpp"
#include "vring.hpp"
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

  signal(SIGINT, signalHandler);

  Subscriber<HelloWorld_t> sub("/hello_wrold/xx");
  r = sub.init();
  while ((0 == r) && (false == lStopped)) {
    size_t size = 0;
    HelloWorld_t *sample = nullptr;
    r = sub.receive(sample, size);
    if (0 == r) {
      ASLOG(INFO, ("receive: %s, len=%d, idx = %u\n", sample->string, (int)size, sub.idx(sample)));
      r = sub.release(sample);
    } else if ((ETIMEDOUT == r) || (ENOMSG == r)) {
      r = 0;
    } else {
      ASLOG(ERROR, ("exit as error %d\n", r));
    }
  }
  return r;
}