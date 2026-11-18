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
#define AS_LOG_ISOTP 0
#define AS_LOG_ISOTPI 2
#define AS_LOG_ISOTPE 3
/* ================================ [ TYPES     ] ============================================== */
struct IsoTp_s {
  uint8_t Channel;
  Std_TimerType TimerErrorNotify;
  uint32_t ErrorTimeout; /* in us */
  struct {
    uint8_t *Data;
    size_t Length;
    size_t Index;
  } Tx;

  struct {
    uint8_t Data[4096];
    size_t Length;
    size_t Index;
    boolean IsInUse;
  } Rx;

  std::thread ServerThread;
  std::mutex Mutex;
  Semaphore Sem;

  volatile boolean Running;
  volatile int Result;

  IsoTp_ParameterType Params;
  void *Priv;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* ISOTP_TYPES_HPP */
