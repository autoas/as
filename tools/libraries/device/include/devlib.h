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
#define DEV_I2C_WR 0x0000
#define DEV_I2C_RD (1u << 0)
#define DEV_I2C_ADDR_10BIT (1u << 2) /* this is a ten bit chip address */
#define DEV_I2C_NO_START (1u << 4)
#define DEV_I2C_IGNORE_NACK (1u << 5)
#define DEV_I2C_NO_READ_ACK (1u << 6) /* when I2C reading, we do not ACK */
#define DEV_I2C_NO_STOP (1u << 7)
#define DEV_I2C_NO_WRITE_ACK (1u << 8)

#define DEV_IOCTL_SET_TIMEOUT 0
#define DEV_IOCTL_TRANSFER 1  /* transfer request for I2C or SPI */
#define DEV_IOCTL_SET_DELAY 2 /* set delay for each operation */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t *data;
  uint16_t len;
  uint16_t addr;
  uint16_t flags;
} dev_i2c_msg_t;

typedef struct {
  uint8_t *src;
  uint8_t *dst;
  uint16_t len;
} dev_spi_msg_t;
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
