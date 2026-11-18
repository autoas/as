/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "linlib.h"
#include "devlib.h"
#include <stdio.h>
#include <string.h>
#include "Std_Timer.h"
#include "Std_Topic.h"

#include <thread>
#include <chrono>

using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
#define LinLBit(v, pos) (((v) >> (pos)) & 0x01)

#define LIN_TYPE_INVALID ((uint8_t)'I')
#define LIN_TYPE_BREAK ((uint8_t)'B')
#define LIN_TYPE_SYNC ((uint8_t)'S')
#define LIN_TYPE_HEADER ((uint8_t)'H')
#define LIN_TYPE_DATA ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA ((uint8_t)'F')
#define LIN_TYPE_EXT_HEADER ((uint8_t)'h')
#define LIN_TYPE_EXT_HEADER_AND_DATA ((uint8_t)'f')

#ifndef LINL_MAX_DATA_SIZE
#define LINL_MAX_DATA_SIZE 64
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static LinL_IdType LinL_GetPid(LinL_IdType id) {
  uint8_t pid;
  uint8_t p0;
  uint8_t p1;

  if (id > 0x3F) {
    /* extended id or already is pid */
    pid = id;
  } else {
    /* calculate the pid for standard LIN id case */
    pid = id & 0x3F;
    p0 = LinLBit(pid, 0) ^ LinLBit(pid, 1) ^ LinLBit(pid, 2) ^ LinLBit(pid, 4);
    p1 = ~(LinLBit(pid, 1) ^ LinLBit(pid, 3) ^ LinLBit(pid, 4) ^ LinLBit(pid, 5));
    pid = pid | (p0 << 6) | (p1 << 7);
  }

  /* for the LIN_USE_EXT_ID. keep the high 3 bytes unchanged */
  return (LinL_IdType)pid | (id & 0xFFFFFF00);
}

static LinL_IdType LinL_GetChecksum(LinL_IdType pid, uint8_t dlc, const uint8_t *data,
                                    int enhanced) {
  int i;
  uint8_t checksum = 0;

  if (enhanced) {
    checksum += (pid >> 24) & 0xFF;
    checksum += (pid >> 16) & 0xFF;
    checksum += (pid >> 8) & 0xFF;
    checksum += pid & 0xFF;
  }

  for (i = 0; i < dlc; i++) {
    checksum += data[i];
  }

  return ~checksum;
}
/* ================================ [ FUNCTIONS ] ============================================== */
int LinL_Open(const char *deviceName, uint32_t port, uint32_t baudrate) {
  int fd;
  char device[128];
  char option[64];
  snprintf(device, sizeof(device), "lin/%s/%u", deviceName, port);
  snprintf(option, sizeof(option), "%u", baudrate);

  fd = Dev_Open(device, option);

  return fd;
}

bool LinL_Write(int busId, LinL_IdType id, uint8_t dlc, const uint8_t *data, bool enhanced) {
  uint8_t sd[LINL_MAX_DATA_SIZE + 8];
  int ret;
  bool r = false;
  size_t len;
  LinL_IdType pid = LinL_GetPid(id);
  STD_TOPIC_LIN(busId, false, id, dlc, data);
  if (id > 0x3F) {
    sd[0] = (uint8_t)LIN_TYPE_EXT_HEADER_AND_DATA;
    sd[1] = (pid >> 24) & 0xFF;
    sd[2] = (pid >> 16) & 0xFF;
    sd[3] = (pid >> 8) & 0xFF;
    sd[4] = pid & 0xFF;
    memcpy(&sd[5], data, dlc);
    sd[5 + dlc] = LinL_GetChecksum(pid, dlc, data, enhanced);
    len = dlc + 6;
  } else {
    sd[0] = (uint8_t)LIN_TYPE_HEADER_AND_DATA;
    sd[1] = pid & 0xFF;
    memcpy(&sd[2], data, dlc);
    sd[2 + dlc] = LinL_GetChecksum(pid, dlc, data, enhanced);
    len = dlc + 3;
  }

  Dev_Lock(busId);
  ret = Dev_Write(busId, sd, len);
  Dev_Unlock(busId);
  if (len == (size_t)ret) {
    r = true;
  }

  return r;
}

bool LinL_Read(int busId, LinL_IdType id, uint8_t dlc, uint8_t *data, bool enhanced, int timeout) {
  uint8_t sd[64 + 4];
  int ret;
  bool r = false;
  LinL_IdType pid = LinL_GetPid(id);
  Std_TimerType timer;
  size_t len;
  uint8_t checksum;

  if (id > 0x3F) {
    sd[0] = (uint8_t)LIN_TYPE_EXT_HEADER;
    sd[1] = (pid >> 24) & 0xFF;
    sd[2] = (pid >> 16) & 0xFF;
    sd[3] = (pid >> 8) & 0xFF;
    sd[4] = pid & 0xFF;
    sd[5] = dlc;
    len = 6;
  } else {
    sd[0] = (uint8_t)LIN_TYPE_HEADER;
    sd[1] = pid & 0xFF;
    sd[2] = dlc;
    len = 3;
  }

  Dev_Lock(busId);
  ret = Dev_Write(busId, sd, len);
  if (len == (size_t)ret) {
    r = true;
  }

  if (r) {
    r = false;
    Std_TimerStart(&timer);
    do {
      std::this_thread::sleep_for(1ms);
      ret = Dev_Read(busId, sd, sizeof(sd));
      if (ret > 0) {
        if ((ret == (dlc + 2)) && (sd[0] == (uint8_t)'D')) {
          r = true;
        }
        break;
      }
    } while (Std_GetTimerElapsedTime(&timer) < ((uint32_t)timeout * 1000));
  }
  Dev_Unlock(busId);

  if (r) {
    /* the under layer must provide frame with checksum according to this linlib */
    checksum = LinL_GetChecksum(pid, dlc, &sd[1], enhanced);
    if (checksum == sd[dlc + 1]) {
      memcpy(data, &sd[1], dlc);
      STD_TOPIC_LIN(busId, true, id, dlc, data);
    } else {
      r = false;
    }
  }

  return r;
}

bool LinL_Close(int busId) {
  int ret = Dev_Close(busId);
  return (0 == ret);
}
