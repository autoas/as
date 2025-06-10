/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef ECUM_PRIV_H
#define ECUM_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "EcuM.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef void (*EcuM_DriverInitFncType)(const void *config);

typedef void (*EcuM_DriverMainFncType)(void);

/* @ECUC_EcuM_00110 */
typedef struct {
  EcuM_DriverInitFncType InitFnc;
  const void *config;
} EcuM_DriverInitItemType;

typedef struct {
  const EcuM_DriverInitItemType *DriverInitListZero;
  const EcuM_DriverInitItemType *DriverInitListOne;
  const EcuM_DriverInitItemType *DriverInitListTwo;
  const EcuM_DriverMainFncType *DriverMainList;
  const EcuM_DriverMainFncType *DriverMemMainList;
  const EcuM_DriverMainFncType *DriverMainFastList;
  uint16_t numOfZero;
  uint16_t numOfOne;
  uint16_t numOfTwo;
  uint16_t numOfMain;
  uint16_t numOfMainMem;
  uint16_t numOfMainFast;
  uint16_t TMinWakup; /* the minimum time to be kept in wake up state */
} EcuM_ConfigType;

typedef struct {
  EcuM_StateType state;
  boolean bRequestRun;
  boolean bRequestPostRun;
  uint16_t Twakeup; /* Timer to hold EcuM in wakeup */
} EcuM_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* ECUM_PRIV_H */
