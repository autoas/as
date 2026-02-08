/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2026 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Service0Skeleton.h"
#include "Sd_Cfg.h"
#include "Sd.h"
#include "plugin.h"
#include "Std_Timer.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SERVICE0 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static Std_TimerType timer1S;
static boolean s_isConnected = FALSE;
static Message_Type message0 = {
  0,
};
/* ================================ [ FUNCTIONS ] ============================================== */
boolean
Sd_ServerServiceService0_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                              uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                              const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                              const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {

  ASLOG(SERVICE0,
        ("CRMC serviceID=0x%04x instanceID=0x%04x majorVersion=0x%02x minorVersion=0x%08x\n",
         serviceID, instanceID, majorVersion, minorVersion));

  return TRUE;
}

void Service0_OnEvent0Subscribed(boolean isSubscribe, TcpIp_SockAddrType *RemoteAddr) {

}

void SomeIp_Service0_OnConnect(uint16_t conId, boolean isConnected) {
  ASLOG(SERVICE0,
        ("OnConnect conId=0x%04x isConnected=%s\n", conId, isConnected ? "TRUE" : "FALSE"));
  s_isConnected = isConnected;
}

Std_ReturnType Service0_Method0(const Message_Type *message, Message_Type *returnMessage) {
  ASLOG(SERVICE0, ("Method0 message: %u [%02x %02x %02x %02x]\n", message->bufferLen,
                   message->buffer[0], message->buffer[1], message->buffer[2], message->buffer[3]));
  *returnMessage = *message;
  return SOMEIP_E_PENDING;
}

Std_ReturnType Service0_Method0_Async(Message_Type *returnMessage) {
  return E_OK;
}

Std_ReturnType Service0_Method2(const Message_Type *message, Message_Type *returnMessage) {
  ASLOG(SERVICE0, ("Method2 message: %u [%02x %02x %02x %02x]\n", message->bufferLen,
                   message->buffer[0], message->buffer[1], message->buffer[2], message->buffer[3]));
  *returnMessage = *message;
  return SOMEIP_E_PENDING;
}

Std_ReturnType Service0_Method2_Async(Message_Type *returnMessage) {
  return E_OK;
}

void SS_Service0_init(void) {
  Std_TimerSet(&timer1S, 1000000);
  Service0_OfferService();
}

void SS_Service0_main(void) {
  uint16_t i;
  if (Std_IsTimerTimeout(&timer1S)) {
    Std_TimerSet(&timer1S, 1000000);
    if (TRUE == s_isConnected) {
      for (i = 0; i < message0.bufferLen; i++) {
        message0.buffer[i] = message0.bufferLen + i;
      }
      message0.bufferLen++;
      Service0_SendEvent0(&message0);
    }
  }
}

void SS_Service0_deinit(void) {
}

REGISTER_PLUGIN(SS_Service0);
