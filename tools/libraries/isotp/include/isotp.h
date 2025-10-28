/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */
#ifndef ISOTP_H
#define ISOTP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define ISOTP_IOCTL_SET_TX_ID 0
#define ISOTP_IOCTL_J1939TP_GET_PGN 1
#define ISOTP_IOCTL_SET_TIMEOUT 2
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  ISOTP_OVER_CAN,
  ISOTP_OVER_LIN,
  ISOTP_OVER_J1939TP,
  ISOTP_OVER_DOIP
} isotp_protocol_t;

typedef struct {
  uint32_t RxCanId;
  uint32_t TxCanId;
  uint8_t BlockSize;
  uint8_t STmin; /* ms */
} isotp_can_param_t;

typedef struct {
  uint32_t timeout; /* ms */
  uint32_t delayUs; /* delay time between request and response for LIN */
  uint32_t RxId;
  uint32_t TxId;
} isotp_lin_param_t;

typedef enum {
  ISOTP_J1939TP_PROTOCOL_BAM = 0,
  ISOTP_J1939TP_PROTOCOL_CMDT,
} isotp_j1939tp_protocal_t;

typedef struct {
  isotp_j1939tp_protocal_t protocol;
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
  uint32_t PgPGN;
  uint16_t STMin;
  uint16_t Tr;
  uint16_t T1;
  uint16_t T2;
  uint16_t T3;
  uint16_t T4;
  uint8_t TxMaxPacketsPerBlock;
} isotp_j1939tp_param_t;

typedef struct {
  uint16_t sourceAddress;
  uint16_t targetAddress;
  uint8_t activationType;
} isotp_doip_param_t;

typedef struct {
  char device[64];
  int port;
  uint32_t baudrate;
  isotp_protocol_t protocol;
  uint32_t ll_dl;
  uint16_t N_TA; /* if 0xFFFF, means no target address used */
  union {
    isotp_can_param_t CAN;
    isotp_lin_param_t LIN;
    isotp_j1939tp_param_t J1939TP;
    isotp_doip_param_t DoIP;
  } U;
} isotp_parameter_t;

typedef struct isotp_s isotp_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
isotp_t *isotp_create(isotp_parameter_t *params);
int isotp_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                   size_t rxSize);
int isotp_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize);
int isotp_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size);
void isotp_destory(isotp_t *isotp);
#ifdef __cplusplus
}
#endif
#endif /* ISOTP_H */
