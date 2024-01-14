/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LINTP_H
#define LINTP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define CanTp_Init LinTp_Init
#define CanTp_RxIndication LinTp_RxIndication
#define CanTp_TxConfirmation LinTp_TxConfirmation
#define CanTp_Transmit LinTp_Transmit
#define CanTp_MainFunction LinTp_MainFunction
#define CanTp_ConfigType LinTp_ConfigType
#define CanTp_Config_s LinTp_Config_s
#define CanTp_GetTxPacketLength LinTp_GetTxPacketLength
#define CanTp_GetRxLeftLength LinTp_GetRxLeftLength
#define CanTp_TriggerTransmit LinTp_TriggerTransmit
#define CanTp_MainFunction_Channel LinTp_MainFunction_Channel
#define CanTp_InitChannel LinTp_InitChannel
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
#include "CanTp.h"
/* ================================ [ FUNCTIONS ] ============================================== */
PduLengthType LinTp_GetTxPacketLength(PduIdType TxPduId);
PduLengthType LinTp_GetRxLeftLength(PduIdType RxPduId);
Std_ReturnType LinTp_TriggerTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
#endif /* LINTP_H */
