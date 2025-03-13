/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2022 Parai Wang <parai@foxmail.com>
 */
#ifndef CANTP_CFG_H
#define CANTP_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define CANTP_MAIN_FUNCTION_PERIOD 10
#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(x)                                                         \
  ((x + CANTP_MAIN_FUNCTION_PERIOD - 1) / CANTP_MAIN_FUNCTION_PERIOD)

// #define CANTP_USE_STD_TIMER
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  const char *device;
  int port;
  uint32_t baudrate;
  uint32_t RxCanId;
  uint32_t TxCanId;
  uint16_t N_TA; /* if 0xFFFF, means no target address used */
  uint8_t ll_dl;
  uint8_t STmin; /* ms */
} CanTp_ParamType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void CanTp_ReConfig(uint8_t Channel, CanTp_ParamType *params);
uint32_t CanIf_CanTpGetTxCanId(uint8_t Channel);
uint32_t CanIf_CanTpGetRxCanId(uint8_t Channel);
#ifdef __cplusplus
}
#endif
#endif /* CANTP_CFG_H */
