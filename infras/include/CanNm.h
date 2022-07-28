/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CANNetworkManagement.pdf
 */
#ifndef _CANNM_H
#define _CANNM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "NmStack_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct CanNm_Config_s CanNm_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CanNm_00208 */
void CanNm_Init(const CanNm_ConfigType *cannmConfigPtr);

/* @SWS_CanNm_91002 */
void CanNm_DeInit(void);

/* @SWS_CanNm_00211 */
Std_ReturnType CanNm_PassiveStartUp(NetworkHandleType nmChannelHandle);

/* @SWS_CanNm_00213 */
Std_ReturnType CanNm_NetworkRequest(NetworkHandleType nmChannelHandle);

/* @SWS_CanNm_00214 */
Std_ReturnType CanNm_NetworkRelease(NetworkHandleType nmChannelHandle);

/* @SWS_CanNm_00215 */
Std_ReturnType CanNm_DisableCommunication(NetworkHandleType nmChannelHandle);

/* @SWS_CanNm_00216 */
Std_ReturnType CanNm_EnableCommunication(NetworkHandleType nmChannelHandle);

/* @SWS_CanNm_00217 */
Std_ReturnType CanNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8_t *nmUserDataPtr);

/* @SWS_CanNm_00218 */
Std_ReturnType CanNm_GetUserData(NetworkHandleType nmChannelHandle, uint8_t *nmUserDataPtr);

/* @SWS_CanNm_00331 */
Std_ReturnType CanNm_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_CanNm_00219 */
Std_ReturnType CanNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8_t *nmNodeIdPtr);

/* @SWS_CanNm_00220 */
Std_ReturnType CanNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle, uint8_t *nmNodeIdPtr);

/* @SWS_CanNm_00221 */
Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle);

/* @SWS_CanNm_00222 */
Std_ReturnType CanNm_GetPduData(NetworkHandleType nmChannelHandle, uint8_t *nmPduDataPtr);

/* @SWS_CanNm_00223 */
Std_ReturnType CanNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType *nmStatePtr,
                              Nm_ModeType *nmModePtr);

/* @SWS_CanNm_00226 */
Std_ReturnType CanNm_RequestBusSynchronization(NetworkHandleType nmChannelHandle);
/* @SWS_CanNm_00227 */
Std_ReturnType CanNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle,
                                                boolean *nmRemoteSleepIndPtr);
/* @SWS_CanNm_00338 */
Std_ReturnType CanNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle, boolean nmSleepReadyBit);

/* @SWS_CanNm_00228 */
void CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_CanNm_00231 */
void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_CanNm_00334 */
void CanNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle);

/* @SWS_CanNm_91001 */
Std_ReturnType CanNm_TriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);

/* @SWS_CanNm_00234 */
void CanNm_MainFunction(void);
#ifdef __cplusplus
}
#endif
#endif /* _CANNM_H */
