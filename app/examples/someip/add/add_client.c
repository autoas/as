/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <string.h>
#include "TcpIp.h"
#include "SoAd.h"
#include "Sd.h"
#include "SomeIp.h"
#include "SomeIpXf.h"

#include "Sd_Cfg.h"
#include "CS_Math.h"
#include "SomeIp_Cfg.h"
#include "SomeIpXf_Cfg.h"

#include "Std_Timer.h"

#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_MATH 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer1s;
static boolean lIsAvailable = FALSE;
static uint8_t Math_addTpRxBuf[8192];
static uint8_t Math_addTpTxBuf[8192];
static Vectors_Type lVectors;
static uint32_t lSummary;
static boolean lIsBusy = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
boolean Sd_ServerService0_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                               uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                               const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                               const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {
  return TRUE;
}

void SomeIp_Math_OnAvailability(boolean isAvailable) {
  ASLOG(MATH, ("%s\n", isAvailable ? "online" : "offline"));
  lIsAvailable = isAvailable;
}

void Math_add_request(void) {
  static uint16_t sessionId = 0;
  int32_t length, i;
  uint32_t requestId;
  uint8_t *data = Math_addTpTxBuf;
  if (lIsAvailable && (FALSE == lIsBusy)) {
    requestId = ((uint32_t)SOMEIP_TX_METHOD_MATH_ADD << 16) | (++sessionId);
    length = sessionId % sizeof(lVectors.A);
    if (length < 32) {
      length = 32;
    }
    lSummary = 0;
    for (i = 0; i < length; i++) {
      lVectors.A[i] = (i + sessionId) & 0xFF;
      lVectors.B[i] = (i * i + sessionId) & 0xFF;
      lSummary += lVectors.A[i] * lVectors.B[i];
    }
    lVectors.ALen = length;
    lVectors.BLen = length;

    length =
      SomeIpXf_EncodeStruct(data, sizeof(Math_addTpTxBuf), &lVectors, &SomeIpXf_StructVectorsDef);
    if (length > 0) {
      lIsBusy = TRUE;
      (void)SomeIp_Request(requestId, data, length);
    }
  }
}

Std_ReturnType SomeIp_Math_add_OnResponse(uint32_t requestId, SomeIp_MessageType *res) {
  Result_Type lResult = {E_NOT_OK, 0};
  int32_t length;
  lIsBusy = FALSE;
  length = SomeIpXf_DecodeStruct(res->data, res->length, &lResult, &SomeIpXf_StructResultDef);
  if (length > 0) {
    ASLOG(MATH,
          ("add response session=%d: ercd = %d summary %u %s %u\n", requestId & 0xFFFF,
           lResult.ercd, lResult.summary, (lResult.summary == lSummary) ? "==" : "!=", lSummary));
  } else {
    ASLOG(MATH, ("Malform response message\n"));
  }
  return E_OK;
}

Std_ReturnType SomeIp_Math_add_OnError(uint32_t requestId, Std_ReturnType ercd) {
  ASLOG(MATH, ("add OnError %X: %d\n", requestId, ercd));
  lIsBusy = FALSE;
  return E_OK;
}

Std_ReturnType SomeIp_Math_add_OnTpCopyRxData(uint32_t requestId, SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length)) < sizeof(Math_addTpRxBuf)) {
    memcpy(&Math_addTpRxBuf[msg->offset], msg->data, msg->length);
    if (FALSE == msg->moreSegmentsFlag) {
      msg->data = Math_addTpRxBuf;
    }
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

Std_ReturnType SomeIp_Math_add_OnTpCopyTxData(uint32_t requestId, SomeIp_TpMessageType *msg) {
  Std_ReturnType ret = E_OK;
  if ((NULL != msg) && ((msg->offset + msg->length) < sizeof(Math_addTpTxBuf))) {
    memcpy(msg->data, &Math_addTpTxBuf[msg->offset], msg->length);
  } else {
    ret = E_NOT_OK;
  }
  return ret;
}

int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  Sd_ClientServiceSetState(SD_CLIENT_SERVICE_HANDLE_ID_MATH, SD_SERVER_SERVICE_AVAILABLE);

  Std_TimerStart(&timer10ms);
  Std_TimerStart(&timer1s);

  for (;;) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      Std_TimerStart(&timer10ms);
      TcpIp_MainFunction();
      SoAd_MainFunction();
      Sd_MainFunction();
      SomeIp_MainFunction();
    }

    if (Std_GetTimerElapsedTime(&timer1s) >= 1000000) {
      Std_TimerStart(&timer1s);
      Math_add_request();
    }
  }

  return 0;
}
