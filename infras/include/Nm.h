/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:  Specification of NetworkManagement Interface AUTOSAR CP Release 4.4.0
 */
#ifndef _NM_H
#define _NM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "NmStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_Nm_00232 */
#define NM_E_UNINIT 0x00u
#define NM_E_INVALID_CHANNEL 0x01u
#define NM_E_PARAM_POINTER 0x02u
/* ================================ [ TYPES     ] ============================================== */
typedef struct Nm_Config_s Nm_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Nm_00030 */
void Nm_Init(const Nm_ConfigType *ConfigPtr);

/* @SWS_Nm_00031 */
Std_ReturnType Nm_PassiveStartUp(NetworkHandleType NetworkHandle);

/* @SWS_Nm_00032 */
Std_ReturnType Nm_NetworkRequest(NetworkHandleType NetworkHandle);

/* @SWS_Nm_00046 */
Std_ReturnType Nm_NetworkRelease(NetworkHandleType NetworkHandle);

/* @SWS_Nm_00033 */
Std_ReturnType Nm_DisableCommunication(NetworkHandleType NetworkHandle);

/* @SWS_Nm_00034 */
Std_ReturnType Nm_EnableCommunication(NetworkHandleType NetworkHandle);

/* @SWS_Nm_00043] */
Std_ReturnType Nm_GetState(NetworkHandleType nmNetworkHandle, Nm_StateType *nmStatePtr,
                           Nm_ModeType *nmModePtr);

/* @SWS_Nm_00118 */
void Nm_MainFunction(void);

/* @SWS_Nm_00154 */
void Nm_NetworkStartIndication(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00156 */
void Nm_NetworkMode(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00162 */
void Nm_BusSleepMode(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00159 */
void Nm_PrepareBusSleepMode(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00234 */
void Nm_TxTimeoutException(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00230 */
void Nm_RepeatMessageIndication(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00192 */
void Nm_RemoteSleepIndication(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00193 */
void Nm_RemoteSleepCancellation(NetworkHandleType nmNetworkHandle);

/* @SWS_Nm_00254 */
void Nm_CoordReadyToSleepIndication(NetworkHandleType nmChannelHandle);

/* @SWS_Nm_00272 */
void Nm_CoordReadyToSleepCancellation(NetworkHandleType nmChannelHandle);

/* @SWS_Nm_00114 */
void Nm_StateChangeNotification(NetworkHandleType nmNetworkHandle, Nm_StateType nmPreviousState,
                                Nm_StateType nmCurrentState);

/* @SWS_Nm_00250 */
void Nm_CarWakeUpIndication(NetworkHandleType nmChannelHandle);

/* @SWS_Nm_00044 */
void Nm_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* _NM_H */
