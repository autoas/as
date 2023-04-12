/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2019 Parai Wang <parai@foxmail.com>
 */
#ifndef _DEVICE_H_
#define _DEVICE_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>
#include <errno.h>
/* ================================ [ MACROS    ] ============================================== */
/* controls for block devices */
#define DEVICE_CTRL_GET_SECTOR_SIZE 0
#define DEVICE_CTRL_GET_BLOCK_SIZE 1
#define DEVICE_CTRL_GET_SECTOR_COUNT 2
#define DEVICE_CTRL_GET_DISK_SIZE 3
#define DEVICE_CTRL_GET_MAC_ADDR 4

#if defined(_WIN32) || defined(linux)
#define DEVICE_REGISTER(name, type, api, priv)                                                     \
  const device_t dev_##name = {                                                                    \
    #name,                                                                                         \
    DEVICE_TYPE_##type,                                                                            \
    {dev_##api##_open, dev_##api##_close, dev_##api##_read, dev_##api##_write, dev_##api##_ctrl},  \
    priv};                                                                                         \
  static void __attribute__((constructor)) _dev_##name##_ctor(void) {                              \
    device_register(&dev_##name);                                                                  \
  }
#else
#define DEVICE_REGISTER(name, type, api, priv)                                                     \
  const device_t __attribute__((section("DeviceTab"))) dev_##name = {                              \
    #name,                                                                                         \
    DEVICE_TYPE_##type,                                                                            \
    {dev_##api##_open, dev_##api##_close, dev_##api##_read, dev_##api##_write, dev_##api##_ctrl},  \
    priv};
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct device device_t;

typedef struct {
  int (*open)(const device_t *device);
  int (*close)(const device_t *device);
  int (*read)(const device_t *device, size_t pos, void *buffer, size_t size);
  int (*write)(const device_t *device, size_t pos, const void *buffer, size_t size);
  int (*ctrl)(const device_t *device, int cmd, void *args);
} device_ops_t;

typedef enum
{
  DEVICE_TYPE_BLOCK,
  DEVICE_TYPE_NET,
  DEVICE_TYPE_CHAR
} device_type_t;

struct device {
  const char *name;
  device_type_t type;
  device_ops_t ops;
  void *priv;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(_WIN32) || defined(linux)
void device_register(const device_t *device);
#endif
const device_t *device_find(const char *name);
#endif /* _DEVICE_H_ */
