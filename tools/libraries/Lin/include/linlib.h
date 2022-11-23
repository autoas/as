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
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int lin_open(const char *device_name, uint32_t port, uint32_t baudrate);
bool lin_write(int busid, uint8_t id, uint8_t dlc, const uint8_t *data, bool enhanced);
bool lin_read(int busid, uint8_t id, uint8_t dlc, uint8_t *data, bool enhanced,
              int timeout /* ms */);
bool lin_close(int busid);
#ifdef __cplusplus
}
#endif
#endif /* LINLIB_H */
