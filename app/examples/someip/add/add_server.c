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
#include "MathSkeleton.h"
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
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
boolean Sd_ServerServiceMath_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                                  uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                                  const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                                  const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {
  return TRUE;
}

void SomeIp_Math_OnConnect(uint16_t conId, boolean isConnected) {
}

Std_ReturnType Math_add(const Vector_Type *A, const Vector_Type *B, Result_Type *returnResult) {
  Std_ReturnType ret = SOMEIP_E_PENDING;
  int32_t length, i;

  if (A->numberLen != B->numberLen) {
    ASLOG(MATH, ("add: wrong inputs %u = %u\n", A->numberLen, B->numberLen));
    ret = E_NOT_OK;
  } else {
    length = A->numberLen;
    returnResult->ercd = E_OK;
    returnResult->numberLen = length;
    returnResult->summary = 0;
    for (i = 0; i < length; i++) {
      returnResult->number[i] = A->number[i] + B->number[i];
      returnResult->summary += (uint32_t)A->number[i] + B->number[i];
    }
    ASLOG(MATH, ("add: summary = %u, len = %u, A=[%u, %u, %u], B=[%u, %u, %u], S=[%u, %u, %u]\n",
                 returnResult->summary, length, A->number[0], A->number[1], A->number[2],
                 B->number[0], B->number[1], B->number[2], returnResult->number[0],
                 returnResult->number[1], returnResult->number[2]));
  }

  return ret;
}

Std_ReturnType Math_add_Async(Result_Type *returnResult) {
  return E_OK;
}

int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  Math_OfferService();

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
