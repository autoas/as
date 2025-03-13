/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */
#ifndef ISOTP_TYPES_HPP
#define ISOTP_TYPES_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "Std_Types.h"
#include "Std_Timer.h"

#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>

#include "Semaphore.hpp"

using namespace as;
using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
struct isotp_s {
  uint8_t Channel;
  Std_TimerType timerErrorNotify;
  volatile uint32_t errorTimeout;
  struct {
    uint8_t *data;
    size_t length;
    size_t index;
  } TX;

  struct {
    uint8_t data[4096];
    size_t length;
    size_t index;
    boolean bInUse;
  } RX;

  std::thread serverThread;
  std::mutex mutex;
  Semaphore sem;

  volatile boolean running;
  volatile int result;

  isotp_parameter_t params;
  void *priv;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

#endif /* ISOTP_TYPES_HPP */
