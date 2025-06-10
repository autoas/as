/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifdef USE_NM
/* ================================ [ INCLUDES  ] ============================================== */
#include "Nm.h"
#include "Nm_Priv.h"
#include "CanNm.h"
#include "OsekNm.h"
#include "UdpNm.h"
#include "ComM.h"
#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define NM_CONFIG (&Nm_Config)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const Nm_ConfigType Nm_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Nm_Init(const Nm_ConfigType *ConfigPtr) {
}

Std_ReturnType Nm_PassiveStartUp(NetworkHandleType NetworkHandle) {
  Std_ReturnType ret = E_NOT_OK;
  const Nm_ChannelConfigType *config;

  DET_VALIDATE(NetworkHandle < NM_CONFIG->numOfChannels, 0x01, NM_E_INVALID_CHANNEL,
               return E_NOT_OK); /* @SWS_Nm_00488 */

  config = &NM_CONFIG->channelConfigs[NetworkHandle];
  switch (config->busType) {
#ifdef USE_CANNM
  case NM_BUSNM_CANNM:
    ret = CanNm_PassiveStartUp(config->handle);
    break;
#endif
#ifdef USE_OSEKNM
  case NM_BUSNM_OSEKNM:
    ret = OsekNm_NetworkRequest(config->handle);
    break;
#endif
  default:
    break;
  }

  return ret;
}

Std_ReturnType Nm_NetworkRequest(NetworkHandleType NetworkHandle) {
  Std_ReturnType ret = E_NOT_OK;
  const Nm_ChannelConfigType *config;

  DET_VALIDATE(NetworkHandle < NM_CONFIG->numOfChannels, 0x02, NM_E_INVALID_CHANNEL,
               return E_NOT_OK); /* @SWS_Nm_00489 */

  config = &NM_CONFIG->channelConfigs[NetworkHandle];
  switch (config->busType) {
#ifdef USE_CANNM
  case NM_BUSNM_CANNM:
    ret = CanNm_NetworkRequest(config->handle);
    break;
#endif
#ifdef USE_OSEKNM
  case NM_BUSNM_OSEKNM:
    ret = OsekNm_NetworkRequest(config->handle);
    break;
#endif
  default:
    break;
  }

  return ret;
}

Std_ReturnType Nm_NetworkRelease(NetworkHandleType NetworkHandle) {
  Std_ReturnType ret = E_NOT_OK;
  const Nm_ChannelConfigType *config;

  DET_VALIDATE(NetworkHandle < NM_CONFIG->numOfChannels, 0x03, NM_E_INVALID_CHANNEL,
               return E_NOT_OK); /* @SWS_Nm_00490 */

  config = &NM_CONFIG->channelConfigs[NetworkHandle];
  switch (config->busType) {
#ifdef USE_CANNM
  case NM_BUSNM_CANNM:
    ret = CanNm_NetworkRelease(config->handle);
    break;
#endif
#ifdef USE_OSEKNM
  case NM_BUSNM_OSEKNM:
    ret = OsekNm_NetworkRelease(config->handle);
    break;
#endif
  default:
    DET_VALIDATE(FALSE, 0x03, NM_E_PARAM_POINTER, ret = E_NOT_OK);
    break;
  }

  return ret;
}

Std_ReturnType Nm_GetState(NetworkHandleType nmNetworkHandle, Nm_StateType *nmStatePtr,
                           Nm_ModeType *nmModePtr) {
  Std_ReturnType ret = E_NOT_OK;
  const Nm_ChannelConfigType *config;

  DET_VALIDATE(nmNetworkHandle < NM_CONFIG->numOfChannels, 0x0E, NM_E_INVALID_CHANNEL,
               return E_NOT_OK); /* @SWS_Nm_00500 */

  config = &NM_CONFIG->channelConfigs[nmNetworkHandle];
  switch (config->busType) {
#ifdef USE_CANNM
  case NM_BUSNM_CANNM:
    ret = CanNm_GetState(config->handle, nmStatePtr, nmModePtr);
    break;
#endif
#ifdef USE_OSEKNM
  case NM_BUSNM_OSEKNM:
    ret = OsekNm_GetState(config->handle, nmModePtr);
    break;
#endif
  default:
    break;
  }

  return ret;
}

void Nm_MainFunction(void) {
}

void Nm_NetworkStartIndication(NetworkHandleType nmNetworkHandle) {
  const Nm_ChannelConfigType *config;
  (void)config;
  DET_VALIDATE(nmNetworkHandle < NM_CONFIG->numOfChannels, 0x11, NM_E_INVALID_CHANNEL, return);
  config = &NM_CONFIG->channelConfigs[nmNetworkHandle];
#ifdef USE_COMM /* @SWS_Nm_00155 */
  ComM_Nm_NetworkStartIndication(config->ComMHandle);
#endif
}

void Nm_NetworkMode(NetworkHandleType nmNetworkHandle) {
  const Nm_ChannelConfigType *config;
  (void)config;
  DET_VALIDATE(nmNetworkHandle < NM_CONFIG->numOfChannels, 0x12, NM_E_INVALID_CHANNEL, return);
  config = &NM_CONFIG->channelConfigs[nmNetworkHandle];
#ifdef USE_COMM /* @SWS_Nm_00158 */
  ComM_Nm_NetworkMode(config->ComMHandle);
#endif
}

void Nm_BusSleepMode(NetworkHandleType nmNetworkHandle) {
  const Nm_ChannelConfigType *config;
  (void)config;
  DET_VALIDATE(nmNetworkHandle < NM_CONFIG->numOfChannels, 0x14, NM_E_INVALID_CHANNEL, return);
  config = &NM_CONFIG->channelConfigs[nmNetworkHandle];
#ifdef USE_COMM /* @SWS_Nm_00163 */
  ComM_Nm_BusSleepMode(config->ComMHandle);
#endif
}

void Nm_PrepareBusSleepMode(NetworkHandleType nmNetworkHandle) {
  const Nm_ChannelConfigType *config;
  (void)config;
  DET_VALIDATE(nmNetworkHandle < NM_CONFIG->numOfChannels, 0x13, NM_E_INVALID_CHANNEL, return);
  config = &NM_CONFIG->channelConfigs[nmNetworkHandle];
#ifdef USE_COMM /* @SWS_Nm_00161 */
  ComM_Nm_PrepareBusSleepMode(config->ComMHandle);
#endif
}

void Nm_TxTimeoutException(NetworkHandleType nmNetworkHandle) {
}

void Nm_RepeatMessageIndication(NetworkHandleType nmNetworkHandle) {
}

void Nm_RemoteSleepIndication(NetworkHandleType nmNetworkHandle) {
}

void Nm_RemoteSleepCancellation(NetworkHandleType nmNetworkHandle) {
}

void Nm_CoordReadyToSleepIndication(NetworkHandleType nmChannelHandle) {
}

void Nm_CoordReadyToSleepCancellation(NetworkHandleType nmChannelHandle) {
}

void Nm_StateChangeNotification(NetworkHandleType nmNetworkHandle, Nm_StateType nmPreviousState,
                                Nm_StateType nmCurrentState) {
}

void Nm_CarWakeUpIndication(NetworkHandleType nmChannelHandle) {
}
#endif
