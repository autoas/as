/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#include "GEN/Rte_Telltale.h"
#include "plugin.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void Swc_TelltaleManager(void);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Telltale_run(void) {
  Rte_Write_Telltale_AirbagState(OnOff_On);
  Rte_Write_Telltale_AutoCruiseState(OnOff_1Hz);
  Rte_Write_Telltale_HighBeamState(OnOff_3Hz);
  Rte_Write_Telltale_LowOilState(OnOff_1Hz);
  Rte_Write_Telltale_PosLampState(OnOff_2Hz);
  Rte_Write_Telltale_SeatbeltDriverState(OnOff_3Hz);
  Rte_Write_Telltale_SeatbeltPassengerState(OnOff_1Hz);
  Rte_Write_Telltale_TPMSState(OnOff_2Hz);
  Rte_Write_Telltale_TurnLeftState(OnOff_3Hz);
  Rte_Write_Telltale_TurnRightState(OnOff_3Hz);
}

void Telltale_init(void) {
}

void Telltale_main(void) {
  Telltale_run();
  Swc_TelltaleManager();
}

void Telltale_deinit(void) {
}

REGISTER_PLUGIN(Telltale)