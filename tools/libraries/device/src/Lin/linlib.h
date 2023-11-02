/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LIN_LIB_H
#define LIN_LIB_H
#include <stdint.h>
#include <pthread.h>
#include <sys/queue.h>
#include "devlib.h"
#include "devlib_priv.h"
/* ================================ [ MACROS    ] ============================================== */
#define LIN_TYPE_INVALID ((uint8_t)'I')
#define LIN_TYPE_BREAK ((uint8_t)'B')
#define LIN_TYPE_SYNC ((uint8_t)'S')
#define LIN_TYPE_HEADER ((uint8_t)'H')
#define LIN_TYPE_DATA ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA ((uint8_t)'F')

#define LIN_TYPE_EXT_HEADER ((uint8_t)'h')
#define LIN_TYPE_EXT_HEADER_AND_DATA ((uint8_t)'f')

#define LIN_MTU sizeof(Lin_FrameType)

#ifndef LIN_MAX_DATA_SIZE
#define LIN_MAX_DATA_SIZE 64
#endif

typedef uint32_t lin_id_t;
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t type;
  lin_id_t pid;
  uint8_t dlc;
  uint8_t data[LIN_MAX_DATA_SIZE];
  uint8_t checksum;
} Lin_FrameType;

struct Lin_Frame_s {
  uint8_t data[LIN_MAX_DATA_SIZE + 6];
  int size;
  STAILQ_ENTRY(Lin_Frame_s) entry;
};

typedef struct Lin_DeviceOps_s Lin_DeviceOpsType;

typedef struct Lin_Device_s {
  char name[DEVICE_NAME_SIZE];
  const Lin_DeviceOpsType *ops;
  pthread_t rx_thread;
  pthread_mutex_t q_lock;
  int killed;
  void *param;
  STAILQ_HEAD(, Lin_Frame_s) head;
  uint32_t size;
  STAILQ_ENTRY(Lin_Device_s) entry;
} Lin_DeviceType;

typedef int (*lin_device_open_t)(Lin_DeviceType *dev, const char *option);
typedef int (*lin_device_write_t)(Lin_DeviceType *dev, Lin_FrameType *frame);
typedef int (*lin_device_read_t)(Lin_DeviceType *dev, Lin_FrameType *frame);
typedef int (*lin_device_ioctl_t)(Lin_DeviceType *dev, int type, const void *data, size_t size);
typedef void (*lin_device_close_t)(Lin_DeviceType *dev);

struct Lin_DeviceOps_s {
  char name[DEVICE_NAME_SIZE];
  lin_device_open_t open;
  lin_device_close_t close;
  lin_device_write_t write;
  lin_device_read_t read;
  lin_device_ioctl_t ioctl;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* LIN_LIB_H */
