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

#define SOAD_SOCKET_CLOSED ((SoAd_SocketStateType)0x00)
#define SOAD_SOCKET_CREATE ((SoAd_SocketStateType)0x01)
#define SOAD_SOCKET_ACCEPT ((SoAd_SocketStateType)0x02)
#define SOAD_SOCKET_READY ((SoAd_SocketStateType)0x03)
#define SOAD_SOCKET_TAKEN_CONTROL ((SoAd_SocketStateType)0x04)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_SoAd_00514 */
typedef void (*SoAd_SoConModeChgNotificationFncType)(SoAd_SoConIdType SoConId,
                                                     SoAd_SoConModeType Mode);

typedef Std_ReturnType (*SoAd_HeaderIndicationFncType)(PduIdType id, const PduInfoType *info,
                                                       PduLengthType *payloadLength);

typedef void (*SoAd_RxIndicationFncType)(PduIdType id, const PduInfoType *info);

/* @SWS_SoAd_00181 */
typedef void (*SoAd_TxConfirmationFncType)(PduIdType id, Std_ReturnType result);

typedef struct {
  SoAd_HeaderIndicationFncType HeaderIndication;
  SoAd_RxIndicationFncType RxIndication;
  SoAd_TxConfirmationFncType TxConfirmation;
} SoAd_InterfaceType;

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
  uint16_t *LocalPort; /* For UDP client */
  PduIdType RxPduId;
  SoAd_SoConIdType SoConId;
  uint16_t GID;
  SoAd_SoConTypeType SoConType;
} SoAd_SocketConnectionType;

/* @ECUC_SoAd_00130 */
typedef struct {
  const SoAd_InterfaceType *IF;
  SoAd_SoConModeChgNotificationFncType SoConModeChgNotification;
  TcpIp_ProtocolType ProtocolType;
  /* https://www.ibm.com/docs/en/zvm/6.4?topic=SSB27U_6.4.0/com.ibm.zvm.v640.kiml0/asonetw.htm */
  uint32_t Remote;          /* if not 0, this is the default remote server IPv4 address */
  SoAd_SoConIdType SoConId; /* where the accepted connection socket id start from*/
  uint16_t Port;
  uint16_t headerLen; /* the length of the header of certain protocol such as DoIP and SOMEIP/SD */
  TcpIp_LocalAddrIdType LocalAddrId;
  uint8_t numOfConnections; /* max number of accepted connections */
  boolean AutomaticSoConSetup;
  boolean IsMulitcast; /* if True, the Remote is a multicast UDP IPv4 address */
} SoAd_SocketConnectionGroupType;

typedef uint8_t SoAd_SocketStateType;

typedef struct {
  uint8_t *data; /* data allocated to recive a packet */
  TcpIp_SocketIdType sock;
  PduLengthType length; /* length of the whole packet size */
  PduLengthType offset;
  TcpIp_SockAddrType RemoteAddr;
  TcpIp_SockAddrType LocalAddr;
  SoAd_SocketStateType state;
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
