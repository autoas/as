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
#define commEnterCritical() EnterCritical()
#define commExitCritical() ExitCritical();
#else
#define commEnterCritical()
#define commExitCritical()
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
static Std_ReturnType ComM_BusSM_RequestMode(NetworkHandleType channel, ComM_ModeType ComMode) {
  Std_ReturnType ret = E_NOT_OK;
  const ComM_ChannelConfigType *ccfg;

  ccfg = &COMM_CONFIG->channelConfigs[channel];

  switch (ccfg->busType) {
#ifdef USE_CANSM
  case COMM_BUS_TYPE_CAN:
    ret = CanSM_RequestComMode(ccfg->smHandle, ComMode);
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
  ccfg = &COMM_CONFIG->channelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL:
    ret = Nm_NetworkRequest(ccfg->nmHandle);
    break;
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_PassiveStartUp(ccfg->nmHandle);
    break;
#ifdef COMM_USE_VARIANT_LIGHT
  case COMM_NM_VARIANT_LIGHT:
    cctx = &COMM_CONFIG->channelContexts[channel];
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

  ccfg = &COMM_CONFIG->channelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL:
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_PassiveStartUp(ccfg->nmHandle);
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
  ccfg = &COMM_CONFIG->channelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL: /* intend no break */
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_NetworkRelease(ccfg->nmHandle);
    break;
#ifdef COMM_USE_VARIANT_LIGHT
  case COMM_NM_VARIANT_LIGHT:
    cctx = &COMM_CONFIG->channelContexts[channel];
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
  ccfg = &COMM_CONFIG->channelConfigs[channel];
  switch (ccfg->NmVariant) {
  case COMM_NM_VARIANT_FULL: /* intend no break */
  case COMM_NM_VARIANT_PASSIVE:
    ret = Nm_GetState(ccfg->nmHandle, nmStatePtr, nmModePtr);
    break;
#ifdef COMM_USE_VARIANT_LIGHT
  case COMM_NM_VARIANT_LIGHT: /* SWS_ComM_00610 */
    cctx = &COMM_CONFIG->channelContexts[channel];
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

  ccfg = &COMM_CONFIG->channelConfigs[channel];

  switch (ccfg->busType) {
#ifdef USE_CANSM
  case COMM_BUS_TYPE_CAN:
    ret = CanSM_GetCurrentComMode(ccfg->smHandle, ComM_ModePtr);
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

  cctx = &COMM_CONFIG->channelContexts[channel];

  if (COMM_FULL_COMMUNICATION == cctx->requestMode) { /* @SWS_ComM_00875 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter No Com Request Pending\n", channel));
      cctx->state = COMM_NO_COM_REQUEST_PENDING;
    }
  }

  if (0u != (cctx->nmFlag & COMM_NM_FLAG_NETWORK_START)) { /* @SWS_ComM_00894 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter No Com Request Pending\n", channel));
      cctx->state = COMM_NO_COM_REQUEST_PENDING;
    }
  }
}

static void ComM_MainFunction_NoComRequestPending(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;
  ComM_ModeType ComMode;
  uint8_t action = COMM_NOCOM_REQ_EXIT;

  cctx = &COMM_CONFIG->channelContexts[channel];

  if (COMM_FULL_COMMUNICATION == cctx->requestMode) {
    if (TRUE == cctx->bCommunicationAllowed) { /* @SWS_ComM_00895 */
      action = COMM_NOCOM_STARTUP_FULL_COM;
    } else {
      action = COMM_NOCOM_REQ_PEND_WAIT;
    }
  }

  if (0u != (cctx->nmFlag & COMM_NM_FLAG_NETWORK_START)) {
    if (TRUE == cctx->bCommunicationAllowed) { /* @SWS_ComM_00895 */
      action = COMM_NOCOM_STARTUP_PASSIVE;
      commEnterCritical();
      cctx->nmFlag &= ~COMM_NM_FLAG_NETWORK_START;
      commExitCritical();
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
          cctx->state = COMM_FULL_COM_NETWORK_REQUESTED;
          commEnterCritical();
          cctx->nmFlag = 0;
          commExitCritical();
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
          cctx->state = COMM_FULL_COM_NETWORK_REQUESTED;
          commEnterCritical();
          cctx->nmFlag = 0;
          commExitCritical();
        }
      }
    }
  } else { /* @SWS_ComM_00897 */
    ret = ComM_BusSM_RequestMode(channel, COMM_NO_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter No Com No Pending Request\n", channel));
      cctx->state = COMM_NO_COM_NO_PENDING_REQUEST;
      commEnterCritical();
      cctx->nmFlag = 0;
      commExitCritical();
    }
  }
}

static void ComM_MainFunction_FullComNetworkRequested(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;

  cctx = &COMM_CONFIG->channelContexts[channel];
  if (COMM_NO_COMMUNICATION == cctx->requestMode) {
    commEnterCritical();
    cctx->nmFlag = 0; /* just clear the nm flag */
    commExitCritical();
    ret = ComM_Nm_Release(channel); /* @SWS_ComM_00133 */
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter Ready Sleep\n", channel));
      cctx->state = COMM_FULL_COM_READY_SLEEP;
    }
  }
}

static void ComM_MainFunction_FullComReadySleep(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;
  Nm_StateType nmState;
  Nm_ModeType nmMode;

  cctx = &COMM_CONFIG->channelContexts[channel];
  if (COMM_FULL_COMMUNICATION == cctx->requestMode) { /* @SWS_ComM_00882 */
    ret = ComM_Nm_Request(channel);                   /* @SWS_ComM_00869 */
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter Full Com Network Requested\n", channel));
      cctx->state = COMM_FULL_COM_NETWORK_REQUESTED;
      commEnterCritical();
      cctx->nmFlag = 0;
      commExitCritical();
    }
  } else if (0u != (cctx->nmFlag & COMM_NM_FLAG_PREPARE_BUS_SLEEP)) {  /* @SWS_ComM_00826 */
    ret = ComM_BusSM_RequestMode(channel, COMM_SILENT_COMMUNICATION); /* @@SWS_ComM_00071 */
    if (E_OK == ret) {
      commEnterCritical();
      cctx->nmFlag &= ~COMM_NM_FLAG_PREPARE_BUS_SLEEP;
      commExitCritical();
      ASLOG(COMM, ("[%d] Enter Silent Com by NM\n", channel));
      cctx->state = COMM_SILENT_COM;
    }
  } else { /* this Nm state polling just make ComM more roboust */
    ret = ComM_Nm_GetState(channel, &nmState, &nmMode);
    if ((E_OK == ret) && (NM_MODE_BUS_SLEEP == nmMode)) {               /* @SWS_ComM_00637 */
      ret = ComM_BusSM_RequestMode(channel, COMM_SILENT_COMMUNICATION); /* @@SWS_ComM_00071 */
      if (E_OK == ret) {
        commEnterCritical();
        cctx->nmFlag &= ~COMM_NM_FLAG_PREPARE_BUS_SLEEP; /* no matter what, clear it */
        commExitCritical();
        ASLOG(COMM, ("[%d] Enter Silent Com\n", channel));
        cctx->state = COMM_SILENT_COM;
      }
    }
  }
}

static void ComM_MainFunction_SilentCom(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  Std_ReturnType ret;
  Nm_StateType nmState;
  Nm_ModeType nmMode;

  cctx = &COMM_CONFIG->channelContexts[channel];
  if (COMM_FULL_COMMUNICATION == cctx->requestMode) { /* @SWS_ComM_00877 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      ASLOG(COMM, ("[%d] Enter Full Com Ready Sleep by User\n", channel));
      cctx->state = COMM_FULL_COM_READY_SLEEP;
    }
  } else if (0u != (cctx->nmFlag & COMM_NM_FLAG_BUS_SLEEP)) {      /* @SWS_ComM_00637 */
    ret = ComM_BusSM_RequestMode(channel, COMM_NO_COMMUNICATION); /* @@SWS_ComM_00073 */
    if (E_OK == ret) {
      commEnterCritical();
      cctx->nmFlag &= ~COMM_NM_FLAG_BUS_SLEEP;
      commExitCritical();
      ASLOG(COMM, ("[%d] Enter No Com by NM\n", channel));
      cctx->state = COMM_NO_COM_NO_PENDING_REQUEST;
    }
  } else if (0u != (cctx->nmFlag & COMM_NM_FLAG_NETWORK_MODE)) { /* @SWS_ComM_00296 */
    ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
    if (E_OK == ret) {
      commEnterCritical();
      cctx->nmFlag &= ~COMM_NM_FLAG_NETWORK_MODE;
      commExitCritical();
      ASLOG(COMM, ("[%d] Enter Full Com Ready Sleep by NM\n", channel));
      cctx->state = COMM_FULL_COM_READY_SLEEP;
    }
  } else { /* this Nm state polling just make ComM more roboust */
    ret = ComM_Nm_GetState(channel, &nmState, &nmMode);
    if (E_OK == ret) {
      if (NM_MODE_BUS_SLEEP == nmMode) { /* @SWS_ComM_00295 */
        /* @SWS_ComM_00073 */
        ret = ComM_BusSM_RequestMode(channel, COMM_NO_COMMUNICATION);
        if (E_OK == ret) {
          ASLOG(COMM, ("[%d] Enter No Com\n", channel));
          cctx->state = COMM_NO_COM_NO_PENDING_REQUEST; /* @SWS_ComM_00898 */
        }
      } else if (NM_MODE_NETWORK == nmMode) { /* @SWS_ComM_00296 */
        ret = ComM_BusSM_RequestMode(channel, COMM_FULL_COMMUNICATION);
        if (E_OK == ret) {
          ASLOG(COMM, ("[%d] Enter Full Com Ready Sleep\n", channel));
          cctx->state = COMM_FULL_COM_READY_SLEEP;
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
  cctx = &COMM_CONFIG->channelContexts[channel];

#ifdef USE_BSWM
  preMode = ComM_GetChannelComMode(channel);
#endif
  switch (cctx->state) {
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

  ccfg = &COMM_CONFIG->channelConfigs[channel];
  cctx = &COMM_CONFIG->channelContexts[channel];
  for (i = 0; i < ccfg->numOfUsers; i++) {
    uctx = &COMM_CONFIG->userContexts[ccfg->users[i]];
    if (uctx->requestMode > comMode) { /* @SWS_ComM_00686 */
      comMode = uctx->requestMode;
    }
  }

  cctx->requestMode = comMode;
}

static ComM_ModeType ComM_GetChannelComMode(NetworkHandleType channel) {
  ComM_ChannelContextType *cctx;
  ComM_ModeType comMode = COMM_FULL_COMMUNICATION;

  cctx = &COMM_CONFIG->channelContexts[channel];
  switch (cctx->state) {
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
void ComM_Init(const ComM_ConfigType *ConfigPtr) {
  uint8_t i;
  ComM_ChannelContextType *cctx;
  ComM_UserContextType *uctx;

#ifdef COMM_USE_PB_CONFIG
  if (NULL != ConfigPtr) {
    COMM_CONFIG = ConfigPtr;
  } else {
    COMM_CONFIG = &ComM_Config;
  }
#else
  (void)ConfigPtr;
#endif
  for (i = 0; i < COMM_CONFIG->numOfChannels; i++) {
    cctx = &COMM_CONFIG->channelContexts[i];
    cctx->state = COMM_NO_COM_NO_PENDING_REQUEST;
    cctx->requestMode = COMM_NO_COMMUNICATION;
    cctx->nmFlag = 0;
#ifdef COMM_USE_VARIANT_LIGHT
    cctx->NmLightTimer = 0;
#endif
    cctx->bCommunicationAllowed = FALSE;
  }

  for (i = 0; i < COMM_CONFIG->numOfUsers; i++) {
    uctx = &COMM_CONFIG->userContexts[i];
    uctx->requestMode = COMM_NO_COMMUNICATION;
  }
}

void ComM_DeInit(void) {
}

Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode) {
  Std_ReturnType ret = E_OK;
  uint8_t i;
  ComM_UserContextType *uctx;
  const ComM_UserConfigType *ucfg;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x05, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(User < COMM_CONFIG->numOfUsers, 0x05, COMM_E_WRONG_PARAMETERS, return E_NOT_OK);
  DET_VALIDATE((COMM_NO_COMMUNICATION == ComMode) || (COMM_FULL_COMMUNICATION == ComMode), 0x05,
               COMM_E_WRONG_PARAMETERS, return E_NOT_OK); /* SWS_ComM_00151 */

  uctx = &COMM_CONFIG->userContexts[User];
  uctx->requestMode = ComMode;

  /* forward channel request mode */
  ucfg = &COMM_CONFIG->userConfigs[User];
  for (i = 0; i < ucfg->numOfChannels; i++) {
    ComM_ForwardChannelRequestMode(ucfg->channels[i]);
  }

  return ret;
}

Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType User, ComM_ModeType *ComMode) {
  Std_ReturnType ret = E_OK;
  ComM_UserContextType *uctx;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x07, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(User < COMM_CONFIG->numOfUsers, 0x07, COMM_E_WRONG_PARAMETERS, return E_NOT_OK);
  DET_VALIDATE(NULL != ComMode, 0x07, COMM_E_PARAM_POINTER, return E_NOT_OK);

  uctx = &COMM_CONFIG->userContexts[User];
  *ComMode = uctx->requestMode;

  return ret;
}

Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType *ComMode) {
  Std_ReturnType ret = E_OK;
  uint8_t i;
  const ComM_UserConfigType *ucfg;
  ComM_ModeType md;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x08, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(User < COMM_CONFIG->numOfUsers, 0x08, COMM_E_WRONG_PARAMETERS, return E_NOT_OK);
  DET_VALIDATE(NULL != ComMode, 0x08, COMM_E_PARAM_POINTER, return E_NOT_OK);

  *ComMode = COMM_FULL_COMMUNICATION;

  ucfg = &COMM_CONFIG->userConfigs[User];
  for (i = 0; i < ucfg->numOfChannels; i++) { /* @SWS_ComM_00176 */
    md = ComM_GetChannelComMode(ucfg->channels[i]);
    if (md < *ComMode) {
      *ComMode = md;
    }
  }

  return ret;
}

void ComM_MainFunction(void) {
  uint8_t i;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x60, COMM_E_UNINIT, return);
  for (i = 0; i < COMM_CONFIG->numOfChannels; i++) {
    ComM_MainFunction_Channel((NetworkHandleType)i);
  }
}

Std_ReturnType ComM_GetState(NetworkHandleType Channel, ComM_StateType *State) {
  Std_ReturnType ret = E_OK;
  ComM_ChannelContextType *cctx;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x34, COMM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(Channel < COMM_CONFIG->numOfChannels, 0x34, COMM_E_WRONG_PARAMETERS,
               return E_NOT_OK);
  DET_VALIDATE(NULL != State, 0x34, COMM_E_PARAM_POINTER, return E_NOT_OK);

  cctx = &COMM_CONFIG->channelContexts[Channel];

  *State = cctx->state;

  return ret;
}

void ComM_BusSM_ModeIndication(NetworkHandleType Channel, ComM_ModeType ComMode) {
  DET_VALIDATE(NULL != COMM_CONFIG, 0x33, COMM_E_UNINIT, return);
  DET_VALIDATE(Channel < COMM_CONFIG->numOfChannels, 0x33, COMM_E_WRONG_PARAMETERS, return);

  (void)Channel;
  (void)ComMode;
}

void ComM_Nm_NetworkStartIndication(NetworkHandleType Channel) {
  ComM_ChannelContextType *cctx;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x15, COMM_E_UNINIT, return);
  DET_VALIDATE(Channel < COMM_CONFIG->numOfChannels, 0x15, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Network Start\n", Channel));
  cctx = &COMM_CONFIG->channelContexts[Channel];
  commEnterCritical();
  cctx->nmFlag |= COMM_NM_FLAG_NETWORK_START;
  commExitCritical();
}

void ComM_Nm_NetworkMode(NetworkHandleType Channel) {
  ComM_ChannelContextType *cctx;
  const ComM_ChannelConfigType *ccfg;
  uint8_t i;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x33, COMM_E_UNINIT, return);
  DET_VALIDATE(Channel < COMM_CONFIG->numOfChannels, 0x33, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Network Mode\n", Channel));
  ccfg = &COMM_CONFIG->channelConfigs[Channel];
  cctx = &COMM_CONFIG->channelContexts[Channel];
  commEnterCritical();
  cctx->nmFlag |= COMM_NM_FLAG_NETWORK_MODE;
  commExitCritical();

  for (i = 0; i < ccfg->numOfComIpduGroups; i++) {
    Com_IpduGroupStart(ccfg->ComIpduGroupIds[i], TRUE);
  }
}

void ComM_Nm_PrepareBusSleepMode(NetworkHandleType Channel) {
  ComM_ChannelContextType *cctx;
  const ComM_ChannelConfigType *ccfg;
  uint8_t i;

  DET_VALIDATE(NULL != COMM_CONFIG, 0x19, COMM_E_UNINIT, return);
  DET_VALIDATE(Channel < COMM_CONFIG->numOfChannels, 0x19, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Prepare Bus Sleep\n", Channel));
  ccfg = &COMM_CONFIG->channelConfigs[Channel];
  cctx = &COMM_CONFIG->channelContexts[Channel];
  commEnterCritical();
  cctx->nmFlag |= COMM_NM_FLAG_PREPARE_BUS_SLEEP;
  commExitCritical();

  for (i = 0; i < ccfg->numOfComIpduGroups; i++) {
    Com_IpduGroupStop(ccfg->ComIpduGroupIds[i]);
  }
}

void ComM_Nm_BusSleepMode(NetworkHandleType Channel) {
  ComM_ChannelContextType *cctx;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x1A, COMM_E_UNINIT, return);
  DET_VALIDATE(Channel < COMM_CONFIG->numOfChannels, 0x1A, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] NM Bus Sleep\n", Channel));
  cctx = &COMM_CONFIG->channelContexts[Channel];
  commEnterCritical();
  cctx->nmFlag |= COMM_NM_FLAG_BUS_SLEEP;
  commExitCritical();
}

void ComM_CommunicationAllowed(NetworkHandleType Channel, boolean Allowed) {
  ComM_ChannelContextType *cctx;
  DET_VALIDATE(NULL != COMM_CONFIG, 0x35, COMM_E_UNINIT, return);
  DET_VALIDATE(Channel < COMM_CONFIG->numOfChannels, 0x35, COMM_E_WRONG_PARAMETERS, return);

  ASLOG(COMM, ("[%d] Communication Allowed %s\n", Channel, Allowed ? "true" : "false"));
  cctx = &COMM_CONFIG->channelContexts[Channel];
  commEnterCritical();
  cctx->bCommunicationAllowed = Allowed;
  commExitCritical();
}

void ComM_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x10, COMM_E_PARAM_POINTER, return);

  versionInfo->vendorID = STD_VENDOR_ID_AS;
  versionInfo->moduleID = MODULE_ID_COMM;
  versionInfo->sw_major_version = 4;
  versionInfo->sw_minor_version = 0;
  versionInfo->sw_patch_version = 0;
}

