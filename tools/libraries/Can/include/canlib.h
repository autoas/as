/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef CANLIB_H
#define CANLIB_H
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
int can_open(const char *device_name, uint32_t port, uint32_t baudrate);
bool can_write(int busid, uint32_t canid, uint8_t dlc, const uint8_t *data);
bool can_read(int busid, uint32_t *canid /* InOut */, uint8_t *dlc /* InOut */, uint8_t *data);
bool can_close(int busid);
bool can_reset(int busid);
#ifdef __cplusplus
}
#endif
#endif /* CANLIB_H */
