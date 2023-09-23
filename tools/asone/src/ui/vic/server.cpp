/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "usomeip/usomeip.hpp"
#include "usomeip/server.hpp"
extern "C" {
#include "SS_VIC.h"
#include "SomeIp_Cfg.h"
#include "SomeIpXf_Cfg.h"
#include "Sd_Cfg.h"
#include "Dio.h"
}
#include "plugin.h"
#include "Std_Timer.h"

#ifdef USE_SOMEIPXF
#include "display.msg.pb.h"
#endif
using namespace as;
using namespace as::usomeip;
/* ================================ [ MACROS    ] ============================================== */
#define RTE_STMO_PORT_WRITE(name, id)                                                              \
  extern "C" Std_ReturnType Rte_Write_Gauge_Stmo_##name(uint16_t data) {                           \
    if (lGaugeStatus[id].degree != data) {                                                         \
      lUpdated = true;                                                                             \
      lGaugeStatus[id].degree = data;                                                              \
    }                                                                                              \
    return E_OK;                                                                                   \
  }
/* ================================ [ TYPES     ] ============================================== */
struct GaugeStatus {
  std::string name;
  uint16_t degree;
  uint16_t prev;
};

struct Telltale {
  std::string name;
  Dio_ChannelType ChannelId;
  Dio_LevelType level;
};
/* ================================ [ DECLARES  ] ============================================== */
class SS_VIC;
/* ================================ [ DATAS     ] ============================================== */
static std::shared_ptr<SS_VIC> SS_Instance = nullptr;
static GaugeStatus lGaugeStatus[] = {
  {"speed", 0},
  {"tacho", 0},
  {"temperature", 0},
  {"fuel", 0},
};

static Telltale lTelltales[] = {
  {"TPMS", 0, STD_LOW},     {"LowOil", 1, STD_LOW},         {"PosLamp", 2, STD_LOW},
  {"TurnLeft", 3, STD_LOW}, {"TurnRight", 4, STD_LOW},      {"AutoCruise", 5, STD_LOW},
  {"HighBeam", 6, STD_LOW}, {"SeatbeltDriver", 7, STD_LOW}, {"SeatbeltPassenger", 8, STD_LOW},
  {"Airbag", 9, STD_LOW},
};

static bool lUpdated = true;
/* ================================ [ LOCALS    ] ============================================== */
RTE_STMO_PORT_WRITE(VehicleSpeed, 0);
RTE_STMO_PORT_WRITE(TachoSpeed, 1);
RTE_STMO_PORT_WRITE(Temperature, 2);
RTE_STMO_PORT_WRITE(Fuel, 3);
/* ================================ [ FUNCTIONS ] ============================================== */

class SS_VIC : public server::Server {
public:
  SS_VIC() {
  }
  ~SS_VIC() {
  }
  void start() {
    identity(SOMEIP_SSID_VIC);
    offer(SD_SERVER_SERVICE_HANDLE_ID_VIC);
    m_BufferPool.create("SS_VIC", 5, 1024 * 1024);
    provide(SD_EVENT_HANDLER_VIC_CLUSTER);
    Std_TimerStart(&m_Timer);
  }

  void stop() {
  }

  void onRequest(std::shared_ptr<Message> msg) {
    usLOG(INFO, "VIC: on request: %s\n", msg->str().c_str());
    msg->reply(E_OK, msg->payload);
  }

  void onFireForgot(std::shared_ptr<Message> msg) {
    usLOG(INFO, "VIC: on fire forgot: %s\n", msg->str().c_str());
  }

  void onError(std::shared_ptr<Message> msg) {
  }

  void onSubscribe(uint16_t eventGroupId, bool isSubscribe) {
    usLOG(INFO, "VIC: event group %d %s\n", eventGroupId,
          isSubscribe ? "subscribed" : "unsubscribed");
    m_Subscribed = isSubscribe;
  }

  void run() {
    if (Std_GetTimerElapsedTime(&m_Timer) > 10000) {
#ifdef USE_SOMEIPXF
      Display_Type display;
      display.gaugesLen = ARRAY_SIZE(lGaugeStatus);
      for (size_t i = 0; i < ARRAY_SIZE(lGaugeStatus); i++) {
        strcpy((char *)display.gauges[i].name, lGaugeStatus[i].name.c_str());
        display.gauges[i].nameLen = lGaugeStatus[i].name.size() + 1;
        display.gauges[i].degree = lGaugeStatus[i].degree;
        if (lGaugeStatus[i].prev != lGaugeStatus[i].degree) {
          lGaugeStatus[i].prev = lGaugeStatus[i].degree;
          lUpdated = true;
        }
      }
      display.telltalesLen = ARRAY_SIZE(lTelltales);
      for (size_t i = 0; i < ARRAY_SIZE(lTelltales); i++) {
        strcpy((char *)display.telltales[i].name, lTelltales[i].name.c_str());
        display.telltales[i].nameLen = lTelltales[i].name.size() + 1;
        Dio_LevelType level = Dio_ReadChannel(lTelltales[i].ChannelId);
        if (STD_HIGH == level) {
          display.telltales[i].on = true;
        } else {
          display.telltales[i].on = false;
        }
        if (lTelltales[i].level != level) {
          lTelltales[i].level = level;
          lUpdated = true;
        }
      }
      if (lUpdated) {
        notify(display);
        lUpdated = false;
      }
#else
      vic::display display;
      for (size_t i = 0; i < ARRAY_SIZE(lGaugeStatus); i++) {
        auto gauge = display.add_gauges();
        gauge->set_name(lGaugeStatus[i].name);
        gauge->set_degree(lGaugeStatus[i].degree);
        if (lGaugeStatus[i].prev != lGaugeStatus[i].degree) {
          lGaugeStatus[i].prev = lGaugeStatus[i].degree;
          lUpdated = true;
        }
      }
      for (size_t i = 0; i < ARRAY_SIZE(lTelltales); i++) {
        auto tt = display.add_telltales();
        tt->set_name(lTelltales[i].name);
        Dio_LevelType level = Dio_ReadChannel(lTelltales[i].ChannelId);
        if (STD_HIGH == level) {
          tt->set_on(true);
        } else {
          tt->set_on(false);
        }
        if (lTelltales[i].level != level) {
          lTelltales[i].level = level;
          lUpdated = true;
        }
      }
      if (lUpdated) {
        notify(display);
        lUpdated = false;
      }
#endif
      Std_TimerStart(&m_Timer);
    }
  }
#ifdef USE_SOMEIPXF
  void notify(Display_Type &display) {
    if (m_Subscribed) {
      auto buffer = m_BufferPool.get();
      if (nullptr != buffer) {
        uint32_t requestId = ((uint32_t)SOMEIP_TX_EVT_VIC_CLUSTER_STATUS << 16) + (++m_SessionId);
        auto r = SomeIpXf_EncodeStruct((uint8_t *)buffer->data, buffer->size, &display,
                                       &SomeIpXf_StructDisplayDef);
        if (r > 0) {
          buffer->size = r;
          server::Server::notify(requestId, buffer);
        }
      }
    }
  }
#else
  void notify(vic::display &display) {
    if (m_Subscribed) {
      auto buffer = m_BufferPool.get();
      if (nullptr != buffer) {
        uint32_t requestId = ((uint32_t)SOMEIP_TX_EVT_VIC_CLUSTER_STATUS << 16) + (++m_SessionId);
        auto r = display.SerializeToArray(buffer->data, buffer->size);
        if (r) {
          buffer->size = display.ByteSizeLong();
          server::Server::notify(requestId, buffer);
        }
      }
    }
  }
#endif

private:
  Std_TimerType m_Timer;
  BufferPool m_BufferPool;
  uint16_t m_SessionId = 0;
  bool m_Subscribed = false;
};

void SS_ExVIC_init(void) {
  SS_Instance = std::make_shared<SS_VIC>();
  SS_Instance->start();
}

void SS_ExVIC_main(void) {
  SS_Instance->run();
}

void SS_ExVIC_deinit(void) {
  SS_Instance->stop();
}
#ifdef USE_SOMEIPXF
void SS_ExVIC_notify(Display_Type &display) {
  SS_Instance->notify(display);
}
#else
void SS_ExVIC_notify(vic::display &display) {
  SS_Instance->notify(display);
}
#endif
REGISTER_PLUGIN(SS_ExVIC);
