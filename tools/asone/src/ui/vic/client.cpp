/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "VICProxy.h"
#include "plugin.h"
#include "Std_Timer.h"
#include "MessageQueue.hpp"
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::shared_ptr<MessageQueue<std::shared_ptr<Display_Type>>> s_MsgQ = nullptr;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void VIC_SubscribeDisplayStatusAck(boolean isSubscribe) {
}

void SomeIp_VIC_OnAvailability(boolean isAvailable) {
}

void VIC_DisplayStatus(const Display_Type* sample) {
  std::shared_ptr<Display_Type> msg = std::make_shared<Display_Type>();
  *msg = *sample;
  s_MsgQ->put(msg);
}

void CS_VIC_init(void) {
  s_MsgQ = MessageQueue<std::shared_ptr<Display_Type>>::add("VIC"); 
  VIC_FindService();
  VIC_SubscribeDisplayStatus();
}

void CS_VIC_main(void) {
}

void CS_VIC_deinit(void) {
}

REGISTER_PLUGIN(CS_VIC);
