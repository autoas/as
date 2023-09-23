/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of UDP Network Management AUTOSAR CP Release 4.4.0
 */
#ifndef _UDPNM_H
#define _UDPNM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "NmStack_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct UdpNm_Config_s UdpNm_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_UdpNm_00208 */
void UdpNm_Init(const UdpNm_ConfigType *ConfigPtr);

/* @SWS_UdpNm_00211 */
Std_ReturnType UdpNm_PassiveStartUp(NetworkHandleType nmChannelHandle);

/* @SWS_UdpNm_00213 */
Std_ReturnType UdpNm_NetworkRequest(NetworkHandleType nmChannelHandle);

/* @SWS_UdpNm_00214 */
Std_ReturnType UdpNm_NetworkRelease(NetworkHandleType nmChannelHandle);

/* @SWS_UdpNm_00215 */
Std_ReturnType UdpNm_DisableCommunication(NetworkHandleType nmChannelHandle);

/* @SWS_UdpNm_00216 */
Std_ReturnType UdpNm_EnableCommunication(NetworkHandleType nmChannelHandle);

/* @SWS_UdpNm_00217 */
Std_ReturnType UdpNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8 *nmUserDataPtr);

/* @SWS_UdpNm_00218 */
Std_ReturnType UdpNm_GetUserData(NetworkHandleType nmChannelHandle, uint8 *nmUserDataPtr);

/* @SWS_UdpNm_00219 */
Std_ReturnType UdpNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8 *nmNodeIdPtr);

/* @SWS_UdpNm_00220 */
Std_ReturnType UdpNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle, uint8 *nmNodeIdPtr);

/* @SWS_UdpNm_00221 */
Std_ReturnType UdpNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle);

/* @SWS_UdpNm_00309 */
Std_ReturnType UdpNm_GetPduData(NetworkHandleType nmChannelHandle, uint8 *nmPduDataPtr);

/* @SWS_UdpNm_00310 */
Std_ReturnType UdpNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType *nmStatePtr,
                              Nm_ModeType *nmModePtr);

/* @SWS_UdpNm_00226 */
Std_ReturnType UdpNm_RequestBusSynchronization(NetworkHandleType nmChannelHandle);

/* @SWS_UdpNm_00227 */
Std_ReturnType UdpNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle,
                                                boolean *NmRemoteSleepIndPtr);

/* @SWS_UdpNm_00324 */
Std_ReturnType UdpNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit);

/* @SWS_UdpNm_00313 */
Std_ReturnType UdpNm_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_UdpNm_00228 */
void UdpNm_SoAdIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_UdpNm_00231 */
void UdpNm_SoAdIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_UdpNm_91001 */
Std_ReturnType UdpNm_TriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);

/* @SWS_UdpNm_00234 */
void UdpNm_MainFunction(void);
#ifdef __cplusplus
}
#endif
#endif /* _UDPNM_H */
