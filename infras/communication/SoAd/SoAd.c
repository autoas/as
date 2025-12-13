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

#define IS_CON_TYPE_OF(con, mask) (0 != ((con)->SoConType & (mask)))

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
static void soAdCreateSocket(SoAd_SoConIdType SoConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  TcpIp_SocketIdType sockId;
  TcpIp_SockAddrType addr;
  Std_ReturnType ret = E_OK;

  sockId = TcpIp_Create(conG->ProtocolType);
  if (sockId >= 0) {
    if (IS_CON_TYPE_OF(connection, SOAD_SOCON_TCP_SERVER | SOAD_SOCON_UDP_SERVER)) {
      TcpIp_SetupAddrFrom(&addr, conG->Remote, conG->Port);
      ret = TcpIp_Bind(sockId, conG->LocalAddrId, &addr.port);
      if ((E_OK == ret) && (TRUE == conG->IsMulitcast)) {
        ret = TcpIp_AddToMulticast(sockId, &addr);
      }
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    } else if (IS_CON_TYPE_OF(connection, SOAD_SOCON_UDP_CLIENT)) {
#ifdef USE_LWIP
      /* See for LWIP, for a UDP socket created without port assigned, it takes several seconds to
       * make it ready to recevice packet, so, give one. */
      addr.port = conG->LocalPort;
#else
      addr.port = TCPIP_PORT_ANY;
#endif
      ret = TcpIp_Bind(sockId, conG->LocalAddrId, &addr.port);
    } else {
      /* do nothing */
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    if (IS_CON_TYPE_OF(connection, SOAD_SOCON_TCP_SERVER)) {
      ret = TcpIp_TcpListen(sockId, conG->numOfConnections);
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    } else if (IS_CON_TYPE_OF(connection, SOAD_SOCON_TCP_CLIENT)) {
      ret = TcpIp_TcpConnect(sockId, &context->RemoteAddr);
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    } else {
      /* do nothing */
    }
  }

  if (E_OK == ret) {
    if (IS_CON_TYPE_OF(connection, SOAD_SOCON_TCP_SERVER)) {
      context->state = SOAD_SOCKET_ACCEPT;
    } else {
      context->state = SOAD_SOCKET_READY;
      context->length = 0;
      context->data = NULL;
    }
    context->sock = sockId;
    if (conG->SoConModeChgNotification) {
      conG->SoConModeChgNotification(SoConId, SOAD_SOCON_ONLINE);
    }
    ASLOG(SOAD, ("[%u] create TcpIP socket %d, next state %d\n", SoConId, sockId, context->state));
  } else {
    ASLOG(SOADE, ("[%u] failed to creat socket with error %d\n", SoConId, sockId));
    context->state = SOAD_SOCKET_CLOSED;
  }
}

static Std_ReturnType soAdSocketRecvStart(SoAd_SoConIdType SoConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t rxLen;
  uint8_t *data = NULL;
  PduInfoType PduInfo;
  uint32_t length = 0; /* length of left data */
  uint8_t header[SOAD_HEADER_MAX_LEN];

  /* For some protocol, such as DoIP and SOMEIP, we need to know the actually packet size from its
   * header */
  rxLen = TcpIp_Tell(context->sock);
  if (conG->headerLen > 0) {
    if (rxLen >= conG->headerLen) {
      rxLen = conG->headerLen;
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
      ASLOG(SOADE, ("[%u] Failed to malloc for %d\n", SoConId, rxLen));
    }
  } else {
    ret = E_OK;
  }

  if (E_OK == ret) {
    length = rxLen;
    if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
      ret = TcpIp_Recv(context->sock, data, &rxLen);
    } else {
      ret = TcpIp_RecvFrom(context->sock, &context->RemoteAddr, data, &rxLen);
    }
    if (E_OK == ret) {
      if (rxLen > 0) {
        ASLOG(SOAD, ("[%u] read %d bytes headerLen %u\n", SoConId, rxLen, conG->headerLen));
        PduInfo.SduDataPtr = data;
        PduInfo.SduLength = rxLen;
        PduInfo.MetaDataPtr = (uint8_t *)&context->RemoteAddr;
        if (conG->headerLen > 0) {
          ret = conG->IF->HeaderIndication(connection->RxPduId, &PduInfo, &length);
          if (E_OK == ret) {
            if (length > 0) { /* the length of left bytes need to be recived */
              context->length = length;
              context->offset = conG->headerLen;
              context->data = Net_MemAlloc((uint32_t)conG->headerLen + length);
              /* allow allocation failed thus the further rx logic to drop data */
              if (NULL == context->data) {
                ASLOG(SOADE, ("[%u] allocate %u rx buffer failed\n", SoConId,
                              (uint32_t)conG->headerLen + length));
              } else {
                (void)memcpy(context->data, data, rxLen);
                ASLOG(SOAD,
                      ("[%u] length = %u, offset=%u\n", SoConId, context->length, context->offset));
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
        ASLOG(SOADE, ("[%u] recv with 0 bytes, actual: %u\n", SoConId, length));
      }
    } else {
      ASLOG(SOADE, ("[%u] socket read failed\n", SoConId));
#if SOAD_ERROR_COUNTER_LIMIT > 0
      context->errorCounter++;
      if (context->errorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
        SoAd_CloseSoCon(SoConId, TRUE);
      }
#endif
    }
    if (data != header) {
      Net_MemFree(data);
    }
  }

  return ret;
}

static Std_ReturnType soAdSocketRecvLeft(SoAd_SoConIdType SoConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t rxLen;
  uint8_t *data = NULL;
  PduInfoType PduInfo;
  uint8_t cache[SOAD_LOCAL_DATA_MAX_SIZE];

  rxLen = TcpIp_Tell(context->sock);
  if (rxLen > 0) {
    if (rxLen > context->length) {
      rxLen = context->length;
    }
    if (NULL != context->data) {
      data = &context->data[context->offset];
    } else {
      data = cache;
      if (rxLen > SOAD_LOCAL_DATA_MAX_SIZE) {
        rxLen = SOAD_LOCAL_DATA_MAX_SIZE;
      }
      ASLOG(SOADE, ("[%u] drop %u bytes\n", SoConId, rxLen));
    }
    ret = E_OK;
  }

  if (E_OK == ret) {
    if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
      ret = TcpIp_Recv(context->sock, data, &rxLen);
    } else {
      ret = TcpIp_RecvFrom(context->sock, &context->RemoteAddr, data, &rxLen);
    }
    if (E_OK == ret) {
      if (rxLen > 0) {
        context->offset += rxLen;
        if (context->length > rxLen) {
          context->length -= rxLen;
        } else {
          context->length = 0;
        }
      }
      if ((0 == context->length) && (NULL != context->data)) {
        ASLOG(SOAD, ("[%u] read %d bytes\n", SoConId, rxLen));
        PduInfo.SduDataPtr = context->data;
        PduInfo.SduLength = context->offset;
        PduInfo.MetaDataPtr = (uint8_t *)&context->RemoteAddr;
        conG->IF->RxIndication(connection->RxPduId, &PduInfo);
        if (NULL != context->data) {
          Net_MemFree(context->data);
          context->data = NULL;
        }
      }
    } else {
      ASLOG(SOADE, ("[%u] socket read failed\n", SoConId));
#if SOAD_ERROR_COUNTER_LIMIT > 0
      context->errorCounter++;
      if (context->errorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
        SoAd_CloseSoCon(SoConId, TRUE);
      }
#endif
    }
  }

  return ret;
}

static void soAdSocketAcceptMain(SoAd_SoConIdType SoConId) {
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  Std_ReturnType ret;
  TcpIp_SocketIdType SocketId;
  SoAd_SocketContextType *actCtx = NULL;
  TcpIp_SockAddrType RemoteAddr;
  int i;

  if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
    ret = TcpIp_TcpAccept(context->sock, &SocketId, &RemoteAddr);
    if (E_OK == ret) {
      ret = E_NOT_OK;
      for (i = 0; i < conG->numOfConnections; i++) {
        if (SOAD_SOCKET_CLOSED == SOAD_CONFIG->Contexts[i + conG->SoConId].state) {
          actCtx = &SOAD_CONFIG->Contexts[i + conG->SoConId];
          actCtx->sock = SocketId;
          actCtx->RemoteAddr = RemoteAddr;
          actCtx->state = SOAD_SOCKET_READY;
          ret = E_OK;
          break;
        }
      }
      if (E_OK != ret) {
        ASLOG(SOADE, ("[%u] accept failed as no free slot\n", SoConId));
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
    ASLOG(SOADE, ("[%u] UDP can't do accept\n", SoConId));
  }
}

static void soAdSocketReadyMain(SoAd_SoConIdType SoConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  Std_ReturnType ret = E_OK;

  if (context->flag & SOAD_TX_ON_GOING) {
    if (NULL != conG->IF->TxConfirmation) {
      conG->IF->TxConfirmation(connection->RxPduId, E_OK);
    }
    context->flag &= ~SOAD_TX_ON_GOING;
  }

  if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
    ret = TcpIp_IsTcpStatusOK(context->sock);
    if (E_OK != ret) {
      SoAd_CloseSoCon(SoConId, TRUE);
      ASLOG(SOADE, ("[%u] close, goto accept\n", SoConId));
    }
  }

  while (E_OK == ret) {
    if (0 == context->length) {
      ret = soAdSocketRecvStart(SoConId);
    } else {
      ret = soAdSocketRecvLeft(SoConId);
    }
  }
}

static void soAdSocketTakeControlMain(SoAd_SoConIdType SoConId) {
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  if (context->flag & SOAD_TX_ON_GOING) {
    context->flag &= ~SOAD_TX_ON_GOING;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void SoAd_Init(const SoAd_ConfigType *ConfigPtr) {
  int i;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;

  if (NULL != ConfigPtr) {
    soAdConfigPtr = ConfigPtr;
  } else {
    soAdConfigPtr = &SoAd_Config;
  }

  Net_MemInit();
  for (i = 0; i < SOAD_CONFIG->numOfConnections; i++) {
    connection = &SOAD_CONFIG->Connections[i];
    context = &SOAD_CONFIG->Contexts[i];
    memset(context, 0, sizeof(SoAd_SocketContextType));
    context->state = SOAD_SOCKET_CLOSED;
#if SOAD_ERROR_COUNTER_LIMIT > 0
    context->errorCounter = 0;
#endif
    if (connection->GID < SOAD_CONFIG->numOfGroups) {
      conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
      if ((FALSE == IS_CON_TYPE_OF(connection, SOAD_SOCON_TCP_ACCEPT)) &&
          conG->AutomaticSoConSetup) {
        context->state = SOAD_SOCKET_CREATE;
      }
      TcpIp_SetupAddrFrom(&context->RemoteAddr, conG->Remote, conG->Port);
      ASLOG(SOAD, ("[%u] init with %d.%d.%d.%d:%u\n", i, context->RemoteAddr.addr[0],
                   context->RemoteAddr.addr[1], context->RemoteAddr.addr[2],
                   context->RemoteAddr.addr[3], context->RemoteAddr.port));
    } else {
      ASLOG(SOADE, ("[%u] Invalid GID\n", i));
    }
  }
}

void SoAd_MainFunction(void) {
  uint16_t i;
  SoAd_SocketContextType *context;
  boolean bLinkedUp = TcpIp_IsLinkedUp();

  for (i = 0; (TRUE == bLinkedUp) && (i < SOAD_CONFIG->numOfConnections); i++) {
    context = &SOAD_CONFIG->Contexts[i];
    switch (context->state) {
    case SOAD_SOCKET_CREATE:
      soAdCreateSocket(i);
      break;
    case SOAD_SOCKET_ACCEPT:
      soAdSocketAcceptMain(i);
      break;
    case SOAD_SOCKET_READY:
      soAdSocketReadyMain(i);
      break;
    case SOAD_SOCKET_TAKEN_CONTROL:
      soAdSocketTakeControlMain(i);
      break;
    default:
      break;
    }
  }
}

Std_ReturnType SoAd_IfTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SoConIdType SoConId;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;
  TcpIp_SockAddrType addr;

  if (TxPduId < SOAD_CONFIG->numOfTxPduIds) {
    SoConId = SOAD_CONFIG->TxPduIdToSoCondIdMap[TxPduId];
    connection = &SOAD_CONFIG->Connections[SoConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_READY <= context->state) {
      if (0 == (context->flag & SOAD_TX_ON_GOING)) {
        ret = E_OK;
      }
    }
  }

  if (E_OK == ret) {
    if (TCPIP_IPPROTO_UDP == conG->ProtocolType) {
      if (PduInfoPtr->MetaDataPtr != NULL) {
        addr = *(const TcpIp_SockAddrType *)PduInfoPtr->MetaDataPtr;
        ret = TcpIp_SendTo(context->sock, &addr, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
      } else {
        TcpIp_SetupAddrFrom(&addr, conG->Remote, conG->Port);
        ret = TcpIp_SendTo(context->sock, &addr, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
      }
    } else {
      ret = TcpIp_Send(context->sock, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
    }

    if (E_OK == ret) {
      if (NULL != conG->IF->TxConfirmation) {
        context->flag |= SOAD_TX_ON_GOING;
      }
    }

#if SOAD_ERROR_COUNTER_LIMIT > 0
    if (E_OK != ret) {
      context->errorCounter++;
      if (context->errorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
        ASLOG(SOADE, ("[%u] If Tx failed, closing\n", SoConId));
        SoAd_CloseSoCon(SoConId, TRUE);
      }
    }
#endif
  }

  return ret;
}

Std_ReturnType SoAd_TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SoConIdType SoConId;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;

  if (TxPduId < SOAD_CONFIG->numOfTxPduIds) {
    SoConId = SOAD_CONFIG->TxPduIdToSoCondIdMap[TxPduId];
    connection = &SOAD_CONFIG->Connections[SoConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_READY <= context->state) {
      if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
        ret = TcpIp_Send(context->sock, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
#if SOAD_ERROR_COUNTER_LIMIT > 0
        if (E_OK != ret) {
          context->errorCounter++;
          if (context->errorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
            SoAd_CloseSoCon(SoConId, TRUE);
          }
        }
#endif
      }
    }
  }

  return ret;
}

Std_ReturnType SoAd_GetSoConId(PduIdType TxPduId, SoAd_SoConIdType *SoConIdPtr) {
  Std_ReturnType ret = E_NOT_OK;

  if ((TxPduId < SOAD_CONFIG->numOfTxPduIds) && (NULL != SoConIdPtr)) {
    *SoConIdPtr = SOAD_CONFIG->TxPduIdToSoCondIdMap[TxPduId];
    ret = E_OK;
  }

  return ret;
}

Std_ReturnType SoAd_GetLocalAddr(SoAd_SoConIdType SoConId, TcpIp_SockAddrType *LocalAddrPtr,
                                 uint8_t *NetmaskPtr, TcpIp_SockAddrType *DefaultRouterPtr) {
  Std_ReturnType ret = E_NOT_OK;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  SoAd_SocketContextType *context;
  if (SoConId < SOAD_CONFIG->numOfConnections) {
    connection = &SOAD_CONFIG->Connections[SoConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (IS_CON_TYPE_OF(connection, SOAD_SOCON_TCP_CLIENT | SOAD_SOCON_TCP_ACCEPT)) {
      /* for TCP client socket */
      if (context->state >= SOAD_SOCKET_READY) {
        ret = TcpIp_GetLocalAddr(context->sock, LocalAddrPtr);
      }
    } else if (IS_CON_TYPE_OF(connection, SOAD_SOCON_UDP_CLIENT)) {
      ret = TcpIp_GetIpAddr(conG->LocalAddrId, LocalAddrPtr, NULL, NULL);
      LocalAddrPtr->port = conG->LocalPort;
    } else if (IS_CON_TYPE_OF(connection, SOAD_SOCON_TCP_SERVER | SOAD_SOCON_UDP_SERVER)) {
      ret = TcpIp_GetIpAddr(conG->LocalAddrId, LocalAddrPtr, NULL, NULL);
      LocalAddrPtr->port = conG->Port;
    } else {
      ASLOG(SOADE, ("Not supported connection type\n"));
      ret = E_NOT_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_SetRemoteAddr(SoAd_SoConIdType SoConId,
                                  const TcpIp_SockAddrType *RemoteAddrPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_CLOSED == context->state) {
      context->RemoteAddr = *RemoteAddrPtr;
      ASLOG(SOAD, ("[%u] setup with %d.%d.%d.%d:%u\n", SoConId, context->RemoteAddr.addr[0],
                   context->RemoteAddr.addr[1], context->RemoteAddr.addr[2],
                   context->RemoteAddr.addr[3], context->RemoteAddr.port));
      ret = E_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_GetRemoteAddr(SoAd_SoConIdType SoConId, TcpIp_SockAddrType *IpAddrPtr) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_READY <= context->state) {
      *IpAddrPtr = context->RemoteAddr;
      ret = E_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_OpenSoCon(SoAd_SoConIdType SoConId) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_CLOSED == context->state) {
      context->flag = 0;
#if SOAD_ERROR_COUNTER_LIMIT > 0
      context->errorCounter = 0;
#endif
      context->state = SOAD_SOCKET_CREATE;
      ret = E_OK;
    } else {
      ASLOG(SOADE, ("[%u] open failed as already in state %d\n", SoConId, context->state));
    }
  }

  return ret;
}

Std_ReturnType SoAd_CloseSoCon(SoAd_SoConIdType SoConId, boolean abort) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;
  const SoAd_SocketConnectionType *connection;
  const SoAd_SocketConnectionGroupType *conG;
  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    connection = &SOAD_CONFIG->Connections[SoConId];
    conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
    if (SOAD_SOCKET_CLOSED != context->state) {
      if (conG->SoConModeChgNotification) {
        conG->SoConModeChgNotification(SoConId, SOAD_SOCON_OFFLINE);
      }
      ret = TcpIp_Close(context->sock, abort);
      if (E_OK == ret) {
        context->state = SOAD_SOCKET_CLOSED;
        TcpIp_SetupAddrFrom(&context->RemoteAddr, conG->Remote, conG->Port);
      } else {
        ASLOG(SOADE, ("[%u] close fail: %d\n", SoConId, ret));
      }
    }
  }

  return ret;
}

void SoAd_GetSoConMode(SoAd_SoConIdType SoConId, SoAd_SoConModeType *ModePtr) {
  SoAd_SocketContextType *context;
  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_CLOSED != context->state) {
      *ModePtr = SOAD_SOCON_ONLINE;
    } else {
      *ModePtr = SOAD_SOCON_OFFLINE;
    }
  }
}

Std_ReturnType SoAd_TakeControl(SoAd_SoConIdType SoConId) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_READY <= context->state) {
      context->state = SOAD_SOCKET_TAKEN_CONTROL;
      ret = E_OK;
    }
  }

  return ret;
}

Std_ReturnType SoAd_SetNonBlock(SoAd_SoConIdType SoConId, boolean nonBlocked) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_TAKEN_CONTROL == context->state) {
      ret = TcpIp_SetNonBlock(context->sock, nonBlocked);
    }
  }

  return ret;
}

Std_ReturnType SoAd_SetTimeout(SoAd_SoConIdType SoConId, uint32_t timeoutMs) {
  Std_ReturnType ret = E_NOT_OK;
  SoAd_SocketContextType *context;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_TAKEN_CONTROL == context->state) {
      ret = TcpIp_SetNonBlock(context->sock, FALSE);
      if (E_OK == ret) {
        ret = TcpIp_SetTimeout(context->sock, timeoutMs);
      }
    }
  }

  return ret;
}

Std_ReturnType SoAd_ControlRecv(SoAd_SoConIdType SoConId, uint8_t *data, uint32_t *length) {
  SoAd_SocketContextType *context;
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  Std_ReturnType ret = E_NOT_OK;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_TAKEN_CONTROL == context->state) {
      if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
        ret = TcpIp_Recv(context->sock, data, length);
        if (E_OK != ret) {
          ASLOG(SOADE, ("[%u] TCP read failed\n", SoConId));
#if SOAD_ERROR_COUNTER_LIMIT > 0
          context->errorCounter++;
          if (context->errorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
            SoAd_CloseSoCon(SoConId, TRUE);
          }
#endif
        }
      } else {
        ret = TcpIp_RecvFrom(context->sock, &context->RemoteAddr, data, length);
      }
    }
  }

  return ret;
}

void SoAd_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x02, SOAD_E_PARAM_POINTER, return);

  versionInfo->vendorID = STD_VENDOR_ID_AS;
  versionInfo->moduleID = MODULE_ID_SOAD;
  versionInfo->sw_major_version = 4;
  versionInfo->sw_minor_version = 0;
  versionInfo->sw_patch_version = 0;
}
