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
  Rte_Write_Telltale_Airbag_Airbag(OnOff_On);
  Rte_Write_Telltale_AutoCruise_AutoCruise(OnOff_1Hz);
  Rte_Write_Telltale_HighBeam_HighBeam(OnOff_3Hz);
  Rte_Write_Telltale_LowOil_LowOil(OnOff_1Hz);
  Rte_Write_Telltale_PosLamp_PosLamp(OnOff_2Hz);
  Rte_Write_Telltale_SeatbeltDriver_SeatbeltDriver(OnOff_3Hz);
  Rte_Write_Telltale_SeatbeltPassenger_SeatbeltPassenger(OnOff_1Hz);
  Rte_Write_Telltale_TPMS_TPMS(OnOff_2Hz);
  Rte_Write_Telltale_TurnLeft_TurnLeft(OnOff_3Hz);
  Rte_Write_Telltale_TurnRight_TurnRight(OnOff_3Hz);
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
