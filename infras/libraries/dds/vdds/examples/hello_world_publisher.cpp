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
  uint32_t sessionId = 0;

#if defined(linux)
  signal(SIGINT, signalHandler);
#endif
  Publisher<HelloWorld_t> pub("/hello_wrold/xx");
  r = pub.init();
  while ((0 == r)
#if defined(linux)
         && (false == lStopped)
#endif
  ) {
    HelloWorld_t *sample = nullptr;
    r = pub.load(sample);
    if (0 == r) {
      int len = snprintf(sample->string, sizeof(sample->string), "hello world: %u", sessionId);
      ASLOG(INFO, ("publish: %s, idx = %u\n", sample->string, pub.idx(sample)));
      r = pub.publish(sample, len);
      sessionId++;
    } else if (ETIMEDOUT == r) {
      r = 0;
    } else {
      ASLOG(ERROR, ("exit as error %d\n", r));
    }
    usleep(1000000);
  }

  return r;
}