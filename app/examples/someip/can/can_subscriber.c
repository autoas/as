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
#include "CANBusProxy.h"
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

void CANBus_SubscribeCanFrameAck(boolean isSubscribe) {
}

void CANBus_CanFrame(const CanFrame_Type *sample) {
  ASLOG(
    CANBUS,
    ("receive: canid = 0x%x, dlc = %d, data = [%02X, %02X, %02X, %02X,%02X, %02X, %02X, %02X]\n",
     sample->canid, sample->dlc, sample->data[0], sample->data[1], sample->data[2], sample->data[3],
     sample->data[4], sample->data[5], sample->data[6], sample->data[7]));
}

int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  CANBus_FindService();
  CANBus_SubscribeCanFrame();

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
