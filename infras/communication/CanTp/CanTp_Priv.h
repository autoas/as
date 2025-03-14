/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Transport Layer AUTOSAR CP Release 4.4.0
 */
#ifndef CANTP_PRIV_H
#define CANTP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef DET_THIS_MODULE_ID
#define DET_THIS_MODULE_ID MODULE_ID_CANTP
#endif

#define CANTP_PHYSICAL ((CanTp_CommunicationType)0)
#define CANTP_FUNCTIONAL ((CanTp_CommunicationType)1)

#ifdef CANTP_USE_STD_TIMER
#define CanTpSetAlarm(v)                                                                           \
  do {                                                                                             \
    Std_TimerInit(&context->timer, ((v) * 1000 * CANTP_MAIN_FUNCTION_PERIOD));                     \
  } while (0)

#define CanTpSignalAlarm()
#define CanTpIsAlarmTimeout() Std_IsTimerTimeout(&context->timer)
#define CanTpIsAlarmStarted() Std_IsTimerStarted(&context->timer)
#define CanTpCancelAlarm() Std_TimerStop(&context->timer)
#else
#define CanTpSetAlarm(v)                                                                           \
  do {                                                                                             \
    context->timer = 1 + (v);                                                                      \
  } while (0)

/* signal the alarm to process one step/tick forward */
#define CanTpSignalAlarm()                                                                         \
  do {                                                                                             \
    if (context->timer > 1) {                                                                      \
      (context->timer)--;                                                                          \
    }                                                                                              \
  } while (0)

#define CanTpIsAlarmTimeout() (1 == context->timer)

#define CanTpIsAlarmStarted() (0 != context->timer)

#define CanTpCancelAlarm()                                                                         \
  do {                                                                                             \
    context->timer = 0;                                                                            \
  } while (0)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* @ECUC_CanTp_00281 */
typedef enum {
  CANTP_EXTENDED,
  CANTP_MIXED,
  CANTP_MIXED29BIT,
  CANTP_NORMALFIXED,
  CANTP_STANDARD
} CanTp_AddressingFormatType;

/* @ECUC_CanTp_00250 */
typedef uint8_t CanTp_CommunicationType;

typedef struct {
  uint8_t *data; /* data buffer to TP frame transmission */
  CanTp_AddressingFormatType AddressingFormat;
  PduIdType CanIfTxPduId;
  PduIdType PduR_RxPduId;
  PduIdType PduR_TxPduId;
  /* Time for transmission of the CAN frame (any N-PDU) on the sender side */
  uint16_t N_As;
  /* Time until reception of the next flow control N-PDU (see ISO 15765-2) */
  uint16_t N_Bs;
  /* Time until reception of the next consecutive frame N-PDU (see ISO 15765-2) */
  uint16_t N_Cr;
  /* @ECUC_CanTp_00252: Sets the duration of the minimum time the CanTp sender shall wait
   * between the transmissions of two CF N-PDUs.*/
  uint8_t STmin;
  uint8_t BS;
  uint8_t N_TA; /* if addressing format is CANTP_EXTENDED.
                 * Note: for Lin, N_TA = 0 is reserved for go to sleep command */
  /* @ECUC_CanTp_00251 */
  uint8_t CanTpRxWftMax; /* can't not be 0xFF */
  uint8_t LL_DL;         /* for CAN it was 8, for CANFD it was 64 */
  uint8_t padding;
  CanTp_CommunicationType comType;
} CanTp_ChannelConfigType;

enum {
  CANTP_IDLE = 0,
  CANTP_WAIT_CF,
  CANTP_WAIT_FIRST_FC,
  CANTP_WAIT_FC,
  CANTP_SEND_CF_DELAY,
  CANTP_SEND_CF_START,
  CANTP_RESEND_SF,
  CANTP_RESEND_FF,
  CANTP_RESEND_FC_CTS,
  CANTP_RESEND_FC_OVFLW,
  CANTP_RESEND_CF,
  CANTP_WAIT_SF_TX_COMPLETED,
  CANTP_WAIT_FF_TX_COMPLETED,
  CANTP_WAIT_CF_TX_COMPLETED,
  CANTP_WAIT_FC_CTS_TX_COMPLETED,
  CANTP_WAIT_FC_OVFLW_TX_COMPLETED,
};

typedef struct {
  PduInfoType PduInfo;
#ifdef CANTP_USE_STD_TIMER
  Std_TimerType timer;
#else
  uint16_t timer;
#endif
  PduLengthType TpSduLength;
  uint8_t cfgBS;
  uint8_t BS;
  uint8_t SN;
  uint8_t STmin;
  uint8_t WftCounter;
  uint8_t state;
} CanTp_ChannelContextType;

struct CanTp_Config_s {
  const CanTp_ChannelConfigType *channelConfigs;
  CanTp_ChannelContextType *channelContexts;
  uint8_t numOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CANTP_PRIV_H */
