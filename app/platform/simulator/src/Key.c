/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dem.h"
#include "OsekNm.h"
#include "CanNm.h"
#include "UdpNm.h"
#include "Std_Critical.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "DoIP.h"
#include "Sd.h"
#include "Std_Debug.h"
#ifdef USE_SHELL
#include "shell.h"
#endif

#ifdef _WIN32
#include <windows.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_KEY 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#ifndef USE_SHELL
static pthread_t lThread;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void handle_key(int ch) {
#ifdef USE_DEM
  Dem_EventIdType EventId;
  static const char testPass[] = "pqwertyuio";
#endif
#ifdef USE_DEM
  if ((ch >= '0') && (ch <= '9')) {
    EventId = ch - '0';
    Dem_SetEventStatus(EventId, DEM_EVENT_STATUS_PREFAILED);
  } else if (ch == 's') {
    Dem_SetOperationCycleState(DEM_OPERATION_CYCLE_IGNITION, DEM_OPERATION_CYCLE_STARTED);
    Dem_SetEnableCondition(0, TRUE);
  } else if (ch == 'k') {
    Dem_SetOperationCycleState(DEM_OPERATION_CYCLE_IGNITION, DEM_OPERATION_CYCLE_STOPPED);
    Dem_SetEnableCondition(0, FALSE);
  } else {
    for (EventId = 0; EventId < sizeof(testPass); EventId++) {
      if (testPass[EventId] == ch) {
        Dem_SetEventStatus(EventId, DEM_EVENT_STATUS_PREPASSED);
      }
    }
  }
#endif
#ifdef USE_OSEKNM
  if (ch == 'x') {
    static int sleeped = TRUE;
    if (FALSE == sleeped) {
      ASLOG(KEY, ("OSEKNM goto sleep\n"));
      GotoMode(0, NM_BusSleep);
      sleeped = TRUE;
    } else {
      ASLOG(KEY, ("OSEKNM goto wakeup\n"));
      GotoMode(0, NM_Awake);
      sleeped = FALSE;
    }
  }
#endif

#ifdef USE_CANNM
  if (ch == 'x') {
    static int requested = FALSE;
    if (FALSE == requested) {
      ASLOG(KEY, ("CanNm request\n"));
      CanNm_NetworkRequest(0);
      requested = TRUE;
    } else {
      ASLOG(KEY, ("CanNm release\n"));
      CanNm_NetworkRelease(0);
      requested = FALSE;
    }
  }
#endif
#ifdef USE_UDPNM
  if (ch == 'x') {
    static int requested = FALSE;
    if (FALSE == requested) {
      ASLOG(KEY, ("UdpNm request\n"));
      UdpNm_NetworkRequest(0);
      requested = TRUE;
    } else {
      ASLOG(KEY, ("UdpNm release\n"));
      UdpNm_NetworkRelease(0);
      requested = FALSE;
    }
  }
#endif
#ifdef USE_DOIP
  if (ch == 'd') {
    static int avtive = FALSE;
    if (FALSE == avtive) {
      ASLOG(KEY, ("DoIP request\n"));
      DoIP_ActivationLineSwitchActive();
      avtive = TRUE;
    } else {
      ASLOG(KEY, ("DoIP release\n"));
      DoIP_ActivationLineSwitchInactive();
      avtive = FALSE;
    }
  }
#endif
#if defined(USE_SD) && !defined(USE_VIC)
  if (ch == 's') {
    static int avtive = FALSE;
    if (FALSE == avtive) {
      ASLOG(KEY, ("SD request\n"));
      Sd_ServerServiceSetState(0, SD_SERVER_SERVICE_AVAILABLE);
      Sd_ClientServiceSetState(0, SD_CLIENT_SERVICE_REQUESTED);
      Sd_ConsumedEventGroupSetState(0, SD_CONSUMED_EVENTGROUP_REQUESTED);
      avtive = TRUE;
    } else {
      ASLOG(KEY, ("SD release\n"));
      Sd_ServerServiceSetState(0, SD_SERVER_SERVICE_DOWN);
      Sd_ClientServiceSetState(0, SD_CLIENT_SERVICE_RELEASED);
      Sd_ConsumedEventGroupSetState(0, SD_CONSUMED_EVENTGROUP_RELEASED);
      avtive = FALSE;
    }
  }
#endif
}
#ifndef USE_SHELL
#ifdef _WIN32
static int _getch(void) {
  int ch = -1;
  int i;
  SHORT state;

  static const char keys[] = "SK1234567890QWERTYUIOPXD";
  static uint64_t KeyFlag = 0x00;

  for (i = 0; i < sizeof(keys); i++) {
    state = GetKeyState((int)keys[i]);
    if (0x80 & state) {
      if (0 == (KeyFlag & (1 << i))) {
        ch = keys[i];
        KeyFlag |= 1 << i;
        break;
      }
    } else {
      KeyFlag &= ~(1 << i);
    }
  }

  if (-1 == ch) {
    usleep(10000);
  } else {
    if ((ch >= 'A') && (ch <= 'Z')) {
      ch = 'a' + ch - 'A';
    }
  }

  return ch;
}
#endif

static void *KeyMonitorThread(void *arg) {
  while (TRUE) {
#ifdef _WIN32
    int ch = _getch();
#else
    int ch = getchar();
#endif
    handle_key(ch);
  }

  return NULL;
}

static void __attribute__((constructor)) _key_mgr_start(void) {
  pthread_create(&lThread, NULL, KeyMonitorThread, NULL);
}
#else /* USE_SHELL */
static int cmdKeyFunc(int argc, const char *argv[]) {
  if (argc != 2) {
    return -1;
  }
  int ch = (int)argv[1][0];
  handle_key(ch);
  return 0;
}
SHELL_REGISTER(key,
               "legacy key to do some sanity test\n"
#ifdef USE_DEM
               "  key 0|1|2|3|4|5|6|7|8|9: report DTC prefail\n"
               "  key p|q|w|e|r|t|y|u|i|o: report DTC prepass\n"
               "  key s: start operation ignition\n"
               "  key k: stop operation ignition\n"
#endif
#if defined(USE_OSEKNM) || defined(USE_CANNM)
               "  key x: toggle NM status\n"
#endif
#ifdef USE_DOIP
               "  key d: toggle DoIP request status\n"
#endif
#if defined(USE_SD) && !defined(USE_VIC)
               "  key s: toggle SOMEIP-SD request status\n"
#endif
               ,
               cmdKeyFunc);
#endif /* USE_SHELL */
/* ================================ [ FUNCTIONS ] ============================================== */
