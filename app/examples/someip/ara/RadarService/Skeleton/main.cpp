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
#include "RadarServiceSkeleton.hpp"

using namespace ara::com::RadarService;
using namespace ara::com::RadarService::events;
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_RADAR 1
/* ================================ [ TYPES     ] ============================================== */
class RadarServiceImpl : public RadarServiceSkeleton {
public:
  RadarServiceImpl(ara::com::InstanceIdentifier instanceId) : RadarServiceSkeleton(instanceId) {
  }

  ~RadarServiceImpl() {
  }

  void Init() {
  }

public:
  Future<AdjustOutput> Adjust(const Position &position) {
    ara::core::Promise<AdjustOutput> promise;
    auto future = promise.get_future();

    // asynchronous call to internal adjust function in a new Thread
    std::thread th(
      [this](const Position &pos, ara::core::Promise<AdjustOutput> prom) {
        prom.set_value(doAdjustInternal(pos));
      },
      std::cref(position), std::move(promise));
    th.detach();

    // we return a future, which might be set or not at this point...
    return future;
  }

private:
  AdjustOutput doAdjustInternal(const Position &position) {
    AdjustOutput out;

    out.success = true;
    out.effective_position = position;
    ASLOG(RADAR, ("Adjust to position (%u, %u, %u)\n", position.x, position.y, position.z));

    return out;
  }
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer1s;
/* ================================ [ LOCALS    ] ============================================== */
static void BrakeEventHandler(RadarServiceImpl &myRadarService) {
  static uint8_t num = 1;
  Result<SampleAllocateePtr<BrakeEvent::SampleType>> rslt = myRadarService.BrakeEvent.Allocate();
  if (true == rslt.HasValue()) {
    SampleAllocateePtr<BrakeEvent::SampleType> curSamplePtr = std::move(rslt.Value());
    curSamplePtr->active = true;
    curSamplePtr->objectsLen = num;
    for (uint32_t i = 0; i < num; i++) {
      curSamplePtr->objects[i].x = num + 1;
      curSamplePtr->objects[i].y = num + 2;
      curSamplePtr->objects[i].z = num + 3;
    }
    num++;
    if (num > 32) {
      num = 1;
    }
    ASLOG(RADAR, ("sending %u radar objects\n", num));
    myRadarService.BrakeEvent.Send(std::move(curSamplePtr));
  }
}

static void RadarServiceMain(void) {
  uint16_t instanceId = SOMEIP_SSID_RADAR_SERVICE;
  ara::core::StringView instanceIdStr((char *)&instanceId, 2);
  RadarServiceImpl myRadarService{InstanceIdentifier(instanceIdStr)};

  myRadarService.Init();

  myRadarService.OfferService();

  while (true) {
    while (ara::com::SubscriptionState::kSubscribed ==
           myRadarService.BrakeEvent.GetSubscriptionState()) {
      BrakeEventHandler(myRadarService);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
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

  std::thread th = std::thread(RadarServiceMain);
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
