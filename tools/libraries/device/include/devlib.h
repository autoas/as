/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef DEV_LIB_H
#define DEV_LIB_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int dev_open(const char *device, const char *option);
int dev_read(int fd, uint8_t *data, size_t size);
int dev_write(int fd, const uint8_t *data, size_t size);
int dev_ioctl(int fd, int type, const void *data, size_t size);
int dev_close(int fd);
int dev_lock(int fd);
int dev_unlock(int fd);
#ifdef __cplusplus
}
#endif
#endif /* DEV_LIB_H */
