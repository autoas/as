/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef DEVLIB_H
#define DEVLIB_H
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
  uint8_t *Data;
  uint16_t Len;
  uint16_t Addr;
  uint16_t Flags;
} Dev_I2cMsgType;

typedef struct {
  uint8_t *Src;
  uint8_t *Dst;
  uint16_t Len;
} Dev_SpiMsgType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int Dev_Open(const char *device, const char *option);
int Dev_Read(int fd, uint8_t *data, size_t size);
int Dev_Write(int fd, const uint8_t *data, size_t size);
int Dev_IoCtl(int fd, int type, const void *data, size_t size);
int Dev_Close(int fd);
int Dev_Lock(int fd);
int Dev_Unlock(int fd);
#ifdef __cplusplus
}
#endif
#endif /* DEVLIB_H */
