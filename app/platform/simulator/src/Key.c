/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dem.h"
#include "OsekNm.h"
#include "CanNm.h"
#include "Std_Critical.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "DoIP.h"
#include "Sd.h"

#ifdef _WIN32
#include <windows.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static pthread_t lThread;
/* ================================ [ LOCALS    ] ============================================== */
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
#ifdef USE_DEM
  Dem_EventIdType EventId;
  static const char testPass[] = "pqwertyuio";
#endif
  while (TRUE) {
#ifdef _WIN32
    int ch = _getch();
#else
    int ch = getchar();
#endif
    (void)ch;
#ifdef USE_DEM
    if ((ch >= '0') && (ch <= '9')) {
      EventId = ch - '0';
      Dem_SetEventStatus(EventId, DEM_EVENT_STATUS_PREFAILED);
    } else if (ch == 's') {
      Dem_SetOperationCycleState(DEM_OPERATION_CYCLE_IGNITION, DEM_OPERATION_CYCLE_STARTED);
    } else if (ch == 'k') {
      Dem_SetOperationCycleState(DEM_OPERATION_CYCLE_IGNITION, DEM_OPERATION_CYCLE_STOPPED);
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
        printf("OSEKNM goto sleep\n");
        GotoMode(0, NM_BusSleep);
        sleeped = TRUE;
      } else {
        printf("OSEKNM goto wakeup\n");
        GotoMode(0, NM_Awake);
        sleeped = FALSE;
      }
    }
#endif

#ifdef USE_CANNM
    if (ch == 'x') {
      static int requested = FALSE;
      if (FALSE == requested) {
        printf("CanNm request\n");
        CanNm_NetworkRequest(0);
        requested = TRUE;
      } else {
        printf("CanNm release\n");
        CanNm_NetworkRelease(0);
        requested = FALSE;
      }
    }
#endif
#ifdef USE_DOIP
    if (ch == 'd') {
      static int avtive = FALSE;
      if (FALSE == avtive) {
        printf("DoIP request\n");
        DoIP_ActivationLineSwitchActive();
        avtive = TRUE;
      } else {
        printf("DoIP release\n");
        DoIP_ActivationLineSwitchInactive();
        avtive = FALSE;
      }
    }
#endif
#ifdef USE_SD
    if (ch == 's') {
      static int avtive = FALSE;
      if (FALSE == avtive) {
        printf("SD request\n");
        Sd_ServerServiceSetState(0, SD_SERVER_SERVICE_AVAILABLE);
        Sd_ClientServiceSetState(0, SD_CLIENT_SERVICE_REQUESTED);
        Sd_ConsumedEventGroupSetState(0, SD_CONSUMED_EVENTGROUP_REQUESTED);
        avtive = TRUE;
      } else {
        printf("SD release\n");
        Sd_ServerServiceSetState(0, SD_SERVER_SERVICE_DOWN);
        Sd_ClientServiceSetState(0, SD_CLIENT_SERVICE_RELEASED);
        Sd_ConsumedEventGroupSetState(0, SD_CONSUMED_EVENTGROUP_RELEASED);
        avtive = FALSE;
      }
    }
#endif
  }

  return NULL;
}

static void __attribute__((constructor)) _key_mgr_start(void) {
  pthread_create(&lThread, NULL, KeyMonitorThread, NULL);
}
/* ================================ [ FUNCTIONS ] ============================================== */
