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
    Std_TimerInit(&context->Timer, ((v) * 1000 * CANTP_MAIN_FUNCTION_PERIOD));                     \
  } while (0)

#define CanTpSignalAlarm()
#define CanTpIsAlarmTimeout() Std_IsTimerTimeout(&context->Timer)
#define CanTpIsAlarmStarted() Std_IsTimerStarted(&context->Timer)
#define CanTpCancelAlarm() Std_TimerStop(&context->Timer)
#else
#define CanTpSetAlarm(v)                                                                           \
  do {                                                                                             \
    context->Timer = 1u + (v);                                                                     \
  } while (0)

/* signal the alarm to process one step/tick forward */
#define CanTpSignalAlarm()                                                                         \
  do {                                                                                             \
    if (context->Timer > 1u) {                                                                     \
      (context->Timer)--;                                                                          \
    }                                                                                              \
  } while (0)

#define CanTpIsAlarmTimeout() (1u == context->Timer)

#define CanTpIsAlarmStarted() (0u != context->Timer)

#define CanTpCancelAlarm()                                                                         \
  do {                                                                                             \
    context->Timer = 0;                                                                            \
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
  uint8_t *Data; /* data buffer to TP frame transmission */
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
  uint8_t CanTpRxWftMax;       /* can't not be 0xFF */
  uint8_t LinkLayerDataLength; /* for CAN it was 8, for CANFD it was 64 */
  uint8_t Padding;
  CanTp_CommunicationType ComType;
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
  Std_TimerType Timer;
#else
  uint16_t Timer;
#endif
  PduLengthType TpSduLength;
  PduLengthType TpSduOffset; /* added for SecOC & Com */
  uint8_t CfgBS;
  uint8_t BS;
  uint8_t SN;
  uint8_t STmin;
  uint8_t WftCounter;
  uint8_t State;
} CanTp_ChannelContextType;

struct CanTp_Config_s {
  const CanTp_ChannelConfigType *ChannelConfigs;
  CanTp_ChannelContextType *ChannelContexts;
  uint8_t NumOfChannels;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CANTP_PRIV_H */
