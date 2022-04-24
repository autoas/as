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
__attribute__((weak)) Std_ReturnType server0_method1_request(uint8_t *data, uint32_t length) {
  return E_NOT_OK;
}
__attribute__((weak)) Std_ReturnType client0_method2_request(uint8_t *data, uint32_t length) {
  return E_NOT_OK;
}
__attribute__((weak)) Std_ReturnType server0_event_group1_event0_notify(uint8_t *data,
                                                                        uint32_t length) {
  return E_NOT_OK;
}
__attribute__((weak)) Std_ReturnType client0_event_group2_event0_notify(uint8_t *data,
                                                                        uint32_t length) {
  return E_NOT_OK;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void SomeIp_MainAppTask(void) {
  static uint8_t counter = 0;
  static uint8_t data[1000];
  uint16_t i;
  counter++;
  for (i = 0; i < sizeof(data); i++) {
    data[i] = (uint8_t)(i + counter);
  }
  server0_event_group1_event0_notify(data, sizeof(data));
  client0_event_group2_event0_notify(data, sizeof(data));
  server0_method1_request(data, sizeof(data));
  client0_method2_request(data, sizeof(data));
}
#endif