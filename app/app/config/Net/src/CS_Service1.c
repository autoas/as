/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2026 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Service1Proxy.h"
#include "Sd_Cfg.h"
#include "Sd.h"
#include "plugin.h"
#include "Std_Timer.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SERVICE1 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer1S;
static Message_Type message0 = {
  0,
};
static boolean s_isAvailable = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void SomeIp_Service1_OnAvailability(boolean isAvailable) {
  ASLOG(SERVICE1, ("%s\n", isAvailable ? "online" : "offline"));
  s_isAvailable = isAvailable;
}

void Service1_SubscribeEvent1Ack(boolean isSubscribe) {
  ASLOG(SERVICE1, ("event1 %s\n", isSubscribe ? "online" : "offline"));
}

void Service1_Method1_Return(Std_ReturnType ercd, const Message_Type *message) {
  if (NULL != message) {
    ASLOG(SERVICE1,
          ("Method1 %s(%u): %u [%02x %02x %02x %02x]\n", ercd == E_OK ? "success" : "failed", ercd,
           message->bufferLen, message->buffer[0], message->buffer[1], message->buffer[2],
           message->buffer[3]));
  } else {
    ASLOG(SERVICE1, ("Method1 %s(%u)\n", ercd == E_OK ? "success" : "failed", ercd));
  }
}

void Service1_Event1(const Message_Type *sample) {
  ASLOG(SERVICE1, ("Event1 message: %u [%02x %02x %02x %02x]\n", sample->bufferLen,
                   sample->buffer[0], sample->buffer[1], sample->buffer[2], sample->buffer[3]));
}

void CS_Service1_init(void) {
  Std_TimerSet(&timer1S, 1000000);
  Service1_FindService();
  Service1_SubscribeEvent1();
}

void CS_Service1_main(void) {
  uint16_t i;
  if (Std_IsTimerTimeout(&timer1S)) {
    Std_TimerSet(&timer1S, 1000000);
    if (TRUE == s_isAvailable) {
      for (i = 0; i < message0.bufferLen; i++) {
        message0.buffer[i] = message0.bufferLen + i;
      }
      message0.bufferLen++;
      Service1_Method1(&message0);
    }
  }
}

void CS_Service1_deinit(void) {
}

REGISTER_PLUGIN(CS_Service1);
