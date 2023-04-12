/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic over IP AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "DoIP.h"
#include "DoIP_Priv.h"
#include "SoAd.h"

#include "PduR_DoIP.h"

#include "Std_Debug.h"
#include <string.h>
#include "NetMem.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DOIP 0
#define AS_LOG_DOIPE 2

#define DOIP_PROTOCOL_VERSION 2
#define DOIP_HEADER_LENGTH 8u

#define DOIP_CONFIG (&DoIP_Config)

#ifndef DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE
#define DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE (DOIP_HEADER_LENGTH + 40)
#endif

/* return this when negative response buffer set */
#define DOIP_E_NOT_OK ((Std_ReturnType)200)

#define DOIP_E_NOT_OK_SILENT ((Std_ReturnType)201)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  const uint8_t *req;
  uint8_t *res; /* response buffer for this message */
  uint16_t payloadType;
  uint32_t payloadLength;
  PduLengthType reqLen;
  PduLengthType resLen;
} DoIP_MsgType;

typedef struct {
  DoIP_ActivationLineType ActivationLineState;
} DoIP_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern const DoIP_ConfigType DoIP_Config;
/* ================================ [ DATAS     ] ============================================== */
static DoIP_ContextType DoIP_Context;
/* ================================ [ LOCALS    ] ============================================== */
static void doipFillHeader(uint8_t *header, uint16_t payloadType, uint32_t payloadLength) {
  header[0] = DOIP_PROTOCOL_VERSION;
  header[1] = ~DOIP_PROTOCOL_VERSION;
  header[2] = (payloadType >> 8) & 0xFF;
  header[3] = payloadType & 0xFF;
  header[4] = (payloadLength >> 24) & 0xFF;
  header[5] = (payloadLength >> 16) & 0xFF;
  header[6] = (payloadLength >> 8) & 0xFF;
  header[7] = payloadLength & 0xFF;
}

static Std_ReturnType doipDecodeMsg(const PduInfoType *PduInfoPtr, DoIP_MsgType *msg) {
  Std_ReturnType ret = E_NOT_OK;

  if ((PduInfoPtr->SduLength >= DOIP_HEADER_LENGTH) && (PduInfoPtr->SduDataPtr != NULL)) {
    /* @SWS_DoIP_00005, @SWS_DoIP_00006 */
    if ((DOIP_PROTOCOL_VERSION == PduInfoPtr->SduDataPtr[0]) &&
        (PduInfoPtr->SduDataPtr[0] = ((~PduInfoPtr->SduDataPtr[1])) & 0xFF)) {

      msg->payloadType = ((uint16_t)PduInfoPtr->SduDataPtr[2] << 8) + PduInfoPtr->SduDataPtr[3];
      msg->payloadLength = ((uint32_t)PduInfoPtr->SduDataPtr[4] << 24) +
                           ((uint32_t)PduInfoPtr->SduDataPtr[5] << 16) +
                           ((uint32_t)PduInfoPtr->SduDataPtr[6] << 8) + PduInfoPtr->SduDataPtr[7];
      msg->req = &PduInfoPtr->SduDataPtr[DOIP_HEADER_LENGTH];
      msg->reqLen = PduInfoPtr->SduLength - DOIP_HEADER_LENGTH;
      ret = E_OK;
    }
  }
  return ret;
}

static PduLengthType doipSetupVehicleAnnouncementResponse(uint8_t *data) {
  Std_ReturnType ret;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  doipFillHeader(data, DOIP_VAN_MSG_OR_VIN_RESPONCE, 33);
  /* @SWS_DoIP_00072 */
  ret = config->GetVin(&data[DOIP_HEADER_LENGTH]);
  if (E_OK != ret) {
    memset(&data[DOIP_HEADER_LENGTH], config->VinInvalidityPattern, 17);
    data[DOIP_HEADER_LENGTH + 31] = 0x00; /* Further action byte */
    data[DOIP_HEADER_LENGTH + 32] = 0x10; /* VIN/GID Status */
  } else {
    data[DOIP_HEADER_LENGTH + 31] = 0x00;
    data[DOIP_HEADER_LENGTH + 32] = 0x00;
  }

  data[DOIP_HEADER_LENGTH + 17] = (config->LogicalAddress >> 8) & 0xFF;
  data[DOIP_HEADER_LENGTH + 18] = config->LogicalAddress & 0xFF;

  config->GetEID(&data[DOIP_HEADER_LENGTH + 19]);
  config->GetGID(&data[DOIP_HEADER_LENGTH + 25]);

  return DOIP_HEADER_LENGTH + 33;
}

Std_ReturnType doipTpSendResponse(PduIdType RxPduId, uint8_t *data, PduLengthType length) {
  PduInfoType PduInfo;
  Std_ReturnType ret;
  if (length > 0) {
    PduInfo.SduDataPtr = data;
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduLength = length;
    ret = SoAd_TpTransmit(RxPduId, &PduInfo);
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

static Std_ReturnType doipHandleVIDRequest(PduIdType RxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  if (0 == msg->payloadLength) {
    msg->resLen = doipSetupVehicleAnnouncementResponse(msg->res);
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle VID Request\n", RxPduId));
  return ret;
}

static Std_ReturnType doipHandleVIDRequestWithEID(PduIdType RxPduId, DoIP_MsgType *msg,
                                                  uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t myEID[6];
  if (6 == msg->payloadLength) {
    config->GetEID(myEID);
    if (0 == memcmp(myEID, msg->req, 6)) {
      msg->resLen = doipSetupVehicleAnnouncementResponse(msg->res);
    } else {
      ret = DOIP_E_NOT_OK_SILENT;
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle VID Request with EID\n", RxPduId));
  return ret;
}

static Std_ReturnType doipHandleVIDRequestWithVIN(PduIdType RxPduId, DoIP_MsgType *msg,
                                                  uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t myVIN[17];

  if (17 == msg->payloadLength) {
    ret = config->GetVin(myVIN);
    if (E_OK != ret) {
      ret = DOIP_E_NOT_OK_SILENT;
    } else {
      if (0 == memcmp(myVIN, msg->req, 17)) {
        msg->resLen = doipSetupVehicleAnnouncementResponse(msg->res);
      } else {
        ret = DOIP_E_NOT_OK_SILENT;
      }
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle VID Request with VIN\n", RxPduId));
  return ret;
}

static Std_ReturnType doipHandleEntityStatusRequest(PduIdType RxPduId, DoIP_MsgType *msg,
                                                    uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t curOpened = 0;
  uint16_t i;

  if (0 == msg->payloadLength) {
    msg->res[DOIP_HEADER_LENGTH] = config->NodeType;
    msg->res[DOIP_HEADER_LENGTH + 1] = config->MaxTesterConnections;
    for (i = 0; i < config->MaxTesterConnections; i++) {
      if (DOIP_CON_OPEN == config->testerConnections[i].context->state) {
        curOpened++;
      }
    }
    msg->res[DOIP_HEADER_LENGTH + 2] = curOpened;
    if (config->EntityStatusMaxByteFieldUse) {
      msg->res[DOIP_HEADER_LENGTH + 3] = (MEMPOOL_NET_MAX_SIZE >> 24) & 0xFF;
      msg->res[DOIP_HEADER_LENGTH + 4] = (MEMPOOL_NET_MAX_SIZE >> 16) & 0xFF;
      msg->res[DOIP_HEADER_LENGTH + 5] = (MEMPOOL_NET_MAX_SIZE >> 8) & 0xFF;
      msg->res[DOIP_HEADER_LENGTH + 6] = MEMPOOL_NET_MAX_SIZE & 0xFF;
      doipFillHeader(msg->res, DOIP_ENTITY_STATUS_RESPONSE, 7);
      msg->resLen = DOIP_HEADER_LENGTH + 7;
    } else {
      doipFillHeader(msg->res, DOIP_ENTITY_STATUS_RESPONSE, 3);
      msg->resLen = DOIP_HEADER_LENGTH + 3;
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle Entity Status Request\n", RxPduId));
  return ret;
}

static Std_ReturnType doipHandleDiagnosticPowerModeInfoRequest(PduIdType RxPduId, DoIP_MsgType *msg,
                                                               uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t PowerState = 0x00;

  if (0 == msg->payloadLength) {
    doipFillHeader(msg->res, DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_RESPONSE, 1);
    /* @SWS_DoIP_00093 */
    ret = config->GetPowerModeStatus(&PowerState);
    if (E_OK == ret) {
      msg->res[DOIP_HEADER_LENGTH] = PowerState;
    } else {
      msg->res[DOIP_HEADER_LENGTH] = 0x00;
    }
    msg->resLen = DOIP_HEADER_LENGTH + 1;
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle Power Mode Request\n", RxPduId));
  return ret;
}

static const DoIP_TesterType *doipFindTester(uint16_t sourceAddress) {
  const DoIP_TesterType *tester = NULL;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  int i;

  for (i = 0; i < config->numOfTesters; i++) {
    if (config->testers[i].TesterSA == sourceAddress) {
      tester = &config->testers[i];
      break;
    }
  }

  return tester;
}

static const DoIP_RoutingActivationType *
doipFindRoutingActivation(const DoIP_TesterType *tester, uint8_t activationType, uint8_t *raid) {
  const DoIP_RoutingActivationType *ra = NULL;
  int i;

  for (i = 0; i < tester->numOfRoutingActivations; i++) {
    if (tester->RoutingActivationRefs[i]->Number == activationType) {
      ra = tester->RoutingActivationRefs[i];
      if (NULL != raid) {
        *raid = i;
      }
    }
  }

  return ra;
};

static void doipBuildRountineActivationResponse(const DoIP_TesterConnectionType *connection,
                                                DoIP_MsgType *msg, Std_ReturnType ret, uint16_t sa,
                                                uint8_t resCode) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  doipFillHeader(msg->res, DOIP_ROUTING_ACTIVATION_RESPONSE, 13);
  msg->res[DOIP_HEADER_LENGTH + 0] = (sa >> 8) & 0xFF; /* Logical Address Tester */
  msg->res[DOIP_HEADER_LENGTH + 1] = sa & 0xFF;

  msg->res[DOIP_HEADER_LENGTH + 2] =
    (config->LogicalAddress >> 8) & 0xFF; /* Logical address of DoIP entity */
  msg->res[DOIP_HEADER_LENGTH + 3] = config->LogicalAddress & 0xFF;
  msg->res[DOIP_HEADER_LENGTH + 4] = resCode;
  msg->resLen = DOIP_HEADER_LENGTH + 13;

  if ((DOIP_E_PENDING == ret) && (0x11 == resCode)) {
    (void)doipTpSendResponse(connection->SoAdTxPdu, msg->res, msg->resLen); /* @SWS_DoIP_00114 */
  }
}

static Std_ReturnType doipCheckTesterConnectionAlive(const DoIP_TesterConnectionType *connection) {
  Std_ReturnType ret = E_OK;
  uint8_t data[DOIP_HEADER_LENGTH];
  const DoIP_ConfigType *config = DOIP_CONFIG;

  if (0 == connection->context->AliveCheckResponseTimer) {
    /* no request is already on going, start one */
    doipFillHeader(data, DOIP_ALIVE_CHECK_REQUEST, 0);
    connection->context->isAlive = FALSE;
    connection->context->AliveCheckResponseTimer = config->AliveCheckResponseTimeout;
    ret = doipTpSendResponse(connection->SoAdTxPdu, data, DOIP_HEADER_LENGTH);
  }
  return ret;
}

static Std_ReturnType doipSocketHandler(const DoIP_TesterConnectionType *tstcon, uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  boolean isAliveCheckOngoing = FALSE;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  int i;
  const DoIP_TesterConnectionType *connection;
  for (i = 0; (i < config->MaxTesterConnections) && (E_OK == ret); i++) {
    connection = &config->testerConnections[i];
    if ((tstcon != connection) && (DOIP_CON_OPEN == connection->context->state) &&
        (tstcon->context->ramgr.tester == connection->context->TesterRef)) {
      ASLOG(DOIP, ("Tester 0x%x is already active on connection %d, check alive\n",
                   tstcon->context->ramgr.tester->TesterSA, i));
      ret = doipCheckTesterConnectionAlive(connection);
      if (E_OK == ret) {
        isAliveCheckOngoing = TRUE;
      } else {
        /* this connection is closed by Tester, close it */
        ASLOG(DOIP, ("Tester SoCon %d not alive, close it\n", i));
        SoAd_CloseSoCon(connection->SoConId, TRUE);
        memset(connection->context, 0, sizeof(DoIP_TesterConnectionContextType));
        ret = E_OK;
      }
    }
  }

  if (E_OK == ret) {
    if (isAliveCheckOngoing) {
      ret = DOIP_E_PENDING;
    }
  }

  return ret;
}

static Std_ReturnType doipSocketHandler2(const DoIP_TesterConnectionType *tstcon,
                                         uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  boolean isAliveCheckOngoing = FALSE;
  int i;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  for (i = 0; (i < config->MaxTesterConnections) && (E_OK == ret); i++) {
    connection = &config->testerConnections[i];
    if ((tstcon != connection) && (DOIP_CON_OPEN == connection->context->state) &&
        (tstcon->context->ramgr.tester == connection->context->TesterRef)) {
      ASLOG(DOIP, ("Tester 0x%x is already active on connection %d, check alive response\n",
                   tstcon->context->ramgr.tester->TesterSA, i));
      if (TRUE == connection->context->isAlive) {
        /* @SWS_DoIP_00106 */
        *resCode = 0x02;
        ret = DOIP_E_NOT_OK;
        break;
      } else if (connection->context->AliveCheckResponseTimer > 0) {
        isAliveCheckOngoing = TRUE;
      } else {
      }
    }
  }

  if (ret == E_OK) {
    if (isAliveCheckOngoing) {
      ret = DOIP_E_PENDING;
    }
  }

  return ret;
}

static Std_ReturnType doipRoutingActivationManager(const DoIP_TesterConnectionType *connection,
                                                   DoIP_MsgType *msg, uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  const DoIP_TesterType *tester = connection->context->ramgr.tester;
  uint8_t raid = connection->context->ramgr.raid;
  const DoIP_RoutingActivationType *ra = tester->RoutingActivationRefs[raid];
  boolean Authentified = FALSE;
  boolean Confirmed = FALSE;
  boolean ongoing = FALSE;

  do {
    switch (connection->context->ramgr.state) {
    case DOIP_RA_SOCKET_HANDLER:
      ret = doipSocketHandler(connection, resCode);
      if (DOIP_E_NOT_OK != ret) {
        connection->context->ramgr.state = DOIP_RA_SOCKET_HANDLER_2;
        ongoing = TRUE;
      } else {
        ongoing = FALSE;
      }
      break;
    case DOIP_RA_SOCKET_HANDLER_2:
      ret = doipSocketHandler2(connection, resCode);
      if (E_OK == ret) {
        connection->context->ramgr.state = DOIP_RA_CHECK_AUTHENTICATION;
        ongoing = TRUE;
      } else {
        ongoing = FALSE;
      }
      break;
    case DOIP_RA_CHECK_AUTHENTICATION:
      ongoing = FALSE;
      ret = ra->AuthenticationCallback(&Authentified, connection->context->ramgr.OEM,
                                       &msg->res[DOIP_HEADER_LENGTH + 9]);
      if ((E_OK == ret) && (Authentified)) {
        /* pass, going to check confirmed or not */
        connection->context->ramgr.state = DOIP_RA_CHECK_CONFIRMATION;
        ongoing = TRUE;
      } else if (DOIP_E_PENDING == ret) {
        /* @SWS_DoIP_00110 */
      } else {
        *resCode = 0x04; /* @SWS_DoIP_00111 */
        ret = DOIP_E_NOT_OK;
      }
      break;
    case DOIP_RA_CHECK_CONFIRMATION:
      ongoing = FALSE;
      ret = ra->ConfirmationCallback(&Confirmed, connection->context->ramgr.OEM,
                                     &msg->res[DOIP_HEADER_LENGTH + 9]);
      if ((E_OK == ret) && (Confirmed)) {
        *resCode = 0x10; /* @SWS_DoIP_00113 */
        connection->context->RAMask |= (1u << raid);
        connection->context->TesterRef = tester;
        connection->context->ramgr.state = DOIP_RA_IDLE;
      } else if (DOIP_E_PENDING == ret) {
        *resCode = 0x11; /* @SWS_DoIP_00114 */
      } else {
        *resCode = 0x05; /* @SWS_DoIP_00274 */
        ret = DOIP_E_NOT_OK;
      }
      break;
    default:
      ongoing = FALSE;
      *resCode = 101; /* internall state error */
      ret = DOIP_E_NOT_OK;
      break;
    }
  } while (ongoing);

  return ret;
}

/* ref https://zhuanlan.zhihu.com/p/359138097 */
static Std_ReturnType doipHandleRoutineActivation(PduIdType RxPduId, DoIP_MsgType *msg,
                                                  uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection =
    &config->testerConnections[config->RxPduIdToConnectionMap[RxPduId]];
  const DoIP_TesterType *tester = NULL;
  const DoIP_RoutingActivationType *ra = NULL;
  uint8_t raid;
  uint16_t sourceAddress;
  uint8_t activationType;
  uint8_t resCode = 0xFF;
  if ((7 == msg->payloadLength) || (11 == msg->payloadLength)) { /* @SWS_DoIP_00117 */
    sourceAddress = ((uint16_t)msg->req[0] << 8) + msg->req[1];
    activationType = msg->req[2];
    tester = doipFindTester(sourceAddress);
    if (NULL == tester) {
      resCode = 0x00; /* @SWS_DoIP_00104 */
    } else {
      ra = doipFindRoutingActivation(tester, activationType, &raid);
      if (NULL == ra) {
        resCode = 0x06; /* SWS_DoIP_00160 */
      }
    }
    if (NULL != ra) {
      if ((0 != connection->context->RAMask) && (connection->context->TesterRef != tester)) {
        resCode = 0x02; /* @SWS_DoIP_00106*/
        ret = DOIP_E_NOT_OK;
      }
    } else {
      ret = DOIP_E_NOT_OK;
    }

    if (E_OK == ret) {
      connection->context->ramgr.tester = tester;
      connection->context->ramgr.raid = raid;
      memcpy(connection->context->ramgr.OEM, &msg->req[7], 4);
      connection->context->ramgr.state = DOIP_RA_SOCKET_HANDLER;
      ret = doipRoutingActivationManager(connection, msg, &resCode);
    }

    doipBuildRountineActivationResponse(connection, msg, ret, sourceAddress, resCode);
    if (DOIP_E_NOT_OK == ret) {
      connection->context->InactivityTimer = 1; /* close it the next MainFunction */
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle Routine Activation Request\n", RxPduId));
  return ret;
}

static void doipHandleRoutineActivationMain() {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  int i;
  uint8_t resCode = 0xFE;
  Std_ReturnType ret;
  DoIP_MsgType msg;
  uint8_t logcalMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];
  msg.res = logcalMsg;
  msg.resLen = sizeof(logcalMsg);

  for (i = 0; i < config->MaxTesterConnections; i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      if (DOIP_RA_IDLE != connection->context->ramgr.state) {
        ret = doipRoutingActivationManager(connection, &msg, &resCode);
        doipBuildRountineActivationResponse(connection, &msg, ret,
                                            connection->context->ramgr.tester->TesterSA, resCode);
        if (DOIP_E_NOT_OK == ret) {
          connection->context->InactivityTimer = 1; /* close it the next MainFunction */
        }

        if (E_OK == ret) {
          connection->context->InactivityTimer = config->GeneralInactivityTime;
          (void)doipTpSendResponse(connection->SoAdTxPdu, msg.res, msg.resLen);
        } else if (DOIP_E_PENDING == ret) {
        } else {
          (void)doipTpSendResponse(connection->SoAdTxPdu, msg.res, msg.resLen);
          ret = BUFREQ_E_NOT_OK;
        }
      }
    }
  }
}

static Std_ReturnType doIpIfHandleMessage(PduIdType RxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;

  switch (msg->payloadType) {
  case DOIP_VID_REQUEST:
    ret = doipHandleVIDRequest(RxPduId, msg, nack);
    break;
  case DOIP_VID_REQUEST_WITH_EID:
    ret = doipHandleVIDRequestWithEID(RxPduId, msg, nack);
    break;
  case DOIP_VID_REQUEST_WITH_VIN:
    ret = doipHandleVIDRequestWithVIN(RxPduId, msg, nack);
    break;
  case DOIP_VAN_MSG_OR_VIN_RESPONCE:
    /* @SWS_DoIP_00293 */
    ret = DOIP_E_NOT_OK_SILENT;
    break;
  case DOIP_ENTITY_STATUS_REQUEST:
    ret = doipHandleEntityStatusRequest(RxPduId, msg, nack);
    break;
  case DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_REQUEST:
    ret = doipHandleDiagnosticPowerModeInfoRequest(RxPduId, msg, nack);
    break;
  default:
    /* @SWS_DoIP_00016 */
    *nack = DOIP_UNKNOW_PAYLOAD_TYPE_NACK;
    ret = E_NOT_OK;
    break;
  }

  return ret;
}

static const DoIP_TargetAddressType *
doipFindTargetAddress(const DoIP_TesterType *tester, const DoIP_TesterConnectionType *connection,
                      uint16_t ta) {
  const DoIP_TargetAddressType *TargetAddressRef = NULL;
  const DoIP_RoutingActivationType *ra;
  int i, j;

  for (i = 0; (i < tester->numOfRoutingActivations) && (NULL == TargetAddressRef); i++) {
    if (connection->context->RAMask & (1u << i)) {
      ra = tester->RoutingActivationRefs[i];
      for (j = 0; (j < ra->numOfTargetAddressRefs) && (NULL == TargetAddressRef); j++) {
        if (ta == ra->TargetAddressRefs[j]->TargetAddress) {
          TargetAddressRef = ra->TargetAddressRefs[j];
        }
      }
    }
  }

  return TargetAddressRef;
}

static void doipRememberDiagMsg(const DoIP_TesterConnectionType *connection, DoIP_MsgType *msg) {
  PduLengthType bufferSize;
  if (DOIP_MSG_IDLE == connection->context->msg.state) {
    bufferSize = msg->payloadLength - 4;
    if (bufferSize > connection->context->TesterRef->NumByteDiagAckNack) {
      bufferSize = connection->context->TesterRef->NumByteDiagAckNack;
    }
    bufferSize += DOIP_HEADER_LENGTH + 5; /* for header */
    if (NULL != connection->context->msg.req) {
      Net_MemFree(connection->context->msg.req);
    }
    connection->context->msg.req = Net_MemAlloc(bufferSize);
    if (NULL != connection->context->msg.req) {
      /* save sa&ta and uds message */
      memcpy(&(connection->context->msg.req[DOIP_HEADER_LENGTH]), msg->req, 4);
      memcpy(&(connection->context->msg.req[DOIP_HEADER_LENGTH + 5]), &msg->req[4],
             msg->reqLen - 4);
    }
  } else {
    if (NULL != connection->context->msg.req) {
      if (connection->context->msg.index < connection->context->TesterRef->NumByteDiagAckNack) {
        bufferSize =
          connection->context->TesterRef->NumByteDiagAckNack - connection->context->msg.index;
        if (bufferSize > msg->reqLen) {
          bufferSize = msg->reqLen;
        }
        memcpy(
          &(connection->context->msg.req[DOIP_HEADER_LENGTH + 5 + connection->context->msg.index]),
          msg->req, bufferSize);
      }
    }
  }
}

static void doipForgetDiagMsg(const DoIP_TesterConnectionType *connection) {
  if (NULL != connection->context->msg.req) {
    /* @SWS_DoIP_00138 */
    Net_MemFree(connection->context->msg.req);
    connection->context->msg.req = NULL;
  }
}

static void doipReplyDiagMsg(const DoIP_TesterConnectionType *connection, DoIP_MsgType *msg,
                             uint8_t resCode) {
  PduLengthType resLen = 5;
  uint16_t payloadType = DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK;
  if (NULL != connection->context->msg.req) {
    /* @SWS_DoIP_00138 */
    msg->res = connection->context->msg.req;
    resLen = connection->context->msg.index;
    if (resLen > connection->context->TesterRef->NumByteDiagAckNack) {
      resLen = connection->context->TesterRef->NumByteDiagAckNack;
    }
    resLen += 5;
  }

  if (0x0 != resCode) {
    payloadType = DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK;
  }
  doipFillHeader(msg->res, payloadType, resLen);
  msg->res[DOIP_HEADER_LENGTH + 4] = resCode;
  msg->resLen = DOIP_HEADER_LENGTH + resLen;
}

static Std_ReturnType doipHandleDiagnosticMessage(PduIdType RxPduId, DoIP_MsgType *msg,
                                                  uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection =
    &config->testerConnections[config->RxPduIdToConnectionMap[RxPduId]];
  const DoIP_TargetAddressType *TargetAddressRef;
  uint16_t sa, ta;
  uint8_t resCode = 0xFF;
  BufReq_ReturnType bufReq;
  PduInfoType pduInfo;
  PduLengthType bufferSize;

  if (DOIP_MSG_IDLE == connection->context->msg.state) {
    if (msg->payloadLength < 5) {
      *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
      ret = E_NOT_OK;
    }

    if (msg->reqLen < 5) {
      *nack = 0x99; /* TODO: internal error */
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    doipRememberDiagMsg(connection, msg);
    if ((0u == connection->context->RAMask) || (NULL == connection->context->TesterRef)) {
      resCode = 0x02; /* @SWS_DoIP_00123 */
      ASLOG(DOIPE, ("not activated\n"));
      ret = DOIP_E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if (DOIP_MSG_IDLE == connection->context->msg.state) {
      sa = ((uint16_t)msg->req[0] << 8) + msg->req[1];
      ta = ((uint16_t)msg->req[2] << 8) + msg->req[3];

      if (sa != connection->context->TesterRef->TesterSA) {
        ASLOG(DOIPE,
              ("sa %X not right, expected %X\n", sa, connection->context->TesterRef->TesterSA));
        resCode = 0x02; /* @SWS_DoIP_00123 */
        ret = DOIP_E_NOT_OK;
      } else {
        TargetAddressRef = doipFindTargetAddress(connection->context->TesterRef, connection, ta);
        if (NULL == TargetAddressRef) {
          resCode = 0x06; /* @SWS_DoIP_00127 */
          ret = DOIP_E_NOT_OK;
        }
      }
    }
  }

  if (E_OK == ret) {
    if (DOIP_MSG_IDLE == connection->context->msg.state) {
      connection->context->msg.TargetAddressRef = TargetAddressRef;
      pduInfo.SduDataPtr = (uint8_t *)&msg->req[4];
      pduInfo.SduLength = msg->reqLen - 4;
      bufReq = PduR_DoIPStartOfReception(TargetAddressRef->RxPduId, &pduInfo,
                                         msg->payloadLength - 4, &bufferSize);
      if (bufReq != BUFREQ_OK) {
        resCode = 0x08; /* @SWS_DoIP_00174 */
        ret = DOIP_E_NOT_OK;
      } else {
        bufReq = PduR_DoIPCopyRxData(TargetAddressRef->RxPduId, &pduInfo, &bufferSize);
        if (bufReq != BUFREQ_OK) {
          connection->context->msg.state = DOIP_MSG_IDLE;
          PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_NOT_OK);
          resCode = 0x08; /* @SWS_DoIP_00174 */
          ret = DOIP_E_NOT_OK;
        } else {
          connection->context->msg.state = DOIP_MSG_RX;
          connection->context->msg.index = msg->reqLen - 4;
          connection->context->msg.TpSduLength = msg->payloadLength - 4;
        }
      }
    } else {
      TargetAddressRef = connection->context->msg.TargetAddressRef;
      asAssert(TargetAddressRef != NULL);
      pduInfo.SduDataPtr = (uint8_t *)msg->req;
      pduInfo.SduLength = msg->reqLen;
      bufReq = PduR_DoIPCopyRxData(TargetAddressRef->RxPduId, &pduInfo, &bufferSize);
      if (bufReq != BUFREQ_OK) {
        PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_NOT_OK);
        connection->context->msg.state = DOIP_MSG_IDLE;
        resCode = 0x08; /* @SWS_DoIP_00174 */
        ret = DOIP_E_NOT_OK;
      } else {
        connection->context->msg.index += msg->reqLen;
      }
    }
  }

  if (E_OK == ret) {
    if (connection->context->msg.index >= connection->context->msg.TpSduLength) {
      connection->context->msg.state = DOIP_MSG_IDLE;
      PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_OK);
      doipFillHeader(msg->res, DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK, 5);
      msg->res[DOIP_HEADER_LENGTH + 4] = 0x0; /* ack code */
      msg->resLen = DOIP_HEADER_LENGTH + 5;
      doipReplyDiagMsg(connection, msg, 0x0);
    } else {
      connection->context->msg.state = DOIP_MSG_RX;
      ret = DOIP_E_PENDING;
    }
  }

  if (ret == DOIP_E_NOT_OK) {
    doipReplyDiagMsg(connection, msg, resCode);
  }

  ASLOG(DOIP, ("[%d] handle Diagnostic Message\n", RxPduId));

  return ret;
}

static Std_ReturnType doipHandleAliveCheckRequest(PduIdType RxPduId, DoIP_MsgType *msg,
                                                  uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;

  if (0 == msg->payloadLength) {
    doipFillHeader(msg->res, DOIP_ALIVE_CHECK_RESPONSE, 2);
    msg->res[DOIP_HEADER_LENGTH + 0] = (config->LogicalAddress >> 8) & 0xFF;
    msg->res[DOIP_HEADER_LENGTH + 1] = config->LogicalAddress & 0xFF;
    msg->resLen = DOIP_HEADER_LENGTH + 2;
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }

  ASLOG(DOIP, ("[%d] handle Alive Check Request\n", RxPduId));

  return ret;
}

static Std_ReturnType doipHandleAliveCheckResponse(PduIdType RxPduId, DoIP_MsgType *msg,
                                                   uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection =
    &config->testerConnections[config->RxPduIdToConnectionMap[RxPduId]];
  uint16_t sa;

  if (2 == msg->payloadLength) {
    sa = ((uint16_t)msg->req[0] << 8) + msg->req[1];
    if (sa != connection->context->TesterRef->TesterSA) {
      /* @SWS_DoIP_00141 */
      ASLOG(DOIPE, ("sa %X not right, expected %X, close it\n", sa,
                    connection->context->TesterRef->TesterSA));
      ret = DOIP_E_NOT_OK_SILENT;
      connection->context->InactivityTimer = 1; /* close it the next MainFunction */
    } else {
      connection->context->isAlive = TRUE;
      connection->context->AliveCheckResponseTimer = 0;
      msg->resLen = 0; /* no response */
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }

  ASLOG(DOIP, ("[%d] handle Alive Check Response\n", RxPduId));

  return ret;
}

static Std_ReturnType doipTpStartOfReception(PduIdType RxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;

  switch (msg->payloadType) {
  case DOIP_ROUTING_ACTIVATION_REQUEST:
    if (msg->reqLen >= msg->payloadLength) {
      ret = doipHandleRoutineActivation(RxPduId, msg, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_ALIVE_CHECK_REQUEST:
    if (msg->reqLen >= msg->payloadLength) {
      ret = doipHandleAliveCheckRequest(RxPduId, msg, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_ALIVE_CHECK_RESPONSE:
    if (msg->reqLen >= msg->payloadLength) {
      ret = doipHandleAliveCheckResponse(RxPduId, msg, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_DIAGNOSTIC_MESSAGE:
    ret = doipHandleDiagnosticMessage(RxPduId, msg, nack);
    break;
  default:
    *nack = DOIP_UNKNOW_PAYLOAD_TYPE_NACK;
    ret = E_NOT_OK;
    break;
  }

  return ret;
}

static Std_ReturnType doipTpCopyRxData(PduIdType RxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  /* only DIAGNOSTIC_MESSAGE allow this */

  ret = doipHandleDiagnosticMessage(RxPduId, msg, nack);

  return ret;
}

static void doipHandleInactivityTimer(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  int i;

  for (i = 0; i < config->MaxTesterConnections; i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      if (connection->context->InactivityTimer > 0) {
        connection->context->InactivityTimer--;
        if (0 == connection->context->InactivityTimer) {
          ASLOG(DOIP, ("Tester SoCon %d InactivityTimer timeout\n", i));
          SoAd_CloseSoCon(connection->SoConId, TRUE);
          doipForgetDiagMsg(connection);
          memset(connection->context, 0, sizeof(DoIP_TesterConnectionContextType));
        }
      }
    }
  }
}

static void doipHandleAliveCheckResponseTimer(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  int i;

  for (i = 0; i < config->MaxTesterConnections; i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      if (connection->context->AliveCheckResponseTimer > 0) {
        connection->context->AliveCheckResponseTimer--;
        if (0 == connection->context->AliveCheckResponseTimer) {
          ASLOG(DOIP, ("Tester SoCon %d AliveCheckResponseTimer timeout\n", i));
          SoAd_CloseSoCon(connection->SoConId, TRUE);
          memset(connection->context, 0, sizeof(DoIP_TesterConnectionContextType));
        }
      }
    }
  }
}

static void doipHandleVehicleAnnouncement(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  int i;
  PduInfoType PduInfo;
  uint8_t logcamMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];

  for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
    if (DOIP_CON_CLOSED != config->UdpVehicleAnnouncementConnections[i].context->state) {
      if (config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer > 0) {
        config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer--;
        if (0 == config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer) {
          ASLOG(DOIP, ("Udp %d VehicleAnnouncement\n", i));
          PduInfo.SduLength = doipSetupVehicleAnnouncementResponse(logcamMsg);
          PduInfo.SduDataPtr = logcamMsg;
          PduInfo.MetaDataPtr = NULL;
          (void)SoAd_IfTransmit(config->UdpVehicleAnnouncementConnections[i].SoAdTxPdu, &PduInfo);
          config->UdpVehicleAnnouncementConnections[i].context->AnnouncementCounter++;
          if (config->UdpVehicleAnnouncementConnections[i].context->AnnouncementCounter <
              config->VehicleAnnouncementCount) {
            config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer =
              config->VehicleAnnouncementInterval;
          }
        }
      }
    }
  }
}

static void doipHandleDiagMsgResponse(void) {
  Std_ReturnType ret = E_NOT_OK;
  BufReq_ReturnType bret;
  PduInfoType PduInfo;
  PduLengthType left;
  PduIdType TxPduId;
  const DoIP_TargetAddressType *TargetAddressRef;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection = NULL;
  int i;
  uint8_t *res;
  uint32_t resLen;

  for (i = 0; i < (NULL == connection) && (config->MaxTesterConnections); i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      TargetAddressRef = connection->context->msg.TargetAddressRef;
      if (NULL != TargetAddressRef) {
        if (DOIP_MSG_TX == connection->context->msg.state) {
          TxPduId = TargetAddressRef->TxPduId;
          connection = &config->testerConnections[i];
          resLen = connection->context->msg.TpSduLength - connection->context->msg.index;
          res = Net_MemGet(&resLen);
          if (NULL != res) {
            ret = E_OK;
          }
        }
      }
    }
  }

  if (E_OK == ret) {
    PduInfo.SduDataPtr = res;
    PduInfo.SduLength = resLen;
    if (PduInfo.SduLength >
        (connection->context->msg.TpSduLength - connection->context->msg.index)) {
      PduInfo.SduLength = connection->context->msg.TpSduLength - connection->context->msg.index;
    }
    bret = PduR_DoIPCopyTxData(TxPduId, &PduInfo, NULL, &left);
    if (BUFREQ_OK == bret) {
      ret = doipTpSendResponse(connection->SoAdTxPdu, PduInfo.SduDataPtr, PduInfo.SduLength);
      if (E_OK != ret) {
        connection->context->msg.state = DOIP_MSG_IDLE;
        PduR_DoIPTxConfirmation(TxPduId, E_NOT_OK);
      }
    } else {
      ret = E_NOT_OK;
    }

    if (E_OK == ret) {
      if (PduInfo.SduLength >= connection->context->msg.TpSduLength) {
        connection->context->msg.state = DOIP_MSG_IDLE;
        PduR_DoIPTxConfirmation(TxPduId, E_OK);
        ASLOG(DOIP, ("[%d] send UDS response done\n", TxPduId));
      } else {
        ASLOG(DOIP, ("[%d] send UDS response on going\n", TxPduId));
      }
    }

    Net_MemFree(res);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void DoIP_Init(const DoIP_ConfigType *ConfigPtr) {
  int i;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
    memset(config->UdpVehicleAnnouncementConnections[i].context, 0,
           sizeof(DoIP_UdpVehicleAnnouncementConnectionContextType));
  }
  for (i = 0; i < config->MaxTesterConnections; i++) {
    memset(config->testerConnections[i].context, 0, sizeof(DoIP_TesterConnectionContextType));
  }
}

void DoIP_ActivationLineSwitchActive(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  int i;

  if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
    ASLOG(DOIP, ("switch to active\n"));
    context->ActivationLineState = DOIP_ACTIVATION_LINE_ACTIVE;
    /* @SWS_DoIP_00204 */
    for (i = 0; i < config->numOfTcpConnections; i++) {
      if (config->TcpConnections[i].RequestAddressAssignment) {
        SoAd_OpenSoCon(config->TcpConnections[i].SoConId);
      }
    }
    for (i = 0; i < config->numOfUdpConnections; i++) {
      if (config->UdpConnections[i].RequestAddressAssignment) {
        SoAd_OpenSoCon(config->UdpConnections[i].SoConId);
      }
    }
    for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
      if (config->UdpVehicleAnnouncementConnections[i].RequestAddressAssignment) {
        SoAd_OpenSoCon(config->UdpVehicleAnnouncementConnections[i].SoConId);
      }
    }
  }
}

void DoIP_ActivationLineSwitchInactive(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  int i;
  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    ASLOG(DOIP, ("switch to inactive\n"));
    for (i = 0; i < config->numOfTcpConnections; i++) {
      if (config->TcpConnections[i].RequestAddressAssignment) {
        SoAd_CloseSoCon(config->TcpConnections[i].SoConId, TRUE);
      }
    }
    for (i = 0; i < config->numOfUdpConnections; i++) {
      if (config->UdpConnections[i].RequestAddressAssignment) {
        SoAd_CloseSoCon(config->UdpConnections[i].SoConId, TRUE);
      }
    }
    for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
      if (config->UdpVehicleAnnouncementConnections[i].RequestAddressAssignment) {
        SoAd_CloseSoCon(config->UdpVehicleAnnouncementConnections[i].SoConId, TRUE);
        config->UdpVehicleAnnouncementConnections[i].context->state = DOIP_CON_CLOSED;
      }
    }
    for (i = 0; i < config->MaxTesterConnections; i++) {
      if (DOIP_CON_CLOSED != config->testerConnections[i].context->state) {
        SoAd_CloseSoCon(config->testerConnections[i].SoConId, TRUE);
        config->testerConnections[i].context->state = DOIP_CON_CLOSED;
      }
    }
    context->ActivationLineState = DOIP_ACTIVATION_LINE_INACTIVE;
  }
}

void DoIP_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode) {
  Std_ReturnType ret = E_NOT_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  int i;

  for (i = 0; i < config->numOfTcpConnections; i++) {
    if (SoConId == config->TcpConnections[i].SoConId) {
      ASLOG(DOIP, ("Tcp %d SoCon Mode %d\n", i, Mode));
      ret = E_OK;
      break;
    }
  }

  for (i = 0; (i < config->numOfUdpConnections) && (E_NOT_OK == ret); i++) {
    if (SoConId == config->UdpConnections[i].SoConId) {
      ASLOG(DOIP, ("Udp SoCon %d Mode %d\n", i, Mode));
      ret = E_OK;
    }
  }

  for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
    if (SoConId == config->UdpVehicleAnnouncementConnections[i].SoConId) {
      if (SOAD_SOCON_ONLINE == Mode) {
        asAssert(DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState);
        config->UdpVehicleAnnouncementConnections[i].context->state = DOIP_CON_OPEN;
        config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer =
          config->InitialVehicleAnnouncementTime;
        config->UdpVehicleAnnouncementConnections[i].context->AnnouncementCounter = 0;
      }
      ASLOG(DOIP, ("UdpVehicleAnnouncement SoCon %d Mode %d\n", i, Mode));
      ret = E_OK;
      break;
    }
  }

  for (i = 0; (i < config->MaxTesterConnections) && (E_NOT_OK == ret); i++) {
    if (SoConId == config->testerConnections[i].SoConId) {
      if (SOAD_SOCON_ONLINE == Mode) {
        asAssert(DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState);
        memset(config->testerConnections[i].context, 0, sizeof(DoIP_TesterConnectionContextType));
        config->testerConnections[i].context->InactivityTimer = config->InitialInactivityTime;
        config->testerConnections[i].context->state = DOIP_CON_OPEN;
      }
      ASLOG(DOIP, ("Tester SoCon %d Mode %d\n", i, Mode));
      ret = E_OK;
    }
  }
}

void DoIP_SoAdIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  uint8_t nack = DOIP_INVALID_PROTOCOL_NACK;
  PduInfoType PduInfo;
  DoIP_ContextType *context = &DoIP_Context;
  DoIP_MsgType msg;
  uint8_t localMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];
  msg.res = localMsg;
  msg.resLen = sizeof(localMsg);

  if (RxPduId < DOIP_CONFIG->numOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
      /* @SWS_DoIP_00202 */
      ret = DOIP_E_NOT_OK_SILENT;
    }
  } else {
    ASLOG(DOIPE, ("IfRxInd with invalid RxPduId %d\n", RxPduId));
    ret = DOIP_E_NOT_OK_SILENT;
  }

  if (E_OK == ret) {
    ret = doipDecodeMsg(PduInfoPtr, &msg);
    if ((E_OK == ret) && (msg.reqLen < msg.payloadLength)) {
      nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    ret = doIpIfHandleMessage(RxPduId, &msg, &nack);
  }

  if (DOIP_E_NOT_OK_SILENT == ret) {
    /* slient as inactive or ... */
  } else {
    if (E_OK != ret) {
      doipFillHeader(msg.res, DOIP_GENERAL_HEADER_NEGATIVE_ACK, 1);
      msg.res[8] = nack;
      msg.resLen = 9;
    }

    PduInfo.SduDataPtr = msg.res;
    PduInfo.MetaDataPtr = PduInfoPtr->MetaDataPtr;
    PduInfo.SduLength = msg.resLen;
    (void)SoAd_IfTransmit(RxPduId, &PduInfo);
  }
}

void DoIP_SoAdIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
}

BufReq_ReturnType DoIP_SoAdTpStartOfReception(PduIdType RxPduId, const PduInfoType *info,
                                              PduLengthType TpSduLength,
                                              PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;

  if (RxPduId < DOIP_CONFIG->numOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
      /* @SWS_DoIP_00202 */
      ret = BUFREQ_E_NOT_OK;
    } else {
      if (DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId] >= config->MaxTesterConnections) {
        /* @SWS_DoIP_00101 */
        ret = BUFREQ_E_NOT_OK;
        ASLOG(DOIPE, ("RxPduId %d get invalid connection map\n", RxPduId));
      }
    }
  } else {
    ASLOG(DOIPE, ("TpStartOfReception with invalid RxPduId %d\n", RxPduId));
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

BufReq_ReturnType DoIP_SoAdTpCopyRxData(PduIdType RxPduId, const PduInfoType *PduInfoPtr,
                                        PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  Std_ReturnType r = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  const DoIP_TesterConnectionType *connection;
  uint8_t nack = DOIP_INVALID_PROTOCOL_NACK;
  DoIP_MsgType msg;
  uint8_t localMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];
  msg.res = localMsg;
  msg.resLen = sizeof(localMsg);

  if (RxPduId < DOIP_CONFIG->numOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
      ret = BUFREQ_E_NOT_OK;
      r = DOIP_E_NOT_OK_SILENT;
    } else {
      if (DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId] < config->MaxTesterConnections) {
        connection = &config->testerConnections[DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId]];
      } else {
        ret = BUFREQ_E_NOT_OK;
        r = DOIP_E_NOT_OK_SILENT;
      }
    }
  } else {
    ASLOG(DOIPE, ("TpStartOfReception with invalid RxPduId %d\n", RxPduId));
    ret = BUFREQ_E_NOT_OK;
    r = DOIP_E_NOT_OK_SILENT;
  }

  if (E_OK == r) {
    if (DOIP_MSG_IDLE == connection->context->msg.state) {
      r = doipDecodeMsg(PduInfoPtr, &msg);
      if (E_OK == r) {
        r = doipTpStartOfReception(RxPduId, &msg, &nack);
      }
    } else {
      msg.req = PduInfoPtr->SduDataPtr;
      msg.reqLen = PduInfoPtr->SduLength;
      r = doipTpCopyRxData(RxPduId, &msg, &nack);
    }
  }

  if (E_NOT_OK == r) {
    doipFillHeader(msg.res, DOIP_GENERAL_HEADER_NEGATIVE_ACK, 1);
    msg.res[DOIP_HEADER_LENGTH] = nack;
    msg.resLen = DOIP_HEADER_LENGTH + 1;
  }

  if (E_OK == r) {
    connection->context->InactivityTimer = config->GeneralInactivityTime;
    (void)doipTpSendResponse(connection->SoAdTxPdu, msg.res, msg.resLen);
  } else if (DOIP_E_NOT_OK_SILENT == r) {
    /* slient */
  } else if (DOIP_E_PENDING == r) {
    /* TODO */
  } else {
    (void)doipTpSendResponse(connection->SoAdTxPdu, msg.res, msg.resLen);
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

void DoIP_MainFunction(void) {
  DoIP_ContextType *context = &DoIP_Context;

  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    doipHandleInactivityTimer();
    doipHandleAliveCheckResponseTimer();
    doipHandleVehicleAnnouncement();
    doipHandleDiagMsgResponse();
    doipHandleRoutineActivationMain();
  }
}

Std_ReturnType DoIP_TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  BufReq_ReturnType bret;
  int i;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  const DoIP_TesterConnectionType *connection = NULL;
  PduInfoType PduInfo;
  PduLengthType left;
  uint16_t sa, ta;
  uint8_t *res;
  uint32_t resLen;

  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    for (i = 0; (NULL == connection) && (i < config->MaxTesterConnections); i++) {
      if (DOIP_CON_CLOSED != config->testerConnections[i].context->state) {
        if (NULL != config->testerConnections[i].context->msg.TargetAddressRef) {
          if (config->testerConnections[i].context->msg.TargetAddressRef->TxPduId == TxPduId) {
            connection = &config->testerConnections[i];
            ret = E_OK;
          }
        }
      }
    }
  }

  if (E_OK == ret) {
    if (connection->context->msg.state != DOIP_MSG_IDLE) {
      ASLOG(DOIPE,
            ("[%d] UDS response when state is %d\n", TxPduId, connection->context->msg.state));
      ret = E_NOT_OK;
    }
  } else {
    ASLOG(DOIPE, ("[%d] no UDS request\n", TxPduId));
  }

  if (E_OK == ret) {
    ASLOG(DOIP, ("[%d] UDS response, len = %d\n", TxPduId, PduInfoPtr->SduLength));
    resLen = DOIP_HEADER_LENGTH + 4 + PduInfoPtr->SduLength;
    res = Net_MemGet(&resLen);
    if (NULL == res) {
      ret = E_NOT_OK;
    }
  }
  if (E_OK == ret) {
    doipFillHeader(res, DOIP_DIAGNOSTIC_MESSAGE, PduInfoPtr->SduLength + 4);
    sa = connection->context->msg.TargetAddressRef->TargetAddress;
    ta = connection->context->TesterRef->TesterSA;
    res[DOIP_HEADER_LENGTH + 0] = (sa >> 8) & 0xFF;
    res[DOIP_HEADER_LENGTH + 1] = sa & 0xFF;
    res[DOIP_HEADER_LENGTH + 2] = (ta >> 8) & 0xFF;
    res[DOIP_HEADER_LENGTH + 3] = ta & 0xFF;
    PduInfo.SduDataPtr = &res[DOIP_HEADER_LENGTH + 4];
    PduInfo.SduLength = PduInfoPtr->SduLength;
    if (PduInfo.SduLength > (resLen - DOIP_HEADER_LENGTH - 4)) {
      PduInfo.SduLength = resLen - DOIP_HEADER_LENGTH - 4;
    }
    bret = PduR_DoIPCopyTxData(TxPduId, &PduInfo, NULL, &left);
    if (BUFREQ_OK == bret) {
      resLen = DOIP_HEADER_LENGTH + PduInfo.SduLength + 4;
      ret = doipTpSendResponse(connection->SoAdTxPdu, res, resLen);
      if (E_OK != ret) {
        PduR_DoIPTxConfirmation(TxPduId, E_NOT_OK);
      }
    } else {
      ret = E_NOT_OK;
    }

    if (E_OK == ret) {
      if (PduInfo.SduLength >= PduInfoPtr->SduLength) {
        PduR_DoIPTxConfirmation(TxPduId, E_OK);
        ASLOG(DOIP, ("[%d] send UDS response done\n", TxPduId));
      } else {
        connection->context->msg.state = DOIP_MSG_TX;
        connection->context->msg.TpSduLength = PduInfoPtr->SduLength;
        connection->context->msg.index = PduInfo.SduLength;
        ASLOG(DOIP, ("[%d] send UDS response on going\n", TxPduId));
      }
    }

    Net_MemFree(res);
  }

  return ret;
}