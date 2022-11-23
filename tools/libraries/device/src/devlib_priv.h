/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef DEV_LIB_PRIV_H
#define DEV_LIB_PRIV_H

/* ================================ [ MACROS    ] ============================================== */
#define DEVICE_NAME_SIZE 128
/* ================================ [ TYPES     ] ============================================== */
typedef int (*dev_open_t)(const char *device, const char *option, void **param);
typedef int (*dev_read_t)(void *param, uint8_t *data, size_t size);
typedef int (*dev_write_t)(void *param, const uint8_t *data, size_t size);
typedef int (*dev_ioctl_t)(void *param, int type, const void *data, size_t size);
typedef void (*dev_close_t)(void *param);

typedef struct {
  char name[DEVICE_NAME_SIZE];
  dev_open_t open;
  dev_read_t read;
  dev_write_t write;
  dev_ioctl_t ioctl;
  dev_close_t close;
} Dev_DeviceOpsType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* DEV_LIB_PRIV_H */
