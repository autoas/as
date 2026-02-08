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
#include "MathProxy.h"
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
static Vector_Type A = {0};
static Vector_Type B = {0};
static Result_Type result = {0};
static boolean lIsBusy = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

void SomeIp_Math_OnAvailability(boolean isAvailable) {
  ASLOG(MATH, ("%s\n", isAvailable ? "online" : "offline"));
  lIsAvailable = isAvailable;
}

void Math_add_request(void) {
  Std_ReturnType ret;
  static uint16_t sessionId = 0;
  int32_t length, i;
  if (lIsAvailable && (FALSE == lIsBusy)) {
    length = sessionId % sizeof(A.number);
    if (length < 32) {
      length = 32;
    }
    result.summary = 0;
    result.numberLen = length;
    for (i = 0; i < length; i++) {
      A.number[i] = (i + sessionId) & 0xFF;
      B.number[i] = (i * i + sessionId) & 0xFF;
      result.number[i] = A.number[i] + B.number[i];
      result.summary += (uint32_t)A.number[i] + B.number[i];
    }
    A.numberLen = length;
    B.numberLen = length;

    ret = Math_add(&A, &B);
    if (ret == E_OK) {
      lIsBusy = TRUE;
      sessionId++;
    }
  }
}

void Math_add_Return(Std_ReturnType ercd, const Result_Type *Result) {
  lIsBusy = FALSE;
  if ((ercd == E_OK) && (NULL != Result) && (Result->summary == result.summary) &&
      (Result->numberLen == result.numberLen) &&
      (0 == memcmp(Result->number, result.number, Result->numberLen))) {
    ASLOG(MATH, ("success: summary = %u, len = %u, S=[%u, %u, %u]\n", Result->summary,
                 Result->numberLen, Result->number[0], Result->number[1], Result->number[2]));
  } else {
    ASLOG(MATH, ("failed(%u): summary = %u vs %u, len = %u vs %u, S=[%u, %u, %u] vs [%u, %u, %u]\n",
                 ercd, Result->summary, result.summary, Result->numberLen, result.numberLen,
                 Result->number[0], Result->number[1], Result->number[2], result.number[0],
                 result.number[1], result.number[2]));
  }
}

int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  Math_FindService();

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
      // Std_TimerStart(&timer1s);
      Math_add_request();
    }
  }

  return 0;
}
