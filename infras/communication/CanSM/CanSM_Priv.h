/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN State Manager AUTOSAR CP R23-11
 */
#ifndef CAN_SM_PRIV_H
#define CAN_SM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Rte_ComM_Type.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_CANSM

#define CANSM_BUSOFF_COUNTER_MAX ((uint8_t)0xFF)

#define CANSM_BSM_S_NOT_INITIALIZED ((CanSM_BusStateType)0)
#define CANSM_BSM_S_PRE_NOCOM ((CanSM_BusStateType)1)
#define CANSM_BSM_WUVALIDATION ((CanSM_BusStateType)2)
#define CANSM_BSM_S_PRE_FULLCOM ((CanSM_BusStateType)3)
#define CANSM_BSM_S_FULLCOM ((CanSM_BusStateType)4)
#define CANSM_BSM_S_SILENTCOM ((CanSM_BusStateType)5)
#define CANSM_BSM_S_CHANGE_BAUDRATE ((CanSM_BusStateType)6)
#define CANSM_BSM_S_SILENTCOM_BOR ((CanSM_BusStateType)7)
#define CANSM_BSM_S_NOCOM ((CanSM_BusStateType)8)

#define CANSM_BSM_S_ENTRY_POINT ((CanSM_BusSubStateType)0)

/* sub state for CANSM_BSM_WUVALIDATION */
#define CANSM_BSM_S_TRCV_NORMAL ((CanSM_BusSubStateType)1)
#define CANSM_BSM_S_CC_STOPPED ((CanSM_BusSubStateType)2)
#define CANSM_BSM_S_CC_STARTED ((CanSM_BusSubStateType)3)
#define CANSM_BSM_WAIT_WUVALIDATION_LEAVE ((CanSM_BusSubStateType)4)

/* sub state for CANSM_BSM_S_PRE_FULLCOM */
/* S_TRCV_NORMAL -> S_CC_STOPPED -> S_CC_STARTED -> ExitPoint to FULLCOM */

/* sub state for CANSM_BSM_S_FULLCOM */
#define CANSM_BSM_S_BUS_OFF_CHECK ((CanSM_BusSubStateType)5)
#define CANSM_BSM_S_RESTART_CC ((CanSM_BusSubStateType)6)
#define CANSM_BSM_S_TX_OFF ((CanSM_BusSubStateType)7)
#define CANSM_BSM_S_NO_BUS_OFF ((CanSM_BusSubStateType)8)
#define CANSM_BSM_S_TX_TIMEOUT_EXCEPTION ((CanSM_BusSubStateType)9)

/* sub state for CANSM_BSM_S_PRE_NOCOM */
/* S_CC_STOPPED -> S_CC_SLEEP ->  -> S_TRCV_STANDBY -> ExitPoint */
#define CANSM_BSM_S_CC_SLEEP ((CanSM_BusSubStateType)10)
#define CANSM_BSM_S_TRCV_STANDBY ((CanSM_BusSubStateType)11)

#define CANSM_BSM_S_EXIT_POINT ((CanSM_BusSubStateType)0xFF)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_CanSM_00637 */
typedef void (*CanSM_UserGetBusOffDelayFncType)(NetworkHandleType network, uint8 *delayCyclesPtr);

typedef uint8_t CanSM_BusStateType;

typedef uint8_t CanSM_BusSubStateType;

typedef struct {
  uint16_t BusOffTimer;
  uint16_t BusOffTxEnsuredTimer;
  uint16_t TxRecoveryTimer;
  uint8_t BusOffCounter;
  ComM_ModeType requestedMode;
  CanSM_BusStateType BusState;
  CanSM_BusSubStateType BusSubState;
  boolean bBusOff;
  boolean bTxTimeout;
  boolean bStartWakeup;
  boolean bStopWakeup;
} CanSM_ChannelContextType;

typedef struct {
  CanSM_UserGetBusOffDelayFncType UserGetBusOffDelayFnc; /* @ECUC_CanSM_00346 */
  uint16_t BorTimeL1;        /* @ECUC_CanSM_00128, short recovery time */
  uint16_t BorTimeL2;        /* @ECUC_CanSM_00129, long recovery time */
  uint16_t BorTimeTxEnsured; /* @ECUC_CanSM_00130 */
  uint16_t TxRecoveryTime;   /* User Defined */
  uint8_t BorCounterL1ToL2;  /* @ECUC_CanSM_00131 */
  uint8_t ControllerId;
  uint8_t TransceiverId;
#ifdef USE_COMM
  NetworkHandleType ComMHandle; /* @ECUC_CanSM_00161 */
#endif
  boolean BorTxConfirmationPolling; /* @ECUC_CanSM_00339 */
  boolean CanTrcvPnEnabled;         /* @ECUC_CanTrcv_00172 */
} CanSM_NetworkConfigType;          /* @ECUC_CanSM_00338 */

struct CanSM_Config_s {
  const CanSM_NetworkConfigType *networks;
  CanSM_ChannelContextType *contexts;
  uint8_t numOfNetworks;
  uint8_t ModeRequestRepetitionMax; /* @ECUC_CanSM_00335 */
  uint16_t RequestRepetitionTime;   /* @ECUC_CanSM_00336 */
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CAN_SM_PRIV_H */
