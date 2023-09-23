/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Socket Adaptor AUTOSAR CP Release 4.4.0
 */
#ifndef _SOAD_PRIV_H
#define _SOAD_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "TcpIp.h"
/* ================================ [ MACROS    ] ============================================== */
#define SOAD_SOCON_TCP_SERVER ((SoAd_SoConTypeType)0x01)
#define SOAD_SOCON_TCP_CLIENT ((SoAd_SoConTypeType)0x02)
#define SOAD_SOCON_TCP_ACCEPT ((SoAd_SoConTypeType)0x04)
#define SOAD_SOCON_UDP_SERVER ((SoAd_SoConTypeType)0x08)
#define SOAD_SOCON_UDP_CLIENT ((SoAd_SoConTypeType)0x10)

#ifndef SOAD_ERROR_COUNTER_LIMIT
#define SOAD_ERROR_COUNTER_LIMIT 3
#endif
/* ================================ [ TYPES     ] ============================================== */

typedef void (*SoAd_SoConModeChgNotificationFncType)(SoAd_SoConIdType SoConId,
                                                     SoAd_SoConModeType Mode);
/* @SWS_SoAd_00106 */
typedef void (*SoAd_IfRxIndicationFncType)(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_SoAd_00663 */
typedef Std_ReturnType (*SoAd_IfTriggerTransmitFncType)(PduIdType TxPduId, PduInfoType *PduInfoPtr);

/* @SWS_SoAd_00107 */
typedef void (*SoAd_IfTxConfirmationFncType)(PduIdType id, Std_ReturnType result);

typedef struct {
  SoAd_IfRxIndicationFncType IfRxIndication;
  SoAd_IfTriggerTransmitFncType IfTriggerTransmit;
  SoAd_IfTxConfirmationFncType IfTxConfirmation;
} SoAd_IfInterfaceType;

/* @SWS_SoAd_00138 */
typedef BufReq_ReturnType (*SoAd_TpStartOfReceptionFncType)(PduIdType id, const PduInfoType *info,
                                                            PduLengthType TpSduLength,
                                                            PduLengthType *bufferSizePtr);

/* @SWS_SoAd_00139 */
typedef BufReq_ReturnType (*SoAd_TpCopyRxDataFncType)(PduIdType id, const PduInfoType *info,
                                                      PduLengthType *bufferSizePtr);

/* @SWS_SoAd_00180 */
typedef void (*SoAd_TpRxIndicationFncType)(PduIdType id, Std_ReturnType result);

/* @SWS_SoAd_00137 */
typedef BufReq_ReturnType (*SoAd_TpCopyTxDataFncType)(PduIdType id, const PduInfoType *info,
                                                      const RetryInfoType *retry,
                                                      PduLengthType *availableDataPtr);

/* @SWS_SoAd_00181 */
typedef void (*SoAd_TpTxConfirmationFncType)(PduIdType id, Std_ReturnType result);

typedef struct {
  SoAd_TpStartOfReceptionFncType TpStartOfReception;
  SoAd_TpCopyRxDataFncType TpCopyRxData;
  SoAd_TpRxIndicationFncType TpRxIndication;
  SoAd_TpCopyTxDataFncType TpCopyTxData;
  SoAd_TpTxConfirmationFncType TpTxConfirmation;
} SoAd_TpInterfaceType;

/* @ECUC_SoAd_00140 */
typedef struct {
  PduLengthType PduUdpTxBufferMin;
  uint16_t UdpAliveSupervisionTimeout;
  uint16_t UdpTriggerTimeout;
  boolean UdpChecksumEnabled;
  boolean SocketUdpListenOnly;
  boolean UdpStrictHeaderLenCheckEnabled;
} SoAd_SocketUdpType;

/* @ECUC_SoAd_00141 */
typedef struct {
  PduLengthType TcpTxQuota;
  uint16_t TcpKeepAliveInterval;
  boolean TcpImmediateTpTxConfirmation;
  boolean TcpInitiate;
  boolean TcpKeepAlive;
  boolean TcpNoDelay;
} SoAd_SocketTcpType;

typedef uint8_t SoAd_SoConTypeType;

/* @ECUC_SoAd_00009 */
typedef struct {
  PduIdType RxPduId;
  SoAd_SoConIdType SoConId;
  uint16_t GID;
  SoAd_SoConTypeType SoConType;
} SoAd_SocketConnectionType;

/* @ECUC_SoAd_00130 */
typedef struct {
  /* SoAd_IfInterfaceType or SoAd_TpInterfaceType */
  const void *Interface;
  SoAd_SoConModeChgNotificationFncType SoConModeChgNotification;
  TcpIp_ProtocolType ProtocolType;
  /* https://www.ibm.com/docs/en/zvm/6.4?topic=SSB27U_6.4.0/com.ibm.zvm.v640.kiml0/asonetw.htm */
  uint32_t Remote;        /* if not 0, this is the default remote server IPv4 address */
  SoAd_SoConIdType SoConId; /* where the accepted connection socket id start from*/
  uint16_t Port;
  TcpIp_LocalAddrIdType LocalAddrId;
  uint8_t numOfConnections; /* max number of accepted connections */
  boolean AutomaticSoConSetup;
  boolean IsTP;
  boolean IsMulitcast;       /* if True, the Remote is a multicast UDP IPv4 address */
} SoAd_SocketConnectionGroupType;

typedef enum
{
  SOAD_SOCKET_CLOSED,
  SOAD_SOCKET_CREATE,
  SOAD_SOCKET_ACCEPT,
  SOAD_SOCKET_READY,
  SOAD_SOCKET_TAKEN_CONTROL,
} SoAd_SocketStateType;

typedef struct {
  TcpIp_SocketIdType sock;
  SoAd_SocketStateType state;
  TcpIp_SockAddrType RemoteAddr;
  TcpIp_SockAddrType LocalAddr;
  uint8_t flag;
#if SOAD_ERROR_COUNTER_LIMIT > 0
  uint8_t errorCounter;
#endif
} SoAd_SocketContextType;

struct SoAd_Config_s {
  const SoAd_SocketConnectionType *Connections;
  SoAd_SocketContextType *Contexts;
  uint16_t numOfConnections;
  const SoAd_SoConIdType *TxPduIdToSoCondIdMap;
  uint8_t numOfTxPduIds;
  const SoAd_SocketConnectionGroupType *ConnectionGroups;
  uint16_t numOfGroups;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _SOAD_PRIV_H */
