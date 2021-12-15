/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LINTP_CFG_H
#define LINTP_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
/* For DCM physical addressing */
#define LINTP_P2P_RX_PDU 0
#define LINTP_P2P_TX_PDU 0

#define LINTP_MAIN_FUNCTION_PERIOD 10
#define LINTP_CONVERT_MS_TO_MAIN_CYCLES(x)                                                         \
  ((x + LINTP_MAIN_FUNCTION_PERIOD - 1) / LINTP_MAIN_FUNCTION_PERIOD)

#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(x) LINTP_CONVERT_MS_TO_MAIN_CYCLES(x)

#define PduR_CanTpCopyTxData Dcm_CopyTxData
#define PduR_CanTpRxIndication Dcm_TpRxIndication
#define PduR_CanTpTxConfirmation Dcm_TpTxConfirmation
#define PduR_CanTpStartOfReception Dcm_StartOfReception
#define PduR_CanTpCopyRxData Dcm_CopyRxData
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* LINTP_CFG_H */
