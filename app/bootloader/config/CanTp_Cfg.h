/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef CANTP_CFG_H
#define CANTP_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
/* For DCM physical addressing */
#define CANTP_P2P_RX_PDU 0
#define CANTP_P2P_TX_PDU 0

/* For DCM functional addressing */
#define CANTP_P2A_RX_PDU 1
#define CANTP_P2A_TX_PDU 1

#define CANTP_CANIF_P2P_TX_PDU 0
#define CANTP_CANIF_P2A_TX_PDU 1

#define CANTP_MAIN_FUNCTION_PERIOD 10
#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(x)                                                         \
  ((x + CANTP_MAIN_FUNCTION_PERIOD - 1) / CANTP_MAIN_FUNCTION_PERIOD)

#ifndef PDUR_DCM_CANTP_ZERO_COST
#define PDUR_DCM_CANTP_ZERO_COST
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CANTP_CFG_H */