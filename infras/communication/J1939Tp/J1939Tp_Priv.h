/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of a Transport Layer for SAE J1939 AUTOSAR CP R23-11
 */
#ifndef J1939_TP_PRIV_H
#define J1939_TP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "J1939Tp.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef DET_THIS_MODULE_ID
#define DET_THIS_MODULE_ID MODULE_ID_J1939TP
#endif

#define J1939TP_PACKET_CM ((J1939Tp_PacketType)0)
#define J1939TP_PACKET_DT ((J1939Tp_PacketType)1)
#define J1939TP_PACKET_DIRECT ((J1939Tp_PacketType)2)

#define J1939TP_RX_CHANNEL ((J1939Tp_ChannelTypeType)0)
#define J1939TP_TX_CHANNEL ((J1939Tp_ChannelTypeType)1)

/* J1939 transport protocol type BAM (Broadcast Announce Message).
 * This protocol uses two N-PDUs: The CmNPdu and the DtNPdu */
#define J1939TP_PROTOCOL_BAM ((J1939Tp_ProtocolType)0)

/* J1939 transport protocol type CMDT (Connection Mode Data Transfer).
 * This protocol uses three N-PDUs: The CmNPdu, the DtNPdu, and the FcNPdu. */
#define J1939TP_PROTOCOL_CMDT ((J1939Tp_ProtocolType)1)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t J1939Tp_ChannelTypeType;

typedef uint8_t J1939Tp_ProtocolType;

typedef struct {
  uint32_t PgPGN;
  uint32_t NextSN;
  PduInfoType PduInfo;
  PduLengthType TpSduLength;
  uint8_t MaxPacketsPerBlock;
  uint8_t NumPacketsThisBlock;
  uint16_t timer;
  uint8_t state;
} J1939Tp_RxChannelContextType;

/* @ECUC_J1939Tp_00053 */
typedef struct {
  J1939Tp_RxChannelContextType *context;
  uint8_t *data; /* data buffer to TP frame transmission */

  PduIdType PduR_RxPduId;
  PduIdType CanIf_TxCmNPdu;

  uint16_t Tr;
  uint16_t T1;
  uint16_t T2;
  uint16_t T3;
  uint16_t T4;

  J1939Tp_ProtocolType RxProtocol;

  /* 0 ~ 253, Destination address (DA) of this channel. This parameter is only required for channels
   * with fixed DA which use N-PDUs with MetaData containing the DA. */
  // uint8_t RxDa;

  /* 0 .. 253, Source address (SA) of this channel. This parameter is only required for channels
   * with fixed SA which use N-PDUs with MetaData containing the SA. */
  // uint8_t RxSa;

  /* Number of TP.DT frames the receiving J1939Tp module allows the sender to send before waiting
   * for another TP.CM_CTS. This parameter is transmitted in the TP.CM_CTS frame, and is thus only
   * relevant for reception of messages via CMDT. When J1939TpRxDynamicBlockCalculation is enabled,
   * this parameter specifies a maximum for the calculated value. For further details on this
   * parameter value see SAE J1939/21. */
  uint8_t RxPacketsPerBlock;

  uint8_t LL_DL;
  uint8_t S;      /* FD session */
  uint8_t ADType; /* Assurance Data Type  */
  uint8_t padding;

  /* Enable receive cancellation using the API J1939Tp_CancelReceive() for this channel. */
  // boolean RxCancellationSupport;

  /* Enable dynamic calculation of "number of packets that can be sent" value in TP.CM_CTS, based on
   * the size of buffers in upper layers reported via StartOfReception and PduR_J1939TpCopyRxData.
   */
  // boolean RxDynamicBlockCalculation;
} J1939Tp_RxChannelType;

typedef struct {
  uint32_t PgPGN;
  uint32_t NextSN;
  PduInfoType PduInfo;
  PduLengthType TpSduLength;
  uint16_t timer;
  uint8_t NumPacketsToSend;
  uint8_t state;
} J1939Tp_TxChannelContextType;

/* @ECUC_J1939Tp_00059 */
typedef struct {
  J1939Tp_TxChannelContextType *context;
  uint8_t *data; /* data buffer to TP frame transmission */

  uint32_t TxPgPGN; /* @PGN of the referenced N-SDUs. */

  PduIdType PduR_TxPduId;
  PduIdType CanIf_TxDirectNPdu;
  PduIdType CanIf_TxCmNPdu;
  PduIdType CanIf_TxDtNPdu;

  /* @SWS_J1939Tp_00234 */
  uint16_t STMin;
  uint16_t Tr;
  uint16_t T1;
  uint16_t T2;
  uint16_t T3;
  uint16_t T4;

  J1939Tp_ProtocolType TxProtocol;

  /* Destination address (DA) of this channel. This parameter is only required for channels with
   * fixed DA which use N-PDUs with MetaData containing the DA. */
  // uint8_t TxDa;

  /* Source address (SA) of this channel. This parameter is only required for channels with fixed SA
   * which use N-PDUs with MetaData containing the SA. */
  // uint8_t TxSa;

  /* Maximum number of TP.DT frames the transmitting J1939Tp module is ready to send before waiting
   * for another TP.CM_CTS. This parameter is transmitted in the TP.CM_RTS frame, and is thus only
   * relevant for transmission of messages via CMDT. When J1939TpTxDynamicBlockCalculation is
   * enabled, this parameter specifies a maximum for the calculated value. For further details on
   * this parameter value see SAE J1939/21. */
  uint8_t TxMaxPacketsPerBlock;

  uint8_t LL_DL;
  uint8_t S;      /* FD session */
  uint8_t ADType; /* Assurance Data Type  */
  uint8_t padding;

  /* Enable transmit cancellation using the API J1939Tp_CancelTransmit() for this channel. */
  // boolean TxCancellationSupport;

  /* Enable dynamic calculation of "maximum number of packets that can be sent" value in TP.CM_RTS,
   * based on the available amount of data in upper layers reported via PduR_ J1939TpCopyTxData. */
  // boolean TxDynamicBlockCalculation;
} J1939Tp_TxChannelType;

typedef uint8_t J1939Tp_PacketType;

typedef struct {
  uint16_t RxChannel;
  J1939Tp_PacketType PacketType;
} J1939Tp_RxPduInfoType;

typedef struct {
  uint16_t TxChannel;
  J1939Tp_PacketType PacketType;
} J1939Tp_TxPduInfoType;

typedef struct {
  union {
    const void *PduInfo;
    const J1939Tp_RxPduInfoType *RxPduInfo;
    const J1939Tp_TxPduInfoType *TxPduInfo;
  };
  J1939Tp_ChannelTypeType ChannelType;
} J1939Tp_PduInfoType;

struct J1939Tp_Config_s {
  const J1939Tp_RxChannelType *RxChannels;
  const J1939Tp_TxChannelType *TxChannels;
  const J1939Tp_PduInfoType *RxPduInfos;
  const J1939Tp_PduInfoType *TxPduInfos;
  uint16_t numOfRxChannels;
  uint16_t numOfTxChannels;
  uint16_t numOfRxPduInfos;
  uint16_t numOfTxPduInfos;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* J1939_TP_PRIV_H */
