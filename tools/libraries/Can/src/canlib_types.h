/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef CANLIB_TYPES_H
#define CANLIB_TYPES_H
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
/* ================================ [ MACROS    ] ============================================== */
#ifndef CAN_DEVICE_NAME_SIZE
#define CAN_DEVICE_NAME_SIZE 128
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef void (*can_device_rx_notification_t)(int busid, uint32_t canid, uint8_t dlc, uint8_t *data);
typedef bool (*can_device_probe_t)(int busid, uint32_t port, uint32_t baudrate,
                                   can_device_rx_notification_t rx_notification);
typedef bool (*can_device_write_t)(uint32_t port, uint32_t canid, uint8_t dlc, const uint8_t *data);
typedef bool (*can_device_reset_t)(uint32_t port);
typedef void (*can_device_close_t)(uint32_t port);

typedef struct {
  char name[CAN_DEVICE_NAME_SIZE];
  can_device_probe_t probe;
  can_device_close_t close;
  can_device_write_t write;
  can_device_reset_t reset;
} Can_DeviceOpsType;

typedef struct {
  char device_name[CAN_DEVICE_NAME_SIZE];
  int busid;
  uint32_t port;
  uint32_t baudrate;
  const Can_DeviceOpsType *ops;
} Can_DeviceType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CANLIB_TYPES_H */
