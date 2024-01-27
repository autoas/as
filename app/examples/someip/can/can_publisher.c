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
#include "SS_CANBus.h"
#include "SomeIp_Cfg.h"
#include "SomeIpXf_Cfg.h"

#include "Std_Timer.h"

#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANBUS 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer1s;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
boolean Sd_ServerService0_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                               uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                               const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                               const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {
  return TRUE;
}

void SomeIp_CANBus_OnConnect(uint16_t conId, boolean isConnected) {
}

void SomeIp_CANBus_canframe_OnSubscribe(boolean isSubscribe, TcpIp_SockAddrType *RemoteAddr) {
  ASLOG(CANBUS, ("canframe %ssubscribed by %d.%d.%d.%d:%d\n", isSubscribe ? "" : "stop ",
                 RemoteAddr->addr[0], RemoteAddr->addr[1], RemoteAddr->addr[2], RemoteAddr->addr[3],
                 RemoteAddr->port));
}

void CANBus_canframe_canframe_notify(void) {
  Std_ReturnType ercd = E_NOT_OK;
  static uint16_t sessionId = 0;
  static CanFrame_Type canFrame = {0, 0, 8, {0}};
  static uint8_t data[256];
  int32_t length;
  uint32_t requestId = ((uint32_t)SOMEIP_TX_EVT_CANBUS_CANFRAME_CANFRAME << 16) + (++sessionId);

  canFrame.canid = sessionId;
  for (length = 0; length < 8; length++) {
    canFrame.data[length] = (requestId + length) & 0xFF;
  }

  length = SomeIpXf_EncodeStruct(data, sizeof(data), &canFrame, &SomeIpXf_StructCanFrameDef);
  if (length > 0) {
    ercd = SomeIp_Notification(requestId, data, length);
    if (E_OK == ercd) {
      ASLOG(CANBUS, ("publish: session=%d, canid = 0x%x, dlc = %d,"
                     " data = [%02X, %02X, %02X, %02X,%02X, %02X, %02X, %02X]\n",
                     sessionId, canFrame.canid, canFrame.dlc, canFrame.data[0], canFrame.data[1],
                     canFrame.data[2], canFrame.data[3], canFrame.data[4], canFrame.data[5],
                     canFrame.data[6], canFrame.data[7]));
    }
  }
}

int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  Sd_ServerServiceSetState(SD_SERVER_SERVICE_HANDLE_ID_CANBUS, SD_SERVER_SERVICE_AVAILABLE);

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
      CANBus_canframe_canframe_notify();
    }
  }

  return 0;
}
