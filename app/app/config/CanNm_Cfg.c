/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "CanNm_Priv.h"
#include "Std_Debug.h"
#include "CanIf.h"
#ifdef _WIN32
#include <stdlib.h>
#endif
#ifdef USE_CANIF
#include "Com/GEN/CanIf_Cfg.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#define CANNM_ImmediateNmCycleTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(100)
#define CANNM_MsgCycleOffset CANNM_CONVERT_MS_TO_MAIN_CYCLES(100)
#define CANNM_MsgCycleTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(1000)
#define CANNM_MsgReducedTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(500)
#define CANNM_MsgTimeoutTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(10)
#define CANNM_RepeatMessageTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(2000)
#define CANNM_NmTimeoutTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(2000)
#define CANNM_WaitBusSleepTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(5000)
#define CANNM_RemoteSleepIndTime CANNM_CONVERT_MS_TO_MAIN_CYCLES(1500)
#define CANNM_NODE_ID 0
#define CANNM_ImmediateNmTransmissions 10

#ifdef _WIN32
#define L_CONST
#else
#define L_CONST const
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#ifdef CANNM_GLOBAL_PN_SUPPORT
static const uint8_t nm0PnFilterMaskByte[2] = {0x55, 0xAA};
#endif
static L_CONST CanNm_ChannelConfigType CanNm_ChannelConfigs[] = {
  {
    CANNM_ImmediateNmCycleTime,
    CANNM_MsgCycleOffset,
    CANNM_MsgCycleTime,
    CANNM_MsgReducedTime,
    CANNM_MsgTimeoutTime,
    CANNM_RepeatMessageTime,
    CANNM_NmTimeoutTime,
    CANNM_WaitBusSleepTime,
#ifdef CANNM_REMOTE_SLEEP_IND_ENABLED
    CANNM_RemoteSleepIndTime,
#endif
#ifdef USE_CANIF
    CANIF_CANNM_TX, /* TxPdu */
#else
    0, /* TxPdu */
#endif
    CANNM_NODE_ID,
    0, /* nmNetworkHandle */
    CANNM_ImmediateNmTransmissions,
    CANNM_PDU_BYTE_1, /* PduCbvPosition */
    CANNM_PDU_BYTE_0, /* PduNidPosition */
    TRUE,             /* ActiveWakeupBitEnabled */
    FALSE,            /* PassiveModeEnabled */
    TRUE,             /* RepeatMsgIndEnabled */
    TRUE,             /* NodeDetectionEnabled */
#ifdef CANNM_GLOBAL_PN_SUPPORT
    TRUE,                        /* PnEnabled */
    TRUE,                        /* AllNmMessagesKeepAwake */
    4,                           /* PnInfoOffset */
    sizeof(nm0PnFilterMaskByte), /* PnInfoLength */
    nm0PnFilterMaskByte,
#endif
  },
};

static CanNm_ChannelContextType CanNm_ChannelContexts[ARRAY_SIZE(CanNm_ChannelConfigs)];

const CanNm_ConfigType CanNm_Config = {
  CanNm_ChannelConfigs,
  CanNm_ChannelContexts,
  ARRAY_SIZE(CanNm_ChannelConfigs),
};
/* ================================ [ LOCALS    ] ============================================== */
#ifdef _WIN32
static void __attribute__((constructor)) _cannm_start(void) {
  char *nodeStr = getenv("CANNM_NODE_ID");
  if (nodeStr != NULL) {
    CanNm_ChannelConfigs[0].NodeId = atoi(nodeStr);
    CanNm_ChannelConfigs[0].MsgReducedTime =
      CANNM_CONVERT_MS_TO_MAIN_CYCLES(500 + (CanNm_ChannelConfigs[0].NodeId * 100));
    ASLOG(INFO, ("CanNm NodeId=%d, ReduceTime=%d\n", CanNm_ChannelConfigs[0].NodeId,
                 CanNm_ChannelConfigs[0].MsgReducedTime));
#ifdef USE_CANIF
    CanIf_SetDynamicTxId(CANIF_CANNM_TX, 0x500 + CanNm_ChannelConfigs[0].NodeId);
#endif
  }
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */

#ifndef USE_CANIF
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  CanNm_RxIndication(Mailbox->ControllerId, PduInfoPtr);
}

void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  CanNm_TxConfirmation(CanTxPduId, E_OK);
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Can_PduType canPdu;

  canPdu.swPduHandle = TxPduId;
  canPdu.length = PduInfoPtr->SduLength;
  canPdu.sdu = PduInfoPtr->SduDataPtr;
  canPdu.id = 0x500 + CanNm_ChannelConfigs[0].NodeId;
  return Can_Write(0, &canPdu);
}
#endif