/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic over IP AUTOSAR CP Release 4.4.0
 */
#ifndef DOIP_H
#define DOIP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "SoAd.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_DoIP_00055 */
#define DOIP_E_PENDING ((Std_ReturnType)16)

#define DOIP_POWER_NOT_READY 0x00u
#define DOIP_POWER_READY 0x01u
#define DOIP_POWER_NOT_SUPPORTED 0x02u
/* ================================ [ TYPES     ] ============================================== */
typedef struct DoIP_Config_s DoIP_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_DoIP_00026 */
void DoIP_Init(const DoIP_ConfigType *configPtr);

/* @SWS_DoIP_00251 @deprecated */
void DoIP_ActivationLineSwitchActive(void);

/* @SWS_DoIP_91001 @deprecated */
void DoIP_ActivationLineSwitchInactive(void);

/* @SWS_DoIP_91000 */
void DoIP_ActivationLineSwitch(uint8_t interfaceId, boolean *active);

/* @SWS_DoIP_91002 */
void DoIP_TriggerVehicleAnnouncement(uint8_t interfaceId);

/* @SWS_DoIP_00022 */
Std_ReturnType DoIP_TpTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr);

/* @SWS_DoIP_00023 */
Std_ReturnType DoIP_TpCancelTransmit(PduIdType txPduId);

/* @SWS_DoIP_00024 */
Std_ReturnType DoIP_TpCancelReceive(PduIdType rxPduId);

/* @SWS_DoIP_00277 */
Std_ReturnType DoIP_IfTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr);

/* @SWS_DoIP_00278 */
Std_ReturnType DoIP_IfCancelTransmit(PduIdType txPduId);

/* @SWS_DoIP_00041 */
void DoIP_MainFunction(void);

/* @SWS_DoIP_00244 */
void DoIP_SoAdIfRxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr);

/* @SWS_DoIP_00245 */
void DoIP_SoAdIfTxConfirmation(PduIdType txPduId, Std_ReturnType result);

/* @SWS_DoIP_00037 */
BufReq_ReturnType DoIP_SoAdTpStartOfReception(PduIdType rxPduId, const PduInfoType *info,
                                              PduLengthType tpSduLength,
                                              PduLengthType *bufferSizePtr);

/* @SWS_DoIP_00038 */
void DoIP_SoAdTpRxIndication(PduIdType rxPduId, Std_ReturnType result);

/* @SWS_DoIP_00033 */
BufReq_ReturnType DoIP_SoAdTpCopyRxData(PduIdType rxPduId, const PduInfoType *info,
                                        PduLengthType *bufferSizePtr);

Std_ReturnType DoIP_HeaderIndication(PduIdType rxPduId, const PduInfoType *info,
                                     uint32_t *payloadLength);
void DoIP_RxIndication(PduIdType rxPduId, const PduInfoType *info);

/* @SWS_DoIP_00039 */
void DoIP_SoConModeChg(SoAd_SoConIdType soConId, SoAd_SoConModeType mode);

/* @SWS_DoIP_00027 */
void DoIP_GetVersionInfo(Std_VersionInfoType *versionInfo);
#ifdef __cplusplus
}
#endif
#endif /* DOIP_H */
