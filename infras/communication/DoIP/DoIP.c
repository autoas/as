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
#include <assert.h>
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DOIP 1
#define AS_LOG_DOIPE 2

#define DOIP_PROTOCOL_VERSION 2
#define DOIP_HEADER_LENGTH 8u

#define DOIP_CONFIG (&DoIP_Config)

/* return this when negative response buffer set */
#define DOIP_E_NOT_OK ((Std_ReturnType)200)

#define DOIP_E_NOT_OK_SILENT ((Std_ReturnType)201)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern const DoIP_ConfigType DoIP_Config;
/* ================================ [ DATAS     ] ============================================== */
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

static void doipSetupVehicleAnnouncementResponse(const DoIP_ChannelConfigType *config) {
  Std_ReturnType ret;
  doipFillHeader(config->txBuf, DOIP_VAN_MSG_OR_VIN_RESPONCE, 33);
  config->context->txLen = DOIP_HEADER_LENGTH + 33;
  /* @SWS_DoIP_00072 */
  ret = config->GetVin(&config->txBuf[DOIP_HEADER_LENGTH]);
  if (E_OK != ret) {
    memset(&config->txBuf[DOIP_HEADER_LENGTH], config->VinInvalidityPattern, 17);
    config->txBuf[DOIP_HEADER_LENGTH + 31] = 0x00; /* Further action byte */
    config->txBuf[DOIP_HEADER_LENGTH + 32] = 0x10; /* VIN/GID Status */
  } else {
    config->txBuf[DOIP_HEADER_LENGTH + 31] = 0x00;
    config->txBuf[DOIP_HEADER_LENGTH + 32] = 0x00;
  }

  config->txBuf[DOIP_HEADER_LENGTH + 17] = (config->LogicalAddress >> 8) & 0xFF;
  config->txBuf[DOIP_HEADER_LENGTH + 18] = config->LogicalAddress & 0xFF;

  config->GetEID(&config->txBuf[DOIP_HEADER_LENGTH + 19]);
  config->GetGID(&config->txBuf[DOIP_HEADER_LENGTH + 25]);
}

Std_ReturnType doipTpSendResponse(PduIdType RxPduId, const DoIP_ChannelConfigType *config) {
  PduInfoType PduInfo;
  Std_ReturnType ret;
  if (config->context->txLen > 0) {
    PduInfo.SduDataPtr = config->txBuf;
    PduInfo.MetaDataPtr = NULL;
    PduInfo.SduLength = config->context->txLen;
    ret = SoAd_TpTransmit(RxPduId, &PduInfo);
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

static Std_ReturnType doipHandleVIDRequest(PduIdType RxPduId, const uint8_t *payload,
                                           uint32_t length, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  uint8_t Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  if (0 == length) {
    doipSetupVehicleAnnouncementResponse(config);
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle VID Request\n", Channel));
  return ret;
}

static const DoIP_TesterType *doipFindTester(const DoIP_ChannelConfigType *config,
                                             uint16_t sourceAddress) {
  const DoIP_TesterType *tester = NULL;
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

static void doipBuildRountineActivationResponse(const DoIP_ChannelConfigType *config,
                                                const DoIP_TesterConnectionType *connection,
                                                Std_ReturnType ret, uint16_t sa, uint8_t resCode) {
  doipFillHeader(config->txBuf, DOIP_ROUTING_ACTIVATION_RESPONSE, 13);
  config->txBuf[DOIP_HEADER_LENGTH + 0] = (sa >> 8) & 0xFF; /* Logical Address Tester */
  config->txBuf[DOIP_HEADER_LENGTH + 1] = sa & 0xFF;

  config->txBuf[DOIP_HEADER_LENGTH + 2] =
    (config->LogicalAddress >> 8) & 0xFF; /* Logical address of DoIP entity */
  config->txBuf[DOIP_HEADER_LENGTH + 3] = config->LogicalAddress & 0xFF;
  config->txBuf[DOIP_HEADER_LENGTH + 4] = resCode;
  config->context->txLen = DOIP_HEADER_LENGTH + 13;

  if ((DOIP_E_PENDING == ret) && (0x11 == resCode)) {
    (void)doipTpSendResponse(connection->SoAdTxPdu, config); /* @SWS_DoIP_00114 */
  }
}

static Std_ReturnType doipCheckTesterConnectionAlive(const DoIP_ChannelConfigType *config,
                                                     const DoIP_TesterConnectionType *connection) {
  Std_ReturnType ret = E_OK;
  if (0 == connection->context->AliveCheckResponseTimer) {
    /* no request is already on going, start one */
    doipFillHeader(config->txBuf, DOIP_ALIVE_CHECK_REQUEST, 0);
    config->context->txLen = DOIP_HEADER_LENGTH;
    connection->context->isAlive = FALSE;
    connection->context->AliveCheckResponseTimer = connection->AliveCheckResponseTimeout;
    ret = doipTpSendResponse(connection->SoAdTxPdu, config);
  }
  return ret;
}

static Std_ReturnType doipSocketHandler(const DoIP_ChannelConfigType *config,
                                        const DoIP_TesterConnectionType *tstcon, uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  boolean isAliveCheckOngoing = FALSE;
  int i;
  const DoIP_TesterConnectionType *connection;
  for (i = 0; (i < config->numOfTesterConnections) && (E_OK == ret); i++) {
    connection = &config->testerConnections[i];
    if ((tstcon != connection) && (DOIP_CON_OPEN == connection->context->state) &&
        (tstcon->context->ramgr.tester == connection->context->TesterRef)) {
      ASLOG(DOIP, ("Tester 0x%x is already active on connection %d, check alive\n",
                   tstcon->context->ramgr.tester->TesterSA, i));
      ret = doipCheckTesterConnectionAlive(config, connection);
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

  if (ret == E_OK) {
    if (isAliveCheckOngoing) {
      ret = DOIP_E_PENDING;
    }
  }

  return ret;
}

static Std_ReturnType doipSocketHandler2(const DoIP_ChannelConfigType *config,
                                         const DoIP_TesterConnectionType *tstcon,
                                         uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  boolean isAliveCheckOngoing = FALSE;
  int i;
  const DoIP_TesterConnectionType *connection;
  for (i = 0; (i < config->numOfTesterConnections) && (E_OK == ret); i++) {
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

static Std_ReturnType doipRoutingActivationManager(const DoIP_ChannelConfigType *config,
                                                   const DoIP_TesterConnectionType *connection,
                                                   uint8_t *resCode) {
  Std_ReturnType ret = E_OK;
  const DoIP_TesterType *tester = connection->context->ramgr.tester;
  uint8_t raid = connection->context->ramgr.raid;
  const DoIP_RoutingActivationType *ra = tester->RoutingActivationRefs[raid];
  boolean Authentified;
  boolean Confirmed;

  boolean ongoing = FALSE;

  do {
    switch (connection->context->ramgr.state) {
    case DOIP_RA_SOCKET_HANDLER:
      ret = doipSocketHandler(config, connection, resCode);
      if (DOIP_E_NOT_OK != ret) {
        connection->context->ramgr.state = DOIP_RA_SOCKET_HANDLER_2;
        ongoing = TRUE;
      } else {
        ongoing = FALSE;
      }
      break;
    case DOIP_RA_SOCKET_HANDLER_2:
      ret = doipSocketHandler2(config, connection, resCode);
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
                                       &config->txBuf[DOIP_HEADER_LENGTH + 9]);
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
                                     &config->txBuf[DOIP_HEADER_LENGTH + 9]);
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
static Std_ReturnType doipHandleRoutineActivation(PduIdType RxPduId, const uint8_t *payload,
                                                  uint32_t length, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  uint8_t Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  const DoIP_TesterConnectionType *connection =
    &config->testerConnections[DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId]];
  const DoIP_TesterType *tester = NULL;
  const DoIP_RoutingActivationType *ra = NULL;
  uint8_t raid;
  uint16_t sourceAddress;
  uint8_t activationType;
  uint8_t resCode = 0xFF;
  if ((7 == length) || (11 == length)) { /* @SWS_DoIP_00117 */
    sourceAddress = ((uint16_t)payload[0] << 8) + payload[1];
    activationType = payload[2];
    tester = doipFindTester(config, sourceAddress);
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
      memcpy(connection->context->ramgr.OEM, &payload[7], 4);
      connection->context->ramgr.state = DOIP_RA_SOCKET_HANDLER;
      ret = doipRoutingActivationManager(config, connection, &resCode);
    }

    doipBuildRountineActivationResponse(config, connection, ret, sourceAddress, resCode);
    if (DOIP_E_NOT_OK == ret) {
      connection->context->InactivityTimer = 1; /* close it the next MainFunction */
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }
  ASLOG(DOIP, ("[%d] handle Routine Activation Request\n", Channel));
  return ret;
}

static void doipHandleRoutineActivationMain(uint8_t Channel) {
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  const DoIP_TesterConnectionType *connection;
  int i;
  uint8_t resCode = 0xFE;
  Std_ReturnType ret;

  for (i = 0; i < config->numOfTesterConnections; i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      if (DOIP_RA_IDLE != connection->context->ramgr.state) {
        ret = doipRoutingActivationManager(config, connection, &resCode);
        doipBuildRountineActivationResponse(config, connection, ret,
                                            connection->context->ramgr.tester->TesterSA, resCode);
        if (DOIP_E_NOT_OK == ret) {
          connection->context->InactivityTimer = 1; /* close it the next MainFunction */
        }

        if (E_OK == ret) {
          connection->context->InactivityTimer = connection->GeneralInactivityTime;
          (void)doipTpSendResponse(connection->SoAdTxPdu, config);
        } else if (DOIP_E_PENDING == ret) {
        } else {
          (void)doipTpSendResponse(connection->SoAdTxPdu, config);
          ret = BUFREQ_E_NOT_OK;
        }
      }
    }
  }
}

static Std_ReturnType doIpIfHandleMessage(PduIdType RxPduId, const PduInfoType *PduInfoPtr,
                                          uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  uint16_t payloadType;
  uint32_t payloadLength;
  payloadType = ((uint16_t)PduInfoPtr->SduDataPtr[2] << 8) + PduInfoPtr->SduDataPtr[3];
  payloadLength = ((uint32_t)PduInfoPtr->SduDataPtr[4] << 24) +
                  ((uint32_t)PduInfoPtr->SduDataPtr[5] << 16) +
                  ((uint32_t)PduInfoPtr->SduDataPtr[6] << 8) + PduInfoPtr->SduDataPtr[7];
  if ((payloadLength + DOIP_HEADER_LENGTH) <= PduInfoPtr->SduLength) {
    switch (payloadType) {
    case DOIP_VID_REQUEST:
      ret = doipHandleVIDRequest(RxPduId, &PduInfoPtr->SduDataPtr[DOIP_HEADER_LENGTH],
                                 payloadLength, nack);
      break;
    case DOIP_VAN_MSG_OR_VIN_RESPONCE:
      /* @SWS_DoIP_00293 */
      ret = DOIP_E_NOT_OK_SILENT;
      break;
    default:
      /* @SWS_DoIP_00016 */
      *nack = DOIP_UNKNOW_PAYLOAD_TYPE_NACK;
      ret = E_NOT_OK;
      break;
    }
  } else {
    *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
    ret = E_NOT_OK;
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
        if (ta == ra->TargetAddressRefs[i].TargetAddress) {
          TargetAddressRef = &ra->TargetAddressRefs[j];
        }
      }
    }
  }

  return TargetAddressRef;
}

static Std_ReturnType doipHandleDiagnosticMessage(PduIdType RxPduId, const uint8_t *payload,
                                                  uint32_t length, uint32_t curLen, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  uint8_t Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  const DoIP_TesterConnectionType *connection =
    &config->testerConnections[DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId]];
  const DoIP_TargetAddressType *TargetAddressRef;
  uint16_t sa, ta;
  uint8_t resCode = 0xFF;
  BufReq_ReturnType bufReq;
  PduInfoType pduInfo;
  PduLengthType bufferSize;

  if (DOIP_MSG_IDLE == connection->context->msg.state) {
    if (length < 5) {
      *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
      ret = E_NOT_OK;
    }

    if (curLen < 5) {
      *nack = 0x99; /* TODO: internal error */
      ret = E_NOT_OK;
    }
  }

  if ((0u == connection->context->RAMask) || (NULL == connection->context->TesterRef)) {
    resCode = 0x02; /* @SWS_DoIP_00123 */
    ASLOG(DOIPE, ("not activated\n"));
    ret = DOIP_E_NOT_OK;
  }

  if (E_OK == ret) {
    if (DOIP_MSG_IDLE == connection->context->msg.state) {
      sa = ((uint16_t)payload[0] << 8) + payload[1];
      ta = ((uint16_t)payload[2] << 8) + payload[3];

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
      connection->context->msg.TpSduLength = length;
      connection->context->msg.TargetAddressRef = TargetAddressRef;
      pduInfo.SduDataPtr = (uint8_t *)&payload[4];
      pduInfo.SduLength = curLen - 4;
      bufReq =
        PduR_DoIPStartOfReception(TargetAddressRef->RxPduId, &pduInfo, length - 4, &bufferSize);
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
          connection->context->msg.index = curLen - 4;
          connection->context->msg.TpSduLength = length - 4;
        }
      }
    } else {
      TargetAddressRef = connection->context->msg.TargetAddressRef;
      assert(TargetAddressRef != NULL);
      pduInfo.SduDataPtr = (uint8_t *)payload;
      pduInfo.SduLength = curLen;
      bufReq = PduR_DoIPCopyRxData(TargetAddressRef->RxPduId, &pduInfo, &bufferSize);
      if (bufReq != BUFREQ_OK) {
        PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_NOT_OK);
        connection->context->msg.state = DOIP_MSG_IDLE;
        resCode = 0x08; /* @SWS_DoIP_00174 */
        ret = DOIP_E_NOT_OK;
      } else {
        connection->context->msg.index += curLen;
      }
    }
  }

  if (E_OK == ret) {
    if (connection->context->msg.index >= connection->context->msg.TpSduLength) {
      connection->context->msg.state = DOIP_MSG_IDLE;
      PduR_DoIPRxIndication(TargetAddressRef->RxPduId, E_OK);
      doipFillHeader(config->txBuf, DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK, 5);
      config->txBuf[DOIP_HEADER_LENGTH + 4] = 0x0; /* ack code */
      config->context->txLen = DOIP_HEADER_LENGTH + 5;
    } else {
      connection->context->msg.state = DOIP_MSG_RX;
      ret = DOIP_E_PENDING;
    }
  }

  if (ret == DOIP_E_NOT_OK) {
    doipFillHeader(config->txBuf, DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK, 5);
    config->txBuf[DOIP_HEADER_LENGTH + 4] = resCode;
    config->context->txLen = DOIP_HEADER_LENGTH + 5;
  }

  ASLOG(DOIP, ("[%d] handle Diagnostic Message\n", Channel));

  return ret;
}

static Std_ReturnType doipHandleAliveCheckRequest(PduIdType RxPduId, const uint8_t *payload,
                                                  uint32_t length, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  uint8_t Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];

  if (0 == length) {
    doipFillHeader(config->txBuf, DOIP_ALIVE_CHECK_RESPONSE, 2);
    config->txBuf[DOIP_HEADER_LENGTH + 0] = (config->LogicalAddress >> 8) & 0xFF;
    config->txBuf[DOIP_HEADER_LENGTH + 1] = config->LogicalAddress & 0xFF;
    config->context->txLen = DOIP_HEADER_LENGTH + 2;
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }

  ASLOG(DOIP, ("[%d] handle Alive Check Request\n", Channel));

  return ret;
}

static Std_ReturnType doipHandleAliveCheckResponse(PduIdType RxPduId, const uint8_t *payload,
                                                   uint32_t length, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  uint8_t Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  const DoIP_TesterConnectionType *connection =
    &config->testerConnections[DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId]];
  uint16_t sa;

  if (2 == length) {
    sa = ((uint16_t)payload[0] << 8) + payload[1];
    if (sa != connection->context->TesterRef->TesterSA) {
      /* @SWS_DoIP_00141 */
      ASLOG(DOIPE, ("sa %X not right, expected %X, close it\n", sa,
                    connection->context->TesterRef->TesterSA));
      ret = DOIP_E_NOT_OK_SILENT;
      connection->context->InactivityTimer = 1; /* close it the next MainFunction */
    } else {
      connection->context->isAlive = TRUE;
      connection->context->AliveCheckResponseTimer = 0;
      config->context->txLen = 0; /* no response */
    }
  } else {
    *nack = DOIP_INVALID_PAYLOAD_LENGTH_NACK;
    ret = E_NOT_OK;
  }

  ASLOG(DOIP, ("[%d] handle Alive Check Response\n", Channel));

  return ret;
}

static Std_ReturnType doipTpStartOfReception(PduIdType RxPduId, const PduInfoType *PduInfoPtr,
                                             PduLengthType *bufferSizePtr, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  uint16_t payloadType;
  uint32_t payloadLength;
  payloadType = ((uint16_t)PduInfoPtr->SduDataPtr[2] << 8) + PduInfoPtr->SduDataPtr[3];
  payloadLength = ((uint32_t)PduInfoPtr->SduDataPtr[4] << 24) +
                  ((uint32_t)PduInfoPtr->SduDataPtr[5] << 16) +
                  ((uint32_t)PduInfoPtr->SduDataPtr[6] << 8) + PduInfoPtr->SduDataPtr[7];

  switch (payloadType) {
  case DOIP_ROUTING_ACTIVATION_REQUEST:
    if ((payloadLength + DOIP_HEADER_LENGTH) <= PduInfoPtr->SduLength) {
      ret = doipHandleRoutineActivation(RxPduId, &PduInfoPtr->SduDataPtr[DOIP_HEADER_LENGTH],
                                        payloadLength, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_ALIVE_CHECK_REQUEST:
    if ((payloadLength + DOIP_HEADER_LENGTH) <= PduInfoPtr->SduLength) {
      ret = doipHandleAliveCheckRequest(RxPduId, &PduInfoPtr->SduDataPtr[DOIP_HEADER_LENGTH],
                                        payloadLength, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_ALIVE_CHECK_RESPONSE:
    if ((payloadLength + DOIP_HEADER_LENGTH) <= PduInfoPtr->SduLength) {
      ret = doipHandleAliveCheckResponse(RxPduId, &PduInfoPtr->SduDataPtr[DOIP_HEADER_LENGTH],
                                         payloadLength, nack);
    } else {
      *nack = DOIP_TOO_MUCH_PAYLOAD_NACK;
      ret = E_NOT_OK;
    }
    break;
  case DOIP_DIAGNOSTIC_MESSAGE:
    ret =
      doipHandleDiagnosticMessage(RxPduId, &PduInfoPtr->SduDataPtr[DOIP_HEADER_LENGTH],
                                  payloadLength, PduInfoPtr->SduLength - DOIP_HEADER_LENGTH, nack);
    break;
  default:
    *nack = DOIP_UNKNOW_PAYLOAD_TYPE_NACK;
    ret = E_NOT_OK;
    break;
  }

  return ret;
}

static Std_ReturnType doipTpCopyRxData(PduIdType RxPduId, const PduInfoType *PduInfoPtr,
                                       PduLengthType *bufferSizePtr, uint8_t *nack) {
  Std_ReturnType ret = E_OK;
  /* only DIAGNOSTIC_MESSAGE allow this */

  ret =
    doipHandleDiagnosticMessage(RxPduId, PduInfoPtr->SduDataPtr, 0, PduInfoPtr->SduLength, nack);

  return ret;
}

static void doipHandleInactivityTimer(uint8_t Channel) {
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  const DoIP_TesterConnectionType *connection;
  int i;

  for (i = 0; i < config->numOfTesterConnections; i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      if (connection->context->InactivityTimer > 0) {
        connection->context->InactivityTimer--;
        if (0 == connection->context->InactivityTimer) {
          ASLOG(DOIP, ("[%d] Tester SoCon %d InactivityTimer timeout\n", Channel, i));
          SoAd_CloseSoCon(connection->SoConId, TRUE);
          memset(connection->context, 0, sizeof(DoIP_TesterConnectionContextType));
        }
      }
    }
  }
}

static void doipHandleAliveCheckResponseTimer(uint8_t Channel) {
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  const DoIP_TesterConnectionType *connection;
  int i;

  for (i = 0; i < config->numOfTesterConnections; i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      if (connection->context->AliveCheckResponseTimer > 0) {
        connection->context->AliveCheckResponseTimer--;
        if (0 == connection->context->AliveCheckResponseTimer) {
          ASLOG(DOIP, ("[%d] Tester SoCon %d AliveCheckResponseTimer timeout\n", Channel, i));
          SoAd_CloseSoCon(connection->SoConId, TRUE);
          memset(connection->context, 0, sizeof(DoIP_TesterConnectionContextType));
        }
      }
    }
  }
}

static void doipHandleVehicleAnnouncement(uint8_t Channel) {
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  int i;
  PduInfoType PduInfo;

  for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
    if (DOIP_CON_CLOSED != config->UdpVehicleAnnouncementConnections[i].context->state) {
      if (config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer > 0) {
        config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer--;
        if (0 == config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer) {
          ASLOG(DOIP, ("[%d] Udp %d VehicleAnnouncement\n", Channel, i));
          doipSetupVehicleAnnouncementResponse(config);
          PduInfo.SduDataPtr = config->txBuf;
          PduInfo.MetaDataPtr = NULL;
          PduInfo.SduLength = config->context->txLen;
          (void)SoAd_IfTransmit(config->UdpVehicleAnnouncementConnections[i].SoAdTxPdu, &PduInfo);
          config->UdpVehicleAnnouncementConnections[i].context->AnnouncementCounter++;
          if (config->UdpVehicleAnnouncementConnections[i].context->AnnouncementCounter <
              config->UdpVehicleAnnouncementConnections[i].VehicleAnnouncementCount) {
            config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer =
              config->UdpVehicleAnnouncementConnections[i].VehicleAnnouncementInterval;
          }
        }
      }
    }
  }
}

static void doipHandleDiagMsgResponse(uint8_t Channel) {
  Std_ReturnType ret;
  BufReq_ReturnType bret;
  PduInfoType PduInfo;
  PduLengthType left;
  PduIdType TxPduId;
  const DoIP_TargetAddressType *TargetAddressRef;
  const DoIP_ChannelConfigType *config = &DOIP_CONFIG->ChannelConfigs[Channel];
  const DoIP_TesterConnectionType *connection = NULL;
  int i;
  for (i = 0; i < (NULL == connection) && (config->numOfTesterConnections); i++) {
    connection = &config->testerConnections[i];
    if (DOIP_CON_CLOSED != connection->context->state) {
      TargetAddressRef = connection->context->msg.TargetAddressRef;
      if (NULL != TargetAddressRef) {
        if (DOIP_MSG_TX == connection->context->msg.state) {
          ret = E_OK;
          TxPduId = TargetAddressRef->TxPduId;
          connection = &config->testerConnections[i];
          PduInfo.SduDataPtr = config->txBuf;
          PduInfo.SduLength = config->txBufLen;
          if (PduInfo.SduLength >
              (connection->context->msg.TpSduLength - connection->context->msg.index)) {
            PduInfo.SduLength =
              connection->context->msg.TpSduLength - connection->context->msg.index;
          }
          bret = PduR_DoIPCopyTxData(TxPduId, &PduInfo, NULL, &left);
          if (BUFREQ_OK == bret) {
            config->context->txLen = PduInfo.SduLength;
            ret = doipTpSendResponse(connection->SoAdTxPdu, config);
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
              ASLOG(DOIP, ("[%d] send UDS response done\n", Channel));
            } else {
              ASLOG(DOIP, ("[%d] send UDS response on going\n", Channel));
            }
          }
        }
      }
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void DoIP_Init(const DoIP_ConfigType *ConfigPtr) {
  int i;
  uint8_t Channel;
  const DoIP_ChannelConfigType *config;
  for (Channel = 0; Channel < DOIP_CONFIG->numOfChannels; Channel++) {
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    config->context->txLen = 0;
    for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
      memset(config->UdpVehicleAnnouncementConnections[i].context, 0,
             sizeof(DoIP_UdpVehicleAnnouncementConnectionContextType));
    }
    for (i = 0; i < config->numOfTesterConnections; i++) {
      memset(config->testerConnections[i].context, 0, sizeof(DoIP_TesterConnectionContextType));
    }
  }
}

void DoIP_ActivationLineSwitchActive(uint8_t Channel) {
  const DoIP_ChannelConfigType *config;
  int i;
  if (Channel < DOIP_CONFIG->numOfChannels) {
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    if (DOIP_ACTIVATION_LINE_INACTIVE == config->context->ActivationLineState) {
      ASLOG(DOIP, ("[%d] switch to active\n", Channel));
      config->context->ActivationLineState = DOIP_ACTIVATION_LINE_ACTIVE;
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
}

void DoIP_ActivationLineSwitchInactive(uint8_t Channel) {
  const DoIP_ChannelConfigType *config;
  int i;

  if (Channel < DOIP_CONFIG->numOfChannels) {
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    if (DOIP_ACTIVATION_LINE_ACTIVE == config->context->ActivationLineState) {
      ASLOG(DOIP, ("[%d] switch to inactive\n", Channel));
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
      for (i = 0; i < config->numOfTesterConnections; i++) {
        if (DOIP_CON_CLOSED != config->testerConnections[i].context->state) {
          SoAd_CloseSoCon(config->testerConnections[i].SoConId, TRUE);
          config->testerConnections[i].context->state = DOIP_CON_CLOSED;
        }
      }
      config->context->ActivationLineState = DOIP_ACTIVATION_LINE_INACTIVE;
    }
  }
}

void DoIP_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode) {
  Std_ReturnType ret = E_NOT_OK;
  const DoIP_ChannelConfigType *config;
  uint8_t Channel;
  int i;
  for (Channel = 0; Channel < DOIP_CONFIG->numOfChannels; Channel++) {
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    for (i = 0; i < config->numOfTcpConnections; i++) {
      if (SoConId == config->TcpConnections[i].SoConId) {
        ASLOG(DOIP, ("[%d] Tcp %d SoCon Mode %d\n", Channel, i, Mode));
        ret = E_OK;
        break;
      }
    }

    for (i = 0; (i < config->numOfUdpConnections) && (E_NOT_OK == ret); i++) {
      if (SoConId == config->UdpConnections[i].SoConId) {
        ASLOG(DOIP, ("[%d] Udp SoCon %d Mode %d\n", Channel, i, Mode));
        ret = E_OK;
      }
    }

    for (i = 0; i < config->numOfUdpVehicleAnnouncementConnections; i++) {
      if (SoConId == config->UdpVehicleAnnouncementConnections[i].SoConId) {
        if (SOAD_SOCON_ONLINE == Mode) {
          assert(DOIP_ACTIVATION_LINE_ACTIVE == config->context->ActivationLineState);
          config->UdpVehicleAnnouncementConnections[i].context->state = DOIP_CON_OPEN;
          config->UdpVehicleAnnouncementConnections[i].context->AnnouncementTimer =
            config->UdpVehicleAnnouncementConnections[i].InitialVehicleAnnouncementTime;
          config->UdpVehicleAnnouncementConnections[i].context->AnnouncementCounter = 0;
        }
        ASLOG(DOIP, ("[%d] UdpVehicleAnnouncement SoCon %d Mode %d\n", Channel, i, Mode));
        ret = E_OK;
        break;
      }
    }

    for (i = 0; (i < config->numOfTesterConnections) && (E_NOT_OK == ret); i++) {
      if (SoConId == config->testerConnections[i].SoConId) {
        if (SOAD_SOCON_ONLINE == Mode) {
          assert(DOIP_ACTIVATION_LINE_ACTIVE == config->context->ActivationLineState);
          memset(config->testerConnections[i].context, 0, sizeof(DoIP_TesterConnectionContextType));
          config->testerConnections[i].context->InactivityTimer =
            config->testerConnections[i].InitialInactivityTime;
          config->testerConnections[i].context->state = DOIP_CON_OPEN;
        }
        ASLOG(DOIP, ("[%d] Tester SoCon %d Mode %d\n", Channel, i, Mode));
        ret = E_OK;
      }
    }
  }
}

void DoIP_SoAdIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_OK;
  uint8_t nack = DOIP_INVALID_PROTOCOL_NACK;
  PduInfoType PduInfo;
  uint8_t Channel;
  const DoIP_ChannelConfigType *config;
  if (RxPduId < DOIP_CONFIG->numOfRxPduIds) {
    Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    if (DOIP_ACTIVATION_LINE_INACTIVE == config->context->ActivationLineState) {
      /* @SWS_DoIP_00202 */
      ret = DOIP_E_NOT_OK_SILENT;
    }
  } else {
    ASLOG(DOIPE, ("IfRxInd with invalid RxPduId %d\n", RxPduId));
    ret = DOIP_E_NOT_OK_SILENT;
  }

  if (E_OK == ret) {
    /* @SWS_DoIP_00004 */
    if ((PduInfoPtr->SduLength >= DOIP_HEADER_LENGTH) && (PduInfoPtr->SduDataPtr != NULL)) {
      /* @SWS_DoIP_00005, @SWS_DoIP_00006 */
      if ((DOIP_PROTOCOL_VERSION == PduInfoPtr->SduDataPtr[0]) &&
          (PduInfoPtr->SduDataPtr[0] = ((~PduInfoPtr->SduDataPtr[1])) & 0xFF)) {
        ret = doIpIfHandleMessage(RxPduId, PduInfoPtr, &nack);
      } else {
        ret = E_NOT_OK;
      }
    } else {
      ret = E_NOT_OK;
    }
  }

  if (DOIP_E_NOT_OK_SILENT == ret) {
    /* slient as inactive or ... */
  } else {
    if (E_OK != ret) {
      doipFillHeader(config->txBuf, DOIP_GENERAL_HEADER_NEGATIVE_ACK, 1);
      config->txBuf[8] = nack;
      config->context->txLen = 9;
    }

    PduInfo.SduDataPtr = config->txBuf;
    PduInfo.MetaDataPtr = PduInfoPtr->MetaDataPtr;
    PduInfo.SduLength = config->context->txLen;
    (void)SoAd_IfTransmit(RxPduId, &PduInfo);
  }
}

void DoIP_SoAdIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result) {
}

BufReq_ReturnType DoIP_SoAdTpStartOfReception(PduIdType RxPduId, const PduInfoType *info,
                                              PduLengthType TpSduLength,
                                              PduLengthType *bufferSizePtr) {
  BufReq_ReturnType ret = BUFREQ_OK;
  const DoIP_ChannelConfigType *config;
  uint8_t Channel;

  if (RxPduId < DOIP_CONFIG->numOfRxPduIds) {
    Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    if (DOIP_ACTIVATION_LINE_INACTIVE == config->context->ActivationLineState) {
      /* @SWS_DoIP_00202 */
      ret = BUFREQ_E_NOT_OK;
    } else {
      if (DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId] >= config->numOfTesterConnections) {
        /* @SWS_DoIP_00101 */
        ret = BUFREQ_E_NOT_OK;
        ASLOG(DOIPE, ("[%d] RxPduId %d get invalid connection map\n", Channel, RxPduId));
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
  const DoIP_ChannelConfigType *config;
  const DoIP_TesterConnectionType *connection;
  uint8_t Channel;
  uint8_t nack = DOIP_INVALID_PROTOCOL_NACK;

  if (RxPduId < DOIP_CONFIG->numOfRxPduIds) {
    Channel = DOIP_CONFIG->RxPduIdToChannelMap[RxPduId];
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    if (DOIP_ACTIVATION_LINE_INACTIVE == config->context->ActivationLineState) {
      ret = BUFREQ_E_NOT_OK;
      r = DOIP_E_NOT_OK_SILENT;
    } else {
      if (DOIP_CONFIG->RxPduIdToConnectionMap[RxPduId] < config->numOfTesterConnections) {
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
      if (PduInfoPtr->SduLength >= DOIP_HEADER_LENGTH) {
        r = doipTpStartOfReception(RxPduId, PduInfoPtr, bufferSizePtr, &nack);
      } else {
        r = E_NOT_OK;
      }
    } else {
      r = doipTpCopyRxData(RxPduId, PduInfoPtr, bufferSizePtr, &nack);
    }
  }

  if (E_NOT_OK == r) {
    doipFillHeader(config->txBuf, DOIP_GENERAL_HEADER_NEGATIVE_ACK, 1);
    config->txBuf[DOIP_HEADER_LENGTH] = nack;
    config->context->txLen = DOIP_HEADER_LENGTH + 1;
  }

  if (E_OK == r) {
    connection->context->InactivityTimer = connection->GeneralInactivityTime;
    (void)doipTpSendResponse(connection->SoAdTxPdu, config);
  } else if (DOIP_E_NOT_OK_SILENT == r) {
    /* slient */
  } else if (DOIP_E_PENDING == r) {
    /* TODO */
  } else {
    (void)doipTpSendResponse(connection->SoAdTxPdu, config);
    ret = BUFREQ_E_NOT_OK;
  }

  return ret;
}

void DoIP_MainFunction(void) {
  uint8_t Channel;
  const DoIP_ChannelConfigType *config;
  for (Channel = 0; Channel < DOIP_CONFIG->numOfChannels; Channel++) {
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    if (DOIP_ACTIVATION_LINE_ACTIVE == config->context->ActivationLineState) {
      doipHandleInactivityTimer(Channel);
      doipHandleAliveCheckResponseTimer(Channel);
      doipHandleVehicleAnnouncement(Channel);
      doipHandleDiagMsgResponse(Channel);
      doipHandleRoutineActivationMain(Channel);
    }
  }
}

Std_ReturnType DoIP_TpTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Std_ReturnType ret = E_NOT_OK;
  BufReq_ReturnType bret;
  uint8_t Channel;
  int i;
  const DoIP_ChannelConfigType *config;
  const DoIP_TesterConnectionType *connection = NULL;
  PduInfoType PduInfo;
  PduLengthType left;
  uint16_t sa, ta;

  for (Channel = 0; (NULL == connection) && (Channel < DOIP_CONFIG->numOfChannels); Channel++) {
    config = &DOIP_CONFIG->ChannelConfigs[Channel];
    if (DOIP_ACTIVATION_LINE_ACTIVE == config->context->ActivationLineState) {
      for (i = 0; (NULL == connection) && (i < config->numOfTesterConnections); i++) {
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
  }

  if (E_OK == ret) {
    if (connection->context->msg.state != DOIP_MSG_IDLE) {
      ASLOG(DOIPE,
            ("[%d] UDS response when state is %d\n", Channel, connection->context->msg.state));
      ret = E_NOT_OK;
    }
  }

  if (E_OK == ret) {
    ASLOG(DOIP, ("[%d] UDS response, len = %d\n", Channel, PduInfoPtr->SduLength));
    doipFillHeader(config->txBuf, DOIP_DIAGNOSTIC_MESSAGE, PduInfoPtr->SduLength + 4);
    sa = connection->context->msg.TargetAddressRef->TargetAddress;
    ta = connection->context->TesterRef->TesterSA;
    config->txBuf[DOIP_HEADER_LENGTH + 0] = (sa >> 8) & 0xFF;
    config->txBuf[DOIP_HEADER_LENGTH + 1] = sa & 0xFF;
    config->txBuf[DOIP_HEADER_LENGTH + 2] = (ta >> 8) & 0xFF;
    config->txBuf[DOIP_HEADER_LENGTH + 3] = ta & 0xFF;
    PduInfo.SduDataPtr = &config->txBuf[DOIP_HEADER_LENGTH + 4];
    PduInfo.SduLength = PduInfoPtr->SduLength;
    if (PduInfo.SduLength > (config->txBufLen - DOIP_HEADER_LENGTH - 4)) {
      PduInfo.SduLength = config->txBufLen - DOIP_HEADER_LENGTH - 4;
    }
    bret = PduR_DoIPCopyTxData(TxPduId, &PduInfo, NULL, &left);
    if (BUFREQ_OK == bret) {
      config->context->txLen = DOIP_HEADER_LENGTH + PduInfo.SduLength + 4;
      ret = doipTpSendResponse(connection->SoAdTxPdu, config);
      if (E_OK != ret) {
        PduR_DoIPTxConfirmation(TxPduId, E_NOT_OK);
      }
    } else {
      ret = E_NOT_OK;
    }

    if (E_OK == ret) {
      if (PduInfo.SduLength >= PduInfoPtr->SduLength) {
        PduR_DoIPTxConfirmation(TxPduId, E_OK);
        ASLOG(DOIP, ("[%d] send UDS response done\n", Channel));
      } else {
        connection->context->msg.state = DOIP_MSG_TX;
        connection->context->msg.TpSduLength = PduInfoPtr->SduLength;
        connection->context->msg.index = PduInfo.SduLength;
        ASLOG(DOIP, ("[%d] send UDS response on going\n", Channel));
      }
    }
  }

  return ret;
}