/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2026 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Service0Proxy.h"
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
static Std_TimerType timer1S;
static Message_Type message0 = {
  0,
};
static boolean s_isAvailable = FALSE;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void SomeIp_Service0_OnAvailability(boolean isAvailable) {
  ASLOG(SERVICE0, ("%s\n", isAvailable ? "online" : "offline"));
  s_isAvailable = isAvailable;
}

void Service0_SubscribeEvent0Ack(boolean isSubscribe) {
  ASLOG(SERVICE0, ("event0 %s\n", isSubscribe ? "online" : "offline"));
}

void Service0_Method0_Return(Std_ReturnType ercd, const Message_Type *Message) {
  if (NULL != Message) {
    ASLOG(SERVICE0,
          ("Method0 %s(%u): %u [%02x %02x %02x %02x]\n", ercd == E_OK ? "success" : "failed", ercd,
           Message->bufferLen, Message->buffer[0], Message->buffer[1], Message->buffer[2],
           Message->buffer[3]));
  } else {
    ASLOG(SERVICE0, ("Method0 %s(%u)\n", ercd == E_OK ? "success" : "failed", ercd));
  }
}

void Service0_Method2_Return(Std_ReturnType ercd, const Message_Type *Message) {
  if (NULL != Message) {
    ASLOG(SERVICE0,
          ("Method2 %s(%u): %u [%02x %02x %02x %02x]\n", ercd == E_OK ? "success" : "failed", ercd,
           Message->bufferLen, Message->buffer[0], Message->buffer[1], Message->buffer[2],
           Message->buffer[3]));
  } else {
    ASLOG(SERVICE0, ("Method2 %s(%u)\n", ercd == E_OK ? "success" : "failed", ercd));
  }
}

void Service0_Event0(const Message_Type *sample) {
  ASLOG(SERVICE0, ("Event0 message: %u [%02x %02x %02x %02x]\n", sample->bufferLen,
                   sample->buffer[0], sample->buffer[1], sample->buffer[2], sample->buffer[3]));
}

void CS_Service0_init(void) {
  Std_TimerSet(&timer1S, 1000000);
  Service0_FindService();
  Service0_SubscribeEvent0();
}

void CS_Service0_main(void) {
  uint16_t i;
  if (Std_IsTimerTimeout(&timer1S)) {
    Std_TimerSet(&timer1S, 1000000);
    if (TRUE == s_isAvailable) {
      for (i = 0; i < message0.bufferLen; i++) {
        message0.buffer[i] = message0.bufferLen + i;
      }
      message0.bufferLen++;
      Service0_Method0(&message0);
      Service0_Method2(&message0);
    }
  }
}

void CS_Service0_deinit(void) {
}

REGISTER_PLUGIN(CS_Service0);
