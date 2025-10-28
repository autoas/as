/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 */
#ifndef TLS_H
#define TLS_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "SoAd.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct TLS_Config_s TLS_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void TLS_Init(const TLS_ConfigType *config);

Std_ReturnType TLS_ServerOpen(uint16_t server);

void TLS_MainFunction(void);

void TLS_SoAdIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

void TLS_SoAdIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

void TLS_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode);

Std_ReturnType TLS_IfTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

BufReq_ReturnType TLS_SoAdStartOfReception(PduIdType RxPduId, const PduInfoType *info,
                                           PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
BufReq_ReturnType TLS_SoAdCopyRxData(PduIdType RxPduId, const PduInfoType *info,
                                     PduLengthType *bufferSizePtr);
void TLS_SoAdRxIndication(PduIdType RxPduId, Std_ReturnType result);
#ifdef __cplusplus
}
#endif
#endif /* TLS_H */
