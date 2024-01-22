/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Socket Adaptor AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SoAd.h"
#include "SoAd_Priv.h"
#include "Std_Debug.h"
#include <string.h>
#include <stdio.h>
#include "NetMem.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SOAD 0
#define AS_LOG_SOADE 2

#define IS_CON_TYPE_OF(con, mask) (0 != ((con)->SoConType & (mask)))

#define SOAD_TX_ON_GOING 0x01

#define SOAD_CONFIG (soAdConfigPtr)
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
  Std_ReturnType ret = E_OK;

  sockId = TcpIp_Create(conG->ProtocolType);
  if (sockId >= 0) {
    if (IS_CON_TYPE_OF(connection,
                       SOAD_SOCON_TCP_SERVER | SOAD_SOCON_UDP_SERVER | SOAD_SOCON_UDP_CLIENT)) {
      ret = TcpIp_Bind(sockId, conG->LocalAddrId, &context->RemoteAddr.port);
      if ((E_OK == ret) && conG->IsMulitcast) {
        ret = TcpIp_AddToMulticast(sockId, &context->RemoteAddr);
      }
      if (E_OK != ret) {
        TcpIp_Close(sockId, TRUE);
      }
    }
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
    }
    context->sock = sockId;
    if (conG->SoConModeChgNotification) {
      conG->SoConModeChgNotification(SoConId, SOAD_SOCON_ONLINE);
    }
    ASLOG(SOAD, ("[%d] create TcpIP socket %d, next state %d\n", SoConId, sockId, context->state));
  } else {
    ASLOG(SOADE, ("[%d] failed to creat socket with error %d\n", SoConId, sockId));
    context->state = SOAD_SOCKET_CLOSED;
  }
}

static void soAdSocketIfRxNotify(SoAd_SocketContextType *context,
                                 const SoAd_SocketConnectionType *connection, uint8_t *data,
                                 uint16_t rxLen) {
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  const SoAd_IfInterfaceType *IF = (const SoAd_IfInterfaceType *)conG->Interface;
  PduInfoType PduInfo;
  PduInfo.SduDataPtr = data;
  PduInfo.SduLength = rxLen;
  PduInfo.MetaDataPtr = (uint8_t *)&context->RemoteAddr;

  if (IF->IfRxIndication) {
    IF->IfRxIndication(connection->RxPduId, &PduInfo);
  }
}

static void soAdSocketTpRxNotify(SoAd_SocketContextType *context,
                                 const SoAd_SocketConnectionType *connection, uint8_t *data,
                                 uint16_t rxLen) {
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  const SoAd_TpInterfaceType *IF = (const SoAd_TpInterfaceType *)conG->Interface;
  PduInfoType PduInfo;
  PduInfo.SduDataPtr = data;
  PduInfo.SduLength = rxLen;
  PduInfo.MetaDataPtr = (uint8_t *)&context->RemoteAddr;
  PduLengthType bufferSize;

  if (IF->TpCopyRxData) {
    IF->TpCopyRxData(connection->RxPduId, &PduInfo, &bufferSize);
  }
}

static Std_ReturnType soAdSocketUdpReadyMain(SoAd_SoConIdType SoConId, uint8_t *dataIn,
                                             uint32_t length) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  Std_ReturnType ret = E_NOT_OK;
  uint32_t rxLen;
  uint8_t *data = NULL;

  if (NULL == dataIn) {
    rxLen = TcpIp_Tell(context->sock);
    if (rxLen > 0) {
      data = Net_MemAlloc((uint32_t)rxLen);
      if (NULL != data) {
        ret = E_OK;
      } else {
        ASLOG(SOADE, ("[%d] Failed to malloc for %d\n", SoConId, rxLen));
      }
    }
  } else {
    data = dataIn;
    rxLen = length;
    ret = E_OK;
  }

  if (E_OK == ret) {
    ret = TcpIp_RecvFrom(context->sock, &context->RemoteAddr, data, &rxLen);
    if (E_OK == ret) {
      if (rxLen > 0) {
        ASLOG(SOAD, ("[%d] UDP read %d bytes\n", SoConId, rxLen));
        if (conG->IsTP) {
          soAdSocketTpRxNotify(context, connection, data, rxLen);
        } else {
          soAdSocketIfRxNotify(context, connection, data, rxLen);
        }
      }
    } else {
      ASLOG(SOADE, ("[%d] UDP read failed\n", SoConId));
    }
    if (data != dataIn) {
      Net_MemFree(data);
    }
  }

  return ret;
}

static Std_ReturnType soAdSocketTcpReadyMain(SoAd_SoConIdType SoConId, uint8_t *dataIn,
                                             uint32_t length) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  Std_ReturnType ret;
  uint32_t rxLen;
  uint8_t *data = NULL;

  ret = TcpIp_IsTcpStatusOK(context->sock);
  if (E_OK == ret) {
    if (NULL == dataIn) {
      rxLen = TcpIp_Tell(context->sock);
      if (rxLen > 0) {
        data = Net_MemAlloc((uint32_t)rxLen);
        if (NULL == data) {
          ret = E_NOT_OK;
          ASLOG(SOADE, ("[%d] Failed to malloc for %d\n", SoConId, rxLen));
        }
      } else {
        ret = E_NOT_OK;
      }
    } else {
      data = dataIn;
      rxLen = length;
    }
    if (E_OK == ret) {
      ret = TcpIp_Recv(context->sock, data, &rxLen);
      if (E_OK == ret) {
        if (rxLen > 0) {
          ASLOG(SOAD, ("[%d] TCP read %d bytes\n", SoConId, rxLen));
          if (conG->IsTP) {
            soAdSocketTpRxNotify(context, connection, data, rxLen);
          } else {
            soAdSocketIfRxNotify(context, connection, data, rxLen);
          }
        }
      } else {
        ASLOG(SOADE, ("[%d] TCP read failed\n", SoConId));
      }
      if (data != dataIn) {
        Net_MemFree(data);
      }
    }
  } else {
    TcpIp_Close(context->sock, TRUE);
    if (conG->SoConModeChgNotification) {
      conG->SoConModeChgNotification(SoConId, SOAD_SOCON_OFFLINE);
    }
    context->state = SOAD_SOCKET_CLOSED;
    ASLOG(SOADE, ("[%d] close, goto accept\n", SoConId));
  }

  return ret;
}

static void soAdSocketAcceptMain(SoAd_SoConIdType SoConId) {
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  const SoAd_TpInterfaceType *IF = (const SoAd_TpInterfaceType *)conG->Interface;
  PduInfoType PduInfo;
  PduLengthType bufferSize;
  Std_ReturnType ret;
  TcpIp_SocketIdType SocketId;
  SoAd_SocketContextType *actCtx = NULL;
  const SoAd_SocketConnectionType *actCnt = NULL;
  TcpIp_SockAddrType RemoteAddr;
  int i;

  if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
    ret = TcpIp_TcpAccept(context->sock, &SocketId, &RemoteAddr);
    if (E_OK == ret) {
      ret = E_NOT_OK;
      for (i = 0; i < conG->numOfConnections; i++) {
        if (SOAD_SOCKET_CLOSED == SOAD_CONFIG->Contexts[i + conG->SoConId].state) {
          actCtx = &SOAD_CONFIG->Contexts[i + conG->SoConId];
          actCnt = &SOAD_CONFIG->Connections[i + conG->SoConId];
          actCtx->sock = SocketId;
          actCtx->RemoteAddr = RemoteAddr;
          actCtx->state = SOAD_SOCKET_READY;
          ret = E_OK;
          break;
        }
      }
      if (E_OK != ret) {
        ASLOG(SOADE, ("[%d] accept failed as no free slot\n", SoConId));
        TcpIp_Close(SocketId, TRUE);
      } else {
        ASLOG(SOAD, ("[%d] accept as new socket\n", i + conG->SoConId));
      }
    }
    if (E_OK == ret) {
      if (conG->SoConModeChgNotification) {
        conG->SoConModeChgNotification(i + conG->SoConId, SOAD_SOCON_ONLINE);
      }
      if (conG->IsTP && IF->TpStartOfReception) {
        PduInfo.SduDataPtr = NULL;
        PduInfo.SduLength = 0;
        PduInfo.MetaDataPtr = (uint8_t *)&actCtx->RemoteAddr;
        IF->TpStartOfReception(actCnt->RxPduId, &PduInfo, 0, &bufferSize);
      }
    }
  } else {
    ASLOG(SOADE, ("[%d] UDP can't do accept\n", SoConId));
  }
}

static void soAdSocketReadyMain(SoAd_SoConIdType SoConId) {
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  const SoAd_TpInterfaceType *tpIF = (const SoAd_TpInterfaceType *)conG->Interface;
  const SoAd_IfInterfaceType *ifIF = (const SoAd_IfInterfaceType *)conG->Interface;
  SoAd_SocketContextType *context = &SOAD_CONFIG->Contexts[SoConId];
  Std_ReturnType ret = E_OK;

  if (context->flag & SOAD_TX_ON_GOING) {
    if (conG->IsTP) {
      if (tpIF->TpTxConfirmation) {
        tpIF->TpTxConfirmation(connection->RxPduId, E_OK);
      }
    } else {
      if (ifIF->IfTxConfirmation) {
        ifIF->IfTxConfirmation(connection->RxPduId, E_OK);
      }
    }
    context->flag &= ~SOAD_TX_ON_GOING;
  }

  while (E_OK == ret) {
    if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
      ret = soAdSocketTcpReadyMain(SoConId, NULL, 0);
    } else {
      ret = soAdSocketUdpReadyMain(SoConId, NULL, 0);
    }
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
    } else {
      ASLOG(SOADE, ("[%d] Invalid GID\n", i));
    }
  }
}

void SoAd_MainFunction(void) {
  int i;
  SoAd_SocketContextType *context;

  for (i = 0; i < SOAD_CONFIG->numOfConnections; i++) {
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
      if (conG->IsTP) {
        if (((const SoAd_TpInterfaceType *)conG->Interface)->TpTxConfirmation) {
          context->flag |= SOAD_TX_ON_GOING;
        }
      } else {
        if (((const SoAd_IfInterfaceType *)conG->Interface)->IfTxConfirmation) {
          context->flag |= SOAD_TX_ON_GOING;
        }
      }
    }

#if SOAD_ERROR_COUNTER_LIMIT > 0
    if (E_OK != ret) {
      context->errorCounter++;
      if (context->errorCounter >= SOAD_ERROR_COUNTER_LIMIT) {
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
    } else { /* SOAD_SOCON_UDP_CLIENT */
      ret = TcpIp_GetIpAddr(conG->LocalAddrId, LocalAddrPtr, NULL, NULL);
      LocalAddrPtr->port = conG->Port;
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
      ASLOG(SOADE, ("[%d] open failed as already in state %d\n", SoConId, context->state));
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
        ASLOG(SOADE, ("[%d] close fail: %d\n", SoConId, ret));
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

Std_ReturnType SoAd_ControlRx(SoAd_SoConIdType SoConId, uint8_t *data, uint32_t length) {
  SoAd_SocketContextType *context;
  const SoAd_SocketConnectionType *connection = &SOAD_CONFIG->Connections[SoConId];
  const SoAd_SocketConnectionGroupType *conG = &SOAD_CONFIG->ConnectionGroups[connection->GID];
  Std_ReturnType ret = E_NOT_OK;

  if (SoConId < SOAD_CONFIG->numOfConnections) {
    context = &SOAD_CONFIG->Contexts[SoConId];
    if (SOAD_SOCKET_TAKEN_CONTROL == context->state) {
      if (TCPIP_IPPROTO_TCP == conG->ProtocolType) {
        ret = soAdSocketTcpReadyMain(SoConId, data, length);
      } else {
        ret = soAdSocketUdpReadyMain(SoConId, data, length);
      }
    }
  }

  return ret;
}