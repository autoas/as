/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "canlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "Std_Types.h"
#include "Std_Timer.h"
#include <vector>
#include <thread>
#include <chrono>
#include <string>

using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  std::string name;
  int baudrate = 500000;
  int port = 0;
} Can_Device_t;
/* ================================ [ DECLARES  ] ============================================== */
static bool s_bStop = false;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void usage(char *prog) {
  printf("usage: %s -d device [-p port] [-b baudrate] -d device [-p port] [-b baudrate]\n", prog);
}

static void sig_handler(int signo) {
  if (signo == SIGINT) {
    s_bStop = true;
  }
}

static void ThreadMain(Can_Device_t device0, Can_Device_t device1) {
  int busid[2];
  bool rv;
  bool bHasMsg;
  can_frame_t frame;

  printf("Bridge:\n");
  printf("  device 0: %s port=%d, baudrare=%d\n", device0.name.c_str(), device0.port,
         device0.baudrate);
  printf("  device 1: %s port=%d, baudrare=%d\n", device1.name.c_str(), device1.port,
         device1.baudrate);

  busid[0] = can_open(device0.name.c_str(), (uint32_t)device0.port, (uint32_t)device0.baudrate);
  if (busid[0] >= 0) {
    busid[1] = can_open(device1.name.c_str(), (uint32_t)device1.port, (uint32_t)device1.baudrate);
  }

  if ((busid[0] >= 0) && (busid[1] >= 0)) {
    while (false == s_bStop) {
      bHasMsg = FALSE;
      frame.canid = -1;
      rv = can_read_v2(busid[0], &frame);
      if (rv) {
        can_write_v2(busid[1], &frame);
        bHasMsg = TRUE;
      }
      frame.canid = -1;
      rv = can_read_v2(busid[1], &frame);
      if (rv) {
        can_write_v2(busid[0], &frame);
        bHasMsg = TRUE;
      }
      if (FALSE == bHasMsg) {
        std::this_thread::sleep_for(1ms); /* sleep to wait a message to be ready */
      }
    }
  }

  if (busid[0] >= 0) {
    (void)can_close(busid[0]);
  }

  if (busid[1] >= 0) {
    (void)can_close(busid[1]);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  int ch;
  std::vector<Can_Device_t> devices;
  std::vector<std::thread *> threads;

  opterr = 0;
  while ((ch = getopt(argc, argv, "b:d:p:")) != -1) {
    switch (ch) {
    case 'b':
      if (devices.size() > 0) {
        Can_Device_t &device = devices.back();
        device.baudrate = atoi(optarg);
      }
      break;
    case 'd': {
      Can_Device_t device;
      device.name = optarg;
      devices.push_back(device);
      break;
    }
    case 'p':
      if (devices.size() > 0) {
        Can_Device_t &device = devices.back();
        device.port = atoi(optarg);
      }
      break;
    default:
      break;
    }
  }

  if ((0 == devices.size()) || (opterr != 0)) {
    usage(argv[0]);
    return -1;
  }

  signal(SIGINT, sig_handler);

  for (size_t i = 0; i < (devices.size() / 2); i++) {
    std::thread *th = new std::thread(ThreadMain, devices[2 * i], devices[2 * i + 1]);
    threads.push_back(th);
  }
  for (auto th : threads) {
    if (th->joinable()) {
      th->join();
    }
  }

  return 0;
}
