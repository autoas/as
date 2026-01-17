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
#include "SomeIp_Cfg.h"
#include "SomeIpXf_Cfg.h"

#include "Std_Timer.h"

#include "Std_Debug.h"

#include <thread>
#include "RadarServiceProxy.hpp"

#include "Std_Debug.h"

using namespace ara::core;
using namespace ara::com;
using namespace ara::com::RadarService;
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_RADAR 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer1s;

static std::unique_ptr<RadarServiceProxy> myRadarProxy;
/* ================================ [ LOCALS    ] ============================================== */
void handleBrakeEventReception() {
  myRadarProxy->BrakeEvent.GetNewSamples([&](SamplePtr<events::BrakeEvent::SampleType> samplePtr) {
    if (true == samplePtr->active) {
      ASLOG(RADAR, ("%u objects: [(%u, %u, %u), (%u, %u, %u), ...]\n", samplePtr->objectsLen,
                    samplePtr->objects[0].x, samplePtr->objects[0].y, samplePtr->objects[0].z,
                    samplePtr->objects[1].x, samplePtr->objects[1].y, samplePtr->objects[1].z));
    } else {
      ASLOG(RADAR, ("sample is not active\n"));
    }
  });
}

void handleUpdateRateReception() {
  myRadarProxy->UpdateRate.GetNewSamples([&](SamplePtr<fields::UpdateRate::FieldType> samplePtr) {
    ASLOG(RADAR, ("UpdateRate: %u\n", *samplePtr));
  });
}

static void RadarServiceMain(void) {
  uint16_t instanceId = SOMEIP_CSID_RADAR_SERVICE;
  StringView instanceIdStr((char *)&instanceId, 2);
  InstanceSpecifier instspec{instanceIdStr};

  Result<ServiceHandleContainer<RadarServiceProxy::HandleType>> rslt({});

  do {
    rslt = RadarServiceProxy::FindService(instspec);
    if (rslt.Value().empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } while (rslt.HasValue() && rslt.Value().empty());

  ASLOG(RADAR, ("Service online\n"));

  ServiceHandleContainer<RadarServiceProxy::HandleType> handles = rslt.Value();

  if (false == handles.empty()) {
    myRadarProxy = std::make_unique<RadarServiceProxy>(handles[0]);

    myRadarProxy->BrakeEvent.Subscribe(10);
    myRadarProxy->UpdateRate.Subscribe(10);

    auto state = myRadarProxy->BrakeEvent.GetSubscriptionState();
    while (state != SubscriptionState::kSubscribed) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      state = myRadarProxy->BrakeEvent.GetSubscriptionState();
    }
    ASLOG(RADAR, ("BrakeEvent online\n"));
    myRadarProxy->BrakeEvent.SetReceiveHandler(handleBrakeEventReception);

    state = myRadarProxy->UpdateRate.GetSubscriptionState();
    while (state != SubscriptionState::kSubscribed) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      state = myRadarProxy->UpdateRate.GetSubscriptionState();
    }
    ASLOG(RADAR, ("UpdateRate online\n"));
    myRadarProxy->UpdateRate.SetReceiveHandler(handleUpdateRateReception);

    while (state == SubscriptionState::kSubscribed) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      state = myRadarProxy->BrakeEvent.GetSubscriptionState();
    }

    ASLOG(RADAR, ("Event offline\n"));
  }
}

static void RadarServiceMainLoop(void) {
  while (true) {
    RadarServiceMain();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    myRadarProxy = nullptr;
  }
}

static void RadarServiceMethodMain(void) {
  while (nullptr == myRadarProxy) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  auto state = myRadarProxy->BrakeEvent.GetSubscriptionState();
  while (state != SubscriptionState::kSubscribed) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    state = myRadarProxy->BrakeEvent.GetSubscriptionState();
  }
  ASLOG(RADAR, ("Method online\n"));

  uint32_t i = 0;
  while (state == SubscriptionState::kSubscribed) {
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      methods::Adjust::Position target_position = {i, i + 1, i + 2};
      Future<methods::Adjust::Output> future = myRadarProxy->Adjust(target_position);
      Result<methods::Adjust::Output> rslt = future.GetResult();
      if (rslt.HasValue()) {
        methods::Adjust::Output output = rslt.Value();
        ASLOG(RADAR, ("Adjust with: %s (%u, %u, %u)\n", output.success ? "success" : "fail",
                      output.effective_position.x, output.effective_position.y,
                      output.effective_position.z));
      } else {
        ASLOG(RADAR, ("Adjust with error: %d\n", rslt.Error().Value()));
      }
    }
    {
      fields::UpdateRate::FieldType newField = i * 3;
      Future<fields::UpdateRate::FieldType> future = myRadarProxy->UpdateRate.Set(newField);
      Result<fields::UpdateRate::FieldType> rslt = future.GetResult();
      if (rslt.HasValue()) {
        fields::UpdateRate::FieldType field = rslt.Value();
        ASLOG(RADAR, ("Set Filed UpdateRate with: %u\n", field));
      } else {
        ASLOG(RADAR, ("Set UpdateRate with error: %d\n", rslt.Error().Value()));
      }
    }
    {
      Future<fields::UpdateRate::FieldType> future = myRadarProxy->UpdateRate.Get();
      Result<fields::UpdateRate::FieldType> rslt = future.GetResult();
      if (rslt.HasValue()) {
        fields::UpdateRate::FieldType field = rslt.Value();
        ASLOG(RADAR, ("Get Filed UpdateRate with: %u\n", field));
      } else {
        ASLOG(RADAR, ("Get UpdateRate with error: %d\n", rslt.Error().Value()));
      }
    }

    state = myRadarProxy->BrakeEvent.GetSubscriptionState();
    i++;
  }
  ASLOG(RADAR, ("Method offline\n"));
}

static void RadarServiceMethodMainLoop(void) {
  while (true) {
    RadarServiceMethodMain();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  TcpIp_Init(NULL);
  SoAd_Init(NULL);
  Sd_Init(NULL);
  SomeIp_Init(NULL);

  Std_TimerStart(&timer10ms);
  Std_TimerStart(&timer1s);

  std::thread th = std::thread(RadarServiceMainLoop);
  std::thread thm = std::thread(RadarServiceMethodMainLoop);
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
    }
  }

  return 0;
}
