/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "usomeip/usomeip.hpp"
#include "usomeip/client.hpp"
extern "C" {
#include "CS_VIC.h"
#include "SomeIp_Cfg.h"
#include "SomeIpXf_Cfg.h"
#include "Sd_Cfg.h"
}
#include "plugin.h"
#include "Std_Timer.h"
#include "MessageQueue.hpp"
using namespace as;
using namespace as::usomeip;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
class CS_VIC;
/* ================================ [ DATAS     ] ============================================== */
static std::shared_ptr<CS_VIC> CS_Instance = nullptr;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
class CS_VIC : public client::Client {
public:
  CS_VIC() {
  }

  ~CS_VIC() {
  }

  void start() {
    m_MsgQ = MessageQueue<std::shared_ptr<Message>>::add("VIC");
    identity(SOMEIP_CSID_VIC);
    m_BufferPool.create("CS_VIC", 5, 1024 * 1024);
    require(SD_CLIENT_SERVICE_HANDLE_ID_VIC);
    subscribe(SD_CONSUMED_EVENT_GROUP_VIC_CLUSTER);
    listen(SOMEIP_RX_EVT_VIC_CLUSTER_STATUS, &m_BufferPool);
  }

  void stop() {
  }

  void onResponse(std::shared_ptr<Message> msg) {
    usLOG(INFO, "VIC: on response: %s\n", msg->str().c_str());
  }

  void onNotification(std::shared_ptr<Message> msg) {
    usLOG(DEBUG, "VIC: on notification: %s\n", msg->str().c_str());
    m_MsgQ->put(msg);
  }

  void onError(std::shared_ptr<Message> msg) {
    usLOG(INFO, "VIC: on error: %s\n", msg->str().c_str());
  }

  void onAvailability(bool isAvailable) {
    usLOG(INFO, "VIC: %s\n", isAvailable ? "online" : "offline");
  }

  void run() {
  }

private:
  BufferPool m_BufferPool;
  std::shared_ptr<MessageQueue<std::shared_ptr<Message>>> m_MsgQ = nullptr;
};

void CS_VIC_init(void) {
  CS_Instance = std::make_shared<CS_VIC>();
  CS_Instance->start();
}

void CS_VIC_main(void) {
  CS_Instance->run();
}

void CS_VIC_deinit(void) {
  CS_Instance->stop();
}

REGISTER_PLUGIN(CS_VIC);
