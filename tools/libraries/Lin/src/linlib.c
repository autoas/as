/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "linlib.h"
#include "devlib.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "Std_Timer.h"
#include "Std_Topic.h"
/* ================================ [ MACROS    ] ============================================== */
#define LIN_BIT(v, pos) (((v) >> (pos)) & 0x01)

#define LIN_TYPE_INVALID ((uint8_t)'I')
#define LIN_TYPE_BREAK ((uint8_t)'B')
#define LIN_TYPE_SYNC ((uint8_t)'S')
#define LIN_TYPE_HEADER ((uint8_t)'H')
#define LIN_TYPE_DATA ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA ((uint8_t)'F')
#define LIN_TYPE_EXT_HEADER ((uint8_t)'h')
#define LIN_TYPE_EXT_HEADER_AND_DATA ((uint8_t)'f')

#ifndef LIN_MAX_DATA_SIZE
#define LIN_MAX_DATA_SIZE 64
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static lin_id_t get_pid(lin_id_t id) {
  uint8_t pid;
  uint8_t p0;
  uint8_t p1;

  if (id > 0x3F) {
    /* extended id or already is pid */
    pid = id;
  } else {
    /* calculate the pid for standard LIN id case */
    pid = id & 0x3F;
    p0 = LIN_BIT(pid, 0) ^ LIN_BIT(pid, 1) ^ LIN_BIT(pid, 2) ^ LIN_BIT(pid, 4);
    p1 = ~(LIN_BIT(pid, 1) ^ LIN_BIT(pid, 3) ^ LIN_BIT(pid, 4) ^ LIN_BIT(pid, 5));
    pid = pid | (p0 << 6) | (p1 << 7);
  }

  /* for the LIN_USE_EXT_ID. keep the high 3 bytes unchanged */
  return (lin_id_t)pid | (id & 0xFFFFFF00);
}

static lin_id_t get_checksum(lin_id_t pid, uint8_t dlc, const uint8_t *data, int enhanced) {
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
int lin_open(const char *device_name, uint32_t port, uint32_t baudrate) {
  int fd;
  char device[128];
  char option[64];
  snprintf(device, sizeof(device), "lin/%s/%u", device_name, port);
  snprintf(option, sizeof(option), "%u", baudrate);

  fd = dev_open(device, option);

  return fd;
}

bool lin_write(int busid, lin_id_t id, uint8_t dlc, const uint8_t *data, bool enhanced) {
  uint8_t sd[LIN_MAX_DATA_SIZE + 8];
  int ret;
  bool r = false;
  size_t len;
  lin_id_t pid = get_pid(id);
  STD_TOPIC_LIN(busid, false, id, dlc, data);
  if (id > 0x3F) {
    sd[0] = (uint8_t)LIN_TYPE_EXT_HEADER_AND_DATA;
    sd[1] = (pid >> 24) & 0xFF;
    sd[2] = (pid >> 16) & 0xFF;
    sd[3] = (pid >> 8) & 0xFF;
    sd[4] = pid & 0xFF;
    memcpy(&sd[5], data, dlc);
    sd[5 + dlc] = get_checksum(pid, dlc, data, enhanced);
    len = dlc + 6;
  } else {
    sd[0] = (uint8_t)LIN_TYPE_HEADER_AND_DATA;
    sd[1] = pid & 0xFF;
    memcpy(&sd[2], data, dlc);
    sd[2 + dlc] = get_checksum(pid, dlc, data, enhanced);
    len = dlc + 3;
  }

  dev_lock(busid);
  ret = dev_write(busid, sd, len);
  dev_unlock(busid);
  if (len == ret) {
    r = true;
  }

  return r;
}

bool lin_read(int busid, lin_id_t id, uint8_t dlc, uint8_t *data, bool enhanced, int timeout) {
  uint8_t sd[64 + 4];
  int ret;
  bool r = false;
  lin_id_t pid = get_pid(id);
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

  dev_lock(busid);
  ret = dev_write(busid, sd, len);
  if (len == ret) {
    r = true;
  }

  if (r) {
    r = false;
    Std_TimerStart(&timer);
    do {
      usleep(1000);
      ret = dev_read(busid, sd, sizeof(sd));
      if (ret > 0) {
        if ((ret == (dlc + 2)) && (sd[0] == (uint8_t)'D')) {
          r = true;
        }
        break;
      }
    } while (Std_GetTimerElapsedTime(&timer) < ((uint32_t)timeout * 1000));
  }
  dev_unlock(busid);

  if (r) {
    /* the under layer must provide frame with checksum according to this linlib */
    checksum = get_checksum(pid, dlc, &sd[1], enhanced);
    if (checksum == sd[dlc + 1]) {
      memcpy(data, &sd[1], dlc);
      STD_TOPIC_LIN(busid, true, id, dlc, data);
    } else {
      r = false;
    }
  }

  return r;
}

bool lin_close(int busid) {
  int ret = dev_close(busid);
  return (0 == ret);
}
