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

#ifdef USE_LINIF
#include "LinIf_Cfg.h"
#endif

#include "EcuM.h"
#include "EcuM_Externals.h"
#include "EcuM_Cfg.h"

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
#if defined(__HIWARE__)
#else
void __weak App_AliveIndicate(void) {
}

boolean __weak App_IsIgOn(void) {
  return TRUE;
}

#if defined(_WIN32) || defined(linux)
boolean __weak Mcu_IsResetRequested(void) {
  return FALSE;
}
#endif

extern boolean Can_WakeupCheck();
void __weak Mcu_EnterSleepMode(void) {
  boolean bWakeup = FALSE;
  ASLOG(INFO, ("Enter Sleep Mode!!!\n"));
  Can_SetControllerMode(0, CAN_CS_STARTED); /* ensure can online */
  while (FALSE == bWakeup) {
    if (TRUE == App_IsIgOn()) {
      bWakeup = TRUE;
    }
#if defined(_WIN32) || defined(linux)
    if (TRUE == Can_WakeupCheck()) {
      bWakeup = TRUE;
    }
#ifdef USE_SHELL
    Shell_MainFunction();
#endif
    usleep(1000);
#endif
  }
  ASLOG(INFO, ("Exit Sleep Mode!!!\n"));
}
#endif
#ifdef USE_STDIO_CAN
extern void stdio_main_function(void);
#endif
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer1ms;
static uint8_t timer10ms;
static uint8_t timer100ms;
/* ================================ [ LOCALS    ] ============================================== */
static boolean IsSleepAllowed(void) {
  boolean bAllowed = TRUE;
  boolean bIgOn = App_IsIgOn();
#if defined(USE_CANNM) || defined(USE_OSEKNM)
  Nm_StateType nmState;
  Nm_ModeType nmMode;
#endif
  if (bIgOn) {
    bAllowed = FALSE;
  }

#ifdef USE_CANNM
  CanNm_GetState(0, &nmState, &nmMode);
  if (NM_MODE_BUS_SLEEP != nmMode) {
    bAllowed = FALSE;
  }
#endif

#ifdef USE_OSEKNM
  OsekNm_GetState(0, &nmMode);
  if (NM_MODE_BUS_SLEEP != nmMode) {
    bAllowed = FALSE;
  }
#endif

  return bAllowed;
}

static void MainTask_10ms(void) {
  EcuM_BswService();

  if (FALSE == App_IsIgOn()) {
    EcuM_ReleaseRUN(0);
  } else {
    EcuM_RequestRUN(0);
  }

  if (IsSleepAllowed()) {
    EcuM_ReleasePOST_RUN(0);
  } else {
    EcuM_RequestPOST_RUN(0);
  }

  EcuM_MainFunction();

#ifdef USE_PLUGIN
  plugin_main();
#endif
}

static void Cdd_VFS_Init(void) {
#if defined(_WIN32) || defined(linux)
  static boolean bCddInitialized = FALSE;
  if (TRUE == bCddInitialized) {
    return; /* CDD VFS only support init once for the simulator reset simulation */
  }
  bCddInitialized = TRUE;
#endif
#ifdef USE_VFS
  int ercd;

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
}

static void Cdd_Init(void) {
  Cdd_VFS_Init();

#ifdef USE_PLUGIN
  plugin_init();
#endif

#ifdef USE_SHELL
  Shell_Init();
#endif
}

void TaskIdleHook(void) {
}

void Task_MainInitTwo(void) {
  Cdd_Init();
  App_Init();
  Std_TimerInit(&timer1ms, 1000);
  timer10ms = 0;
  timer100ms = 0;
}

void Task_MainLoop(void) {
  EcuM_StartupTwo();
  Task_MainInitTwo();
  for (;
#if defined(_WIN32) || defined(linux)
       FALSE == Mcu_IsResetRequested()
#endif
         ;) {
    if (TRUE == Std_IsTimerTimeout(&timer1ms)) {
      Std_TimerSet(&timer1ms, 1000);
      timer10ms++;
      if (timer10ms >= 10) {
        timer10ms = 0;
        STD_TRACE_APP(MAIN_TASK_10MS_B);
        MainTask_10ms();
        STD_TRACE_APP(MAIN_TASK_10MS_E);
      }
      timer100ms++;
      if (timer100ms >= 100) {
        timer100ms = 0;
        App_AliveIndicate();
        /* below to do profile EcuM_BswServiceFast each 100ms */
        // STD_TRACE_APP(MAIN_TASK_FAST_B);
        // EcuM_BswServiceFast();
        // STD_TRACE_APP(MAIN_TASK_FAST_E);
      }
      EcuM_BswServiceFast();
    }
#ifdef USE_SHELL
    Shell_MainFunction();
#endif
    App_MainFunction();
#if defined(USE_STDIO_CAN) || defined(USE_STDIO_OUT)
    stdio_main_function();
#endif
#ifdef USE_OSAL
    OSAL_SleepUs(1000);
#else
    TaskIdleHook();
#endif

    STD_TRACE_APP_MAIN();
  }
}

#ifdef USE_OSAL
void TaskMainTaskIdle(void) {
  while (1) {
    TaskIdleHook();
  }
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
#if defined(_WIN32) || defined(linux)
  while (TRUE) {
#endif
    EcuM_Init();

#ifdef USE_OSAL
    OSAL_Start();
#else
  Task_MainLoop();
#endif
#if defined(_WIN32) || defined(linux)
    usleep(1000000);
    ASLOG(INFO, ("reset...\n"));
  }
#endif
  return 0;
}

void EcuM_AL_EnterRUN(void) {
#ifdef USE_COMM
  ComM_RequestComMode(0, COMM_FULL_COMMUNICATION);
  ComM_CommunicationAllowed(0, TRUE);
  ComM_CommunicationAllowed(1, TRUE);
#else
#ifdef USE_CANSM
  CanSM_RequestComMode(0, COMM_FULL_COMMUNICATION);
#else
#ifdef USE_CAN
  Can_SetControllerMode(0, CAN_CS_STARTED);
#endif
#ifdef USE_CANIF
  CanIf_SetPduMode(0, CANIF_ONLINE);
#endif
#endif
#endif /* USE_COMM */

#ifdef USE_OSEKNM
  OsekNm_Talk(0);
  OsekNm_Start(0);
  OsekNm_GotoMode(0, OSEKNM_AWAKE);
#endif

#if defined(USE_CANNM) && !defined(USE_COMM)
  CanNm_NetworkRequest(0);
#endif

#if defined(USE_COM) && !defined(USE_CANNM)
  Com_IpduGroupStart(0, TRUE);
#endif

#ifdef USE_UDPNM
  UdpNm_NetworkRequest(0);
#endif

#ifdef USE_LINIF
  LinIf_WakeUp(0);
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
  LinIf_ScheduleRequest(0, 0);
#endif

#ifdef LINIF_SCHTBL_LIN1
  LinIf_WakeUp(1);
#if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER
  LinIf_ScheduleRequest(1, LINIF_SCHTBL_LIN1);
#endif
#endif
#endif
}

void EcuM_AL_EnterPOST_RUN(void) {
#ifdef USE_UDPNM
  UdpNm_NetworkRelease(0);
#endif

#ifdef USE_COMM
  ComM_CommunicationAllowed(0, FALSE);
  ComM_CommunicationAllowed(1, FALSE);
  ComM_RequestComMode(0, COMM_NO_COMMUNICATION);
#else
#ifdef USE_COM
  Com_IpduGroupStop(0);
#endif

#ifdef USE_CANNM
  CanNm_NetworkRelease(0);
#endif
#endif /* USE_COMM */

#ifdef USE_OSEKNM
  OsekNm_GotoMode(0, OSEKNM_BUS_SLEEP);
#endif

#ifdef USE_LINIF
  LinIf_GotoSleep(0);
#endif
}

void EcuM_AL_EnterSLEEP(void) {
  Mcu_EnterSleepMode(); /* with this to simulate sleep mode */
  /* Here wakeup */
  EcuM_AL_DriverInitZero();
  EcuM_AL_DriverInitOne();
  /* EcuM_AL_DriverInitTwo(); */
  /* skip NvM stack initialization as it time consuming */

#ifdef USE_DEM
  Dem_Init(NULL);
#endif
#ifdef USE_DCM
  Dcm_Init(NULL);
#endif

  Task_MainInitTwo();

/* wakeup, at least to ensure CAN RX OK */
#ifdef USE_CANSM
  CanSM_StartWakeupSource(0);
  CanSM_StartWakeupSource(1);
  CanSM_MainFunction();
#endif
#ifdef USE_CAN
  Can_SetControllerMode(0, CAN_CS_STARTED);
  Can_SetControllerMode(1, CAN_CS_STARTED);
#endif
#ifdef USE_CANIF
  CanIf_SetPduMode(0, CANIF_ONLINE);
  CanIf_SetPduMode(1, CANIF_ONLINE);
#endif
#ifdef USE_COMM
  ComM_CommunicationAllowed(0, TRUE);
  ComM_CommunicationAllowed(1, TRUE);
  ComM_MainFunction();
#endif
#ifdef USE_CANNM
  if (App_IsIgOn()) {
    CanNm_NetworkRequest(0);
    CanNm_NetworkRequest(1);
  } else {
    CanNm_PassiveStartUp(0);
    CanNm_PassiveStartUp(1);
  }
  CanNm_MainFunction();
#endif
}
