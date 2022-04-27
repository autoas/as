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
typedef enum
{
  NM_stOff = 0,
  NM_stInit,
  NM_stInitReset,
  NM_stNormal,
  NM_stNormalPrepSleep,
  NM_stTwbsNormal,
  NM_stBusSleep,
  NM_stLimphome,
  NM_stLimphomePrepSleep,
  NM_stTwbsLimphome,
  NM_stOn /* in fact if not Off then ON. */
          // ...  and so on
} NMStateType;

typedef struct {
  uint16_t _TTx;
  uint16_t _TTyp;
  uint16_t _TMax;
  uint16_t _TError;
  uint16_t _TWbs;
  uint16_t NodeMask;
  NodeIdType NodeId;
  uint8_t rx_limit;
  uint8_t tx_limit;
  PduIdType txPduId;
} OsekNm_ChannelConfigType;

typedef union {
  uint8_t w;
  struct {
    unsigned NMactive : 1;
    unsigned bussleep : 1;
    unsigned configurationstable : 1;
  } W;
} NetworkStatusType;

#ifdef _WIN32
typedef Std_TimerType NMTimerType;
#else
typedef uint16_t NMTimerType;
#endif

typedef struct {
  NMStateType nmState;
  NMPduType nmTxPdu;
  struct {
    NMTimerType _TTx;
    NMTimerType _TTyp;
    NMTimerType _TMax;
    NMTimerType _TError;
    NMTimerType _TWbs;
  } Alarm;
  struct {
    NetworkStatusType SMask;
    NetworkStatusType NetworkStatus;
  } nmStatus;
  union {
    uint8_t w;
    struct {
      unsigned stable : 1;
      unsigned limphome : 1;
    } W;
  } nmMerker;
  uint8_t nmRxCount;
  uint8_t nmTxCount;
} OsekNm_ChannelContextType;

struct OsekNm_Config_s {
  const OsekNm_ChannelConfigType *channelConfigs;
  OsekNm_ChannelContextType *channelContexts;
  uint8_t numOfChannels;
};
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _OSEK_NM_PRIV_H */
