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
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
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
#endif /* _NM_H */
