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
/* ================================ [ MACROS    ] ============================================== */
#define LIN_BIT(v, pos) (((v) >> (pos)) & 0x01)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static uint8_t get_pid(uint8_t id) {
  uint8_t pid = id & 0x3F;
  uint8_t p0 = LIN_BIT(pid, 0) ^ LIN_BIT(pid, 1) ^ LIN_BIT(pid, 2) ^ LIN_BIT(pid, 4);
  uint8_t p1 = ~(LIN_BIT(pid, 1) ^ LIN_BIT(pid, 3) ^ LIN_BIT(pid, 4) ^ LIN_BIT(pid, 5));
  pid = pid | (p0 << 6) | (p1 << 7);

  return pid;
}

static uint8_t get_checksum(uint8_t pid, uint8_t dlc, const uint8_t *data, int enhanced) {
  int i;
  uint8_t checksum = 0;

  if (enhanced) {
    checksum += pid;
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

bool lin_write(int busid, uint8_t id, uint8_t dlc, const uint8_t *data, bool enhanced) {
  uint8_t sd[64 + 4];
  int ret;
  bool r = false;

  sd[0] = (uint8_t)'F';
  sd[1] = get_pid(id);
  memcpy(&sd[2], data, dlc);
  sd[2 + dlc] = get_checksum(sd[1], dlc, data, enhanced);

  dev_lock(busid);
  ret = dev_write(busid, sd, dlc + 3);
  dev_unlock(busid);
  if ((dlc + 3) == ret) {
    r = true;
  }

  return r;
}

bool lin_read(int busid, uint8_t id, uint8_t dlc, uint8_t *data, bool enhanced, int timeout) {
  uint8_t sd[64 + 4];
  int ret;
  bool r = false;
  uint8_t pid = get_pid(id);
  Std_TimerType timer;
  // uint8_t checksum;

  sd[0] = (uint8_t)'H';
  sd[1] = pid;
  sd[2] = dlc;

  dev_lock(busid);
  ret = dev_write(busid, sd, 3);
  if (3 == ret) {
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
    } while (Std_GetTimerElapsedTime(&timer) < ((uint32_t)timeout*1000));
  }
  dev_unlock(busid);

  if (r) {
#if 0
    checksum = get_checksum(pid, dlc, &sd[1], enhanced);
    if (checksum == sd[dlc + 1]) {
      memcpy(data, &sd[1], dlc);
    } else {
      r = false;
    }
#else
    memcpy(data, &sd[1], dlc);
#endif
  }

  return r;
}

bool lin_close(int busid) {
  int ret = dev_close(busid);
  return (0 == ret);
}
