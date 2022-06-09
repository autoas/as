/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mcu.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include <string.h>
#include <assert.h>

#ifdef USE_CAN
#include "Can.h"
#include "CanIf.h"
#include "CanIf_Can.h"
#include "CanTp.h"
#include "PduR_CanTp.h"
#ifdef USE_OSEKNM
#include "OsekNm.h"
#endif
#ifdef USE_CANNM
#include "CanNm.h"
#endif
#endif

#ifdef USE_PDUR
#include "PduR.h"
#endif

#ifdef USE_COM
#include "Com.h"
#include "./config/Com/GEN/Com_Cfg.h"
#include "PduR_Com.h"
#endif

#ifdef USE_DLL
#include "Dll.h"
#endif

#ifdef USE_LINTP
#include "LinTp.h"
#endif

#include "Dcm.h"
#ifdef USE_DEM
#include "Dem.h"
#endif
#ifdef USE_FLS
#include "Fls.h"
#endif
#ifdef USE_FEE
#include "Fee.h"
#endif
#ifdef USE_EEP
#include "Eep.h"
#endif
#ifdef USE_EA
#include "Ea.h"
#endif
#ifdef USE_NVM
#include "NvM.h"
#endif

#ifdef USE_TCPIP
#include "TcpIp.h"
#endif

#ifdef USE_SOAD
#include "SoAd.h"
#endif

#ifdef USE_DOIP
#include "DoIP.h"
#endif

#ifdef USE_SD
#include "Sd.h"
#endif

#ifdef USE_SOMEIP
#include "SomeIp.h"
#endif

#ifdef USE_PLUGIN
#include "plugin.h"
#endif

#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#include "app.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void App_AliveIndicate(void);
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer100ms;
/* ================================ [ LOCALS    ] ============================================== */
static void MemoryTask(void) {
#ifdef USE_EEP
  Eep_MainFunction();
#endif
#ifdef USE_EA
  Ea_MainFunction();
#endif
#ifdef USE_FLS
  Fls_MainFunction();
#endif
#ifdef USE_FEE
  Fee_MainFunction();
#endif
#ifdef USE_NVM
  NvM_MainFunction();
#endif
}

static void MainTask_10ms(void) {
#ifdef USE_CAN
#ifdef USE_CANTP
  CanTp_MainFunction();
#endif
#ifdef USE_OSEKNM
  OsekNm_MainFunction();
#endif
#ifdef USE_CANNM
  CanNm_MainFunction();
#endif
#endif
#ifdef USE_LINTP
  LinTp_MainFunction();
#endif
#ifdef USE_COM
  Com_MainFunction();
#endif

  MemoryTask();
#ifdef USE_DCM
  Dcm_MainFunction();
#endif

#ifdef USE_DOIP
  DoIP_MainFunction();
#endif
#ifdef USE_SD
  Sd_MainFunction();
#endif
#ifdef USE_SOMEIP
  SomeIp_MainFunction();
#endif

#ifdef USE_PLUGIN
  plugin_main();
#endif
}

static void Net_Init(void) {
#ifdef USE_TCPIP
  TcpIp_Init(NULL);
#endif
#ifdef USE_SOAD
  SoAd_Init(NULL);
#endif
#ifdef USE_DOIP
  DoIP_Init(NULL);
#endif
#ifdef USE_SD
  Sd_Init(NULL);
#endif
#ifdef USE_SOMEIP
  SomeIp_Init(NULL);
#endif
}

static void BSW_Init(void) {
#ifdef USE_CAN
  Can_Init(NULL);
  Can_SetControllerMode(0, CAN_CS_STARTED);
#ifdef USE_CANTP
  CanTp_Init(NULL);
#endif
#ifdef USE_OSEKNM
  OsekNm_Init(NULL);
  TalkNM(0);
  StartNM(0);
  GotoMode(0, NM_BusSleep);
#endif
#ifdef USE_CANNM
  CanNm_Init(NULL);
#endif
#endif
#ifdef USE_DLL
  DLL_Init(NULL);
  DLL_ScheduleRequest(0, 0);
#endif
#ifdef USE_LINTP
  LinTp_Init(NULL);
#endif

#ifdef USE_PDUR
  PduR_Init(NULL);
#endif

#ifdef USE_COM
  Com_Init(NULL);
#endif

#ifdef USE_EEP
  Eep_Init(NULL);
#endif
#ifdef USE_EA
  Ea_Init(NULL);
#endif
#ifdef USE_FLS
  Fls_Init(NULL);
#endif
#ifdef USE_FEE
  Fee_Init(NULL);
#endif
#ifdef USE_NVM
  NvM_Init(NULL);
  while (MEMIF_IDLE != NvM_GetStatus()) {
    MemoryTask();
  }
  NvM_ReadAll();
  while (MEMIF_IDLE != NvM_GetStatus()) {
    MemoryTask();
  }
#endif

#ifdef USE_DEM
  Dem_PreInit();
  Dem_Init(NULL);
#endif
#ifdef USE_DCM
  Dcm_Init(NULL);
#endif

#ifdef USE_PLUGIN
  plugin_init();
#endif
}

void Task_MainLoop(void) {
  Net_Init();
  App_Init();
  Std_TimerStart(&timer10ms);
  Std_TimerStart(&timer100ms);
  for (;;) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      Std_TimerStart(&timer10ms);
      MainTask_10ms();
    }

    if (Std_GetTimerElapsedTime(&timer100ms) >= 100000) {
      Std_TimerStart(&timer100ms);
      App_AliveIndicate();
    }
#ifdef USE_DCM
    Dcm_MainFunction_Request();
#endif
#ifdef USE_CAN
    Can_MainFunction_Write();
    Can_MainFunction_Read();
#endif
#ifdef USE_DLL
    DLL_MainFunction();
    DLL_MainFunction_Read();
#endif
#ifdef USE_TCPIP
    TcpIp_MainFunction();
#endif
#ifdef USE_SOAD
    SoAd_MainFunction();
#endif
    App_MainFunction();
#ifdef USE_FREERTOS
    vTaskDelay(1);
#elif defined(_WIN32) || defined(linux)
    Std_Sleep(1000);
#endif
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  ASLOG(INFO, ("application build @ %s %s\n", __DATE__, __TIME__));

  Mcu_Init(NULL);

  BSW_Init();

#ifdef USE_FREERTOS
  xTaskCreate((TaskFunction_t)Task_MainLoop, "MainLoop", configMINIMAL_STACK_SIZE, NULL,
              tskIDLE_PRIORITY + 1, NULL);
  vTaskStartScheduler();
#else
  Task_MainLoop();
#endif

  return 0;
}
