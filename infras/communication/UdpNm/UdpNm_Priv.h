/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of UDP Network Management AUTOSAR CP Release 4.4.0
 */
#ifndef UDPNM_PRIV_H
#define UDPNM_PRIV_H
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
  NetworkHandleType NmNetworkHandle;
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
    UdpNm_TimerType Tx;
    UdpNm_TimerType TxTimeout;
    UdpNm_TimerType NmTimeout;
    union {
      UdpNm_TimerType RepeatMessage;
      UdpNm_TimerType WaitBusSleep;
    };
#ifdef UDPNM_REMOTE_SLEEP_IND_ENABLED
    UdpNm_TimerType RemoteSleepInd;
#endif
  } Alarm;
  uint16_t Flags;
  Nm_StateType State;
  uint8_t TxCounter;
  uint8_t Data[UDPNM_PDU_LENGTH];
  uint8_t RxPdu[UDPNM_PDU_LENGTH];
} UdpNm_ChannelContextType;

struct UdpNm_Config_s {
  const UdpNm_ChannelConfigType *ChannelConfigs;
  UdpNm_ChannelContextType *ChannelContexts;
  uint8_t NumOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* UDPNM_PRIV_H */
