/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef DEVLIB_PRIV_H
#define DEVLIB_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <string>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef int (*Dev_OpenFuncType)(const char *device, const char *option, void **param);
typedef int (*Dev_ReadFuncType)(void *param, uint8_t *data, size_t size);
typedef int (*Dev_WriteFuncType)(void *param, const uint8_t *data, size_t size);
typedef int (*Dev_IoCtlFuncType)(void *param, int type, const void *data, size_t size);
typedef void (*Dev_CloseFuncType)(void *param);

typedef struct {
  std::string Name;
  Dev_OpenFuncType Open;
  Dev_ReadFuncType Read;
  Dev_WriteFuncType Write;
  Dev_IoCtlFuncType IoCtl;
  Dev_CloseFuncType Close;
} Dev_DeviceOpsType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* DEVLIB_PRIV_H */
