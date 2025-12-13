/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN State Manager AUTOSAR CP R23-11
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanSM.h"
#include "CanSM_Cfg.h"
#include "CanSM_Priv.h"
#include "CanSM_CanIf.h"
#include "CanIf.h"
#include "ComM.h"
#include "Std_Debug.h"

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#ifdef CANSM_USE_PB_CONFIG
#define CANSM_CONFIG cansmConfig
#else
#define CANSM_CONFIG (&CanSM_Config)
#endif

#define AS_LOG_CANSM 0
#define AS_LOG_CANSMW 2
#define AS_LOG_CANSME 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const CanSM_ConfigType CanSM_Config;

static void CanSM_PreNoCom(NetworkHandleType network);
static void CanSM_WakeupValidation(NetworkHandleType network);
static void CanSM_PreFullCom(NetworkHandleType network);
static void CanSM_FullCom(NetworkHandleType network);
static void CanSM_SilentCom(NetworkHandleType network);
static void CanSM_SilentComBOR(NetworkHandleType network);
/* ================================ [ DATAS     ] ============================================== */
static boolean bCanSmEcuPassive = FALSE;
#ifdef CANSM_USE_PB_CONFIG
static const CanSM_ConfigType *cansmConfig = NULL;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void CanSM_PreNoCom_DeinitPnSupported(NetworkHandleType network) {
  (void)network;
  /* TODO: */
}

static void CanSM_PreNoCom_DeinitPnNotSupported(NetworkHandleType network) {
  Std_ReturnType ret;
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  if (CANSM_BSM_S_ENTRY_POINT == context->BusSubState) {
    context->BusSubState = CANSM_BSM_S_CC_STOPPED;
  }

  if (CANSM_BSM_S_CC_STOPPED == context->BusSubState) { /* @SWS_CanSM_00464 */
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STOPPED);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_CC_SLEEP;
    } else {
      ASLOG(CANSME, ("[%d] fail to stop CC\n", network));
    }
  }

  if (CANSM_BSM_S_CC_SLEEP == context->BusSubState) { /* @SWS_CanSM_00468 */
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_SLEEP);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_TRCV_NORMAL;
    } else {
      ASLOG(CANSME, ("[%d] fail to sleep CC\n", network));
    }
  }

  if (CANSM_BSM_S_TRCV_NORMAL == context->BusSubState) { /* @SWS_CanSM_00472 */
    ret = CanIf_SetTrcvMode(config->TransceiverId, CANTRCV_TRCVMODE_NORMAL);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_TRCV_STANDBY;
    } else {
      ASLOG(CANSME, ("[%d] fail to set Trcv normal\n", network));
    }
  }

  if (CANSM_BSM_S_TRCV_STANDBY == context->BusSubState) { /* @SWS_CanSM_00476 */
    ret = CanIf_SetTrcvMode(config->TransceiverId, CANTRCV_TRCVMODE_STANDBY);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_EXIT_POINT;
    } else {
      ASLOG(CANSME, ("[%d] fail to set Trcv Standby\n", network));
    }
  }
}

static void CanSM_PreNoCom(NetworkHandleType network) {
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  if (TRUE == config->CanTrcvPnEnabled) {
    CanSM_PreNoCom_DeinitPnSupported(network);
  } else {
    CanSM_PreNoCom_DeinitPnNotSupported(network);
  }

  if (CANSM_BSM_S_EXIT_POINT == context->BusSubState) {
    if (TRUE == context->bStartWakeup) {
      context->bStartWakeup = FALSE;
      context->BusState = CANSM_BSM_WUVALIDATION;
      context->BusSubState = CANSM_BSM_S_TRCV_NORMAL;
      ASLOG(CANSM, ("[%d] Goto Wakeup Validation\n", network));
      CanSM_WakeupValidation(network);
    } else {
      ASLOG(CANSM, ("[%d] Goto No Com\n", network));
      context->BusState = CANSM_BSM_S_NOCOM;
#ifdef USE_COMM /* @SWS_CanSM_00651 */
      ComM_BusSM_ModeIndication(config->ComMHandle, COMM_NO_COMMUNICATION);
#endif
    }
  }
}

static void CanSM_WakeupValidation(NetworkHandleType network) {
  Std_ReturnType ret;
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  if (CANSM_BSM_S_TRCV_NORMAL == context->BusSubState) { /* @SWS_CanSM_00623 */
    ret = CanIf_SetTrcvMode(config->TransceiverId, CANTRCV_TRCVMODE_NORMAL);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_CC_STOPPED;
    } else {
      ASLOG(CANSME, ("[%d]Fail to set Trcv normal\n", network));
    }
  }

  if (CANSM_BSM_S_CC_STOPPED == context->BusSubState) { /* @SWS_CanSM_00627 */
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STOPPED);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_CC_STARTED;
    } else {
      ASLOG(CANSME, ("[%d]Fail to stop CC\n", network));
    }
  }

  if (CANSM_BSM_S_CC_STARTED == context->BusSubState) { /* @SWS_CanSM_00631 */
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STARTED);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_WAIT_WUVALIDATION_LEAVE;
    } else {
      ASLOG(CANSME, ("[%d]Fail to stop CC\n", network));
    }
  }

  if (CANSM_BSM_WAIT_WUVALIDATION_LEAVE == context->BusSubState) {
    if ((COMM_FULL_COMMUNICATION == context->requestedMode) ||
        (COMM_FULL_COMMUNICATION_WITH_WAKEUP_REQUEST == context->requestedMode)) {
      /* T_FULL_COM_MODE_REQUEST */
      ASLOG(CANSM, ("[%d] Goto Pre Full Com\n", network));
      context->BusState = CANSM_BSM_S_PRE_FULLCOM;
      context->BusSubState = CANSM_BSM_S_EXIT_POINT; /* direct goto exit point */
      CanSM_PreFullCom(network);
    } else if (TRUE == context->bStopWakeup) {
      context->bStopWakeup = FALSE;
      ASLOG(CANSM, ("[%d] Goto Pre No Com\n", network));
      context->BusState = CANSM_BSM_S_PRE_NOCOM;
      context->BusSubState = CANSM_BSM_S_ENTRY_POINT;
    } else {
      /* do nothing */
    }
  }
}

static void CanSM_PreFullCom(NetworkHandleType network) {
  Std_ReturnType ret;
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  if (CANSM_BSM_S_TRCV_NORMAL == context->BusSubState) { /* @SWS_CanSM_00483 */
    ret = CanIf_SetTrcvMode(config->TransceiverId, CANTRCV_TRCVMODE_NORMAL);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_CC_STOPPED;
    } else {
      ASLOG(CANSME, ("[%d]Fail to set Trcv normal\n", network));
    }
  }

  if (CANSM_BSM_S_CC_STOPPED == context->BusSubState) { /* @SWS_CanSM_00487 */
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STOPPED);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_CC_STARTED;
    } else {
      ASLOG(CANSME, ("[%d]Fail to stop CC\n", network));
    }
  }

  if (CANSM_BSM_S_CC_STARTED == context->BusSubState) { /* @SWS_CanSM_00491 */
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STARTED);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_EXIT_POINT;
    } else {
      ASLOG(CANSME, ("[%d]Fail to start CC\n", network));
    }
  }

  if (CANSM_BSM_S_EXIT_POINT == context->BusSubState) {
    if (FALSE == bCanSmEcuPassive) { /* @SWS_CanSM_00539 */
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_ONLINE);
    } else { /* @SWS_CanSM_00647 */
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_TX_OFFLINE_ACTIVE);
    }
    if (E_OK == ret) {
      ASLOG(CANSM, ("[%d] Goto Full Com\n", network));
      context->BusState = CANSM_BSM_S_FULLCOM;
      context->BusSubState = CANSM_BSM_S_BUS_OFF_CHECK;
      context->BusOffTxEnsuredTimer = config->BorTimeTxEnsured;
#ifdef USE_COMM /* @SWS_CanSM_00435 */
      ComM_BusSM_ModeIndication(config->ComMHandle, COMM_FULL_COMMUNICATION);
#endif
    } else {
      ASLOG(CANSME, ("[%d]Fail to E_FULL_COM\n", network));
    }
  }
}

static void CanSM_FullCom(NetworkHandleType network) {
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;
  Std_ReturnType ret;

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  if ((CANSM_BSM_S_BUS_OFF_CHECK == context->BusSubState) ||
      (CANSM_BSM_S_NO_BUS_OFF == context->BusSubState)) {
    if (TRUE == context->bBusOff) {
      context->bBusOff = FALSE;
      if (context->BusOffCounter < CANSM_BUSOFF_COUNTER_MAX) {
        context->BusOffCounter++;
      }
      ASLOG(CANSM, ("[%d]BusOff: %d\n", network, context->BusOffCounter));
      if (context->BusOffCounter <= config->BorCounterL1ToL2) {
        context->BusOffTimer = config->BorTimeL1;
      } else {
        context->BusOffTimer = config->BorTimeL2;
      }
      /* E_BUS_OFF */
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_OFFLINE);
      if (E_OK == ret) {
        ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STOPPED);
        if (E_OK != ret) {
          ASLOG(CANSME, ("[%d]Fail to stop CC\n", network));
        }
      } else {
        ASLOG(CANSME, ("[%d]Fail to set offline\n", network));
      }
      /* anyway goto restart CC mode */
      context->BusSubState = CANSM_BSM_S_RESTART_CC;
    }
  }

  if (CANSM_BSM_S_BUS_OFF_CHECK == context->BusSubState) {
    if (context->BusOffTxEnsuredTimer > 0u) {
      context->BusOffTxEnsuredTimer--;
      if (0u == context->BusOffTxEnsuredTimer) { /* @SWS_CanSM_00496 */
        context->BusOffCounter = 0;
        context->BusSubState = CANSM_BSM_S_NO_BUS_OFF;
        ASLOG(CANSM, ("[%d] No BusOff\n", network));
      }
    }
  }

  if (CANSM_BSM_S_RESTART_CC == context->BusSubState) { /* @SWS_CanSM_00509 */
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STARTED);
    if (E_OK == ret) {
      ASLOG(CANSM, ("[%d] Restart CC\n", network));
      context->BusSubState = CANSM_BSM_S_TX_OFF;
    } else {
      ASLOG(CANSME, ("[%d] Fail to start CC\n", network));
    }
  }

  if (CANSM_BSM_S_TX_OFF == context->BusSubState) {
    if (context->BusOffTimer > 0u) {
      context->BusOffTimer--;
    }
    if (0u == context->BusOffTimer) {
      if (FALSE == bCanSmEcuPassive) { /* @SWS_CanSM_00516 */
        ret = CanIf_SetPduMode(config->ControllerId, CANIF_ONLINE);
      } else { /* SWS_CanSM_00648 */
        ret = CanIf_SetPduMode(config->ControllerId, CANIF_TX_OFFLINE_ACTIVE);
      }
      if (E_OK == ret) {
        ASLOG(CANSM, ("[%d] BusOff Recovery\n", network));
        context->BusOffTxEnsuredTimer = config->BorTimeTxEnsured;
        context->BusSubState = CANSM_BSM_S_BUS_OFF_CHECK;
      } else {
        ASLOG(CANSME, ("[%d]Fail to E_TX_ON\n", network));
      }
    }
  }

  if (CANSM_BSM_S_TX_TIMEOUT_EXCEPTION == context->BusSubState) {
    if (context->TxRecoveryTimer > 0u) {
      context->TxRecoveryTimer--;
    }
    if (0u == context->TxRecoveryTimer) {
      /* @SWS_CanSM_00655 */
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_ONLINE);
      if (E_OK == ret) {
        ASLOG(CANSM, ("[%d] Tx Recovery\n", network));
        context->BusSubState = CANSM_BSM_S_NO_BUS_OFF;
      } else {
        ASLOG(CANSME, ("[%d]Fail to TxTimeout Exit\n", network));
      }
    }
  }

  if (CANSM_BSM_S_NO_BUS_OFF == context->BusSubState) {
    if ((COMM_SILENT_COMMUNICATION == context->requestedMode) ||
        (COMM_NO_COMMUNICATION == context->requestedMode)) {
      /* @SWS_CanSM_00541 */
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_TX_OFFLINE);
      if (E_OK == ret) {
        ASLOG(CANSM, ("[%d] Goto Silent Com\n", network));
        context->BusState = CANSM_BSM_S_SILENTCOM;
        context->BusSubState = CANSM_BSM_S_ENTRY_POINT;
#ifdef USE_COMM /* @SWS_CanSM_00538 */
        ComM_BusSM_ModeIndication(config->ComMHandle, COMM_SILENT_COMMUNICATION);
#endif
      } else {
        ASLOG(CANSME, ("[%d]Fail to E_FULL_TO_SILENT_COM\n", network));
      }
    } else if (TRUE == context->bTxTimeout) {
      context->bTxTimeout = FALSE;
      ASLOG(CANSM, ("[%d] Tx Timeout\n", network));
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_OFFLINE);
      if (E_OK == ret) {
        ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STOPPED);
        if (E_OK != ret) {
          ASLOG(CANSME, ("[%d] Fail to stop CC\n", network));
        } else {
          ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STARTED);
          if (E_OK == ret) {
            context->BusSubState = CANSM_BSM_S_TX_TIMEOUT_EXCEPTION;
            context->TxRecoveryTimer = config->TxRecoveryTime;
          } else {
            ASLOG(CANSME, ("[%d] Fail to start CC\n", network));
          }
        }
      } else {
        ASLOG(CANSME, ("[%d] Fail to set offline\n", network));
      }
    } else {
      /* do nothing */
    }
  }
}

static void CanSM_SilentCom(NetworkHandleType network) {
  Std_ReturnType ret;
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  if ((COMM_FULL_COMMUNICATION == context->requestedMode) ||
      (COMM_FULL_COMMUNICATION_WITH_WAKEUP_REQUEST == context->requestedMode)) {
    /* @SWS_CanSM_00550: the same as E_FULL_COM */
    if (FALSE == bCanSmEcuPassive) { /* @SWS_CanSM_00539 */
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_ONLINE);
    } else { /* @SWS_CanSM_00647 */
      ret = CanIf_SetPduMode(config->ControllerId, CANIF_TX_OFFLINE_ACTIVE);
    }
    if (E_OK == ret) {
      ASLOG(CANSM, ("[%d] Goto Full Com\n", network));
      context->BusState = CANSM_BSM_S_FULLCOM;
      context->BusSubState = CANSM_BSM_S_BUS_OFF_CHECK;
      context->BusOffTxEnsuredTimer = config->BorTimeTxEnsured;
#ifdef USE_COMM /* @SWS_CanSM_00550 */
      ComM_BusSM_ModeIndication(config->ComMHandle, COMM_FULL_COMMUNICATION);
#endif
    } else {
      ASLOG(CANSME, ("[%d]Fail to E_FULL_COM\n", network));
    }
  } else if (TRUE == context->bBusOff) {
    ASLOG(CANSM, ("[%d] Goto Full Com\n", network));
    context->bBusOff = FALSE;
    context->BusState = CANSM_BSM_S_SILENTCOM_BOR;
    context->BusSubState = CANSM_BSM_S_RESTART_CC;
  } else if (COMM_NO_COMMUNICATION == context->requestedMode) {
    ASLOG(CANSM, ("[%d] Goto Pre No Com\n", network));
    context->BusState = CANSM_BSM_S_PRE_NOCOM;
    context->BusSubState = CANSM_BSM_S_ENTRY_POINT;
  } else {
    /* do nothing */
  }
}

static void CanSM_SilentComBOR(NetworkHandleType network) {
  Std_ReturnType ret;
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];

  if (CANSM_BSM_S_RESTART_CC == context->BusSubState) {
    ret = CanIf_SetControllerMode(config->ControllerId, CAN_CS_STARTED);
    if (E_OK == ret) {
      context->BusSubState = CANSM_BSM_S_EXIT_POINT;
    }
  }

  if (CANSM_BSM_S_EXIT_POINT == context->BusSubState) {
    if (COMM_NO_COMMUNICATION == context->requestedMode) {
      ASLOG(CANSM, ("[%d] Goto Pre No Com\n", network));
      context->BusState = CANSM_BSM_S_PRE_NOCOM;
      context->BusSubState = CANSM_BSM_S_ENTRY_POINT;
    } else {
      ASLOG(CANSM, ("[%d] Goto Silent Com\n", network));
      context->BusState = CANSM_BSM_S_SILENTCOM;
      context->BusSubState = CANSM_BSM_S_ENTRY_POINT;
    }
  }
}

static void CanSM_NoCom(NetworkHandleType network) {
  CanSM_ChannelContextType *context;

  context = &CANSM_CONFIG->contexts[network];
  if (TRUE == context->bStartWakeup) {
    context->bStartWakeup = FALSE;
    context->BusState = CANSM_BSM_WUVALIDATION;
    context->BusSubState = CANSM_BSM_S_TRCV_NORMAL;
    ASLOG(CANSM, ("[%d] Goto Wakeup Validation\n", network));
    CanSM_WakeupValidation(network);
  } else if (COMM_FULL_COMMUNICATION == context->requestedMode) {
    context->BusState = CANSM_BSM_S_PRE_FULLCOM;
    context->BusSubState = CANSM_BSM_S_TRCV_NORMAL;
    ASLOG(CANSM, ("[%d] Goto Pre Full Com\n", network));
    CanSM_PreFullCom(network);
  } else {
    /* do nothing */
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void CanSM_Init(const CanSM_ConfigType *ConfigPtr) {
  uint8_t i;
  CanSM_ChannelContextType *context;

#ifdef CANSM_USE_PB_CONFIG
  if (NULL != ConfigPtr) {
    CANSM_CONFIG = ConfigPtr;
  } else {
    CANSM_CONFIG = &CanSM_Config;
  }
#else
  (void)ConfigPtr;
#endif

  for (i = 0; i < CANSM_CONFIG->numOfNetworks; i++) {
    context = &CANSM_CONFIG->contexts[i];
    context->requestedMode = COMM_NO_COMMUNICATION;
    context->BusOffTimer = 0;
    context->BusOffCounter = 0;
    context->BusOffTxEnsuredTimer = 0;
    context->BusState = CANSM_BSM_S_PRE_NOCOM;
    context->BusSubState = CANSM_BSM_S_ENTRY_POINT;
    bCanSmEcuPassive = FALSE;
  }
}

void CanSM_DeInit(void) {
  uint8_t i;
  CanSM_ChannelContextType *context;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x14, CANSM_E_UNINIT, return);

  for (i = 0; i < CANSM_CONFIG->numOfNetworks; i++) {
    context = &CANSM_CONFIG->contexts[i];
    /* @SWS_CanSM_00660 */
    DET_VALIDATE(CANSM_BSM_S_NOCOM != context->BusState, 0x14, CANSM_E_NOT_IN_NO_COM, (void)0);
    context->BusState = CANSM_BSM_S_NOT_INITIALIZED;
  }
}

Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType ComM_Mode) {
  Std_ReturnType ret = E_OK;
  CanSM_ChannelContextType *context;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x02, CANSM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < CANSM_CONFIG->numOfNetworks, 0x02, CANSM_E_INVALID_NETWORK_HANDLE,
               return E_NOT_OK); /* @SWS_CanSM_00183 */

  context = &CANSM_CONFIG->contexts[network];
  context->requestedMode = ComM_Mode;

  return ret;
}

Std_ReturnType CanSM_SetEcuPassive(boolean CanSM_Passive) {
  Std_ReturnType ret = E_OK;
  bCanSmEcuPassive = CanSM_Passive;
  return ret;
}

Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType *ComM_ModePtr) {
  Std_ReturnType ret = E_OK;
  CanSM_ChannelContextType *context;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x03, CANSM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < CANSM_CONFIG->numOfNetworks, 0x03, CANSM_E_INVALID_NETWORK_HANDLE,
               return E_NOT_OK); /* @SWS_CanSM_00187 */

  DET_VALIDATE(NULL != ComM_ModePtr, 0x03, CANSM_E_PARAM_POINTER,
               return E_NOT_OK); /* @SWS_CanSM_00360 */

  context = &CANSM_CONFIG->contexts[network];
  switch (context->BusState) {
  case CANSM_BSM_S_FULLCOM:
    if ((CANSM_BSM_S_BUS_OFF_CHECK == context->BusSubState) ||
        (CANSM_BSM_S_NO_BUS_OFF == context->BusSubState)) {
      *ComM_ModePtr = COMM_FULL_COMMUNICATION;
    } else { /* @SWS_CanSM_00521 */
      *ComM_ModePtr = COMM_SILENT_COMMUNICATION;
    }
    break;
  case CANSM_BSM_S_SILENTCOM:
    *ComM_ModePtr = COMM_SILENT_COMMUNICATION;
    break;
  default:
    *ComM_ModePtr = COMM_NO_COMMUNICATION;
    break;
  }

  return ret;
}

Std_ReturnType CanSM_StartWakeupSource(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;
  CanSM_ChannelContextType *context;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x11, CANSM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < CANSM_CONFIG->numOfNetworks, 0x11, CANSM_E_INVALID_NETWORK_HANDLE,
               return E_NOT_OK); /* @SWS_CanSM_00613 */

  context = &CANSM_CONFIG->contexts[network];
  context->bStartWakeup = TRUE;

  return ret;
}

Std_ReturnType CanSM_StopWakeupSource(NetworkHandleType network) {
  Std_ReturnType ret = E_OK;
  CanSM_ChannelContextType *context;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x12, CANSM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < CANSM_CONFIG->numOfNetworks, 0x12, CANSM_E_INVALID_NETWORK_HANDLE,
               return E_NOT_OK); /* @SWS_CanSM_00621 */

  context = &CANSM_CONFIG->contexts[network];
  context->bStopWakeup = TRUE;

  return ret;
}

void CanSM_ControllerBusOff(uint8_t ControllerId) {
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x14, CANSM_E_UNINIT, return);
  DET_VALIDATE(ControllerId < CANSM_CONFIG->numOfNetworks, 0x14, CANSM_E_PARAM_CONTROLLER,
               return); /* @SWS_CanSM_00189 */

  /* For this simple implementation: ControllerId == network */
  context = &CANSM_CONFIG->contexts[ControllerId];
  config = &CANSM_CONFIG->networks[ControllerId];

  context->bBusOff = TRUE;
  /* immediately switch TX offline */
  (void)CanIf_SetPduMode(config->ControllerId, CANIF_TX_OFFLINE);
}

void CanSM_TxTimeoutException(NetworkHandleType Channel) {
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x0B, CANSM_E_UNINIT, return);
  DET_VALIDATE(Channel < CANSM_CONFIG->numOfNetworks, 0x0B, CANSM_E_INVALID_NETWORK_HANDLE,
               return); /* @SWS_CanSM_00412 */

  /* For this simple implementation: Channel == network */
  context = &CANSM_CONFIG->contexts[Channel];
  config = &CANSM_CONFIG->networks[Channel];

  context->bTxTimeout = TRUE;
  /* immediately switch TX offline */
  (void)CanIf_SetPduMode(config->ControllerId, CANIF_TX_OFFLINE);
}

void CanSM_MainFunction(void) {
  uint8_t i;
  CanSM_ChannelContextType *context;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0x05, CANSM_E_UNINIT, return);
  for (i = 0; i < CANSM_CONFIG->numOfNetworks; i++) {
    context = &CANSM_CONFIG->contexts[i];
    switch (context->BusState) {
    case CANSM_BSM_S_PRE_NOCOM:
      CanSM_PreNoCom((NetworkHandleType)i);
      break;
    case CANSM_BSM_WUVALIDATION:
      CanSM_WakeupValidation((NetworkHandleType)i);
      break;
    case CANSM_BSM_S_PRE_FULLCOM:
      CanSM_PreFullCom((NetworkHandleType)i);
      break;
    case CANSM_BSM_S_FULLCOM:
      CanSM_FullCom((NetworkHandleType)i);
      break;
    case CANSM_BSM_S_SILENTCOM:
      CanSM_SilentCom((NetworkHandleType)i);
      break;
    case CANSM_BSM_S_SILENTCOM_BOR:
      CanSM_SilentComBOR((NetworkHandleType)i);
      break;
    case CANSM_BSM_S_NOCOM:
      CanSM_NoCom((NetworkHandleType)i);
      break;
    default: /* do nothing */
      break;
    }
  }
}

Std_ReturnType CanSM_GetBusoff_Substate(NetworkHandleType network,
                                        CanSM_BusOffRecoveryStateType *BORStatePtr) {
  Std_ReturnType ret = E_OK;
  CanSM_ChannelContextType *context;
  const CanSM_NetworkConfigType *config;

  DET_VALIDATE(NULL != CANSM_CONFIG, 0xF1, CANSM_E_UNINIT, return E_NOT_OK);
  DET_VALIDATE(network < CANSM_CONFIG->numOfNetworks, 0xF1, CANSM_E_INVALID_NETWORK_HANDLE,
               return E_NOT_OK);

  context = &CANSM_CONFIG->contexts[network];
  config = &CANSM_CONFIG->networks[network];
  if (CANSM_BSM_S_FULLCOM == context->BusState) {
    switch (context->BusSubState) {
    case CANSM_BSM_S_BUS_OFF_CHECK:
      *BORStatePtr = CANSM_S_BUS_OFF_CHECK;
      break;
    case CANSM_BSM_S_RESTART_CC:
      *BORStatePtr = CANSM_S_RESTART_CC;
      break;
    case CANSM_BSM_S_TX_OFF:
      if (context->BusOffCounter <= config->BorCounterL1ToL2) {
        *BORStatePtr = CANSM_S_BUS_OFF_RECOVERY_L1;
      } else {
        *BORStatePtr = CANSM_S_BUS_OFF_RECOVERY_L2;
      }
      break;
    case CANSM_BSM_S_NO_BUS_OFF:
      *BORStatePtr = CANSM_BOR_IDLE;
      break;
    default:
      ASLOG(CANSME, ("[%d] Fatal as in sub state %d\n", network, context->BusSubState));
      ret = E_NOT_OK;
      break;
    }
  } else {
    ASLOG(CANSME,
          ("[%d] can't provide bus off state when in state %d\n", network, context->BusState));
    ret = E_NOT_OK;
  }

  return ret;
}

void CanSM_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x01, CANSM_E_PARAM_POINTER, return);

  versionInfo->vendorID = STD_VENDOR_ID_AS;
  versionInfo->moduleID = MODULE_ID_CANSM;
  versionInfo->sw_major_version = 4;
  versionInfo->sw_minor_version = 0;
  versionInfo->sw_patch_version = 0;
}
