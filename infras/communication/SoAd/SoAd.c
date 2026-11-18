/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Socket Adaptor AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SoAd.h"
#include "SoAd_Cfg.h"
#include "SoAd_Priv.h"
#include "Std_Debug.h"
#include <string.h>
#include <stdio.h>
#include "NetMem.h"

#include "Det.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SOAD 0
#define AS_LOG_SOADE 2

#define SoAdIsConTypeOf(con, mask) (0 != ((con)->SoConType & (mask)))

#define SOAD_TX_ON_GOING 0x01

#define SOAD_CONFIG (soAdConfigPtr)

#ifndef SOAD_LOCAL_DATA_MAX_SIZE
#define SOAD_LOCAL_DATA_MAX_SIZE 128
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const SoAd_ConfigType SoAd_Config;
/* ================================ [ DATAS     ] ============================================== */
static const SoAd_ConfigType *soAdConfigPtr = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static void SoAd_CreateSocket(SoAd_SoConIdType soConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[soConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[soConId];
  TcpIp_SocketIdType sockId;
  uint16_t port;
  Std_ReturnType ret = E_OK;

  sockId = TcpIp_Create(conG->ProtocolType);
  if (sockId >= 0) {
    if (SoAdIsConTypeOf(connection, SOAD_SOCON_TCP_SERVER | SOAD_SOCON_UDP_SERVER)) {
      port = conG->Port;
      ret = TcpIp_Bind(sockId, conG->LocalAddrId, &port);
      if ((E_OK == ret) && (TRUE == conG->IsMulitcast)) {
        ret = TcpIp_AddToMulticast(sockId, &context->RemoteAddr);
      }
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    } else if (SoAdIsConTypeOf(connection, SOAD_SOCON_UDP_CLIENT)) {
#ifdef USE_LWIP
      /* See for LWIP, for a UDP socket created without port assigned, it takes several seconds to
       * make it ready to recevice packet, so, give one. */
      port = conG->LocalPort;
#else
      port = TCPIP_PORT_ANY;
#endif
      if (TRUE == conG->IsMulitcast) {
        port = context->RemoteAddr.Port;
      }
      ret = TcpIp_Bind(sockId, conG->LocalAddrId, &port);
      if ((E_OK == ret) && (TRUE == conG->IsMulitcast)) {
        ret = TcpIp_AddToMulticast(sockId, &context->RemoteAddr);
      }
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    } else {
      /* do nothing */
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    if (SoAdIsConTypeOf(connection, SOAD_SOCON_TCP_SERVER)) {
      ret = TcpIp_TcpListen(sockId, conG->NumOfConnections);
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    } else if (SoAdIsConTypeOf(connection, SOAD_SOCON_TCP_CLIENT)) {
      ret = TcpIp_TcpConnect(sockId, &context->RemoteAddr);
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    } else {
      /* do nothing */
    }
  }

  if (E_OK == ret) {
    if (SoAdIsConTypeOf(connection, SOAD_SOCON_TCP_SERVER)) {
      context->State = SOAD_SOCKET_ACCEPT;
    } else {
      context->State = SOAD_SOCKET_READY;
      context->Length = 0;
      if (NULL != context->Data) {
        ASLOG(SOADE, ("[%u] context cache data not NULL when create\n", soConId));
        Net_MemFree(context->Data);
      }
      context->Data = NULL;
    }
    context->Sock = sockId;
    if (conG->SoConModeChgNotification) {
      conG->SoConModeChgNotification(soConId, SOAD_SOCON_ONLINE);
    }
    ASLOG(SOAD, ("[%u] create TcpIP socket %d, next state %d\n", soConId, sockId, context->State));
  } else {
    ASLOG(SOADE, ("[%u] failed to creat socket with error %d\n", soConId, sockId));
    context->State = SOAD_SOCKET_CLOSED;
  }
}

static Std_ReturnType SoAd_SocketRecvStart(SoAd_SoConIdType soConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[soConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[soConId];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t rxLen;
  uint8_t *data = NULL;
  PduInfoType PduInfo;
  uint32_t length = 0; /* length of left data */
  uint8_t header[SOAD_HEADER_MAX_LEN];

  /* For some protocol, such as DoIP and SOMEIP, we need to know the actually packet size from its
   * header */
  rxLen = TcpIp_Tell(context->Sock);
  if (conG->HeaderLen > 0) {
    if (rxLen >= conG->HeaderLen) {
      rxLen = conG->HeaderLen;
      data = header;
    } else {
      rxLen = 0; /* not enough data */
    }
  }

  if (0 == rxLen) {
    /* nothing there */
  } else if (NULL == data) { /* allocate data if no header TP control */
    data = Net_MemAlloc((uint32_t)rxLen);
    if (NULL != data) {
      ret = E_OK;
    } else {
      ASLOG(SOADE, ("[%u] Failed to malloc for %d\n", soConId, rxLen));
    }
  } else {
    ret = E_OK;
  }

  if (E_OK == ret) {
    length = rxLen;
    if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
      ret = TcpIp_Recv(context->Sock, data, &rxLen);
    } else {
      ret = TcpIp_RecvFrom(context->Sock, &context->RemoteAddr, data, &rxLen);
    }
    if (E_OK == ret) {
      if (rxLen > 0) {
        ASLOG(SOAD, ("[%u] read %d bytes headerLen %u\n", soConId, rxLen, conG->HeaderLen));
        PduInfo.SduDataPtr = data;
        PduInfo.SduLength = rxLen;
        PduInfo.MetaDataPtr = (uint8_t *)&context->RemoteAddr;
        if (conG->HeaderLen > 0) {
          ret = conG->IF->HeaderIndication(connection->RxPduId, &PduInfo, &length);
          if (E_OK == ret) {
            if (length > 0) { /* the length of left bytes need to be recived */
              context->Length = length;
              context->Offset = conG->HeaderLen;
              if (NULL != context->Data) {
                ASLOG(SOADE, ("[%u] context cache data not NULL when recv start\n", soConId));
                Net_MemFree(context->Data);
              }
              context->Data = Net_MemAlloc((uint32_t)conG->HeaderLen + length);
              /* allow allocation failed thus the further rx logic to drop data */
              if (NULL == context->Data) {
                ASLOG(SOADE, ("[%u] allocate %u rx buffer failed\n", soConId,
                              (uint32_t)conG->HeaderLen + length));
              } else {
                (void)memcpy(context->Data, data, rxLen);
                ASLOG(SOAD,
                      ("[%u] length = %u, offset=%u\n", soConId, context->Length, context->Offset));
              }
            } else {
              conG->IF->RxIndication(connection->RxPduId, &PduInfo);
            }
          }
        } else {
          if (NULL != conG->IF->RxIndication) {
            conG->IF->RxIndication(connection->RxPduId, &PduInfo);
          }
        }
      } else {
        ASLOG(SOADE, ("[%u] recv with 0 bytes, actual: %u\n", soConId, length));
      }
    } else {
      ASLOG(SOADE, ("[%u] socket read failed\n", soConId));
#if SOAD_ERROR_COUNTER_LIMIT > 0
      context->ErrorCounter++;
      if (context->ErrorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
        SoAd_CloseSoCon(soConId, TRUE);
        ASLOG(SOADE, ("[%u] socket closed\n", soConId));
      }
#else
      ASLOG(SOADE, ("[%u] socket closed\n", soConId));
#endif
    }
    if (data != header) {
      Net_MemFree(data);
    }
  }

  return ret;
}

static Std_ReturnType SoAd_SocketRecvLeft(SoAd_SoConIdType soConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[soConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[soConId];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t rxLen;
  uint8_t *data = NULL;
  PduInfoType PduInfo;
  uint8_t cache[SOAD_LOCAL_DATA_MAX_SIZE];

  rxLen = TcpIp_Tell(context->Sock);
  if (rxLen > 0) {
    if (rxLen > context->Length) {
      rxLen = context->Length;
    }
    if (NULL != context->Data) {
      data = &context->Data[context->Offset];
    } else {
      data = cache;
      if (rxLen > SOAD_LOCAL_DATA_MAX_SIZE) {
        rxLen = SOAD_LOCAL_DATA_MAX_SIZE;
      }
      ASLOG(SOADE, ("[%u] drop %u bytes\n", soConId, rxLen));
    }
    ret = E_OK;
  }

  if (E_OK == ret) {
    if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
      ret = TcpIp_Recv(context->Sock, data, &rxLen);
    } else {
      ret = TcpIp_RecvFrom(context->Sock, &context->RemoteAddr, data, &rxLen);
    }
    if (E_OK == ret) {
      if (rxLen > 0) {
        context->Offset += rxLen;
        if (context->Length > rxLen) {
          context->Length -= rxLen;
        } else {
          context->Length = 0;
        }
      }
      if ((0 == context->Length) && (NULL != context->Data)) {
        ASLOG(SOAD, ("[%u] read %d bytes\n", soConId, rxLen));
        PduInfo.SduDataPtr = context->Data;
        PduInfo.SduLength = context->Offset;
        PduInfo.MetaDataPtr = (uint8_t *)&context->RemoteAddr;
        conG->IF->RxIndication(connection->RxPduId, &PduInfo);
        if (NULL != context->Data) {
          Net_MemFree(context->Data);
          context->Data = NULL;
        }
      }
    } else {
      ASLOG(SOADE, ("[%u] socket read failed\n", soConId));
#if SOAD_ERROR_COUNTER_LIMIT > 0
      context->ErrorCounter++;
      if (context->ErrorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
        SoAd_CloseSoCon(soConId, TRUE);
      }
#endif
    }
  }

  return ret;
}

static void SoAd_SocketAcceptMain(SoAd_SoConIdType soConId) {
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[soConId];
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[soConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  Std_ReturnType ret;
  TcpIp_SocketIdType SocketId;
  SoAd_SocketContextType *actCtx = NULL;
  TcpIp_SockAddrType RemoteAddr;
  int i;

  if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
    ret = TcpIp_TcpAccept(context->Sock, &SocketId, &RemoteAddr);
    if (E_OK == ret) {
      ret = E_NOT_OK;
      for (i = 0; i < conG->NumOfConnections; i++) {
        if (SOAD_SOCKET_CLOSED == SOAD_CONFIG->Contexts[i + conG->SoConId].State) {
          actCtx = &SOAD_CONFIG->Contexts[i + conG->SoConId];
          actCtx->Sock = SocketId;
          actCtx->RemoteAddr = RemoteAddr;
          actCtx->State = SOAD_SOCKET_READY;
          ret = E_OK;
          break;
        }
      }
      if (E_OK != ret) {
        ASLOG(SOADE, ("[%u] accept failed as no free slot\n", soConId));
        TcpIp_Close(SocketId, TRUE);
      } else {
        ASLOG(SOAD, ("[%u] accept as new socket\n", i + conG->SoConId));
      }
    }
    if (E_OK == ret) {
      if (conG->SoConModeChgNotification) {
        conG->SoConModeChgNotification(i + conG->SoConId, SOAD_SOCON_ONLINE);
      }
    }
  } else {
    ASLOG(SOADE, ("[%u] UDP can't do accept\n", soConId));
  }
}

static void SoAd_SocketReadyMain(SoAd_SoConIdType soConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[soConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[soConId];
  Std_ReturnType ret = E_OK;

  if (context->Flag & SOAD_TX_ON_GOING) {
    if (NULL != conG->IF->TxConfirmation) {
      conG->IF->TxConfirmation(connection->RxPduId, E_OK);
    }
    context->Flag &= ~SOAD_TX_ON_GOING;
  }

  if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
    ret = TcpIp_IsTcpStatusOK(context->Sock);
    if (E_OK != ret) {
      SoAd_CloseSoCon(soConId, TRUE);
      ASLOG(SOADE, ("[%u] close, goto accept\n", soConId));
    }
  }

  while (E_OK == ret) {
    if (0 == context->Length) {
      ret = SoAd_SocketRecvStart(soConId);
    } else {
      ret = SoAd_SocketRecvLeft(soConId);
    }
  }
}

static void SoAd_SocketTakeControlMain(SoAd_SoConIdType soConId) {
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[soConId];
  if (context->Flag & SOAD_TX_ON_GOING) {
    context->Flag &= ~SOAD_TX_ON_GOING;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void SoAd_Init(const SoAd_ConfigType *configPtr) {
  int i;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;

  if (NULL != configPtr) {
    soAdConfigPtr = configPtr;
  } else {
    soAdConfigPtr = &SoAd_Config;
  }

  Net_MemInit();
  for (i = 0; i < SOAD_CONFIG->NumOfConnections; i++) {
    connection = &SOAD_CONFIG->Connections[i];
    context = &SOAD_CONFIG->Contexts[i];
    memset(context, 0, sizeof(SoAd_SocketContextType));
    context->State = SOAD_SOCKET_CLOSED;
#if SOAD_ERROR_COUNTER_LIMIT > 0
    context->ErrorCounter = 0;
#endif
    if (connection->GID < SOAD_CONFIG->NumOfGroups) {
      conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
      if ((FALSE == SoAdIsConTypeOf(connection, SOAD_SOCON_TCP_ACCEPT)) &&
          conG->AutomaticSoConSetup) {
        context->State = SOAD_SOCKET_CREATE;
      }
      TcpIp_SetupAddrFrom(&context->RemoteAddr, conG->Remote, conG->Port);
      ASLOG(SOAD, ("[%u] init with %d.%d.%d.%d:%u\n", i, context->RemoteAddr.Addr[0],
                   context->RemoteAddr.Addr[1], context->RemoteAddr.Addr[2],
                   context->RemoteAddr.Addr[3], context->RemoteAddr.Port));
    } else {
      ASLOG(SOADE, ("[%u] Invalid GID\n", i));
    }
  }
}

void SoAd_MainFunction(void) {
  uint16_t i;
  SoAd_SocketContextType *context;
  boolean bLinkedUp = TcpIp_IsLinkedUp();

  for (i = 0; (TRUE == bLinkedUp) && (i < SOAD_CONFIG->NumOfConnections); i++) {
    context = &SOAD_CONFIG->Contexts[i];
    switch (context->State) {
    case SOAD_SOCKET_CREATE:
      SoAd_CreateSocket(i);
      break;
    case SOAD_SOCKET_ACCEPT:
      SoAd_SocketAcceptMain(i);
      break;
    case SOAD_SOCKET_READY:
      SoAd_SocketReadyMain(i);
      break;
    case SOAD_SOCKET_TAKEN_CONTROL:
      SoAd_SocketTakeControlMain(i);
      break;
    default:
      break;
    }
  }
}

Std_ReturnType SoAd_IfTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SoConIdType SoConId;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;
  TcpIp_SockAddrType addr;

  if (txPduId < SOAD_CONFIG->NumOfTxPduIds) {
    SoConId = SOAD_CONFIG->TxPduIdToSoCondIdMap[txPduId];
    connection = &SOAD_CONFIG->Connections[SoConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_READY <= context->State) {
      if (0 == (context->Flag & SOAD_TX_ON_GOING)) {
        ret = E_OK;
      }
    }
  }

  if (E_OK == ret) {
    if (TCPIP_IPPROTO_UDP == conG->ProtocolType) {
      if (pduInfoPtr->MetaDataPtr != NULL) {
        addr = *(const TcpIp_SockAddrType *)pduInfoPtr->MetaDataPtr;
        ret = TcpIp_SendTo(context->Sock, &addr, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
      } else {
        TcpIp_SetupAddrFrom(&addr, conG->Remote, conG->Port);
        ret = TcpIp_SendTo(context->Sock, &addr, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
      }
    } else {
      ret = TcpIp_Send(context->Sock, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
    }

    if (E_OK == ret) {
      if (NULL != conG->IF->TxConfirmation) {
        context->Flag |= SOAD_TX_ON_GOING;
      }
    }

#if SOAD_ERROR_COUNTER_LIMIT > 0
    if (E_OK != ret) {
      context->ErrorCounter++;
      if (context->ErrorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
        ASLOG(SOADE, ("[%u] If Tx failed, closing\n", SoConId));
        SoAd_CloseSoCon(SoConId, TRUE);
      }
    }
#endif
  }

  return ret;
}

Std_ReturnType SoAd_TpTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SoConIdType SoConId;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;

  if (txPduId < SOAD_CONFIG->NumOfTxPduIds) {
    SoConId = SOAD_CONFIG->TxPduIdToSoCondIdMap[txPduId];
    connection = &SOAD_CONFIG->Connections[SoConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_READY <= context->State) {
      if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
        ret = TcpIp_Send(context->Sock, pduInfoPtr->SduDataPtr, pduInfoPtr->SduLength);
#if SOAD_ERROR_COUNTER_LIMIT > 0
        if (E_OK != ret) {
          context->ErrorCounter++;
          if (context->ErrorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
            SoAd_CloseSoCon(SoConId, TRUE);
          }
        }
#endif
      }
    }
  }

  return ret;
}

Std_ReturnType SoAd_GetSoConId(PduIdType txPduId, SoAd_SoConIdType *soConIdPtr) {
  Std_ReturnType ret = E_NOT_OK;

  if ((txPduId < SOAD_CONFIG->NumOfTxPduIds) && (NULL != soConIdPtr)) {
    *soConIdPtr = SOAD_CONFIG->TxPduIdToSoCondIdMap[txPduId];
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType SoAd_GetLocalAddr(SoAd_SoConIdType soConId, TcpIp_SockAddrType *localAddrPtr,
                                 uint8_t *netmaskPtr, TcpIp_SockAddrType *defaultRouterPtr) {
  Std_ReturnType ret = E_NOT_OK;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;
  if (soConId < SOAD_CONFIG->NumOfConnections) {
    connection = &SOAD_CONFIG->Connections[soConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SoAdIsConTypeOf(connection, SOAD_SOCON_TCP_CLIENT | SOAD_SOCON_TCP_ACCEPT)) {
      /* for TCP client socket */
      if (context->State >= SOAD_SOCKET_READY) {
        ret = TcpIp_GetLocalAddr(context->Sock, localAddrPtr);
      }
    } else if (SoAdIsConTypeOf(connection, SOAD_SOCON_UDP_CLIENT)) {
#ifdef USE_LWIP
      ret = TcpIp_GetIpAddr(conG->LocalAddrId, localAddrPtr, NULL, NULL);
      localAddrPtr->Port = conG->LocalPort;
#else
      ret = TcpIp_GetLocalAddr(context->Sock, localAddrPtr);
      if (E_OK == ret) {
        ret = TcpIp_GetIpAddr(conG->LocalAddrId, localAddrPtr, NULL, NULL);
      }
#endif
    } else if (SoAdIsConTypeOf(connection, SOAD_SOCON_TCP_SERVER | SOAD_SOCON_UDP_SERVER)) {
      ret = TcpIp_GetIpAddr(conG->LocalAddrId, localAddrPtr, NULL, NULL);
      localAddrPtr->Port = conG->Port;
    } else {
      ASLOG(SOADE, ("Not supported connection type\n"));
      ret = E_NOT_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_SetRemoteAddr(SoAd_SoConIdType soConId,
                                  const TcpIp_SockAddrType *remoteAddrPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_CLOSED == context->State) {
      context->RemoteAddr = *remoteAddrPtr;
      ASLOG(SOAD, ("[%u] setup with %d.%d.%d.%d:%u\n", soConId, context->RemoteAddr.Addr[0],
                   context->RemoteAddr.Addr[1], context->RemoteAddr.Addr[2],
                   context->RemoteAddr.Addr[3], context->RemoteAddr.Port));
      ret = E_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_GetRemoteAddr(SoAd_SoConIdType soConId, TcpIp_SockAddrType *ipAddrPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_READY <= context->State) {
      *ipAddrPtr = context->RemoteAddr;
      ret = E_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_OpenSoCon(SoAd_SoConIdType soConId) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_CLOSED == context->State) {
      context->Flag = 0;
#if SOAD_ERROR_COUNTER_LIMIT > 0
      context->ErrorCounter = 0;
#endif
      context->State = SOAD_SOCKET_CREATE;
      ret = E_OK;
    } else {
      ASLOG(SOADE, ("[%u] open failed as already in state %d\n", soConId, context->State));
    }
  }

  return ret;
}

Std_ReturnType SoAd_CloseSoCon(SoAd_SoConIdType soConId, boolean abort) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    connection = &SOAD_CONFIG->Connections[soConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    if (SOAD_SOCKET_CLOSED != context->State) {
      ret = TcpIp_Close(context->Sock, abort);
      if (E_OK == ret) {
        context->State = SOAD_SOCKET_CLOSED;
        TcpIp_SetupAddrFrom(&context->RemoteAddr, conG->Remote, conG->Port);
      } else {
        ASLOG(SOADE, ("[%u] close fail: %d\n", soConId, ret));
      }
      if (NULL != context->Data) {
        ASLOG(SOADE, ("[%u] context cache data not NULL when close\n", soConId));
        Net_MemFree(context->Data);
        context->Data = NULL;
      }
      if (conG->SoConModeChgNotification) {
        conG->SoConModeChgNotification(soConId, SOAD_SOCON_OFFLINE);
      }
    }
  }

  return ret;
}

void SoAd_GetSoConMode(SoAd_SoConIdType soConId, SoAd_SoConModeType *modePtr) {
  SoAd_SocketContextType *context;
  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_CLOSED != context->State) {
      *modePtr = SOAD_SOCON_ONLINE;
    } else {
      *modePtr = SOAD_SOCON_OFFLINE;
    }
  }
}

Std_ReturnType SoAd_TakeControl(SoAd_SoConIdType soConId) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_READY <= context->State) {
      context->State = SOAD_SOCKET_TAKEN_CONTROL;
      ret = E_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_SetNonBlock(SoAd_SoConIdType soConId, boolean nonBlocked) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_TAKEN_CONTROL == context->State) {
      ret = TcpIp_SetNonBlock(context->Sock, nonBlocked);
    }
  }

  return ret;
}

Std_ReturnType SoAd_SetTimeout(SoAd_SoConIdType soConId, uint32_t timeoutMs) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_TAKEN_CONTROL == context->State) {
      ret = TcpIp_SetNonBlock(context->Sock, FALSE);
      if (E_OK == ret) {
        ret = TcpIp_SetTimeout(context->Sock, timeoutMs);
      }
    }
  }

  return ret;
}

Std_ReturnType SoAd_ControlRecv(SoAd_SoConIdType soConId, uint8_t *data, uint32_t *length) {
  SoAd_SocketContextType *context;
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[soConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  Std_ReturnType ret = E_NOT_OK;

  if (soConId < SOAD_CONFIG->NumOfConnections) {
    context = &SOAD_CONFIG->Contexts[soConId];
    if (SOAD_SOCKET_TAKEN_CONTROL == context->State) {
      if (NULL == data) {
        *length = TcpIp_Tell(context->Sock);
      } else {
        if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
          ret = TcpIp_Recv(context->Sock, data, length);
          if (E_OK != ret) {
            ASLOG(SOADE, ("[%u] TCP read failed\n", soConId));
#if SOAD_ERROR_COUNTER_LIMIT > 0
            context->ErrorCounter++;
            if (context->ErrorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
              SoAd_CloseSoCon(soConId, TRUE);
            }
#endif
          }
        } else {
          ret = TcpIp_RecvFrom(context->Sock, &context->RemoteAddr, data, length);
        }
      }
    }
  }

  return ret;
}

void SoAd_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x02, SOAD_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_SOAD;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 0;
}
