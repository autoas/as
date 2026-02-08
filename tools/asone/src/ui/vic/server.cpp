/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "VICSkeleton.h"
#include "Dio.h"
#include "plugin.h"
#include "Std_Timer.h"
#include <cstring>
#include <string>
/* ================================ [ MACROS    ] ============================================== */
#define RTE_STMO_PORT_WRITE(name, id)                                                              \
  extern "C" Std_ReturnType Rte_Write_Stmo_##name##_##name(uint16_t data) {                        \
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
/* ================================ [ DATAS     ] ============================================== */
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

static Std_TimerType timer;
/* ================================ [ LOCALS    ] ============================================== */
RTE_STMO_PORT_WRITE(VehicleSpeed, 0);
RTE_STMO_PORT_WRITE(TachoSpeed, 1);
RTE_STMO_PORT_WRITE(Temperature, 2);
RTE_STMO_PORT_WRITE(Fuel, 3);
/* ================================ [ FUNCTIONS ] ============================================== */
boolean Sd_ServerServiceVIC_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,
                                 uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,
                                 const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,
                                 const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray) {
  return true;
}

void SomeIp_VIC_OnConnect(uint16_t conId, boolean isConnected) {
}

void VIC_OnDisplayStatusSubscribed(boolean isSubscribe, TcpIp_SockAddrType *RemoteAddr) {

}

void SS_ExVIC_init(void) {
  VIC_OfferService();
  Std_TimerInit(&timer, 10000);
}

void SS_ExVIC_main(void) {
  if (Std_IsTimerTimeout(&timer)) {
    Std_TimerSet(&timer, 10000);
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
      VIC_SendDisplayStatus(&display);
      lUpdated = false;
    }
  }
}

void SS_ExVIC_deinit(void) {
}

REGISTER_PLUGIN(SS_ExVIC);
