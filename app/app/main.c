/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Mcu.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include <string.h>
#if defined(_WIN32) || defined(linux)
#include <stdlib.h>
#include <unistd.h>
#endif

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
#ifdef USE_CANTSYN
#include "CanTSyn.h"
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



#ifdef USE_LINIF
#include "LinIf.h"
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

#ifdef USE_UDPNM
#include "UdpNm.h"
#endif

#ifdef USE_PLUGIN
#include "plugin.h"
#endif

#ifdef USE_OSAL
#include "osal.h"
#endif


#ifdef USE_SHELL
#include "shell.h"
#endif

#ifdef USE_VFS
#include "vfs.h"
#endif

#include "app.h"

#ifdef USE_TRACE_APP
#include "TraceApp_Cfg.h"
#else
#define STD_TRACE_APP(ev)
#define STD_TRACE_APP_MAIN()
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void App_AliveIndicate(void);
#ifdef USE_STDIO_CAN
extern void stdio_main_function(void);
#endif
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
#ifdef USE_CANTSYN
  CanTSyn_MainFunction();
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

#ifdef USE_DEM
  Dem_MainFunction();
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

#ifdef USE_UDPNM
  UdpNm_MainFunction();
#endif

#ifdef USE_PLUGIN
  plugin_main();
#endif
}

static void Net_Init(void) {
#ifdef USE_VFS
  int ercd;
#endif
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

#ifdef USE_UDPNM
  UdpNm_Init(NULL);
#endif

#ifdef USE_PLUGIN
  plugin_init();
#endif

#ifdef USE_VFS
  vfs_init();
#ifdef USE_LWEXT4
  extern const device_t dev_sd1;
  ercd = vfs_mount(&dev_sd1, "ext", "/");
  if (0 != ercd) {
    ercd = vfs_mkfs(&dev_sd1, "ext");
    if (0 == ercd) {
      ercd = vfs_mount(&dev_sd1, "ext", "/");
    }
  }
  ASLOG(INFO, ("mount sd1 on / %s\n", ercd ? "failed" : "okay"));
#endif
#ifdef USE_FATFS
#ifdef USE_LWEXT4
#define FATFS_MP "/dos"
  vfs_mkdir(FATFS_MP, 0777);
#else
#define FATFS_MP "/"
#endif
  extern const device_t dev_sd0;
  ercd = vfs_mount(&dev_sd0, "vfat", FATFS_MP);
  if (0 != ercd) {
    ercd = vfs_mkfs(&dev_sd0, "vfat");
    if (0 == ercd) {
      ercd = vfs_mount(&dev_sd0, "vfat", FATFS_MP);
    }
  }
  ASLOG(INFO, ("mount sd0 on %s %s\n", FATFS_MP, ercd ? "failed" : "okay"));
#endif
#if defined(_WIN32) || defined(linux)
  extern const device_t dev_host;
  vfs_mkdir("/share", 0777);
  vfs_mount(&dev_host, "host", "/share");
#endif
#endif

#ifdef USE_SHELL
  Shell_Init();
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
#ifdef USE_CANTSYN
  CanTSyn_Init(NULL);
#endif
#endif

#ifdef USE_LINIF
  Lin_Init(NULL);
  LinIf_Init(NULL);
  LinIf_ScheduleRequest(0, 0);
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
}

void Task_MainLoop(void) {
  BSW_Init();
  Net_Init();
  App_Init();
  Std_TimerStart(&timer10ms);
  Std_TimerStart(&timer100ms);
  for (;;) {
    if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
      Std_TimerStart(&timer10ms);
      STD_TRACE_APP(MAIN_TASK_10MS_B);
      MainTask_10ms();
      STD_TRACE_APP(MAIN_TASK_10MS_E);
    }

    if (Std_GetTimerElapsedTime(&timer100ms) >= 100000) {
      Std_TimerStart(&timer100ms);
      STD_TRACE_APP(APP_ALIVE);
      App_AliveIndicate();
    }
#ifdef USE_DCM
    Dcm_MainFunction_Request();
#endif
#ifdef USE_CAN
    Can_MainFunction_Write();
    Can_MainFunction_Read();
#endif

#ifdef USE_LINIF
    Lin_MainFunction();
    Lin_MainFunction_Read();
    LinIf_MainFunction();
#endif
#ifdef USE_TCPIP
    TcpIp_MainFunction();
#endif
#ifdef USE_SOAD
    SoAd_MainFunction();
#endif
#ifdef USE_SHELL
    Shell_MainFunction();
#endif
    App_MainFunction();
#if defined(USE_STDIO_CAN) || defined(USE_STDIO_OUT)
    stdio_main_function();
#endif
    STD_TRACE_APP_MAIN();
#ifdef USE_OSAL
    OSAL_SleepUs(1000);
#endif
  }
}

#ifdef USE_OSAL
void TaskMainTaskIdle(void) {
  while (1)
    ;
}
void StartupHook(void) {
  OSAL_ThreadCreate((OSAL_ThreadEntryType)Task_MainLoop, NULL);
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(_WIN32) || defined(linux)
void Can_ReConfig(uint8_t Controller, const char *device, int port, uint32_t baudrate);
#endif

int main(int argc, char *argv[]) {
  ASLOG(INFO, ("application build @ %s %s\n", __DATE__, __TIME__));

#if defined(_WIN32) || defined(linux)
  {
    int ch;
    opterr = 0;
    while ((ch = getopt(argc, argv, "d:v:")) != -1) {
      switch (ch) {
      case 'd':
        Can_ReConfig(0, optarg, 0, 500000);
        break;
      case 'v':
        std_set_log_level(atoi(optarg));
        break;
      default:
        printf("Usage: %s -d can0_device -v level\n", argv[0]);
        return 0;
        break;
      }
    }
  }
#endif

  Mcu_Init(NULL);

#ifdef USE_OSAL
  OSAL_Start();
#else
  Task_MainLoop();
#endif

  return 0;
}
