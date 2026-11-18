/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LINLIB_H
#define LINLIB_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef uint32_t LinL_IdType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int LinL_Open(const char *deviceName, uint32_t port, uint32_t baudrate);
bool LinL_Write(int busId, LinL_IdType id, uint8_t dlc, const uint8_t *data, bool enhanced);
bool LinL_Read(int busId, LinL_IdType id, uint8_t dlc, uint8_t *data, bool enhanced,
               int timeout /* ms */);
bool LinL_Close(int busId);
#ifdef __cplusplus
}
#endif
#endif /* LINLIB_H */
