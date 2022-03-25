/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic over IP AUTOSAR CP Release 4.4.0
 */
#ifndef _DOIP_H
#define _DOIP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "SoAd.h"
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_DoIP_00055 */
#define DOIP_E_PENDING ((Std_ReturnType)16)
/* ================================ [ TYPES     ] ============================================== */
typedef struct DoIP_Config_s DoIP_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_DoIP_00026 */
void DoIP_Init(const DoIP_ConfigType *ConfigPtr);

/* @SWS_DoIP_00251 */
void DoIP_ActivationLineSwitchActive(void);

/* @SWS_DoIP_91001 */
void DoIP_ActivationLineSwitchInactive(void);

/* @SWS_DoIP_00022 */
Std_ReturnType DoIP_TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_DoIP_00023 */
Std_ReturnType DoIP_TpCancelTransmit(PduIdType TxPduId);

/* @SWS_DoIP_00024 */
Std_ReturnType DoIP_TpCancelReceive(PduIdType RxPduId);

/* @SWS_DoIP_00277 */
Std_ReturnType DoIP_IfTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_DoIP_00278 */
Std_ReturnType DoIP_IfCancelTransmit(PduIdType TxPduId);

/* @SWS_DoIP_00041 */
void DoIP_MainFunction(void);

/* @SWS_DoIP_00244 */
void DoIP_SoAdIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_DoIP_00245 */
void DoIP_SoAdIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_DoIP_00037 */
BufReq_ReturnType DoIP_SoAdTpStartOfReception(PduIdType RxPduId, const PduInfoType *info,
                                              PduLengthType TpSduLength,
                                              PduLengthType *bufferSizePtr);

/* @SWS_DoIP_00038 */
void DoIP_SoAdTpRxIndication(PduIdType RxPduId, Std_ReturnType result);

/* @SWS_DoIP_00033 */
BufReq_ReturnType DoIP_SoAdTpCopyRxData(PduIdType RxPduId, const PduInfoType *info,
                                        PduLengthType *bufferSizePtr);

/* @SWS_DoIP_00039 */
void DoIP_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode);
#endif /* _DOIP_H */
