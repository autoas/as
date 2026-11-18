/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComM.h"
#include "ComM_Cfg.h"
#include "ComM_Priv.h"
#include "Std_Debug.h"
#include "CanSM.h"
#include "Nm.h"
#include "ComM.h"
#include "Std_Critical.h"
#include "BswM_ComM.h"

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_COMM 0
#define AS_LOG_COMMI 0
#define AS_LOG_COMME 3

#ifdef COMM_USE_PB_CONFIG
#define COMM_CONFIG commConfig
#else
#define COMM_CONFIG (&ComM_Config)
#endif

#ifdef USE_COMM_CRITICAL
#define ComMEnterCritical() EnterCritical()
#define ComMExitCritical() ExitCritical();
#else
#define ComMEnterCritical()
#define ComMExitCritical()
#endif

#define COMM_NOCOM_REQ_PEND_WAIT 0u
#define COMM_NOCOM_STARTUP_FULL_COM 1u
#define COMM_NOCOM_STARTUP_PASSIVE 2u
#define COMM_NOCOM_REQ_EXIT 3u
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const ComM_ConfigType ComM_Config;

static ComM_ModeType ComM_GetChannelComMode(NetworkHandleType channel);
/* ================================ [ DATAS     ] ============================================== */
#ifdef COMM_USE_PB_CONFIG
static const ComM_ConfigType *commConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static Std_ReturnType ComM_BusSM_RequestMode(NetworkHandleType channel, ComM_ModeType comMode) {
  Std_ReturnType ret = E_NOT_OK;
  const ComM_ChannelConfigType *ccfg;

  ccfg = &COMM_CONFIG->ChannelConfigs[channel];

  switch (ccfg->BusType) {
#ifdef USE_CANSM
  case COMM_BUS_TYPE_CAN:
    ret = CanSM_RequestComMode(ccfg->SmHandle, comMode);
    break;
#endif
  default:
    break;
  }
  return ret;
}

static Std_ReturnType ComM_Nm_Request(NetworkHandleType channel) {
  Std_ReturnType ret = E_NOT_OK;
  const ComM_ChannelConfigType *ccfg;
#ifdef COMM_USE_VARIANT_LIGHT
  ComM_ChannelContextType *cctx;
#endif
  ccfg = &COMM_CONFIG->ChannelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL:
    ret = Nm_NetworkRequest(ccfg->NmHandle);
    break;
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_PassiveStartUp(ccfg->NmHandle);
    break;
#ifdef COMM_USE_VARIANT_LIGHT
  case COMM_NM_VARIANT_LIGHT:
    cctx = &COMM_CONFIG->ChannelContexts[channel];
    cctx->NmLightTimer = 0; /* @SWS_ComM_00892 */
    break;
#endif
  default:
    ret = E_OK;
    break;
  }

  return ret;
}

static Std_ReturnType ComM_Nm_PassiveStartup(NetworkHandleType channel) {
  Std_ReturnType ret = E_NOT_OK;
  const ComM_ChannelConfigType *ccfg;

  ccfg = &COMM_CONFIG->ChannelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL:
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_PassiveStartUp(ccfg->NmHandle);
    break;
  default:
    ret = E_OK;
    break;
  }

  return ret;
}

static Std_ReturnType ComM_Nm_Release(NetworkHandleType channel) {
  Std_ReturnType ret = E_NOT_OK;
  const ComM_ChannelConfigType *ccfg;
#ifdef COMM_USE_VARIANT_LIGHT
  ComM_ChannelContextType *cctx;
#endif
  ccfg = &COMM_CONFIG->ChannelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL: /* intend no break */
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_NetworkRelease(ccfg->NmHandle);
    break;
#ifdef COMM_USE_VARIANT_LIGHT
  case COMM_NM_VARIANT_LIGHT:
    cctx = &COMM_CONFIG->ChannelContexts[channel];
    cctx->NmLightTimer = ccfg->NmLightTimeout; /* @SWS_ComM_00891 */
    break;
#endif
  default:
    ret = E_OK;
    break;
  }

  return ret;
}

static Std_ReturnType ComM_Nm_GetState(NetworkHandleType channel, Nm_StateType *nmStatePtr,
                                       Nm_ModeType *nmModePtr) {
  Std_ReturnType ret = E_NOT_OK;
  const ComM_ChannelConfigType *ccfg;
#ifdef COMM_USE_VARIANT_LIGHT
  ComM_ChannelContextType *cctx;
#endif
  ccfg = &COMM_CONFIG->ChannelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL: /* intend no break */
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_GetState(ccfg->NmHandle, nmStatePtr, nmModePtr);
    break;
#ifdef COMM_USE_VARIANT_LIGHT
  case COMM_NM_VARIANT_LIGHT: /* SWS_ComM_00610 */
    cctx = &COMM_CONFIG->ChannelContexts[channel];
    if (cctx->NmLightTimer > 0) {
      cctx->NmLightTimer--;
      if (0 == cctx->NmLightTimer) {
        *nmModePtr = NM_MODE_BUS_SLEEP;
      } else {
        *nmModePtr = NM_MODE_NETWORK;
      }
    } else {
      *nmModePtr = NM_MODE_BUS_SLEEP;
    }
    break;
#endif
  default:
    ret = E_OK;
    break;
  }

  return ret;
}

static Std_ReturnType ComM_BusSM_GetCurrentComMode(NetworkHandleType channel,
                                                   ComM_ModeType *ComM_ModePtr) {
  Std_ReturnType ret = E_NOT_OK;
  const ComM_ChannelConfigType *ccfg;

  ccfg = &COMM_CONFIG->ChannelConfigs[channel];

  switch (ccfg->BusType) {
#ifdef USE_CANSM
  case COMM_BUS_TYPE_CAN:
    ret = CanSM_GetCurrentComMode(ccfg->SmHandle, ComM_ModePtr);
    break;
#endif
  default:
    break;
  }
  return ret;
}

static void ComM_MainFunction_NoComNoPendingRequest(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;

  cctx = &COMM_CONFIG->ChannelContexts[channel];

  if (COMM_FULL_COMMUNICATION == cctx->RequestMode) { /* @SWS_ComM_00875 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter No Com Request Pending\n", channel));
      cctx->State = COMM_NO_COM_REQUEST_PENDING;
    }
  }

  if (0u != (cctx->NmFlag & COMM_NM_FLAG_NETWORK_START)) { /* @SWS_ComM_00894 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter No Com Request Pending\n", channel));
      cctx->State = COMM_NO_COM_REQUEST_PENDING;
    }
  }
}

static void ComM_MainFunction_NoComRequestPending(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;
  ComM_ModeType ComMode;
  uint8_t action = COMM_NOCOM_REQ_EXIT;

  cctx = &COMM_CONFIG->ChannelContexts[channel];

  if (COMM_FULL_COMMUNICATION == cctx->RequestMode) {
    if (TRUE == cctx->CommunicationAllowed) { /* @SWS_ComM_00895 */
      action = COMM_NOCOM_STARTUP_FULL_COM;
    } else {
      action = COMM_NOCOM_REQ_PEND_WAIT;
    }
  }

  if (0u != (cctx->NmFlag & COMM_NM_FLAG_NETWORK_START)) {
    if (TRUE == cctx->CommunicationAllowed) { /* @SWS_ComM_00895 */
      action = COMM_NOCOM_STARTUP_PASSIVE;
      ComMEnterCritical();
      cctx->NmFlag &= ~COMM_NM_FLAG_NETWORK_START;
      ComMExitCritical();
    } else {
      action = COMM_NOCOM_REQ_PEND_WAIT;
    }
  }

  if (COMM_NOCOM_REQ_PEND_WAIT == action) {
    /* Wait communicaiton allowed */
  } else if (COMM_NOCOM_STARTUP_FULL_COM == action) {
    ret = ComM_BusSM_GetCurrentComMode(channel, &ComMode);
    if (E_OK == ret) {
      if (COMM_FULL_COMMUNICATION == ComMode) {
        ret = ComM_Nm_Request(channel);
        if (E_OK == ret) {
          ASLOG(COMM, ("[%d] Enter Full Com\n", channel));
          cctx->State = COMM_FULL_COM_NETWORK_REQUESTED;
          ComMEnterCritical();
          cctx->NmFlag = 0;
          ComMExitCritical();
        }
      }
    }
  } else if (COMM_NOCOM_STARTUP_PASSIVE == action) {
    ret = ComM_BusSM_GetCurrentComMode(channel, &ComMode);
    if (E_OK == ret) {
      if (COMM_FULL_COMMUNICATION == ComMode) {
        ret = ComM_Nm_PassiveStartup(channel);
        if (E_OK == ret) {
          ASLOG(COMM, ("[%d] Passive Startup to Full Com\n", channel));
          cctx->State = COMM_FULL_COM_NETWORK_REQUESTED;
          ComMEnterCritical();
          cctx->NmFlag = 0;
          ComMExitCritical();
        }
      }
    }
  } else { /* @SWS_ComM_00897 */
    ret = ComM_BusSM_RequestMode(channel, COMM_NO_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter No Com No Pending Request\n", channel));
      cctx->State = COMM_NO_COM_NO_PENDING_REQUEST;
      ComMEnterCritical();
      cctx->NmFlag = 0;
      ComMExitCritical();
    }
  }
}

static void ComM_MainFunction_FullComNetworkRequested(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;

  cctx = &COMM_CONFIG->ChannelContexts[channel];
  if (COMM_NO_COMMUNICATION == cctx->RequestMode) {
    ComMEnterCritical();
    cctx->NmFlag = 0; /* just clear the nm flag */
    ComMExitCritical();
    ret = ComM_Nm_Release(channel); /* @SWS_ComM_00133 */
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter Ready Sleep\n", channel));
      cctx->State = COMM_FULL_COM_READY_SLEEP;
    }
  }
}

static void ComM_MainFunction_FullComReadySleep(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;
  Nm_StateType nmState;
  Nm_ModeType nmMode;

  cctx = &COMM_CONFIG->ChannelContexts[channel];
  if (COMM_FULL_COMMUNICATION == cctx->RequestMode) { /* @SWS_ComM_00882 */
    ret = ComM_Nm_Request(channel);                   /* @SWS_ComM_00869 */
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter Full Com Network Requested\n", channel));
      cctx->State = COMM_FULL_COM_NETWORK_REQUESTED;
      ComMEnterCritical();
      cctx->NmFlag = 0;
      ComMExitCritical();
    }
  } else if (0u != (cctx->NmFlag & COMM_NM_FLAG_PREPARE_BUS_SLEEP)) { /* @SWS_ComM_00826 */
    ret = ComM_BusSM_RequestMode(channel, COMM_SILENT_COMMUNICATION); /* @@SWS_ComM_00071 */
    if (E_OK == ret) {
      ComMEnterCritical();
      cctx->NmFlag &= ~COMM_NM_FLAG_PREPARE_BUS_SLEEP;
      ComMExitCritical();
      ASLOG(COMM, ("[%d] Enter Silent Com by NM\n", channel));
      cctx->State = COMM_SILENT_COM;
    }
  } else { /* this Nm state polling just make ComM more roboust */
    ret = ComM_Nm_GetState(channel, &nmState, &nmMode);
    if ((E_OK == ret) && (NM_MODE_BUS_SLEEP == nmMode)) {               /* @SWS_ComM_00637 */
      ret = ComM_BusSM_RequestMode(channel, COMM_SILENT_COMMUNICATION); /* @@SWS_ComM_00071 */
      if (E_OK == ret) {
        ComMEnterCritical();
        cctx->NmFlag &= ~COMM_NM_FLAG_PREPARE_BUS_SLEEP; /* no matter what, clear it */
        ComMExitCritical();
        ASLOG(COMM, ("[%d] Enter Silent Com\n", channel));
        cctx->State = COMM_SILENT_COM;
      }
    }
  }
}

static void ComM_MainFunction_SilentCom(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;
  Nm_StateType nmState;
  Nm_ModeType nmMode;

  cctx = &COMM_CONFIG->ChannelContexts[channel];
  if (COMM_FULL_COMMUNICATION == cctx->RequestMode) { /* @SWS_ComM_00877 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter Full Com Ready Sleep by User\n", channel));
      cctx->State = COMM_FULL_COM_READY_SLEEP;
    }
  } else if (0u != (cctx->NmFlag & COMM_NM_FLAG_BUS_SLEEP)) {     /* @SWS_ComM_00637 */
    ret = ComM_BusSM_RequestMode(channel, COMM_NO_COMMUNICATION); /* @@SWS_ComM_00073 */
    if (E_OK == ret) {
      ComMEnterCritical();
      cctx->NmFlag &= ~COMM_NM_FLAG_BUS_SLEEP;
      ComMExitCritical();
      ASLOG(COMM, ("[%d] Enter No Com by NM\n", channel));
      cctx->State = COMM_NO_COM_NO_PENDING_REQUEST;
    }
  } else if (0u != (cctx->NmFlag & COMM_NM_FLAG_NETWORK_MODE)) { /* @SWS_ComM_00296 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      ComMEnterCritical();
      cctx->NmFlag &= ~COMM_NM_FLAG_NETWORK_MODE;
      ComMExitCritical();
      ASLOG(COMM, ("[%d] Enter Full Com Ready Sleep by NM\n", channel));
      cctx->State = COMM_FULL_COM_READY_SLEEP;
    }
  } else { /* this Nm state polling just make ComM more roboust */
    ret = ComM_Nm_GetState(channel, &nmState, &nmMode);
    if (E_OK == ret) {
      if (NM_MODE_BUS_SLEEP == nmMode) { /* @SWS_ComM_00295 */
        /* @SWS_ComM_00073 */
        ret = ComM_BusSM_RequestMode(channel, COMM_NO_COMMUNICATION);
        if (E_OK == ret) {
          ASLOG(COMM, ("[%d] Enter No Com\n", channel));
          cctx->State = COMM_NO_COM_NO_PENDING_REQUEST; /* @SWS_ComM_00898 */
        }
      } else if (NM_MODE_NETWORK == nmMode) { /* @SWS_ComM_00296 */
        ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
        if (E_OK == ret) {
          ASLOG(COMM, ("[%d] Enter Full Com Ready Sleep\n", channel));
          cctx->State = COMM_FULL_COM_READY_SLEEP;
        }
      } else {
        /* do nothing */
      }
    }
  }
}

static void ComM_MainFunction_Channel(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
#ifdef USE_BSWM
  ComM_ModeType preMode;
  ComM_ModeType newMode;
#endif
  cctx = &COMM_CONFIG->ChannelContexts[channel];

#ifdef USE_BSWM
  preMode = ComM_GetChannelComMode(channel);
#endif
  switch (cctx->State) {
  case COMM_NO_COM_NO_PENDING_REQUEST:
    ComM_MainFunction_NoComNoPendingRequest(channel);
    break;
  case COMM_NO_COM_REQUEST_PENDING:
    ComM_MainFunction_NoComRequestPending(channel);
    break;
  case COMM_FULL_COM_NETWORK_REQUESTED:
    ComM_MainFunction_FullComNetworkRequested(channel);
    break;
  case COMM_FULL_COM_READY_SLEEP:
    ComM_MainFunction_FullComReadySleep(channel);
    break;
  case COMM_SILENT_COM:
    ComM_MainFunction_SilentCom(channel);
    break;
  default:
    break;
  }

#ifdef USE_BSWM
  newMode = ComM_GetChannelComMode(channel);
  if (preMode != newMode) {
    BswM_ComM_CurrentMode(channel, newMode);
  }
#endif
}

static void ComM_ForwardChannelRequestMode(NetworkHandleType channel) {
  const ComM_ChannelConfigType *ccfg;
  ComM_ChannelContextType *cctx;
  ComM_UserContextType *uctx;
  ComM_ModeType comMode = COMM_NO_COMMUNICATION;
  uint8_t i;

  ccfg = &COMM_CONFIG->ChannelConfigs[channel];
  cctx = &COMM_CONFIG->ChannelContexts[channel];
  for (i = 0; i < ccfg->NumOfUsers; i++) {
    uctx = &COMM_CONFIG->UserContexts[ccfg->Users[i]];
    if (uctx->RequestMode > comMode) { /* @SWS_ComM_00686 */
      comMode = uctx->RequestMode;
    }
  }

  cctx->RequestMode = comMode;
}

static ComM_ModeType ComM_GetChannelComMode(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  ComM_ModeType comMode = COMM_FULL_COMMUNICATION;

  cctx = &COMM_CONFIG->ChannelContexts[channel];
  switch (cctx->State) {
  case COMM_FULL_COM_NETWORK_REQUESTED: /* intended no break */
  case COMM_FULL_COM_READY_SLEEP:
    comMode = COMM_FULL_COMMUNICATION;
    break;
  case COMM_SILENT_COM:
    comMode = COMM_SILENT_COMMUNICATION;
    break;
  default:
    comMode = COMM_NO_COMMUNICATION;
    break;
  }

  return comMode;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void ComM_Init(const ComM_ConfigType *configPtr) {
  uint8_t i;
  ComM_ChannelContextType *cctx;
  ComM_UserContextType *uctx;

#ifdef COMM_USE_PB_CONFIG
  if (NULL != configPtr) {
    COMM_CONFIG = configPtr;
  } else {
    COMM_CONFIG = &ComM_Config;
  }
#else
  (void)configPtr;
#endif
  for (i = 0; i < COMM_CONFIG->NumOfChannels; i++) {
    cctx = &COMM_CONFIG->ChannelContexts[i];
    cctx->State = COMM_NO_COM_NO_PENDING_REQUEST;
    cctx->RequestMode = COMM_NO_COMMUNICATION;
    cctx->NmFlag = 0;
#ifdef COMM_USE_VARIANT_LIGHT
    cctx->NmLightTimer = 0;
#endif
    cctx->CommunicationAllowed = FALSE;
  }

  for (i = 0; i < COMM_CONFIG->NumOfUsers; i++) {
    uctx = &COMM_CONFIG->UserContexts[i];
    uctx->RequestMode = COMM_NO_COMMUNICATION;
  }
}

void ComM_DeInit(void) {
}

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType user, ComM_ModeType comMode) {
  Std_ReturnType ret = E_OK;
  uint8_t i;
  ComM_UserContextType *uctx;
  const ComM_UserConfigType *ucfg;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x05, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(user < COMM_CONFIG->NumOfUsers, 0x05, COMM_E_WRONG_PARAMETERS, return E_NOT_OK);
  DET_VALIDATE((COMM_NO_COMMUNICATION == comMode) || (COMM_FULL_COMMUNICATION == comMode), 0x05,
               COMM_E_WRONG_PARAMETERS, return E_NOT_OK); /* SWS_ComM_00151 */

  uctx = &COMM_CONFIG->UserContexts[user];
  uctx->RequestMode = comMode;

  /* forward channel request mode */
  ucfg = &COMM_CONFIG->UserConfigs[user];
  for (i = 0; i < ucfg->NumOfChannels; i++) {
    ComM_ForwardChannelRequestMode(ucfg->Channels[i]);
  }

  return ret;
}

Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType user, ComM_ModeType *comMode) {
  Std_ReturnType ret = E_OK;
  ComM_UserContextType *uctx;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x07, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(user < COMM_CONFIG->NumOfUsers, 0x07, COMM_E_WRONG_PARAMETERS, return E_NOT_OK);
  DET_VALIDATE(NULL != comMode, 0x07, COMM_E_PARAM_POINTER, return E_NOT_OK);

  uctx = &COMM_CONFIG->UserContexts[user];
  *comMode = uctx->RequestMode;

  return ret;
}

Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType user, ComM_ModeType *comMode) {
  Std_ReturnType ret = E_OK;
  uint8_t i;
  const ComM_UserConfigType *ucfg;
  ComM_ModeType md;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x08, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(user < COMM_CONFIG->NumOfUsers, 0x08, COMM_E_WRONG_PARAMETERS, return E_NOT_OK);
  DET_VALIDATE(NULL != comMode, 0x08, COMM_E_PARAM_POINTER, return E_NOT_OK);

  *comMode = COMM_FULL_COMMUNICATION;

  ucfg = &COMM_CONFIG->UserConfigs[user];
  for (i = 0; i < ucfg->NumOfChannels; i++) { /* @SWS_ComM_00176 */
    md = ComM_GetChannelComMode(ucfg->Channels[i]);
    if (md < *comMode) {
      *comMode = md;
    }
  }

  return ret;
}

void ComM_MainFunction(void) {
  uint8_t i;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x60, COMM_E_UNINIT, return);
  for (i = 0; i < COMM_CONFIG->NumOfChannels; i++) {
    ComM_MainFunction_Channel((NetworkHandleType)i);
  }
}

Std_ReturnType ComM_GetState(NetworkHandleType channel, ComM_StateType *state) {
  Std_ReturnType ret = E_OK;
  ComM_ChannelContextType *cctx;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x34, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(channel < COMM_CONFIG->NumOfChannels, 0x34, COMM_E_WRONG_PARAMETERS,
               return E_NOT_OK);
  DET_VALIDATE(NULL != state, 0x34, COMM_E_PARAM_POINTER, return E_NOT_OK);

  cctx = &COMM_CONFIG->ChannelContexts[channel];

  *state = cctx->State;

  return ret;
}

void ComM_BusSM_ModeIndication(NetworkHandleType channel, ComM_ModeType comMode) {
  DET_VALIDATE(NULL != COMM_CONFIG, 0x33, COMM_E_UNINIT, return);
  DET_VALIDATE(channel < COMM_CONFIG->NumOfChannels, 0x33, COMM_E_WRONG_PARAMETERS, return);

  (void)channel;
  (void)comMode;
}

void ComM_Nm_NetworkStartIndication(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x15, COMM_E_UNINIT, return);
  DET_VALIDATE(channel < COMM_CONFIG->NumOfChannels, 0x15, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Network Start\n", channel));
  cctx = &COMM_CONFIG->ChannelContexts[channel];
  ComMEnterCritical();
  cctx->NmFlag |= COMM_NM_FLAG_NETWORK_START;
  ComMExitCritical();
}

void ComM_Nm_NetworkMode(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  const ComM_ChannelConfigType *ccfg;
  uint8_t i;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x33, COMM_E_UNINIT, return);
  DET_VALIDATE(channel < COMM_CONFIG->NumOfChannels, 0x33, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Network Mode\n", channel));
  ccfg = &COMM_CONFIG->ChannelConfigs[channel];
  cctx = &COMM_CONFIG->ChannelContexts[channel];
  ComMEnterCritical();
  cctx->NmFlag |= COMM_NM_FLAG_NETWORK_MODE;
  ComMExitCritical();

  for (i = 0; i < ccfg->NumOfComIpduGroups; i++) {
    Com_IpduGroupStart(ccfg->ComIpduGroupIds[i], TRUE);
  }
}

void ComM_Nm_PrepareBusSleepMode(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  const ComM_ChannelConfigType *ccfg;
  uint8_t i;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x19, COMM_E_UNINIT, return);
  DET_VALIDATE(channel < COMM_CONFIG->NumOfChannels, 0x19, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Prepare Bus Sleep\n", channel));
  ccfg = &COMM_CONFIG->ChannelConfigs[channel];
  cctx = &COMM_CONFIG->ChannelContexts[channel];
  ComMEnterCritical();
  cctx->NmFlag |= COMM_NM_FLAG_PREPARE_BUS_SLEEP;
  ComMExitCritical();

  for (i = 0; i < ccfg->NumOfComIpduGroups; i++) {
    Com_IpduGroupStop(ccfg->ComIpduGroupIds[i]);
  }
}

void ComM_Nm_BusSleepMode(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x1A, COMM_E_UNINIT, return);
  DET_VALIDATE(channel < COMM_CONFIG->NumOfChannels, 0x1A, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Bus Sleep\n", channel));
  cctx = &COMM_CONFIG->ChannelContexts[channel];
  ComMEnterCritical();
  cctx->NmFlag |= COMM_NM_FLAG_BUS_SLEEP;
  ComMExitCritical();
}

void ComM_CommunicationAllowed(NetworkHandleType channel, boolean allowed) {
  ComM_ChannelContextType *cctx;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x35, COMM_E_UNINIT, return);
  DET_VALIDATE(channel < COMM_CONFIG->NumOfChannels, 0x35, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] Communication allowed %s\n", channel, allowed ? "true" : "false"));
  cctx = &COMM_CONFIG->ChannelContexts[channel];
  ComMEnterCritical();
  cctx->CommunicationAllowed = allowed;
  ComMExitCritical();
}

void ComM_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x10, COMM_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_COMM;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
