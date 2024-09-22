/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "app.h"
#include "Std_Timer.h"
#include "Std_Debug.h"
#include "Nm.h"
#include "CanNm.h"
#include "Com.h"
#if defined(USE_COM) && defined(_WIN32)
#include "Com_Cfg.h"
#include <time.h>
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_APP 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void User_Init(void);
extern void User_MainTask10ms(void);
extern void SomeIp_MainAppTask(void);
/* ================================ [ DATAS     ] ============================================== */
static Std_TimerType timer10ms;
static Std_TimerType timer1s;
/* ================================ [ LOCALS    ] ============================================== */
#if defined(USE_COM) && defined(_WIN32)
static void appUpdateComTxMsg(void) {
  uint32_t year = 2021;
  uint8_t month = 7;
  uint8_t day = 21;
  uint8_t hour = 21;
  uint8_t minute = 47;
  uint8_t second = 0;
#if defined(_WIN32)
  time_t t = time(0);
  struct tm *lt = localtime(&t);
  year = (1900 + lt->tm_year);
  month = lt->tm_mon + 1;
  day = lt->tm_mday;
  hour = lt->tm_hour;
  minute = lt->tm_min;
  second = lt->tm_sec;
#endif

  Com_SendSignal(COM_SID_year, &year);
  Com_SendSignal(COM_SID_month, &month);
  Com_SendSignal(COM_SID_day, &day);
  Com_SendSignal(COM_SID_hour, &hour);
  Com_SendSignal(COM_SID_minute, &minute);
  Com_SendSignal(COM_SID_second, &second);
  Com_SendSignalGroup(COM_GID_SystemTime);
}
static void appCheckComRxMsg(void) {
  uint16_t VehicleSpeed, TachoSpeed;
  uint8_t Led1Sts, Led2Sts, Led3Sts;

  Com_ReceiveSignal(COM_SID_VehicleSpeed, &VehicleSpeed);
  Com_ReceiveSignal(COM_SID_TachoSpeed, &TachoSpeed);
  Com_ReceiveSignal(COM_SID_Led1Sts, &Led1Sts);
  Com_ReceiveSignal(COM_SID_Led2Sts, &Led2Sts);
  Com_ReceiveSignal(COM_SID_Led3Sts, &Led3Sts);

  ASLOG(APP, ("VehicleSpeed=%d, TachoSpeed=%d, Led1Sts=%d, Led2Sts=%d, Led3Sts=%d\n", VehicleSpeed,
              TachoSpeed, Led1Sts, Led2Sts, Led3Sts));
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
void App_Init(void) {
  User_Init();
  Std_TimerStart(&timer10ms);
  Std_TimerStart(&timer1s);
}

void App_MainFunction(void) {
  if (Std_GetTimerElapsedTime(&timer10ms) >= 10000) {
    User_MainTask10ms();
    Std_TimerStart(&timer10ms);
  }

  if (Std_GetTimerElapsedTime(&timer1s) >= 1000000) {
    Std_TimerStart(&timer1s);
#if defined(USE_COM) && defined(_WIN32)
    appUpdateComTxMsg();
    appCheckComRxMsg();
#endif
#ifdef USE_SOMEIP
    SomeIp_MainAppTask();
#endif
  }
}

#if !defined(USE_NM) && (defined(USE_CANNM) || defined(USE_UDPNM))
void Nm_NetworkStartIndication(NetworkHandleType nmNetworkHandle) {
#ifdef USE_CANNM
  CanNm_PassiveStartUp(nmNetworkHandle);
#endif
}
void Nm_NetworkMode(NetworkHandleType nmNetworkHandle) {
#ifdef USE_CANNM
  CanNm_ConfirmPnAvailability(0);
#ifdef USE_COM
  Com_IpduGroupStart(0, TRUE);
#endif
#endif
}
void Nm_BusSleepMode(NetworkHandleType nmNetworkHandle) {
}
void Nm_PrepareBusSleepMode(NetworkHandleType nmNetworkHandle) {
#ifdef USE_CANNM
#ifdef USE_COM
  Com_IpduGroupStop(0);
#endif
#endif
}
void Nm_TxTimeoutException(NetworkHandleType nmNetworkHandle) {
  ASLOG(ERROR, ("Nm_TxTimeoutException(%d)\n", nmNetworkHandle));
}

void Nm_RepeatMessageIndication(NetworkHandleType nmNetworkHandle) {
}

void Nm_RemoteSleepIndication(NetworkHandleType nmNetworkHandle) {
  ASLOG(INFO, ("%d: NM Remote Sleep Ind\n", nmNetworkHandle));
}

void Nm_RemoteSleepCancellation(NetworkHandleType nmNetworkHandle) {
  ASLOG(INFO, ("%d: NM Remote Sleep Cancel\n", nmNetworkHandle));
}

void Nm_CoordReadyToSleepIndication(NetworkHandleType nmChannelHandle) {
}

void Nm_CoordReadyToSleepCancellation(NetworkHandleType nmChannelHandle) {
}
#endif
