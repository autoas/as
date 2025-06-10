/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef J1939TP_CFG_H
#define J1939TP_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define J1939TP_MAIN_FUNCTION_PERIOD 10
#define J1939TP_CONVERT_MS_TO_MAIN_CYCLES(x)                                                       \
  ((x + J1939TP_MAIN_FUNCTION_PERIOD - 1) / J1939TP_MAIN_FUNCTION_PERIOD)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  const char *device;
  int port;
  uint32_t baudrate;
  uint8_t protocol;
  struct {
    uint32_t CM;
    uint32_t Direct;
    uint32_t DT;
    uint32_t FC;
  } TX;
  struct {
    uint32_t CM;
    uint32_t Direct;
    uint32_t DT;
    uint32_t FC;
  } RX;
  uint16_t STMin;
  uint16_t Tr;
  uint16_t T1;
  uint16_t T2;
  uint16_t T3;
  uint16_t T4;
  uint8_t TxMaxPacketsPerBlock;
  uint8_t ll_dl;
} J1939Tp_ParamType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void J1939Tp_ReConfig(uint8_t Channel, J1939Tp_ParamType *params);
#ifdef __cplusplus
}
#endif
#endif /* J1939TP_CFG_H */
