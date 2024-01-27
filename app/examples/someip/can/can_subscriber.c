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
#include "CS_CANBus.h"
#include "SomeIp_Cfg.h"
#include "SomeIpXf_Cfg.h"

#include "Std_Timer.h"

#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_CANBUS 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static boolean lIsAvailable = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
boolean Sd_ServerService0_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                               uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                               const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                               const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {
  return TRUE;
}

void SomeIp_CANBus_OnAvailability(boolean isAvailable) {
  ASLOG(CANBUS, ("%s\n", isAvailable ? "online" : "offline"));
  lIsAvailable = isAvailable;
}

Std_ReturnType SomeIp_CANBus_canframe_canframe_OnNotification(uint32_t requestId,
                                                              SomeIp_MessageType *evt) {
  CanFrame_Type canFrame;
  int32_t length;
  length = SomeIpXf_DecodeStruct(evt->data, evt->length, &canFrame, &SomeIpXf_StructCanFrameDef);
  if (length > 0) {
    ASLOG(CANBUS, ("receive: session=%d, canid = 0x%x, dlc = %d,"
                   " data = [%02X, %02X, %02X, %02X,%02X, %02X, %02X, %02X]\n",
                   requestId & 0xFFFF, canFrame.canid, canFrame.dlc, canFrame.data[0],
                   canFrame.data[1], canFrame.data[2], canFrame.data[3], canFrame.data[4],
                   canFrame.data[5], canFrame.data[6], canFrame.data[7]));
  }
  return E_OK;
}

int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  Sd_ClientServiceSetState(SD_CLIENT_SERVICE_HANDLE_ID_CANBUS, SD_SERVER_SERVICE_AVAILABLE);
  Sd_ConsumedEventGroupSetState(SD_CONSUMED_EVENT_GROUP_CANBUS_CANFRAME,
                                SD_CONSUMED_EVENTGROUP_REQUESTED);

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
