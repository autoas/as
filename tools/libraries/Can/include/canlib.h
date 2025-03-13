/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef CANLIB_H
#define CANLIB_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stdbool.h>
#include "PAL.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define CAN_ID_EXTENDED 0x80000000U

#ifndef CAN_MAX_MTU
#define CAN_MAX_MTU 64
#endif
/* ================================ [ TYPES     ] ============================================== */

typedef struct {
  uint32_t canid;
  uint8_t dlc;
  uint8_t data[CAN_MAX_MTU];
  uint64_t timestamp; /* in nanoseconds */
} can_frame_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int can_open(const char *device_name, uint32_t port, uint32_t baudrate);
bool can_write(int busid, uint32_t canid, uint8_t dlc, const uint8_t *data);
bool can_read(int busid, uint32_t *canid /* InOut */, uint8_t *dlc /* InOut */, uint8_t *data);
bool can_write_v2(int busid, can_frame_t *can_frame);
bool can_read_v2(int busid, can_frame_t *can_frame);
bool can_close(int busid);
bool can_reset(int busid);

/* wait a specific message to be received. if canid = -1, any CAN message */
bool can_wait(int busid, uint32_t canid, uint32_t timeoutMs);
#ifdef __cplusplus
}
#endif
#endif /* CANLIB_H */
