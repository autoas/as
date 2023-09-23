/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CANNetworkManagement.pdf
 */
#ifndef _CAN_NM_PRIV_H
#define _CAN_NM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "NmStack_Types.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#define CANNM_PDU_BYTE_0 ((CanNm_PduPositionType)0)
#define CANNM_PDU_BYTE_1 ((CanNm_PduPositionType)1)
#define CANNM_PDU_OFF ((CanNm_PduPositionType)2)

#define CANNM_INVALID_NODE_ID 0xFF
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t CanNm_PduPositionType;
typedef struct {
  uint16_t ImmediateNmCycleTime; /* @ECUC_CanNm_00057 */
  uint16_t MsgCycleOffset;       /* @ECUC_CanNm_00029 */
  uint16_t MsgCycleTime;         /* @ECUC_CanNm_00028 */
  uint16_t MsgReducedTime;       /* @ECUC_CanNm_00043 */
  uint16_t MsgTimeoutTime;       /* @ECUC_CanNm_00030 */
  /* dependency: RepeatMessageTime = n * CanNmMsgCycleTime;
   * RepeatMessageTime > ImmediateNmTransmissions * ImmediateNmCycleTime */
  uint16_t RepeatMessageTime; /* @ECUC_CanNm_00022 */
  uint16_t NmTimeoutTime;     /* @ECUC_CanNm_00020 */
  uint16_t WaitBusSleepTime;  /* @ECUC_CanNm_00021 */
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
  uint16_t RemoteSleepIndTime; /* @ECUC_CanNm_00023 */
#endif
  PduIdType TxPdu; /* @ECUC_CanNm_00048 */
  uint8_t NodeId;  /* @ECUC_CanNm_00031 */
  NetworkHandleType nmNetworkHandle;
  uint8_t ImmediateNmTransmissions;     /* @ECUC_CanNm_00056 */
  CanNm_PduPositionType PduCbvPosition; /* @ECUC_CanNm_00026 */
  CanNm_PduPositionType PduNidPosition; /* @ECUC_CanNm_00025 */
  boolean ActiveWakeupBitEnabled;       /* @ECUC_CanNm_00084 */
  boolean PassiveModeEnabled;           /* @ECUC_CanNm_00010 */
  boolean RepeatMsgIndEnabled;          /* @ECUC_CanNm_00089 */
  boolean NodeDetectionEnabled;         /* @ECUC_CanNm_00088 */
#ifdef CANNM_GLOBAL_PN_SUPPORT
  boolean PnEnabled;               /* @ECUC_CanNm_00066 */
  boolean AllNmMessagesKeepAwake;  /* @ECUC_CanNm_00068 */
  uint8_t PnInfoOffset;            /* @ECUC_CanNm_00060 */
  uint8_t PnInfoLength;            /* @ECUC_CanNm_00061 */
  const uint8_t *PnFilterMaskByte; /* @ECUC_CanNm_00069 */
#endif
} CanNm_ChannelConfigType;

#ifdef _WIN32
typedef Std_TimerType CanNm_TimerType;
#else
typedef uint16_t CanNm_TimerType;
#endif

typedef struct {
  struct {
    CanNm_TimerType _Tx;
    CanNm_TimerType _TxTimeout;
    CanNm_TimerType _NMTimeout;
    union {
      CanNm_TimerType _RepeatMessage;
      CanNm_TimerType _WaitBusSleep;
    };
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    CanNm_TimerType _RemoteSleepInd;
#endif
  } Alarm;
  uint16_t flags;
  Nm_StateType state;
  uint8_t TxCounter;
  uint8_t data[8];
  uint8_t rxPdu[8];
} CanNm_ChannelContextType;

struct CanNm_Config_s {
  const CanNm_ChannelConfigType *ChannelConfigs;
  CanNm_ChannelContextType *ChannelContexts;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _CAN_NM_PRIV_H */
