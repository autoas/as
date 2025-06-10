/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: nm253
 */
#ifndef _OSEK_NM_PRIV_H
#define _OSEK_NM_PRIV_H
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
typedef enum
{
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
  uint16_t _TTx;
  uint16_t _TTyp;
  uint16_t _TMax;
  uint16_t _TError;
  uint16_t _TWbs;
  uint16_t NodeMask;
  OsekNm_NodeIdType NodeId;
  uint8_t rx_limit;
  uint8_t tx_limit;
  PduIdType txPduId;
} OsekNm_ChannelConfigType;

typedef uint8_t OsekNm_NetworkStatusType;

#ifdef OSEKNM_USE_STD_TIMER
typedef Std_TimerType OsekNm_TimerType;
#else
typedef uint16_t OsekNm_TimerType;
#endif

typedef struct {
  OsekNm_StateType nmState;
  OsekNm_PduType nmTxPdu;
  struct {
    OsekNm_TimerType _TTx;
    OsekNm_TimerType _TTyp;
    OsekNm_TimerType _TMax;
    OsekNm_TimerType _TError;
    OsekNm_TimerType _TWbs;
  } Alarm;
  struct {
    OsekNm_NetworkStatusType SMask;
    OsekNm_NetworkStatusType NetworkStatus;
  } nmStatus;
  uint8_t nmMerker;
  uint8_t nmRxCount;
  uint8_t nmTxCount;
  OsekNm_ModeNameType requestMode;
  boolean bRequestOn;
  boolean bWakeupSignal;
} OsekNm_ChannelContextType;

struct OsekNm_Config_s {
  const OsekNm_ChannelConfigType *channelConfigs;
  OsekNm_ChannelContextType *channelContexts;
  uint8_t numOfChannels;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _OSEK_NM_PRIV_H */
