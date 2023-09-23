/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 */
#ifndef CANTP_CFG_H
#define CANTP_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define CANTP_MAIN_FUNCTION_PERIOD 10
#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(x)                                                         \
  ((x + CANTP_MAIN_FUNCTION_PERIOD - 1) / CANTP_MAIN_FUNCTION_PERIOD)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  const char *device;
  int port;
  uint32_t baudrate;
  uint32_t RxCanId;
  uint32_t TxCanId;
  uint8_t ll_dl;
} CanTp_ParamType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void CanTp_ReConfig(uint8_t Channel, CanTp_ParamType* params);
uint32_t CanIf_CanTpGetTxCanId(uint8_t Channel);
uint32_t CanIf_CanTpGetRxCanId(uint8_t Channel);
#endif /* CANTP_CFG_H */