/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef USE_SOMEIP
#include "SomeIp.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SOMEIP 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType SomeIp_RequestFnc_server0_method1(const uint8_t *reqData, uint32_t reqLen,
                                                 uint8_t *resData, uint32_t *resLen) {
  uint32_t i;
  for (i = 0; i < reqLen; i++) {
    resData[reqLen - 1 - i] = reqData[i];
  }
  *resLen = reqLen;
  ASLOG(SOMEIP, ("server0 method1 request: len=%d, data=[%02X %02X %02X %02X ...]\n", reqLen,
                 reqData[0], reqData[1], reqData[2], reqData[3]));
  return E_OK;
}

Std_ReturnType SomeIp_ResponseFnc_server0_method1(const uint8_t *resData, uint32_t resLen) {
  ASLOG(SOMEIP, ("client0 method1 response: len=%d, data=[%02X %02X %02X %02X ...]\n", resLen,
                 resData[0], resData[1], resData[2], resData[3]));
  return E_OK;
}

Std_ReturnType SomeIp_RequestFnc_client0_method2(const uint8_t *reqData, uint32_t reqLen,
                                                 uint8_t *resData, uint32_t *resLen) {
  uint32_t i;
  for (i = 0; i < reqLen; i++) {
    resData[reqLen - 1 - i] = reqData[i];
  }
  *resLen = reqLen;
  ASLOG(SOMEIP, ("client0 method2 request: len=%d, data=[%02X %02X %02X %02X ...]\n", reqLen,
                 reqData[0], reqData[1], reqData[2], reqData[3]));
  return E_OK;
}

Std_ReturnType SomeIp_ResponseFnc_client0_method2(const uint8_t *resData, uint32_t resLen) {
  ASLOG(SOMEIP, ("client0 method2 response: len=%d, data=[%02X %02X %02X %02X ...]\n", resLen,
                 resData[0], resData[1], resData[2], resData[3]));
  return E_OK;
}

Std_ReturnType SomeIp_NotifyFnc_server0_event_group1_event0(const uint8_t *evtData,
                                                            uint32_t evtLen) {
  ASLOG(SOMEIP, ("server0 event0: len=%d, data=[%02X %02X %02X %02X ...]\n", evtLen, evtData[0],
                 evtData[1], evtData[2], evtData[3]));
  return E_OK;
}

Std_ReturnType SomeIp_NotifyFnc_client0_event_group2_event0(const uint8_t *evtData,
                                                            uint32_t evtLen) {
  ASLOG(SOMEIP, ("client0 event0: len=%d, data=[%02X %02X %02X %02X ...]\n", evtLen, evtData[0],
                 evtData[1], evtData[2], evtData[3]));
  return E_OK;
}

void SomeIp_MainAppTask(void) {
  static uint8_t counter = 0;
  counter++;
  SomeIp_Notification(0, &counter, 1);
  SomeIp_Request(0, &counter, 1);
}
#endif