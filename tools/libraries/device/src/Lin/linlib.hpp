/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LINLIB_HPP
#define LINLIB_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <mutex>
#include <thread>
#include <string>
#include <sys/queue.h>
#include "devlib.h"
#include "devlib_priv.hpp"
/* ================================ [ MACROS    ] ============================================== */
#define DEVLIN_TYPE_INVALID ((uint8_t)'I')
#define DEVLIN_TYPE_BREAK ((uint8_t)'B')
#define DEVLIN_TYPE_SYNC ((uint8_t)'S')
#define DEVLIN_TYPE_HEADER ((uint8_t)'H')
#define DEVLIN_TYPE_DATA ((uint8_t)'D')
#define DEVLIN_TYPE_HEADER_AND_DATA ((uint8_t)'F')

#define DEVLIN_TYPE_EXT_HEADER ((uint8_t)'h')
#define DEVLIN_TYPE_EXT_HEADER_AND_DATA ((uint8_t)'f')

#define DEVLIN_MTU sizeof(Dev_LinFrameType)

#ifndef DEVLIN_MAX_DATA_SIZE
#define DEVLIN_MAX_DATA_SIZE 64
#endif

typedef uint32_t LinL_IdType;
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t Type;
  LinL_IdType Pid;
  uint8_t Dlc;
  uint8_t Data[DEVLIN_MAX_DATA_SIZE];
  uint8_t Checksum;
} Dev_LinFrameType;

typedef struct Dev_LinFrameBuf_s Dev_LinFrameBufType;

struct Dev_LinFrameBuf_s {
  uint8_t Data[DEVLIN_MAX_DATA_SIZE + 6];
  int Size;
  STAILQ_ENTRY(Dev_LinFrameBuf_s) Entry;
};

typedef struct Dev_LinDeviceOps_s Dev_LinDeviceOpsType;

typedef struct Dev_LinDevice_s {
  std::string Name;
  const Dev_LinDeviceOpsType *Ops;
  std::thread RxThread;
  std::mutex QLock;
  int IsKilled;
  void *Param;
  STAILQ_HEAD(, Dev_LinFrameBuf_s) Head;
  uint32_t Size;
  STAILQ_ENTRY(Dev_LinDevice_s) Entry;
} Dev_LinDeviceType;

typedef int (*Dev_LinDeviceOpenFuncType)(Dev_LinDeviceType *dev, const char *option);
typedef int (*Dev_LinDeviceWriteFuncType)(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
typedef int (*Dev_LinDeviceReadFuncType)(Dev_LinDeviceType *dev, Dev_LinFrameType *frame);
typedef int (*Dev_LinDeviceIoCtlFuncType)(Dev_LinDeviceType *dev, int type, const void *data, size_t size);
typedef void (*Dev_LinDeviceCloseFuncType)(Dev_LinDeviceType *dev);

struct Dev_LinDeviceOps_s {
  std::string Name;
  Dev_LinDeviceOpenFuncType Open;
  Dev_LinDeviceCloseFuncType Close;
  Dev_LinDeviceWriteFuncType Write;
  Dev_LinDeviceReadFuncType Read;
  Dev_LinDeviceIoCtlFuncType IoCtl;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* LINLIB_HPP */
