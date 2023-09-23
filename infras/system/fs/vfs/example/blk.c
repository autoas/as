/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#include "device.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_BLKDEV 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static int dev_asblk_open(const device_t *device) {
  char name[64];
  FILE *fp;
  uint8_t *data;
  uint32_t blkid = (uint32_t)(uint64_t)device->priv;

  snprintf(name, sizeof(name), "asblk%d.img", blkid);
  fp = fopen(name, "rb");
  if (NULL == fp) {
    fp = fopen(name, "wb+");
    asAssert(fp);
    data = malloc(1024 * 1024);
    asAssert(data);
    memset(data, 0xFF, 1024 * 1024);
    for (int i = 0; i < 32; i++) {
      fwrite(data, 1, 1024 * 1024, fp);
    }
    free(data);
    fclose(fp);
    ASLOG(BLKDEV, ("simulation on new created 32Mb %s\n", name));
  } else {
    ASLOG(BLKDEV, ("simulation on old %s\n", name));
    fclose(fp);
  }

  return 0;
}

static int dev_asblk_close(const device_t *device) {
  return 0;
}

static int dev_asblk_read(const device_t *device, size_t pos, void *buffer, size_t size) {
  int res = 0;
  int len;
  char name[64];
  FILE *fp;
  uint32_t blkid = (uint32_t)(uint64_t)device->priv;

  snprintf(name, sizeof(name), "asblk%d.img", blkid);
  ASLOG(BLKDEV, ("read %s pos=%u size=%u\n", name, (uint32_t)pos, (uint32_t)size));
  if (0 != access(name, F_OK)) {
    dev_asblk_open(device);
  }
  fp = fopen(name, "rb");
  asAssert(fp);
  fseek(fp, 512 * pos, SEEK_SET);
  len = fread(buffer, sizeof(char), size * 512, fp);
  if (len != size * 512) {
    res = -1;
  }
  fclose(fp);
  return res;
}

static int dev_asblk_write(const device_t *device, size_t pos, const void *buffer, size_t size) {
  int res = 0;
  int len;
  char name[64];
  FILE *fp;
  uint32_t blkid = (uint32_t)(uint64_t)device->priv;

  snprintf(name, sizeof(name), "asblk%d.img", blkid);
  ASLOG(BLKDEV, ("write %s pos=%u size=%u\n", name, (uint32_t)pos, (uint32_t)size));
  if (0 != access(name, F_OK)) {
    dev_asblk_open(device);
  }
  fp = fopen(name, "rb+");
  asAssert(fp);
  fseek(fp, 512 * pos, SEEK_SET);
  len = fwrite(buffer, sizeof(char), size * 512, fp);
  if (len != size * 512) {
    res = -1;
  }
  fclose(fp);
  return res;
}

static int dev_asblk_ctrl(const device_t *device, int cmd, void *args) {
  char name[64];
  FILE *fp;
  uint32_t blkid = (uint32_t)(uint64_t)device->priv;
  size_t size;
  int ercd = 0;

  snprintf(name, sizeof(name), "asblk%d.img", blkid);
  if (0 != access(name, F_OK)) {
    dev_asblk_open(device);
  }
  switch (cmd) {
  case DEVICE_CTRL_GET_SECTOR_SIZE:
    *(size_t *)args = 512;
    break;
  case DEVICE_CTRL_GET_BLOCK_SIZE:
    *(size_t *)args = 4096;
    break;
  case DEVICE_CTRL_GET_SECTOR_COUNT:
    fp = fopen(name, "rb");
    asAssert(fp);
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    *(size_t *)args = size / 512;
    break;
  case DEVICE_CTRL_GET_DISK_SIZE:
    fp = fopen(name, "rb");
    asAssert(fp);
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    *(size_t *)args = size;
    break;
  default:
    ercd = EINVAL;
    break;
  }

  return ercd;
}

DEVICE_REGISTER(sd0, BLOCK, asblk, (void *)0);
DEVICE_REGISTER(sd1, BLOCK, asblk, (void *)1);
DEVICE_REGISTER(sd2, BLOCK, asblk, (void *)2);
DEVICE_REGISTER(sd3, BLOCK, asblk, (void *)3);
/* ================================ [ FUNCTIONS ] ============================================== */
