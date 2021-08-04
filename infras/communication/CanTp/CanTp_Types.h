/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Transport Layer AUTOSAR CP Release 4.4.0
 */
#ifndef CANTP_TYPES_H
#define CANTP_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* @ECUC_CanTp_00281 */
typedef enum
{
  CANTP_EXTENDED,
  CANTP_MIXED,
  CANTP_MIXED29BIT,
  CANTP_NORMALFIXED,
  CANTP_STANDARD
} CanTp_AddressingFormatType;

typedef struct {
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
  uint8_t N_TA; /* if addressing format is CANTP_EXTENDED */
  /* @ECUC_CanTp_00251 */
  uint8_t CanTpRxWftMax; /* can't not be 0xFF */
  uint8_t LL_DL;         /* for CAN it was 8, for CANFD it was 64 */
  uint8_t padding;
  uint8_t *data; /* data buffer to TP frame transmission */
} CanTp_ChannelConfigType;

enum
{
  CANTP_IDLE = 0,
  CANTP_WAIT_CF,
  CANTP_WAIT_FIRST_FC,
  CANTP_WAIT_FC,
  CANTP_SEND_CF_DELAY,
  CANTP_RESEND_SF,
  CANTP_RESEND_FF,
  CANTP_RESEND_FC,
  CANTP_RESEND_CF,
  CANTP_WAIT_SF_TX_COMPLETED,
  CANTP_WAIT_FF_TX_COMPLETED,
  CANTP_WAIT_CF_TX_COMPLETED,
  CANTP_WAIT_FC_TX_COMPLETED,
};

typedef struct {
  PduInfoType PduInfo;
  uint16_t timer;
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
#endif /* CANTP_TYPES_H */