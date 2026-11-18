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
} IsoTp_ProtocolType;

typedef enum {
  ISOTP_CAN_V1 = 1,
  ISOTP_CAN_V2
} IsoTp_CanVersionType;

typedef struct {
  uint32_t RxCanId;
  uint32_t TxCanId;
  IsoTp_CanVersionType Version;
  uint8_t BlockSize;
  uint8_t STmin; /* ms */
} IsoTp_CanParamType;

typedef struct {
  uint32_t Timeout; /* ms */
  uint32_t DelayUs; /* delay time between request and response for LIN */
  uint32_t RxId;
  uint32_t TxId;
} IsoTp_LinParamType;

typedef enum {
  ISOTP_J1939TP_PROTOCOL_BAM = 0,
  ISOTP_J1939TP_PROTOCOL_CMDT,
} IsoTp_J1939TpProtocolType;

typedef struct {
  IsoTp_J1939TpProtocolType Protocol;
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
} IsoTp_J1939TpParamType;

typedef struct {
  uint16_t SourceAddress;
  uint16_t TargetAddress;
  uint8_t ActivationType;
} IsoTp_DoIpParamType;

typedef struct {
  char Device[64];
  int Port;
  uint32_t Baudrate;
  IsoTp_ProtocolType Protocol;
  uint32_t LinkLayerDataLength;
  uint16_t N_TA; /* if 0xFFFF, means no target address used */
  union {
    IsoTp_CanParamType CAN;
    IsoTp_LinParamType LIN;
    IsoTp_J1939TpParamType J1939TP;
    IsoTp_DoIpParamType DoIP;
  } U;
} IsoTp_ParameterType;

typedef struct IsoTp_s IsoTp_Type;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
IsoTp_Type *IsoTp_Create(IsoTp_ParameterType *params);
int IsoTp_Transmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                   size_t rxSize);
int IsoTp_Receive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize);
int IsoTp_IoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size);
void IsoTp_Destroy(IsoTp_Type *isotp);
#ifdef __cplusplus
}
#endif
#endif /* ISOTP_H */
