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
  Std_ReturnType Rte_Read_Gauge_Com_##name(type *data) {                                           \
    return Com_ReceiveSignal(COM_SID_##name, data);                                                \
  }
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
RTE_COM_PORT_READ_IMPL(VehicleSpeed, UINT16_T)
RTE_COM_PORT_READ_IMPL(TachoSpeed, UINT16_T)

void Gauge_Run(void) {
  uint16_t VehicleSpeed = 0;
  uint16_t TachoSpeed = 0;

  Rte_Read_Com_VehicleSpeed(&VehicleSpeed);
  Rte_Read_Com_TachoSpeed(&TachoSpeed);

  Rte_Write_Stmo_VehicleSpeed(VehicleSpeed);
  Rte_Write_Stmo_TachoSpeed(TachoSpeed);
}

void Gauge_init(void) {
}

void Gauge_main(void) {
  Gauge_Run();
}

void Gauge_deinit(void) {
}

REGISTER_PLUGIN(Gauge)
