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
#if defined(linux)
static bool lStopped = false;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#if defined(linux)
static void signalHandler(int sig) {
  lStopped = true;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int r = 0;
#if defined(linux)
  signal(SIGINT, signalHandler);
#endif
  Subscriber<HelloWorld_t> sub("/hello_wrold/xx");
  while ((0 == sub.getLastError())
#if defined(linux)
         && (false == lStopped)
#endif
  ) {
    size_t size = 0;
    auto sample = (HelloWorld_t *)sub.receive(size);
    if (nullptr != sample) {
      ASLOG(INFO, ("receive: %s, len=%d, idx = %u\n", sample->string, (int)size, sub.idx(sample)));
      sub.release(sample);
    }
  }
  return r;
}