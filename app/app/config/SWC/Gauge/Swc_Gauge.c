/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "GEN/Rte_Gauge.h"
#include "plugin.h"
#include "../../Com/GEN/Com_Cfg.h"
#include "Com.h"
/* ================================ [ MACROS    ] ============================================== */
#define RTE_COM_PORT_READ_IMPL(name, type)                                                         \
  Std_ReturnType Rte_Read_Com_##name##_##name(type *data) {                                        \
    return Com_ReceiveSignal(COM_SID_CAN0_RxMsgAbsInfo_##name, data);                              \
  }
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
RTE_COM_PORT_READ_IMPL(VehicleSpeed, uint16_t)
RTE_COM_PORT_READ_IMPL(TachoSpeed, uint16_t)

void Gauge_Run(void) {
  uint16_t VehicleSpeed = 0;
  uint16_t TachoSpeed = 0;

  Rte_Read_Com_VehicleSpeed_VehicleSpeed(&VehicleSpeed);
  Rte_Read_Com_TachoSpeed_TachoSpeed(&TachoSpeed);

  Rte_Write_Stmo_VehicleSpeed_VehicleSpeed(VehicleSpeed);
  Rte_Write_Stmo_TachoSpeed_TachoSpeed(TachoSpeed);
}

void Gauge_init(void) {
}

void Gauge_main(void) {
  Gauge_Run();
}

void Gauge_deinit(void) {
}

REGISTER_PLUGIN(Gauge)
