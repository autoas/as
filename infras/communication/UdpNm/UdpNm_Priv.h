/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of UDP Network Management AUTOSAR CP Release 4.4.0
 */
#ifndef _CAN_NM_PRIV_H
#define _CAN_NM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "NmStack_Types.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#define UDPNM_PDU_BYTE_0 ((UdpNm_PduPositionType)0)
#define UDPNM_PDU_BYTE_1 ((UdpNm_PduPositionType)1)
#define UDPNM_PDU_OFF ((UdpNm_PduPositionType)0xFF)

#define UDPNM_INVALID_NODE_ID 0xFF
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_UdpNm_00304 */
typedef uint8_t UdpNm_PduPositionType;

typedef struct {
  uint16_t ImmediateNmCycleTime; /* @ECUC_UdpNm_00079 */
  uint16_t MsgCycleOffset;       /* @ECUC_UdpNm_00029 */
  uint16_t MsgCycleTime;         /* @ECUC_UdpNm_00028 */
  uint16_t MsgTimeoutTime;       /* @ */
  /* dependency: RepeatMessageTime = n * UdpNmMsgCycleTime;
   * RepeatMessageTime > ImmediateNmTransmissions * ImmediateNmCycleTime */
  uint16_t RepeatMessageTime; /* @ECUC_UdpNm_00022 */
  uint16_t NmTimeoutTime;     /* @ECUC_UdpNm_00020 */
  uint16_t WaitBusSleepTime;  /* @ECUC_UdpNm_00021 */
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
  uint16_t RemoteSleepIndTime; /* @ECUC_UdpNm_00023 */
#endif
  PduIdType TxPdu; /* @ECUC_UdpNm_00036 */
  uint8_t NodeId;  /* @SWS_UdpNm_00013 */
  NetworkHandleType nmNetworkHandle;
  uint8_t ImmediateNmTransmissions;     /* @ECUC_UdpNm_00075 */
  UdpNm_PduPositionType PduCbvPosition; /* @ECUC_UdpNm_00026 */
  UdpNm_PduPositionType PduNidPosition; /* @ECUC_UdpNm_00025 */
  boolean ActiveWakeupBitEnabled;       /* @ECUC_UdpNm_00074 */
  boolean PassiveModeEnabled;           /* @ECUC_UdpNm_00010 */
  boolean RepeatMsgIndEnabled;          /* @ECUC_UdpNm_00092 */
  boolean NodeDetectionEnabled;         /* @ECUC_UdpNm_00090  */
#ifdef UDPNM_GLOBAL_PN_SUPPORT
  boolean PnEnabled;               /* @ECUC_UdpNm_00061 */
  boolean AllNmMessagesKeepAwake;  /* @ECUC_UdpNm_00089 */
  uint8_t PnInfoOffset;            /* @ECUC_UdpNm_00068 */
  uint8_t PnInfoLength;            /* @ECUC_UdpNm_00069 */
  const uint8_t *PnFilterMaskByte; /* @ECUC_UdpNm_00070 */
#endif
} UdpNm_ChannelConfigType;

#ifdef _WIN32
typedef Std_TimerType UdpNm_TimerType;
#else
typedef uint16_t UdpNm_TimerType;
#endif

typedef struct {
  struct {
    UdpNm_TimerType _Tx;
    UdpNm_TimerType _TxTimeout;
    UdpNm_TimerType _NMTimeout;
    union {
      UdpNm_TimerType _RepeatMessage;
      UdpNm_TimerType _WaitBusSleep;
    };
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    UdpNm_TimerType _RemoteSleepInd;
#endif
  } Alarm;
  uint16_t flags;
  Nm_StateType state;
  uint8_t TxCounter;
  uint8_t data[UDPNM_PDU_LENGTH];
  uint8_t rxPdu[UDPNM_PDU_LENGTH];
} UdpNm_ChannelContextType;

struct UdpNm_Config_s {
  const UdpNm_ChannelConfigType *ChannelConfigs;
  UdpNm_ChannelContextType *ChannelContexts;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _CAN_NM_PRIV_H */
