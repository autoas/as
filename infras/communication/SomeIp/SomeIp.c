/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SomeIp.h"
#include "SomeIp_Priv.h"
#include "SomeIp_Cfg.h"
#include "Std_Debug.h"
#include "Sd.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SOMEIP 0
#define AS_LOG_SOMEIPI 2
#define AS_LOG_SOMEIPE 3

/* @SWS_SomeIpXf_00031 */
#define SOMEIP_MSG_REQUEST 0x00
#define SOMEIP_MSG_REQUEST_NO_RETURN 0x01
#define SOMEIP_MSG_NOTIFICATION 0x02
#define SOMEIP_MSG_RESPONSE 0x80
#define SOMEIP_MSG_ERROR 0x81

#define SOMEIP_MSG_REQUEST_ACK 0x40
#define SOMEIP_MSG_REQUEST_NO_RETURN_ACK 0x41
#define SOMEIP_MSG_NOTIFICATION_ACK 0x42

#define SOMEIP_MSG_RESPONSE_ACK 0xC0
#define SOMEIP_MSG_ERROR_ACK 0xC1

#define SOMEIP_CONFIG (&SomeIp_Config)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_SomeIpXf_00152 */
typedef struct {
  uint16_t serviceId;
  uint16_t methodId;
  uint32_t length;
  uint16_t clientId;
  uint16_t sessionId;
  uint8_t interfaceVersion;
  uint8_t messageType;
  uint8_t returnCode;
} SomeIp_HeaderType;
/* ================================ [ DECLARES  ] ============================================== */
extern const SomeIp_ConfigType SomeIp_Config;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static Std_ReturnType SomeIp_DecodeHeader(const uint8_t *data, uint32_t length,
                                          SomeIp_HeaderType *header) {
  Std_ReturnType ret = E_OK;
  if (length >= 16) {
    if (data[12] != 1) { /* @SWS_SomeIpXf_00029 */
      ASLOG(SOMEIP, ("invlaid protocol version\n"));
      ret = SOMEIPXF_E_WRONG_PROTOCOL_VERSION;
    } else {
      header->length =
        ((uint32_t)data[4] << 24) + ((uint32_t)data[5] << 16) + ((uint32_t)data[6] << 8) + data[7];
      if ((header->length + 8) == length) {
        header->serviceId = ((uint16_t)data[0] << 8) + data[1];
        header->methodId = ((uint16_t)data[2] << 8) + data[3];
        header->clientId = ((uint16_t)data[8] << 8) + data[9];
        header->sessionId = ((uint16_t)data[10] << 8) + data[11];
        header->interfaceVersion = data[13];
        header->messageType = data[14];
        header->returnCode = data[15];
      } else {
        ASLOG(SOMEIP, ("length not valid\n"));
        ret = SOMEIPXF_E_MALFORMED_MESSAGE;
      }
    }
  } else {
    ASLOG(SOMEIP, ("message too short\n"));
    ret = SOMEIPXF_E_MALFORMED_MESSAGE;
  }
  return ret;
}

static void SomeIp_BuildHeader(uint8_t *header, uint16_t serviceId, uint16_t methodId,
                               uint16_t clientId, uint16_t sessionId, uint8_t interfaceVersion,
                               uint8_t messageType, uint8_t returnCode, uint32_t payloadLength) {
  uint32_t length = payloadLength + 8;
  header[0] = (serviceId >> 8) & 0xFF;
  header[1] = serviceId & 0xFF;
  header[2] = (methodId >> 8) & 0xFF;
  header[3] = methodId & 0xFF;
  header[4] = (length >> 24) & 0xFF;
  header[5] = (length >> 16) & 0xFF;
  header[6] = (length >> 8) & 0xFF;
  header[7] = length & 0xFF;
  header[8] = (clientId >> 8) & 0xFF;
  header[9] = clientId & 0xFF;
  header[10] = (sessionId >> 8) & 0xFF;
  header[11] = sessionId & 0xFF;
  header[12] = 1;
  header[13] = interfaceVersion;
  header[14] = messageType;
  header[15] = returnCode;
}

static void SomeIp_InitServer(const SomeIp_ServerServiceType *config) {
  int i;
  for (i = 0; i < config->numOfConnections; i++) {
    memset(config->connections[i].context, 0, sizeof(SomeIp_ServerServiceContextType));
  }
}

static void SomeIp_InitClient(const SomeIp_ClientServiceType *config) {
  SomeIp_ClientServiceContextType *context = config->context;
  memset(context, 0, sizeof(SomeIp_ClientServiceContextType));
}

static Std_ReturnType SomeIp_HandleServerMessage_Request(const SomeIp_ServerServiceType *config,
                                                         uint16_t conId, SomeIp_HeaderType *header,
                                                         const uint8_t *payload, uint32_t length) {
  Std_ReturnType ret;
  const SomeIp_ServerConnectionType *connection = &config->connections[conId];
  SomeIp_ServerServiceContextType *context = connection->context;

  const SomeIp_ServerMethodType *method = NULL;
  uint32_t resLen = connection->bufferLen - 16;
  int i;
  for (i = 0; i < config->numOfMethods; i++) {
    if (config->methods[i].methodId == header->methodId) {
      if (((0xFF == config->methods[i].interfaceVersion) ||
           (config->methods[i].interfaceVersion == header->interfaceVersion))) {
        method = &config->methods[i];
        break;
      } else {
        ret = SOMEIPXF_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (NULL != method) {
    ret = method->requestFnc(payload, length, &connection->buffer[16], &resLen);
    if (E_OK == ret) {
      SomeIp_BuildHeader(connection->buffer, config->serviceId, method->methodId, header->clientId,
                         header->sessionId, method->interfaceVersion, SOMEIP_MSG_RESPONSE, ret,
                         resLen);
      context->txLen = resLen + 16;
    } else if (SOMEIP_E_PENDING == ret) {
      /* response pending */
    } else {
      /* sending Nack */
    }
  } else {
    ret = SOMEIPXF_E_UNKNOWN_METHOD;
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleClientMessage_Respose(const SomeIp_ClientServiceType *config,
                                                         SomeIp_HeaderType *header,
                                                         const uint8_t *payload, uint32_t length) {
  Std_ReturnType ret;
  const SomeIp_ClientMethodType *method = NULL;
  int i;
  for (i = 0; i < config->numOfMethods; i++) {
    if (config->methods[i].methodId == header->methodId) {
      if (((0xFF == config->methods[i].interfaceVersion) ||
           (config->methods[i].interfaceVersion == header->interfaceVersion))) {
        method = &config->methods[i];
        break;
      } else {
        ret = SOMEIPXF_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (NULL != method) {
    ret = method->responseFnc(payload, length);
  } else {
    ret = SOMEIPXF_E_UNKNOWN_METHOD;
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleServerMessage(const SomeIp_ServerServiceType *config,
                                                 uint16_t conId,
                                                 const TcpIp_SockAddrType *RemoteAddr,
                                                 SomeIp_HeaderType *header, const uint8_t *payload,
                                                 uint32_t length) {
  Std_ReturnType ret = E_OK;
  PduInfoType PduInfo;

  if (conId < config->numOfConnections) {

    switch (header->messageType) {
    case SOMEIP_MSG_REQUEST:
      ret = SomeIp_HandleServerMessage_Request(config, conId, header, payload, length);
      break;
    default:
      ASLOG(SOMEIPE, ("unsupported message type 0x%x\n", header->messageType));
      ret = SOMEIPXF_E_WRONG_MESSAGE_TYPE;
      break;
    }
  } else {
    ASLOG(SOMEIPE, ("invalid connection ID\n"));
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    PduInfo.MetaDataPtr = (uint8_t *)RemoteAddr;
    PduInfo.SduDataPtr = config->connections[conId].buffer;
    PduInfo.SduLength = config->connections[conId].context->txLen;
    (void)SoAd_IfTransmit(config->connections[conId].TxPduId, &PduInfo);
  }

  return ret;
}

static Std_ReturnType
SomeIp_HandleClientMessage_Notification(const SomeIp_ClientServiceType *config,
                                        SomeIp_HeaderType *header, const uint8_t *payload,
                                        uint32_t length) {
  Std_ReturnType ret;
  const SomeIp_ClientEventType *event = NULL;
  int i;
  for (i = 0; i < config->numOfEvents; i++) {
    if (config->events[i].eventId == header->methodId) {
      if (((0xFF == config->events[i].interfaceVersion) ||
           (config->events[i].interfaceVersion == header->interfaceVersion))) {
        event = &config->events[i];
        break;
      } else {
        ret = SOMEIPXF_E_WRONG_INTERFACE_VERSION;
      }
    }
  }

  if (NULL != event) {
    ret = event->notifyFnc(payload, length);
  } else {
    ret = SOMEIPXF_E_UNKNOWN_METHOD;
  }

  return ret;
}

static Std_ReturnType SomeIp_HandleClientMessage(const SomeIp_ClientServiceType *config,
                                                 const TcpIp_SockAddrType *RemoteAddr,
                                                 SomeIp_HeaderType *header, const uint8_t *payload,
                                                 uint32_t length) {
  Std_ReturnType ret = E_OK;

  switch (header->messageType) {
  case SOMEIP_MSG_NOTIFICATION:
    ret = SomeIp_HandleClientMessage_Notification(config, header, payload, length);
    break;
  case SOMEIP_MSG_RESPONSE:
    ret = SomeIp_HandleClientMessage_Respose(config, header, payload, length);
    break;
  default:
    ASLOG(SOMEIP, ("unsupported message type 0x%x\n", header->messageType));
    ret = SOMEIPXF_E_WRONG_MESSAGE_TYPE;
    break;
  }

  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void SomeIp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  SomeIp_HeaderType header;
  Std_ReturnType ret;
  uint16_t index;
  const TcpIp_SockAddrType *RemoteAddr = (const TcpIp_SockAddrType *)PduInfoPtr->MetaDataPtr;

  ret = SomeIp_DecodeHeader(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength, &header);
  if (E_OK == ret) {
    ASLOG(SOMEIP,
          ("[%d] service 0x%x:0x%x:%d message type %d return code %d payload %d bytes from client "
           "0x%x:%d\n",
           RxPduId, header.serviceId, header.methodId, header.interfaceVersion, header.messageType,
           header.returnCode, header.length - 8, header.clientId, header.sessionId));
    if (RxPduId < SOMEIP_CONFIG->numOfPIDs) {
      index = SOMEIP_CONFIG->PID2ServiceMap[RxPduId];
      if (index < SOMEIP_CONFIG->numOfService) {
        if (SOMEIP_CONFIG->services[index].isServer) {
          ret = SomeIp_HandleServerMessage(
            (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[index].service,
            SOMEIP_CONFIG->PID2ServiceConnectionMap[RxPduId], RemoteAddr, &header,
            &PduInfoPtr->SduDataPtr[16], header.length - 8);
        } else {
          ret = SomeIp_HandleClientMessage(
            (const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[index].service, RemoteAddr,
            &header, &PduInfoPtr->SduDataPtr[16], header.length - 8);
        }
      } else {
        ret = E_NOT_OK;
        ASLOG(SOMEIPE, ("invalid configuration for PID2ServiceMap\n"));
      }
    } else {
      ret = SOMEIPXF_E_NOT_REACHABLE;
    }
  }
}

void SomeIp_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode) {
  ASLOG(SOMEIP, ("SoConId %d Mode %d\n", SoConId, Mode));
}

Std_ReturnType SomeIp_Request(uint16_t TxMethodId, uint8_t *data, uint32_t length) {
  Std_ReturnType ret = E_OK;
  const SomeIp_ClientServiceType *config;
  SomeIp_ClientServiceContextType *context;
  const SomeIp_ClientMethodType *method;
  uint16_t index;
  PduInfoType PduInfo;
  TcpIp_SockAddrType RemoteAddr;

  if (TxMethodId < SOMEIP_CONFIG->numOfTxMethods) {
    index = SOMEIP_CONFIG->TxMethod2ServiceMap[TxMethodId];
    config = (const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[index].service;
    if ((length + 16) > config->bufferLen) {
      ret = E_NOT_OK;
    } else {
      ret = Sd_GetProviderAddr(config->sdHandleID, &RemoteAddr);
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    context = config->context;
    if (0 == context->sessionId) {
      context->sessionId = 1;
    }
    index = SOMEIP_CONFIG->TxMethod2PerServiceMap[TxMethodId];
    method = &config->methods[index];
    SomeIp_BuildHeader(config->buffer, config->serviceId, method->methodId, config->clientId,
                       context->sessionId, method->interfaceVersion, SOMEIP_MSG_REQUEST, 0, length);
    memcpy(&config->buffer[16], data, length);
    PduInfo.MetaDataPtr = (uint8_t *)&RemoteAddr;
    PduInfo.SduDataPtr = config->buffer;
    PduInfo.SduLength = length + 16;
    ret = SoAd_IfTransmit(config->TxPduId, &PduInfo);
    if (E_OK == ret) {
      context->sessionId++;
    } else {
      ASLOG(SOMEIPE, ("Failed to request service method %x:%x to %d.%d.%d.%d:%d\n",
                      config->serviceId, method->methodId, RemoteAddr.addr[0], RemoteAddr.addr[1],
                      RemoteAddr.addr[2], RemoteAddr.addr[3], RemoteAddr.port));
    }
  }

  return ret;
}

Std_ReturnType SomeIp_Notification(uint16_t TxEventId, uint8_t *data, uint32_t length) {
  Std_ReturnType ret = E_OK;
  Std_ReturnType ret2 = E_OK;
  const SomeIp_ServerServiceType *config;
  const SomeIp_ServerConnectionType *connection;
  SomeIp_ServerServiceContextType *context;
  const SomeIp_ServerEventType *event;
  uint16_t index;
  PduInfoType PduInfo;
  Sd_EventHandlerSubscriberType *Subscribers;
  Sd_EventHandlerSubscriberType *sub;
  uint16_t numOfSubscribers;
  int i;

  if (TxEventId < SOMEIP_CONFIG->numOfTxEvents) {
    index = SOMEIP_CONFIG->TxEvent2ServiceMap[TxEventId];
    config = (const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[index].service;
    /* TODO: going to support TCP, now this only works for UDP */
    connection = &config->connections[0];
    if ((length + 16) > connection->bufferLen) {
      ret = E_NOT_OK;
    }
  } else {
    ret = E_NOT_OK;
  }

  if (E_OK == ret) {
    context = connection->context;
    index = SOMEIP_CONFIG->TxEvent2PerServiceMap[TxEventId];
    event = &config->events[index];
    ret = Sd_GetSubscribers(event->sdHandleID, &Subscribers, &numOfSubscribers);
  }

  if (E_OK == ret) {
    SomeIp_BuildHeader(connection->buffer, config->serviceId, event->eventId, config->clientId,
                       context->sessionId, event->interfaceVersion, SOMEIP_MSG_NOTIFICATION, 0,
                       length);
    memcpy(&connection->buffer[16], data, length);
    for (i = 0; i < numOfSubscribers; i++) {
      sub = &Subscribers[i];
      if (0 != sub->flags) {
        if (0 == sub->sessionId) {
          sub->sessionId = 1;
        }
        PduInfo.MetaDataPtr = (uint8_t *)&sub->RemoteAddr;
        PduInfo.SduDataPtr = connection->buffer;
        PduInfo.SduLength = length + 16;
        ret2 = SoAd_IfTransmit(connection->TxPduId, &PduInfo);
        if (E_OK == ret2) {
          sub->sessionId++;
        } else {
          ret = E_NOT_OK;
          ASLOG(SOMEIPE, ("Failed to notify event %x:%x to %d.%d.%d.%d:%d\n", config->serviceId,
                          event->eventId, sub->RemoteAddr.addr[0], sub->RemoteAddr.addr[1],
                          sub->RemoteAddr.addr[2], sub->RemoteAddr.addr[3], sub->RemoteAddr.port));
          /* TODO: cache data and schedule transmit next time */
        }
      }
    }
  }

  return ret;
}

void SomeIp_Init(const SomeIp_ConfigType *ConfigPtr) {
  int i;
  for (i = 0; i < SOMEIP_CONFIG->numOfService; i++) {
    if (SOMEIP_CONFIG->services[i].isServer) {
      SomeIp_InitServer((const SomeIp_ServerServiceType *)SOMEIP_CONFIG->services[i].service);
    } else {
      SomeIp_InitClient((const SomeIp_ClientServiceType *)SOMEIP_CONFIG->services[i].service);
    }
  }
}
