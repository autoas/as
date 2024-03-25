/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "vdds.hpp"
#include <unistd.h>
#include <signal.h>
#include "Std_Debug.h"
#include <thread>

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

static void PubMain(int periodMs) {
  int r;
  uint32_t sessionId = 0;

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
      exit(r);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
  }
}

static void SubMain(int subId) {
  int r;
  Subscriber<HelloWorld_t> sub("/hello_wrold/xx");

  r = sub.init();
  while ((0 == r) && (false == lStopped)) {
    size_t size = 0;
    HelloWorld_t *sample = nullptr;
    r = sub.receive(sample, size);
    if (0 == r) {
      ASLOG(INFO, ("%d: receive: %s, len=%d, idx = %u\n", subId, sample->string, (int)size,
                   sub.idx(sample)));
      r = sub.release(sample);
    } else if ((ETIMEDOUT == r) || (ENOMSG == r)) {
      r = 0;
    } else {
      ASLOG(ERROR, ("%d: exit as error %d\n", subId, r));
      exit(r);
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int r = 0;
  int periodMs = 1000;
  int nSub = 1;
  std::vector<std::thread *> threads;

  int opt;
  while ((opt = getopt(argc, argv, "n:p:")) != -1) {
    switch (opt) {
    case 'n':
      nSub = atoi(optarg);
      break;
    case 'p':
      periodMs = atoi(optarg);
      break;
    default:
      break;
    }
  }

  signal(SIGINT, signalHandler);

  std::thread *th = new std::thread(PubMain, periodMs);
  threads.push_back(th);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  for (int i = 0; i < nSub; i++) {
    th = new std::thread(SubMain, i);
    threads.push_back(th);
  }

  for (auto &th : threads) {
    th->join();
  }

  return r;
}