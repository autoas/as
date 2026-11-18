/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: nm253
 */
#ifndef OSEKNM_PRIV_H
#define OSEKNM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "OsekNm.h"
#ifdef _WIN32
#include "Std_Timer.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define OSEKNM_STATUS_ACTIVE ((OsekNm_NetworkStatusType)0x01)
#define OSEKNM_STATUS_BUS_SLEEP ((OsekNm_NetworkStatusType)0x02)
#define OSEKNM_STATUS_CONFIGURATION_STABLE ((OsekNm_NetworkStatusType)0x04)

#define OSEKNM_MERKER_STABLE ((uint8_t)0x01)
#define OSEKNM_MERKER_LIMPHOME ((uint8_t)0x02)
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  OSEKNM_STATE_OFF = 0,
  OSEKNM_STATE_NORMAL,
  OSEKNM_STATE_NORMAL_PREPARE_SLEEP,
  OSEKNM_STATE_WAIT_BUS_SLEEP_NORMAL,
  OSEKNM_STATE_BUS_SLEEP,
  OSEKNM_STATE_LIMPHOME,
  OSEKNM_STATE_LIMPHOME_PREPARE_SLEEP,
  OSEKNM_STATE_WAIT_BUS_SLEEP_LIMPHOME,
  OSEKNM_STATE_ON
} OsekNm_StateType;

typedef struct {
  uint16_t TTx;
  uint16_t TTyp;
  uint16_t TMax;
  uint16_t TError;
  uint16_t TWbs;
  uint16_t NodeMask;
  OsekNm_NodeIdType NodeId;
  uint8_t RxLimit;
  uint8_t TxLimit;
  PduIdType TxPduId;
} OsekNm_ChannelConfigType;

typedef uint8_t OsekNm_NetworkStatusType;

#ifdef OSEKNM_USE_STD_TIMER
typedef Std_TimerType OsekNm_TimerType;
#else
typedef uint16_t OsekNm_TimerType;
#endif

typedef struct {
  OsekNm_StateType NmState;
  OsekNm_PduType NmTxPdu;
  struct {
    OsekNm_TimerType TTx;
    OsekNm_TimerType TTyp;
    OsekNm_TimerType TMax;
    OsekNm_TimerType TError;
    OsekNm_TimerType TWbs;
  } Alarm;
  struct {
    OsekNm_NetworkStatusType SMask;
    OsekNm_NetworkStatusType NetworkStatus;
  } Status;
  uint8_t NmMerker;
  uint8_t NmRxCount;
  uint8_t NmTxCount;
  OsekNm_ModeNameType RequestMode;
  boolean RequestOn;
  boolean WakeupSignal;
} OsekNm_ChannelContextType;

struct OsekNm_Config_s {
  const OsekNm_ChannelConfigType *ChannelConfigs;
  OsekNm_ChannelContextType *ChannelContexts;
  uint8_t NumOfChannels;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* OSEKNM_PRIV_H */
