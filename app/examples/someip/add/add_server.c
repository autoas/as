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
#include "SS_Math.h"
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

static uint8_t Math_addTpRxBuf[8192];
static Vectors_Type lVectors;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
boolean Sd_ServerService0_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                               uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                               const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                               const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {
  return TRUE;
}

void SomeIp_Math_OnConnect(uint16_t conId, boolean isConnected) {
}

Std_ReturnType SomeIp_Math_add_OnRequest(uint32_t requestId, SomeIp_MessageType *req,
                                         SomeIp_MessageType *res) {
  Std_ReturnType ret = E_OK;
  Result_Type lResult = {E_OK, 0};
  int32_t length, i;

  length = SomeIpXf_DecodeStruct(req->data, req->length, &lVectors, &SomeIpXf_StructVectorsDef);
  if (length > 0) {
    for (i = 0; i < lVectors.ALen; i++) {
      lResult.summary += lVectors.A[i] * lVectors.B[i];
    }
  } else {
    ASLOG(MATH, ("Malform request message\n"));
    lResult.ercd = E_NOT_OK;
  }

  length = SomeIpXf_EncodeStruct(res->data, res->length, &lResult, &SomeIpXf_StructResultDef);
  if (length > 0) {
    res->length = length;
    ASLOG(MATH, ("add request session=%d: ercd = %d summary = %u\n", requestId & 0xFFFF,
                 lResult.ercd, lResult.summary));
  } else {
    ret = E_NOT_OK;
  }

  return ret;
}
Std_ReturnType SomeIp_Math_add_OnFireForgot(uint32_t requestId, SomeIp_MessageType *req) {
  ASLOG(MATH, ("add OnFireForgot %X: len=%d, data=[%02X %02X %02X %02X ...]\n", requestId,
               req->length, req->data[0], req->data[1], req->data[2], req->data[3]));
  return E_OK;
}

Std_ReturnType SomeIp_Math_add_OnAsyncRequest(uint32_t requestId, SomeIp_MessageType *res) {
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
  return E_NOT_OK;
}

int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  Sd_ServerServiceSetState(SD_SERVER_SERVICE_HANDLE_ID_MATH, SD_SERVER_SERVICE_AVAILABLE);

  Std_TimerStart(&timer10ms);

  for (;;) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      Std_TimerStart(&timer10ms);
      TcpIp_MainFunction();
      SoAd_MainFunction();
      Sd_MainFunction();
      SomeIp_MainFunction();
    }
  }

  return 0;
}
