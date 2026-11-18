/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef CANLIB_TYPES_HPP
#define CANLIB_TYPES_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef USE_STD_PRINTF
#undef USE_STD_PRINTF
#endif
#include "Std_Debug.h"
#include "Std_Types.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <string>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef void (*CanL_DeviceRxNotificationType)(int busId, uint32_t canId, uint8_t dlc, uint8_t *data,
                                              uint64_t timestamp);
typedef bool (*CanL_DeviceProbeType)(int busId, uint32_t port, uint32_t baudrate,
                                     CanL_DeviceRxNotificationType rxNotification);
typedef bool (*CanL_DeviceWriteType)(uint32_t port, uint32_t canId, uint8_t dlc,
                                     const uint8_t *data, uint64_t timestamp);
typedef bool (*CanL_DeviceResetType)(uint32_t port);
typedef void (*CanL_DeviceCloseType)(uint32_t port);
typedef void (*CanL_DeviceReadType)(uint32_t port);

typedef struct {
  std::string Name;
  CanL_DeviceProbeType Probe;
  CanL_DeviceCloseType Close;
  CanL_DeviceWriteType Write;
  CanL_DeviceResetType Reset;
  CanL_DeviceReadType Read;
} CanL_DeviceOpsType;

typedef struct {
  std::string DeviceName;
  int BusId;
  uint32_t Port;
  uint32_t Baudrate;
  const CanL_DeviceOpsType *Ops;
} CanL_DeviceType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
bool CanL_IsPerfMode(void);
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CANLIB_TYPES_HPP */
