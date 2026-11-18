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
#include "TLS.h"
#include "PduR_DoIP.h"

#include "Std_Debug.h"
#include <string.h>
#include "NetMem.h"

#include "Det.h"
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
  const uint8_t *Req;
  uint8_t *Res; /* response buffer for this message */
  uint16_t PayloadType;
  uint32_t PayloadLength;
  PduLengthType ReqLen;
  PduLengthType ResLen;
} DoIP_MsgType;

typedef struct {
  DoIP_ActivationLineType ActivationLineState;
} DoIP_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern const DoIP_ConfigType DoIP_Config;

static void DoIP_ForgetDiagMsg(const DoIP_TesterConnectionType *connection);
/* ================================ [ DATAS     ] ============================================== */
static DoIP_ContextType DoIP_Context;
/* ================================ [ LOCALS    ] ============================================== */
static void DoIP_FillHeader(uint8_t *header, uint16_t payloadType, uint32_t payloadLength) {
  header[0] = DOIP_PROTOCOL_VERSION;
  header[1] = ~DOIP_PROTOCOL_VERSION;
  header[2] = (payloadType >> 8) & 0xFF;
  header[3] = payloadType & 0xFF;
  header[4] = (payloadLength >> 24) & 0xFF;
  header[5] = (payloadLength >> 16) & 0xFF;
  header[6] = (payloadLength >> 8) & 0xFF;
  header[7] = payloadLength & 0xFF;
}

static Std_ReturnType DoIP_DecodeMsg(const PduInfoType *pduInfoPtr, DoIP_MsgType *msg) {
  Std_ReturnType ret = E_NOT_OK;

  if ((pduInfoPtr->SduLength >= DOIP_HEADER_LENGTH) && (pduInfoPtr->SduDataPtr != NULL)) {
    /* @SWS_DoIP_00005, @SWS_DoIP_00006 */
    if ((DOIP_PROTOCOL_VERSION == pduInfoPtr->SduDataPtr[0]) &&
        (pduInfoPtr->SduDataPtr[0] == ((~pduInfoPtr->SduDataPtr[1]) & 0xFF))) {

      msg->PayloadType = ((uint16_t)pduInfoPtr->SduDataPtr[2] << 8) + pduInfoPtr->SduDataPtr[3];
      msg->PayloadLength = ((uint32_t)pduInfoPtr->SduDataPtr[4] << 24) +
                           ((uint32_t)pduInfoPtr->SduDataPtr[5] << 16) +
                           ((uint32_t)pduInfoPtr->SduDataPtr[6] << 8) + pduInfoPtr->SduDataPtr[7];
      msg->Req = &pduInfoPtr->SduDataPtr[DOIP_HEADER_LENGTH];
      msg->ReqLen = pduInfoPtr->SduLength - DOIP_HEADER_LENGTH;
      ret = E_OK;
    }
  }
  return ret;
}

static PduLengthType DoIP_SetupVehicleAnnouncementResponse(uint8_t *data) {
  Std_ReturnType ret;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_FillHeader(data, DOIP_VAN_MSG_OR_VIN_RESPONCE, 33);
  /* @SWS_DoIP_00072 */
  ret = config->GetVin(&data[DOIP_HEADER_LENGTH]);
  if (E_OK != ret) {
    memset(&data[DOIP_HEADER_LENGTH], config->VinInvalidityPattern, 17);
    data[DOIP_HEADER_LENGTH + 31] = DOIP_NO_FURTHER_ACTION_REQUIRED;
    data[DOIP_HEADER_LENGTH + 32] = DOIP_VIN_GID_INCOMPLETE_NOT_SYNCHRONIZED;
  } else {
    data[DOIP_HEADER_LENGTH + 31] = DOIP_NO_FURTHER_ACTION_REQUIRED;
    data[DOIP_HEADER_LENGTH + 32] = DOIP_VIN_GID_ARE_SYNCHRONIZED;
  }

  data[DOIP_HEADER_LENGTH + 17] = (config->LogicalAddress >> 8) & 0xFF;
  data[DOIP_HEADER_LENGTH + 18] = config->LogicalAddress & 0xFF;

  config->GetEID(&data[DOIP_HEADER_LENGTH + 19]);
  config->GetGID(&data[DOIP_HEADER_LENGTH + 25]);

  return DOIP_HEADER_LENGTH + 33;
}

Std_ReturnType doipTpSendResponse(const DoIP_TesterConnectionType *connection, uint8_t *data,
                                  PduLengthType length) {
  PduInfoType PduInfo;
  Std_ReturnType ret;
  if (length > 0) {
    PduInfo.SduDataPtr = data;
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduLength = length;
#ifdef USE_TLS
    if (TRUE == connection->EnableTLS) {
      ret = TLS_IfTransmit(connection->SoAdTxPdu, &PduInfo);
    } else {
      ret = SoAd_TpTransmit(connection->SoAdTxPdu, &PduInfo);
    }
#else
    ret = SoAd_TpTransmit(connection->SoAdTxPdu, &PduInfo);
#endif
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

static Std_ReturnType DoIP_HandleVIDRequest(PduIdType rxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  if (0u == msg->PayloadLength) {
    msg->ResLen = DoIP_SetupVehicleAnnouncementResponse(msg->Res);
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle VID Request\n", rxPduId));
  return ret;
}

static Std_ReturnType DoIP_HandleVIDRequestWithEID(PduIdType rxPduId, DoIP_MsgType *msg,
                                                   uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t myEID[6];
  if (6u == msg->PayloadLength) {
    ret = config->GetEID(myEID);
    if ((E_OK == ret) && (0u == memcmp(myEID, msg->Req, 6u))) {
      msg->ResLen = DoIP_SetupVehicleAnnouncementResponse(msg->Res);
    } else {
      ret = DOIP_E_NOT_OK_SILENT;
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle VID Request with EID\n", rxPduId));
  return ret;
}

static Std_ReturnType DoIP_HandleVIDRequestWithVIN(PduIdType rxPduId, DoIP_MsgType *msg,
                                                   uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t myVIN[17];

  if (17u == msg->PayloadLength) {
    ret = config->GetVin(myVIN);
    if (E_OK != ret) {
      ret = DOIP_E_NOT_OK_SILENT;
    } else {
      if (0u == memcmp(myVIN, msg->Req, 17u)) {
        msg->ResLen = DoIP_SetupVehicleAnnouncementResponse(msg->Res);
      } else {
        ret = DOIP_E_NOT_OK_SILENT;
      }
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle VID Request with VIN\n", rxPduId));
  return ret;
}

static Std_ReturnType DoIP_HandleEntityStatusRequest(PduIdType rxPduId, DoIP_MsgType *msg,
                                                     uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t curOpened = 0u;
  uint16_t i;

  if (0u == msg->PayloadLength) {
    msg->Res[DOIP_HEADER_LENGTH] = config->NodeType;
    msg->Res[DOIP_HEADER_LENGTH + 1u] = (uint8_t)config->MaxTesterConnections;
    for (i = 0u; i < config->MaxTesterConnections; i++) {
      if (DOIP_CON_OPEN == config->TesterConnections[i].Context->State) {
        curOpened++;
      }
    }
    msg->Res[DOIP_HEADER_LENGTH + 2u] = curOpened;
    if (TRUE == config->EntityStatusMaxByteFieldUse) {
      msg->Res[DOIP_HEADER_LENGTH + 3u] = (MEMPOOL_NET_MAX_SIZE >> 24) & 0xFF;
      msg->Res[DOIP_HEADER_LENGTH + 4u] = (MEMPOOL_NET_MAX_SIZE >> 16) & 0xFF;
      msg->Res[DOIP_HEADER_LENGTH + 5u] = (MEMPOOL_NET_MAX_SIZE >> 8) & 0xFF;
      msg->Res[DOIP_HEADER_LENGTH + 6u] = MEMPOOL_NET_MAX_SIZE & 0xFF;
      DoIP_FillHeader(msg->Res, DOIP_ENTITY_STATUS_RESPONSE, 7u);
      msg->ResLen = DOIP_HEADER_LENGTH + 7u;
    } else {
      DoIP_FillHeader(msg->Res, DOIP_ENTITY_STATUS_RESPONSE, 3u);
      msg->ResLen = DOIP_HEADER_LENGTH + 3u;
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle Entity Status Request\n", rxPduId));
  return ret;
}

static Std_ReturnType DoIP_HandleDiagnosticPowerModeInfoRequest(PduIdType rxPduId,
                                                                DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint8_t PowerState = DOIP_POWER_NOT_READY;

  if (0u == msg->PayloadLength) {
    DoIP_FillHeader(msg->Res, DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_RESPONSE, 1u);
    /* @SWS_DoIP_00093 */
    ret = config->GetPowerModeStatus(&PowerState);
    if (E_OK == ret) {
      msg->Res[DOIP_HEADER_LENGTH] = PowerState;
    } else {
      msg->Res[DOIP_HEADER_LENGTH] = DOIP_POWER_NOT_READY;
    }
    msg->ResLen = DOIP_HEADER_LENGTH + 1u;
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle Power Mode Request\n", rxPduId));
  return ret;
}

static const DoIP_TesterType *DoIP_FindTester(uint16_t sourceAddress) {
  const DoIP_TesterType *tester = NULL;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint16_t i;

  for (i = 0u; i < config->NumOfTesters; i++) {
    if (config->Testers[i].TesterSA == sourceAddress) {
      tester = &config->Testers[i];
      break;
    }
  }

  return tester;
}

static const DoIP_RoutingActivationType *
doipFindRoutingActivation(const DoIP_TesterType *tester, uint8_t activationType, uint8_t *raid) {
  const DoIP_RoutingActivationType *ra = NULL;
  uint16_t i;

  for (i = 0; i < tester->NumOfRoutingActivations; i++) {
    if (tester->RoutingActivationRefs[i]->Number == activationType) {
      ra = tester->RoutingActivationRefs[i];
      if (NULL != raid) {
        *raid = (uint8_t)i;
      }
    }
  }

  return ra;
};

static void DoIP_BuildRoutineActivationResponse(const DoIP_TesterConnectionType *connection,
                                                DoIP_MsgType *msg, Std_ReturnType ret, uint16_t sa,
                                                uint8_t resCode) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  Std_ReturnType ret2;
  uint32_t payloadLength = 9u;

  ret2 = config->GetRoutingActivationResponseOem(&msg->Res[DOIP_HEADER_LENGTH + 9u]);
  if (E_OK == ret2) { /* has OEM */
    payloadLength += 4u;
  }
  DoIP_FillHeader(msg->Res, DOIP_ROUTING_ACTIVATION_RESPONSE, payloadLength);
  msg->Res[DOIP_HEADER_LENGTH + 0u] = (sa >> 8) & 0xFF; /* Logical Address Tester */
  msg->Res[DOIP_HEADER_LENGTH + 1u] = sa & 0xFF;

  msg->Res[DOIP_HEADER_LENGTH + 2u] =
    (config->LogicalAddress >> 8) & 0xFF; /* Logical address of DoIP entity */
  msg->Res[DOIP_HEADER_LENGTH + 3u] = config->LogicalAddress & 0xFF;
  msg->Res[DOIP_HEADER_LENGTH + 4u] = resCode;
  (void)memset(&msg->Res[DOIP_HEADER_LENGTH + 5u], 0, 4);
  msg->ResLen = DOIP_HEADER_LENGTH + payloadLength;

  if ((DOIP_E_PENDING == ret) && (DOIP_RA_PENDING_CONFIRMATION == resCode)) {
    (void)doipTpSendResponse(connection, msg->Res, msg->ResLen); /* @SWS_DoIP_00114 */
  }
}

static Std_ReturnType DoIP_CheckTesterConnectionAlive(const DoIP_TesterConnectionType *connection) {
  Std_ReturnType ret = E_OK;
  uint8_t data[DOIP_HEADER_LENGTH];
  const DoIP_ConfigType *config = DOIP_CONFIG;

  if (0u == connection->Context->AliveCheckResponseTimer) {
    /* no request is already on going, start one */
    DoIP_FillHeader(data, DOIP_ALIVE_CHECK_REQUEST, 0);
    connection->Context->IsAlive = FALSE;
    connection->Context->AliveCheckResponseTimer = config->AliveCheckResponseTimeout;
    ret = doipTpSendResponse(connection, data, DOIP_HEADER_LENGTH);
  }
  return ret;
}

static Std_ReturnType DoIP_SocketHandler(const DoIP_TesterConnectionType *tstcon,
                                         uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  boolean isAliveCheckOngoing = FALSE;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint16_t i;
  const DoIP_TesterConnectionType *connection;
  for (i = 0; (i < config->MaxTesterConnections) && (E_OK == ret); i++) {
    connection = &config->TesterConnections[i];
    if ((tstcon != connection) && (DOIP_CON_OPEN == connection->Context->State) &&
        (tstcon->Context->Ramgr.Tester == connection->Context->TesterRef)) {
      ASLOG(DOIP, ("Tester 0x%x is already active on connection %d, check alive\n",
                   tstcon->Context->Ramgr.Tester->TesterSA, i));
      ret = DoIP_CheckTesterConnectionAlive(connection);
      if (E_OK == ret) {
        isAliveCheckOngoing = TRUE;
      } else {
        /* this connection is closed by Tester, close it */
        ASLOG(DOIP, ("Tester SoCon %d not alive, close it\n", i));
        SoAd_CloseSoCon(connection->SoConId, TRUE);
        DoIP_ForgetDiagMsg(connection);
        memset(connection->Context, 0, sizeof(DoIP_TesterConnectionContextType));
        ret = E_OK;
      }
    }
  }

  if (E_OK == ret) {
    if (TRUE == isAliveCheckOngoing) {
      ret = DOIP_E_PENDING;
    }
  }

  return ret;
}

static Std_ReturnType DoIP_SocketHandler2(const DoIP_TesterConnectionType *tstcon,
                                          uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  boolean isAliveCheckOngoing = FALSE;
  uint16_t i;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  for (i = 0; (i < config->MaxTesterConnections) && (E_OK == ret); i++) {
    connection = &config->TesterConnections[i];
    if ((tstcon != connection) && (DOIP_CON_OPEN == connection->Context->State) &&
        (tstcon->Context->Ramgr.Tester == connection->Context->TesterRef)) {
      ASLOG(DOIP, ("Tester 0x%x is already active on connection %d, check alive response\n",
                   tstcon->Context->Ramgr.Tester->TesterSA, i));
      if (TRUE == connection->Context->IsAlive) {
        /* @SWS_DoIP_00105 */
        *resCode = DOIP_RA_SA_ALREADY_ACTIVE;
        ret = DOIP_E_NOT_OK;
        break;
      } else if (connection->Context->AliveCheckResponseTimer > 0) {
        isAliveCheckOngoing = TRUE;
      } else {
      }
    }
  }

  if (E_OK == ret) {
    if (TRUE == isAliveCheckOngoing) {
      ret = DOIP_E_PENDING;
    }
  }

  return ret;
}

static Std_ReturnType DoIP_RoutingActivationManager(const DoIP_TesterConnectionType *connection,
                                                    DoIP_MsgType *msg, uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  const DoIP_TesterType *tester = connection->Context->Ramgr.Tester;
  uint8_t raid = connection->Context->Ramgr.Raid;
  const DoIP_RoutingActivationType *ra = tester->RoutingActivationRefs[raid];
  boolean Authentified = FALSE;
  boolean Confirmed = FALSE;
  boolean ongoing = FALSE;

  do {
    switch (connection->Context->Ramgr.State) {
    case DOIP_RA_SOCKET_HANDLER:
      ret = DoIP_SocketHandler(connection, resCode);
      if (DOIP_E_NOT_OK != ret) {
        connection->Context->Ramgr.State = DOIP_RA_SOCKET_HANDLER_2;
        ongoing = TRUE;
      } else {
        ongoing = FALSE;
      }
      break;
    case DOIP_RA_SOCKET_HANDLER_2:
      ret = DoIP_SocketHandler2(connection, resCode);
      if (E_OK == ret) {
        connection->Context->Ramgr.State = DOIP_RA_CHECK_AUTHENTICATION;
        ongoing = TRUE;
      } else {
        ongoing = FALSE;
      }
      break;
    case DOIP_RA_CHECK_AUTHENTICATION:
      ongoing = FALSE;
      ret = ra->AuthenticationCallback(&Authentified, connection->Context->Ramgr.OEM,
                                       &msg->Res[DOIP_HEADER_LENGTH + 9]);
      if ((E_OK == ret) && (TRUE == Authentified)) {
        /* pass, going to check confirmed or not */
        connection->Context->Ramgr.State = DOIP_RA_CHECK_CONFIRMATION;
        ongoing = TRUE;
      } else if (DOIP_E_PENDING == ret) {
        /* @SWS_DoIP_00110 */
      } else {
        *resCode = DOIP_RA_AUTHENTICATION_FAILED; /* @SWS_DoIP_00111 */
        ret = DOIP_E_NOT_OK;
      }
      break;
    case DOIP_RA_CHECK_CONFIRMATION:
      ongoing = FALSE;
      ret = ra->ConfirmationCallback(&Confirmed, connection->Context->Ramgr.OEM,
                                     &msg->Res[DOIP_HEADER_LENGTH + 9]);
      if ((E_OK == ret) && (Confirmed)) {
        *resCode = DOIP_RA_OK; /* @SWS_DoIP_00113 */
        connection->Context->RAMask |= (1u << raid);
        connection->Context->TesterRef = tester;
        connection->Context->Ramgr.State = DOIP_RA_IDLE;
      } else if (DOIP_E_PENDING == ret) {
        *resCode = DOIP_RA_PENDING_CONFIRMATION; /* @SWS_DoIP_00114 */
      } else {
        *resCode = DOIP_RA_REJECTED_CONFIRMATION; /* @SWS_DoIP_00274 */
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

static Std_ReturnType DoIP_HandleRoutineActivation(PduIdType rxPduId, DoIP_MsgType *msg,
                                                   uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection =
    &config->TesterConnections[config->RxPduIdToConnectionMap[rxPduId]];
  const DoIP_TesterType *tester = NULL;
  const DoIP_RoutingActivationType *ra = NULL;
  uint8_t raid;
  uint16_t sourceAddress;
  uint8_t activationType;
  uint8_t resCode = 0xFF;
  if ((7u == msg->PayloadLength) || (11u == msg->PayloadLength)) { /* @SWS_DoIP_00117 */
    sourceAddress = ((uint16_t)msg->Req[0] << 8) + msg->Req[1];
    activationType = msg->Req[2];
    tester = DoIP_FindTester(sourceAddress);
    if (NULL == tester) {
      resCode = DOIP_RA_UNKNOWN_SA; /* @SWS_DoIP_00104 */
    } else {
      ra = doipFindRoutingActivation(tester, activationType, &raid);
      if (NULL == ra) {
        resCode = DOIP_RA_UNSUPPORTED_TYPE; /* SWS_DoIP_00160 */
      }
    }
    if (NULL != ra) {
      if ((0u != connection->Context->RAMask) && (connection->Context->TesterRef != tester)) {
        resCode = DOIP_RA_CONNECTION_IN_USE; /* @SWS_DoIP_00106*/
        ret = DOIP_E_NOT_OK;
      }
    } else {
      ret = DOIP_E_NOT_OK;
    }

    if (E_OK == ret) {
      connection->Context->Ramgr.Tester = tester;
      connection->Context->Ramgr.Raid = raid;
      memcpy(connection->Context->Ramgr.OEM, &msg->Req[7], 4);
      connection->Context->Ramgr.State = DOIP_RA_SOCKET_HANDLER;
      ret = DoIP_RoutingActivationManager(connection, msg, &resCode);
    }

    DoIP_BuildRoutineActivationResponse(connection, msg, ret, sourceAddress, resCode);
    if (DOIP_E_NOT_OK == ret) {
      connection->Context->InactivityTimer = 1; /* close it the next MainFunction */
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle Routine Activation Request\n", rxPduId));
  return ret;
}

static void DoIP_HandleRoutineActivationMain() {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  uint16_t i;
  uint8_t resCode = 0xFE;
  Std_ReturnType ret;
  DoIP_MsgType msg;
  uint8_t logcalMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];
  msg.Res = logcalMsg;
  msg.ResLen = sizeof(logcalMsg);

  for (i = 0; i < config->MaxTesterConnections; i++) {
    connection = &config->TesterConnections[i];
    if (DOIP_CON_CLOSED != connection->Context->State) {
      if (DOIP_RA_IDLE != connection->Context->Ramgr.State) {
        ret = DoIP_RoutingActivationManager(connection, &msg, &resCode);
        DoIP_BuildRoutineActivationResponse(connection, &msg, ret,
                                            connection->Context->Ramgr.Tester->TesterSA, resCode);
        if (DOIP_E_NOT_OK == ret) {
          connection->Context->InactivityTimer = 1; /* close it the next MainFunction */
        }

        if (E_OK == ret) {
          connection->Context->InactivityTimer = config->GeneralInactivityTime;
          (void)doipTpSendResponse(connection, msg.Res, msg.ResLen);
        } else if (DOIP_E_PENDING == ret) {
        } else {
          (void)doipTpSendResponse(connection, msg.Res, msg.ResLen);
          ret = BUFREQ_E_NOT_OK;
        }
      }
    }
  }
}

static Std_ReturnType DoIP_IfHandleMessage(PduIdType rxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;

  switch (msg->PayloadType) {
  case DOIP_VID_REQUEST:
    ret = DoIP_HandleVIDRequest(rxPduId, msg, nack);
    break;
  case DOIP_VID_REQUEST_WITH_EID:
    ret = DoIP_HandleVIDRequestWithEID(rxPduId, msg, nack);
    break;
  case DOIP_VID_REQUEST_WITH_VIN:
    ret = DoIP_HandleVIDRequestWithVIN(rxPduId, msg, nack);
    break;
  case DOIP_VAN_MSG_OR_VIN_RESPONCE:
    /* @SWS_DoIP_00293 */
    ret = DOIP_E_NOT_OK_SILENT;
    break;
  case DOIP_ENTITY_STATUS_REQUEST:
    ret = DoIP_HandleEntityStatusRequest(rxPduId, msg, nack);
    break;
  case DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_REQUEST:
    ret = DoIP_HandleDiagnosticPowerModeInfoRequest(rxPduId, msg, nack);
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

  for (i = 0; (i < tester->NumOfRoutingActivations) && (NULL == TargetAddressRef); i++) {
    if (0u != (connection->Context->RAMask & (1u << i))) {
      ra = tester->RoutingActivationRefs[i];
      for (j = 0; (j < ra->NumOfTargetAddressRefs) && (NULL == TargetAddressRef); j++) {
        if (ta == ra->TargetAddressRefs[j]->TargetAddress) {
          TargetAddressRef = ra->TargetAddressRefs[j];
          ASLOG(DOIP, ("target address %x is %u\n", ta, j));
        }
      }
    }
  }

  return TargetAddressRef;
}

static Std_ReturnType DoIP_IsTaConfigured(const DoIP_TesterType *tester, uint16_t ta) {
  Std_ReturnType ret = E_NOT_OK;
  const DoIP_RoutingActivationType *ra;
  int i, j;

  for (i = 0; (i < tester->NumOfRoutingActivations) && (E_NOT_OK == ret); i++) {
    ra = tester->RoutingActivationRefs[i];
    for (j = 0; (j < ra->NumOfTargetAddressRefs) && (E_NOT_OK == ret); j++) {
      if (ta == ra->TargetAddressRefs[j]->TargetAddress) {
        ret = E_OK;
      }
    }
  }

  return ret;
}

static void DoIP_RememberDiagMsg(const DoIP_TesterConnectionType *connection, DoIP_MsgType *msg) {
  PduLengthType bufferSize;
  PduLengthType copySize;
  if (DOIP_MSG_IDLE == connection->Context->Msg.State) {
    bufferSize = msg->PayloadLength - 4;
    if (bufferSize > connection->Context->TesterRef->NumByteDiagAckNack) {
      bufferSize = connection->Context->TesterRef->NumByteDiagAckNack;
    }
    bufferSize += DOIP_HEADER_LENGTH + 5; /* for header */
    if (NULL != connection->Context->Msg.Req) {
      Net_MemFree(connection->Context->Msg.Req);
    }
    connection->Context->Msg.Req = Net_MemAlloc(bufferSize);
    if (NULL != connection->Context->Msg.Req) {
      /* save sa&ta and uds message */
      copySize = msg->ReqLen - 4u;
      if (copySize > (bufferSize - DOIP_HEADER_LENGTH - 5u)) {
        copySize = bufferSize - DOIP_HEADER_LENGTH - 5u;
      }
      memcpy(&(connection->Context->Msg.Req[DOIP_HEADER_LENGTH]), msg->Req, 4);
      memcpy(&(connection->Context->Msg.Req[DOIP_HEADER_LENGTH + 5u]), &msg->Req[4], copySize);
    }
  } else {
    if (NULL != connection->Context->Msg.Req) {
      if (connection->Context->Msg.Index < connection->Context->TesterRef->NumByteDiagAckNack) {
        bufferSize =
          connection->Context->TesterRef->NumByteDiagAckNack - connection->Context->Msg.Index;
        if (bufferSize > msg->ReqLen) {
          bufferSize = msg->ReqLen;
        }
        memcpy(
          &(connection->Context->Msg.Req[DOIP_HEADER_LENGTH + 5u + connection->Context->Msg.Index]),
          msg->Req, bufferSize);
      }
    }
  }
}

static void DoIP_ForgetDiagMsg(const DoIP_TesterConnectionType *connection) {
  if (NULL != connection->Context->Msg.Req) {
    /* @SWS_DoIP_00138 */
    Net_MemFree(connection->Context->Msg.Req);
    connection->Context->Msg.Req = NULL;
  }
}

static void DoIP_ReplyDiagMsg(const DoIP_TesterConnectionType *connection, DoIP_MsgType *msg,
                              uint8_t resCode, uint16_t sa, uint16_t ta) {
  PduLengthType resLen = 5;
  uint16_t payloadType = DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK;
  if (NULL != connection->Context->Msg.Req) {
    /* @SWS_DoIP_00138 */
    msg->Res = connection->Context->Msg.Req;
    resLen = connection->Context->Msg.Index;
    if (resLen > connection->Context->TesterRef->NumByteDiagAckNack) {
      resLen = connection->Context->TesterRef->NumByteDiagAckNack;
    }
    resLen += 5;
  }

  if (0x0 != resCode) {
    payloadType = DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK;
  }
  DoIP_FillHeader(msg->Res, payloadType, resLen);
  msg->Res[DOIP_HEADER_LENGTH + 0u] = (ta >> 8) & 0xFF;
  msg->Res[DOIP_HEADER_LENGTH + 1u] = ta & 0xFF;
  msg->Res[DOIP_HEADER_LENGTH + 2u] = (sa >> 8) & 0xFF;
  msg->Res[DOIP_HEADER_LENGTH + 3u] = sa & 0xFF;
  msg->Res[DOIP_HEADER_LENGTH + 4u] = resCode;
  msg->ResLen = DOIP_HEADER_LENGTH + resLen;
}

static Std_ReturnType DoIP_HandleDiagnosticMessage(PduIdType rxPduId, DoIP_MsgType *msg,
                                                   uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection =
    &config->TesterConnections[config->RxPduIdToConnectionMap[rxPduId]];
  const DoIP_TargetAddressType *TargetAddressRef;
  uint16_t sa, ta;
  uint8_t resCode = 0xFFu;
  BufReq_ReturnType bufReq;
  PduInfoType pduInfo;
  PduLengthType bufferSize;

  if (DOIP_MSG_IDLE == connection->Context->Msg.State) {
    if (msg->PayloadLength < 5u) {
      *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
      ret = E_NOT_OK;
    }

    if (msg->ReqLen < 5) {
      *nack = 0x99; /* TODO: internal error */
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    DoIP_RememberDiagMsg(connection, msg);
    if ((0u == connection->Context->RAMask) || (NULL == connection->Context->TesterRef)) {
      resCode = DOIP_DIAG_INVALID_SA; /* @SWS_DoIP_00123 */
      ASLOG(DOIPE, ("not activated\n"));
      ret = DOIP_E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    if (DOIP_MSG_IDLE == connection->Context->Msg.State) {
      sa = ((uint16_t)msg->Req[0] << 8) + msg->Req[1];
      ta = ((uint16_t)msg->Req[2] << 8) + msg->Req[3];

      if (sa != connection->Context->TesterRef->TesterSA) {
        ASLOG(DOIPE,
              ("sa %X not right, expected %X\n", sa, connection->Context->TesterRef->TesterSA));
        resCode = DOIP_DIAG_INVALID_SA; /* @SWS_DoIP_00123 */
        ret = DOIP_E_NOT_OK;
      } else {
        TargetAddressRef = doipFindTargetAddress(connection->Context->TesterRef, connection, ta);
        if (NULL == TargetAddressRef) {
          ret = DoIP_IsTaConfigured(connection->Context->TesterRef, ta);
          if (E_OK == ret) {
            resCode = DOIP_DIAG_TARGET_UNREACHABLE; /* @SWS_DoIP_00127 */
          } else {
            resCode = DOIP_DIAG_UNKNOWN_TA; /* @SWS_DoIP_00124 */
          }
          ret = DOIP_E_NOT_OK;
        }
      }
    }
  }

  if (E_OK == ret) {
    if (DOIP_MSG_IDLE == connection->Context->Msg.State) {
      connection->Context->Msg.TargetAddressRef = TargetAddressRef;
      pduInfo.SduDataPtr = (uint8_t *)&msg->Req[4];
      pduInfo.SduLength = msg->ReqLen - 4u;
      bufReq = PduR_DoIPStartOfReception(TargetAddressRef->RxPduId, &pduInfo,
                                         msg->PayloadLength - 4u, &bufferSize);
      if (bufReq != BUFREQ_OK) {
        resCode = DOIP_DIAG_TP_ERROR; /* @SWS_DoIP_00174 */
        ret = DOIP_E_NOT_OK;
      } else {
        bufReq = PduR_DoIPCopyRxData(TargetAddressRef->RxPduId, &pduInfo, &bufferSize);
        if (bufReq != BUFREQ_OK) {
          connection->Context->Msg.State = DOIP_MSG_IDLE;
          PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_NOT_OK);
          resCode = DOIP_DIAG_TP_ERROR; /* @SWS_DoIP_00174 */
          ret = DOIP_E_NOT_OK;
        } else {
          connection->Context->Msg.State = DOIP_MSG_RX;
          connection->Context->Msg.Index = msg->ReqLen - 4u;
          connection->Context->Msg.TpSduLength = msg->PayloadLength - 4u;
        }
      }
    } else {
      TargetAddressRef = connection->Context->Msg.TargetAddressRef;
      asAssert(TargetAddressRef != NULL);
      pduInfo.SduDataPtr = (uint8_t *)msg->Req;
      pduInfo.SduLength = msg->ReqLen;
      bufReq = PduR_DoIPCopyRxData(TargetAddressRef->RxPduId, &pduInfo, &bufferSize);
      if (bufReq != BUFREQ_OK) {
        PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_NOT_OK);
        connection->Context->Msg.State = DOIP_MSG_IDLE;
        resCode = DOIP_DIAG_TP_ERROR; /* @SWS_DoIP_00174 */
        ret = DOIP_E_NOT_OK;
      } else {
        connection->Context->Msg.Index += msg->ReqLen;
      }
    }
  }

  if (E_OK == ret) {
    if (connection->Context->Msg.Index >= connection->Context->Msg.TpSduLength) {
      connection->Context->Msg.State = DOIP_MSG_IDLE;
      PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_OK);
      DoIP_FillHeader(msg->Res, DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK, 5);
      msg->Res[DOIP_HEADER_LENGTH + 4u] = 0x0u; /* ack code */
      msg->ResLen = DOIP_HEADER_LENGTH + 5u;
      DoIP_ReplyDiagMsg(connection, msg, 0x0u, sa, ta);
    } else {
      connection->Context->Msg.State = DOIP_MSG_RX;
      ret = DOIP_E_PENDING;
    }
  }

  if (ret == DOIP_E_NOT_OK) {
    DoIP_ReplyDiagMsg(connection, msg, resCode, sa, ta);
  }

  ASLOG(DOIP, ("[%d] handle Diagnostic Message\n", rxPduId));

  return ret;
}

static Std_ReturnType DoIP_HandleAliveCheckRequest(PduIdType rxPduId, DoIP_MsgType *msg,
                                                   uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;

  if (0u == msg->PayloadLength) {
    DoIP_FillHeader(msg->Res, DOIP_ALIVE_CHECK_RESPONSE, 2);
    msg->Res[DOIP_HEADER_LENGTH + 0u] = (config->LogicalAddress >> 8) & 0xFF;
    msg->Res[DOIP_HEADER_LENGTH + 1u] = config->LogicalAddress & 0xFF;
    msg->ResLen = DOIP_HEADER_LENGTH + 2;
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }

  ASLOG(DOIP, ("[%d] handle Alive Check Request\n", rxPduId));

  return ret;
}

static Std_ReturnType DoIP_HandleAliveCheckResponse(PduIdType rxPduId, DoIP_MsgType *msg,
                                                    uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection =
    &config->TesterConnections[config->RxPduIdToConnectionMap[rxPduId]];
  uint16_t sa;

  if (2 == msg->PayloadLength) {
    sa = ((uint16_t)msg->Req[0] << 8) + msg->Req[1];
    if (sa != connection->Context->TesterRef->TesterSA) {
      /* @SWS_DoIP_00141 */
      ASLOG(DOIPE, ("sa %X not right, expected %X, close it\n", sa,
                    connection->Context->TesterRef->TesterSA));
      ret = DOIP_E_NOT_OK_SILENT;
      connection->Context->InactivityTimer = 1; /* close it the next MainFunction */
    } else {
      connection->Context->IsAlive = TRUE;
      connection->Context->AliveCheckResponseTimer = 0;
      msg->ResLen = 0; /* no response */
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }

  ASLOG(DOIP, ("[%d] handle Alive Check Response\n", rxPduId));

  return ret;
}

static Std_ReturnType DoIP_TpStartOfReception(PduIdType rxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;

  switch (msg->PayloadType) {
  case DOIP_ROUTING_ACTIVATION_REQUEST:
    if (msg->ReqLen >= msg->PayloadLength) {
      ret = DoIP_HandleRoutineActivation(rxPduId, msg, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_ALIVE_CHECK_REQUEST:
    if (msg->ReqLen >= msg->PayloadLength) {
      ret = DoIP_HandleAliveCheckRequest(rxPduId, msg, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_ALIVE_CHECK_RESPONSE:
    if (msg->ReqLen >= msg->PayloadLength) {
      ret = DoIP_HandleAliveCheckResponse(rxPduId, msg, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_DIAGNOSTIC_MESSAGE:
    ret = DoIP_HandleDiagnosticMessage(rxPduId, msg, nack);
    break;
  default:
    *nack = DOIP_UNKNOW_PAYLOAD_TYPE_NACK;
    ret = E_NOT_OK;
    break;
  }

  return ret;
}

static Std_ReturnType DoIP_TpCopyRxData(PduIdType rxPduId, DoIP_MsgType *msg, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  /* only DIAGNOSTIC_MESSAGE allow this */

  ret = DoIP_HandleDiagnosticMessage(rxPduId, msg, nack);

  return ret;
}

static void DoIP_HandleInactivityTimer(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  uint16_t i;

  for (i = 0; i < config->MaxTesterConnections; i++) {
    connection = &config->TesterConnections[i];
    if (DOIP_CON_CLOSED != connection->Context->State) {
      if (connection->Context->InactivityTimer > 0) {
        connection->Context->InactivityTimer--;
        if (0u == connection->Context->InactivityTimer) { /* @SWS_DoIP_00144 */
          ASLOG(DOIP, ("Tester SoCon %d InactivityTimer timeout\n", i));
          SoAd_CloseSoCon(connection->SoConId, TRUE);
          DoIP_ForgetDiagMsg(connection);
          memset(connection->Context, 0, sizeof(DoIP_TesterConnectionContextType));
        }
      }
    }
  }
}

static void DoIP_HandleAliveCheckResponseTimer(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection;
  uint16_t i;

  for (i = 0; i < config->MaxTesterConnections; i++) {
    connection = &config->TesterConnections[i];
    if (DOIP_CON_CLOSED != connection->Context->State) {
      if (connection->Context->AliveCheckResponseTimer > 0) {
        connection->Context->AliveCheckResponseTimer--;
        if (0u == connection->Context->AliveCheckResponseTimer) {
          ASLOG(DOIP, ("Tester SoCon %d AliveCheckResponseTimer timeout\n", i));
          SoAd_CloseSoCon(connection->SoConId, TRUE);
          DoIP_ForgetDiagMsg(connection);
          memset(connection->Context, 0, sizeof(DoIP_TesterConnectionContextType));
        }
      }
    }
  }
}

static void DoIP_HandleVehicleAnnouncement(void) {
  Std_ReturnType ret;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  uint16_t i;
  PduInfoType PduInfo;
  uint8_t logcamMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];

  for (i = 0; i < config->NumOfUdpVehicleAnnouncementConnections; i++) {
    if (DOIP_CON_CLOSED != config->UdpVehicleAnnouncementConnections[i].Context->State) {
      if (config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementTimer > 0) {
        config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementTimer--;
        if (0u == config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementTimer) {
          ASLOG(DOIP, ("Udp %d VehicleAnnouncement\n", i));
          PduInfo.SduLength = DoIP_SetupVehicleAnnouncementResponse(logcamMsg);
          PduInfo.SduDataPtr = logcamMsg;
          PduInfo.MetaDataPtr = NULL;
          ret = SoAd_IfTransmit(config->UdpVehicleAnnouncementConnections[i].SoAdTxPdu, &PduInfo);
          if (E_OK == ret) {
            config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementCounter++;
            if (config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementCounter <
                config->VehicleAnnouncementCount) {
              config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementTimer =
                config->VehicleAnnouncementInterval;
            }
          } else { /* retry the next time as maybe lwip netif not linked up */
            config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementTimer = 1;
          }
        }
      }
    }
  }
}

static void DoIP_HandleDiagMsgResponse(void) {
  Std_ReturnType ret = E_NOT_OK;
  BufReq_ReturnType bret;
  PduInfoType PduInfo;
  PduLengthType left;
  PduLengthType offset;
  PduIdType TxPduId;
  const DoIP_TargetAddressType *TargetAddressRef;
  const DoIP_TargetNodeType *targetNode = NULL;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  const DoIP_TesterConnectionType *connection = NULL;
  uint16_t i;
  uint16_t j;
  uint8_t *res;
  uint32_t resLen;

  for (i = 0; (NULL == connection) && (i < config->MaxTesterConnections); i++) {
    connection = &config->TesterConnections[i];
    if (DOIP_CON_CLOSED != connection->Context->State) {
      TargetAddressRef = connection->Context->Msg.TargetAddressRef;
      if (NULL != TargetAddressRef) {
        for (j = 0; (NULL == connection) && (j < TargetAddressRef->NumTargetNodes); j++) {
          targetNode = &TargetAddressRef->TargetNodes[j];
          if (DOIP_MSG_TX == targetNode->Context->State) {
            TxPduId = targetNode->TxPduId;
            connection = &config->TesterConnections[i];
            resLen = targetNode->Context->TpSduLength - targetNode->Context->Index;
            res = Net_MemGet(&resLen);
            if (NULL != res) {
              ret = E_OK;
            }
          }
        }
      }
    }
  }

  if (E_OK == ret) {
    PduInfo.SduDataPtr = res;
    PduInfo.SduLength = resLen;
    if (PduInfo.SduLength > (targetNode->Context->TpSduLength - targetNode->Context->Index)) {
      PduInfo.SduLength = targetNode->Context->TpSduLength - targetNode->Context->Index;
    }
    offset = targetNode->Context->Index;
    PduInfo.MetaDataPtr = (uint8_t *)&offset;
    bret = PduR_DoIPCopyTxData(TxPduId, &PduInfo, NULL, &left);
    if (BUFREQ_OK == bret) {
      ret = doipTpSendResponse(connection, PduInfo.SduDataPtr, PduInfo.SduLength);
      if (E_OK != ret) {
        targetNode->Context->State = DOIP_MSG_IDLE;
        PduR_DoIPTxConfirmation(TxPduId, E_NOT_OK);
      }
    } else {
      ret = E_NOT_OK;
    }

    if (E_OK == ret) {
      targetNode->Context->Index += PduInfo.SduLength;
      if (targetNode->Context->Index >= targetNode->Context->TpSduLength) {
        targetNode->Context->State = DOIP_MSG_IDLE;
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
void DoIP_Init(const DoIP_ConfigType *configPtr) {
  uint16_t i;
  uint16_t j;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  for (i = 0; i < config->NumOfUdpVehicleAnnouncementConnections; i++) {
    memset(config->UdpVehicleAnnouncementConnections[i].Context, 0,
           sizeof(DoIP_UdpVehicleAnnouncementConnectionContextType));
  }
  for (i = 0; i < config->MaxTesterConnections; i++) {
    memset(config->TesterConnections[i].Context, 0, sizeof(DoIP_TesterConnectionContextType));
  }

  for (i = 0; i < config->NumOfTargetAddresss; i++) {
    for (j = 0; j < config->TargetAddresss[i].NumTargetNodes; j++) {
      memset(config->TargetAddresss[i].TargetNodes[j].Context, 0,
             sizeof(DoIP_TargetNodeContextType));
    }
  }
}

void DoIP_ActivationLineSwitchActive(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  uint16_t i;

  if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
    ASLOG(DOIP, ("switch to active\n"));
    context->ActivationLineState = DOIP_ACTIVATION_LINE_ACTIVE;
    /* @SWS_DoIP_00204 @SWS_DoIP_00306 */
    for (i = 0; i < config->NumOfTcpConnections; i++) {
      if (config->TcpConnections[i].RequestAddressAssignment) {
        SoAd_OpenSoCon(config->TcpConnections[i].SoConId);
      }
    }
    for (i = 0; i < config->NumOfUdpConnections; i++) {
      if (config->UdpConnections[i].RequestAddressAssignment) {
        SoAd_OpenSoCon(config->UdpConnections[i].SoConId);
      }
    }
    for (i = 0; i < config->NumOfUdpVehicleAnnouncementConnections; i++) {
      if (config->UdpVehicleAnnouncementConnections[i].RequestAddressAssignment) {
        SoAd_OpenSoCon(config->UdpVehicleAnnouncementConnections[i].SoConId);
      }
    }
  }
}

void DoIP_ActivationLineSwitchInactive(void) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  uint16_t i;
  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    ASLOG(DOIP, ("switch to inactive\n")); /* @SWS_DoIP_00234 */
    for (i = 0; i < config->NumOfTcpConnections; i++) {
      if (config->TcpConnections[i].RequestAddressAssignment) {
        SoAd_CloseSoCon(config->TcpConnections[i].SoConId, TRUE);
      }
    }
    for (i = 0; i < config->NumOfUdpConnections; i++) {
      if (config->UdpConnections[i].RequestAddressAssignment) {
        SoAd_CloseSoCon(config->UdpConnections[i].SoConId, TRUE);
      }
    }
    for (i = 0; i < config->NumOfUdpVehicleAnnouncementConnections; i++) {
      if (config->UdpVehicleAnnouncementConnections[i].RequestAddressAssignment) {
        SoAd_CloseSoCon(config->UdpVehicleAnnouncementConnections[i].SoConId, TRUE);
        config->UdpVehicleAnnouncementConnections[i].Context->State = DOIP_CON_CLOSED;
      }
    }
    for (i = 0; i < config->MaxTesterConnections; i++) {
      if (DOIP_CON_CLOSED != config->TesterConnections[i].Context->State) {
        SoAd_CloseSoCon(config->TesterConnections[i].SoConId, TRUE);
        config->TesterConnections[i].Context->State = DOIP_CON_CLOSED;
      }
    }
    context->ActivationLineState = DOIP_ACTIVATION_LINE_INACTIVE;
  }
}

void DoIP_ActivationLineSwitch(uint8_t interfaceId, boolean *active) {
  DoIP_ContextType *context = &DoIP_Context;
  boolean bActive;

  DET_VALIDATE(0u == interfaceId, 0x0e, DOIP_E_INVALID_PARAMETER, return);
  DET_VALIDATE(NULL != active, 0x0e, DOIP_E_PARAM_POINTER, return);

  (void)interfaceId; /* only support 1 interface */
  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    bActive = TRUE;
  } else {
    bActive = FALSE;
  }

  if (TRUE == *active) {
    DoIP_ActivationLineSwitchActive();
  } else {
    DoIP_ActivationLineSwitchInactive();
  }

  *active = bActive;
}

void DoIP_TriggerVehicleAnnouncement(uint8_t interfaceId) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;

  DET_VALIDATE(0u == interfaceId, 0x0d, DOIP_E_INVALID_PARAMETER, return);

  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    config->UdpVehicleAnnouncementConnections[0].Context->AnnouncementTimer =
      config->InitialVehicleAnnouncementTime;
    config->UdpVehicleAnnouncementConnections[0].Context->AnnouncementCounter = 0;
  }
}

void DoIP_SoConModeChg(SoAd_SoConIdType soConId, SoAd_SoConModeType mode) {
  Std_ReturnType ret = E_NOT_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  uint16_t i;

  for (i = 0; i < config->NumOfTcpConnections; i++) {
    if (soConId == config->TcpConnections[i].SoConId) {
      ASLOG(DOIP, ("Tcp %d SoCon mode %d\n", i, mode));
      ret = E_OK;
      break;
    }
  }

  for (i = 0; (i < config->NumOfUdpConnections) && (E_NOT_OK == ret); i++) {
    if (soConId == config->UdpConnections[i].SoConId) {
      ASLOG(DOIP, ("Udp SoCon %d mode %d\n", i, mode));
      ret = E_OK;
    }
  }

  for (i = 0; i < config->NumOfUdpVehicleAnnouncementConnections; i++) {
    if (soConId == config->UdpVehicleAnnouncementConnections[i].SoConId) {
      if (SOAD_SOCON_ONLINE == mode) { /* @SWS_DoIP_00205 */
        asAssert(DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState);
        config->UdpVehicleAnnouncementConnections[i].Context->State = DOIP_CON_OPEN;
        config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementTimer =
          config->InitialVehicleAnnouncementTime;
        config->UdpVehicleAnnouncementConnections[i].Context->AnnouncementCounter = 0;
      }
      ASLOG(DOIP, ("UdpVehicleAnnouncement SoCon %d mode %d\n", i, mode));
      ret = E_OK;
      break;
    }
  }

  for (i = 0; (i < config->MaxTesterConnections) && (E_NOT_OK == ret); i++) {
    if (soConId == config->TesterConnections[i].SoConId) {
      if (SOAD_SOCON_ONLINE == mode) { /* @SWS_DoIP_00143 */
        asAssert(DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState);
        DoIP_ForgetDiagMsg(&config->TesterConnections[i]);
        memset(config->TesterConnections[i].Context, 0, sizeof(DoIP_TesterConnectionContextType));
        config->TesterConnections[i].Context->InactivityTimer = config->InitialInactivityTime;
        config->TesterConnections[i].Context->State = DOIP_CON_OPEN;
      }
      ASLOG(DOIP, ("Tester SoCon %d mode %d\n", i, mode));
      ret = E_OK;
    }
  }
}

void DoIP_SoAdIfRxIndication(PduIdType rxPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_OK;
  uint8_t nack = DOIP_INVALID_PROTOCOL_NACK;
  PduInfoType PduInfo;
  DoIP_ContextType *context = &DoIP_Context;
  DoIP_MsgType msg;
  uint8_t localMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];
  msg.Res = localMsg;
  msg.ResLen = sizeof(localMsg);

  if (rxPduId < DOIP_CONFIG->NumOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
      /* @SWS_DoIP_00202 */
      ret = DOIP_E_NOT_OK_SILENT;
    }
  } else {
    ASLOG(DOIPE, ("IfRxInd with invalid RxPduId %d\n", rxPduId));
    ret = DOIP_E_NOT_OK_SILENT;
  }

  if (E_OK == ret) {
    ret = DoIP_DecodeMsg(pduInfoPtr, &msg);
    if ((E_OK == ret) && (msg.ReqLen < msg.PayloadLength)) {
      nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    ret = DoIP_IfHandleMessage(rxPduId, &msg, &nack);
  }

  if (DOIP_E_NOT_OK_SILENT == ret) {
    /* slient as inactive or ... */
  } else {
    if (E_OK != ret) {
      DoIP_FillHeader(msg.Res, DOIP_GENERAL_HEADER_NEGATIVE_ACK, 1);
      msg.Res[8] = nack;
      msg.ResLen = 9;
    }

    PduInfo.SduDataPtr = msg.Res;
    PduInfo.MetaDataPtr = pduInfoPtr->MetaDataPtr;
    PduInfo.SduLength = msg.ResLen;
    /** @note Generally, only 1 UDP connections for DoIP */
    (void)SoAd_IfTransmit(DOIP_CONFIG->UdpConnections[0].SoAdTxPdu, &PduInfo);
  }
}

void DoIP_SoAdIfTxConfirmation(PduIdType txPduId, Std_ReturnType result) {
}

BufReq_ReturnType DoIP_SoAdTpStartOfReception(PduIdType rxPduId, const PduInfoType *info,
                                              PduLengthType tpSduLength,
                                              PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;

  if (rxPduId < DOIP_CONFIG->NumOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
      /* @SWS_DoIP_00202 */
      ret = BUFREQ_E_NOT_OK;
    } else {
      if (DOIP_CONFIG->RxPduIdToConnectionMap[rxPduId] >= config->MaxTesterConnections) {
        /* @SWS_DoIP_00101 */
        ret = BUFREQ_E_NOT_OK;
        ASLOG(DOIPE, ("rxPduId %d get invalid connection map\n", rxPduId));
      }
    }
  } else {
    ASLOG(DOIPE, ("TpStartOfReception with invalid rxPduId %d\n", rxPduId));
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

BufReq_ReturnType DoIP_SoAdTpCopyRxData(PduIdType rxPduId, const PduInfoType *pduInfoPtr,
                                        PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  Std_ReturnType r = E_OK;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  const DoIP_TesterConnectionType *connection;
  uint8_t nack = DOIP_INVALID_PROTOCOL_NACK;
  DoIP_MsgType msg;
  uint8_t localMsg[DOIP_SHORT_MSG_LOCAL_BUFFER_SIZE];
  msg.Res = localMsg;
  msg.ResLen = sizeof(localMsg);

  if (rxPduId < DOIP_CONFIG->NumOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_INACTIVE == context->ActivationLineState) {
      ret = BUFREQ_E_NOT_OK;
      r = DOIP_E_NOT_OK_SILENT;
    } else {
      if (DOIP_CONFIG->RxPduIdToConnectionMap[rxPduId] < config->MaxTesterConnections) {
        connection = &config->TesterConnections[DOIP_CONFIG->RxPduIdToConnectionMap[rxPduId]];
      } else {
        ret = BUFREQ_E_NOT_OK;
        r = DOIP_E_NOT_OK_SILENT;
      }
    }
  } else {
    ASLOG(DOIPE, ("TpStartOfReception with invalid RxPduId %d\n", rxPduId));
    ret = BUFREQ_E_NOT_OK;
    r = DOIP_E_NOT_OK_SILENT;
  }

  if (E_OK == r) {
    if (DOIP_MSG_IDLE == connection->Context->Msg.State) {
      r = DoIP_DecodeMsg(pduInfoPtr, &msg);
      if (E_OK == r) {
        r = DoIP_TpStartOfReception(rxPduId, &msg, &nack);
      }
    } else {
      msg.Req = pduInfoPtr->SduDataPtr;
      msg.ReqLen = pduInfoPtr->SduLength;
      r = DoIP_TpCopyRxData(rxPduId, &msg, &nack);
    }
  }

  if (E_NOT_OK == r) {
    DoIP_FillHeader(msg.Res, DOIP_GENERAL_HEADER_NEGATIVE_ACK, 1);
    msg.Res[DOIP_HEADER_LENGTH] = nack;
    msg.ResLen = DOIP_HEADER_LENGTH + 1;
  }

  if (E_OK == r) {
    connection->Context->InactivityTimer = config->GeneralInactivityTime;
    (void)doipTpSendResponse(connection, msg.Res, msg.ResLen);
  } else if (DOIP_E_NOT_OK_SILENT == r) {
    /* slient */
  } else if (DOIP_E_PENDING == r) {
    /* TODO */
  } else {
    (void)doipTpSendResponse(connection, msg.Res, msg.ResLen);
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

void DoIP_MainFunction(void) {
  DoIP_ContextType *context = &DoIP_Context;

  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    DoIP_HandleInactivityTimer();
    DoIP_HandleAliveCheckResponseTimer();
    DoIP_HandleVehicleAnnouncement();
    DoIP_HandleDiagMsgResponse();
    DoIP_HandleRoutineActivationMain();
  }
}

Std_ReturnType DoIP_TpTransmit(PduIdType txPduId, const PduInfoType *pduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  BufReq_ReturnType bret;
  uint16_t i;
  uint16_t j;
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  const DoIP_TesterConnectionType *connection = NULL;
  const DoIP_TargetAddressType *TargetAddressRef;
  const DoIP_TargetNodeType *targetNode = NULL;
  PduInfoType PduInfo;
  PduLengthType offset;
  PduLengthType left;
  uint16_t sa, ta;
  uint8_t *res;
  uint32_t resLen;

  /* the payload is pulled from the upper layer through PduR_DoIPCopyTxData(),
   * so a NULL SduDataPtr (e.g. the Dcm NRC 0x78 response-pending PDU) is valid,
   * only the PduInfo pointer and the length are required here. */
  DET_VALIDATE((NULL != pduInfoPtr) && (pduInfoPtr->SduLength > 0u), 0x53, DOIP_E_PARAM_POINTER,
               return E_NOT_OK);

  if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
    for (i = 0; (NULL == connection) && (i < config->MaxTesterConnections); i++) {
      if (DOIP_CON_CLOSED != config->TesterConnections[i].Context->State) {
        if (NULL != config->TesterConnections[i].Context->Msg.TargetAddressRef) {
          TargetAddressRef = config->TesterConnections[i].Context->Msg.TargetAddressRef;
          for (j = 0; (NULL == connection) && (j < TargetAddressRef->NumTargetNodes); j++) {
            if (TargetAddressRef->TargetNodes[j].DoipTxPduId == txPduId) {
              connection = &config->TesterConnections[i];
              targetNode = &TargetAddressRef->TargetNodes[j];
              ret = E_OK;
            }
          }
        }
      }
    }
  }

  if (E_OK == ret) {
    if (connection->Context->Msg.State != DOIP_MSG_IDLE) {
      ASLOG(DOIPE,
            ("[%d] UDS response when state is %d\n", txPduId, connection->Context->Msg.State));
      ret = E_NOT_OK;
    }
  } else {
    ASLOG(DOIPE, ("[%d] no UDS request\n", txPduId));
  }

  if (E_OK == ret) {
    ASLOG(DOIP, ("[%d] UDS response, len = %d, PduR Tx %u\n", txPduId, pduInfoPtr->SduLength,
                 targetNode->TxPduId));
    resLen = DOIP_HEADER_LENGTH + 4 + pduInfoPtr->SduLength;
    res = Net_MemGet(&resLen);
    if (NULL == res) {
      ret = E_NOT_OK;
    }
  }
  if (E_OK == ret) {
    DoIP_FillHeader(res, DOIP_DIAGNOSTIC_MESSAGE, pduInfoPtr->SduLength + 4);
    sa = targetNode->TargetAddress;
    ta = connection->Context->TesterRef->TesterSA;
    res[DOIP_HEADER_LENGTH + 0u] = (sa >> 8) & 0xFF;
    res[DOIP_HEADER_LENGTH + 1u] = sa & 0xFF;
    res[DOIP_HEADER_LENGTH + 2u] = (ta >> 8) & 0xFF;
    res[DOIP_HEADER_LENGTH + 3u] = ta & 0xFF;
    PduInfo.SduDataPtr = &res[DOIP_HEADER_LENGTH + 4u];
    PduInfo.SduLength = pduInfoPtr->SduLength;
    if (PduInfo.SduLength > (resLen - DOIP_HEADER_LENGTH - 4)) {
      PduInfo.SduLength = resLen - DOIP_HEADER_LENGTH - 4;
    }
    offset = 0;
    PduInfo.MetaDataPtr = (uint8_t *)&offset;
    bret = PduR_DoIPCopyTxData(targetNode->TxPduId, &PduInfo, NULL, &left);
    if (BUFREQ_OK == bret) {
      resLen = DOIP_HEADER_LENGTH + PduInfo.SduLength + 4;
      ret = doipTpSendResponse(connection, res, resLen);
      if (E_OK != ret) {
        PduR_DoIPTxConfirmation(targetNode->TxPduId, E_NOT_OK);
      }
    } else {
      ret = E_NOT_OK;
    }

    if (E_OK == ret) {
      if (PduInfo.SduLength >= pduInfoPtr->SduLength) {
        PduR_DoIPTxConfirmation(targetNode->TxPduId, E_OK);
        ASLOG(DOIP, ("[%d] send UDS response done\n", txPduId));
      } else {
        targetNode->Context->State = DOIP_MSG_TX;
        targetNode->Context->TpSduLength = pduInfoPtr->SduLength;
        targetNode->Context->Index = PduInfo.SduLength;
        ASLOG(DOIP, ("[%d] send UDS response on going\n", txPduId));
      }
    }

    Net_MemFree(res);
  }

  return ret;
}

Std_ReturnType DoIP_HeaderIndication(PduIdType rxPduId, const PduInfoType *info,
                                     uint32_t *payloadLength) {
  Std_ReturnType ret = E_NOT_OK;
  DoIP_ContextType *context = &DoIP_Context;
  DoIP_MsgType msg;

  if (rxPduId < DOIP_CONFIG->NumOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
      ret = E_OK;
    }
  } else {
    ASLOG(DOIPE, ("HeaderRxInd with invalid rxPduId %u\n", rxPduId));
  }

  if (E_OK == ret) {
    ret = DoIP_DecodeMsg(info, &msg);
    if (E_OK == ret) {
      *payloadLength = msg.PayloadLength;
      ret = E_OK;
    }
  }

  return ret;
}

void DoIP_RxIndication(PduIdType rxPduId, const PduInfoType *info) {
  const DoIP_ConfigType *config = DOIP_CONFIG;
  DoIP_ContextType *context = &DoIP_Context;
  PduLengthType bufferSize;

  if (rxPduId < DOIP_CONFIG->NumOfRxPduIds) {
    if (DOIP_ACTIVATION_LINE_ACTIVE == context->ActivationLineState) {
      if (DOIP_CONFIG->RxPduIdToConnectionMap[rxPduId] < config->MaxTesterConnections) {
        DoIP_SoAdTpCopyRxData(rxPduId, info, &bufferSize);
      } else {
        DoIP_SoAdIfRxIndication(rxPduId, info);
      }
    }
  } else {
    ASLOG(DOIPE, ("RxInd with invalid rxPduId %u\n", rxPduId));
  }
}

void DoIP_GetVersionInfo(Std_VersionInfoType *versionInfo) {
  DET_VALIDATE(NULL != versionInfo, 0x00, DOIP_E_PARAM_POINTER, return);

  versionInfo->VendorId = STD_VENDOR_ID_AS;
  versionInfo->ModuleId = MODULE_ID_DOIP;
  versionInfo->SwMajorVersion = 4;
  versionInfo->SwMinorVersion = 0;
  versionInfo->SwPatchVersion = 5;
}

/** @brief release notes
 * - 4.0.1: Fix assignment instead of comparison in protocol check issue.
 * - 4.0.2: Support optional OEM for routing activation response.
 * - 4.0.3: Fix memory leak issue during socket close.
 * - 4.0.4: Add target address config check and diag message error handling.
 * - 4.0.5: Allow NULL SduDataPtr in DoIP_TpTransmit (payload pulled via PduR_DoIPCopyTxData).
 */
