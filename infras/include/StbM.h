/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/standards/R20-11/CP/AUTOSAR_SWS_SynchronizedTimeBaseManager.pdf
 */
#ifndef __STB_M_H__
#define __STB_M_H__
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define STBM_STATUS_TIMEOUT ((StbM_TimeBaseStatusType)0x01)
#define STBM_STATUS_SYNC_TO_GATEWAY ((StbM_TimeBaseStatusType)0x04)
#define STBM_STATUS_GLOBAL_TIME_BASE ((StbM_TimeBaseStatusType)0x08)
#define STBM_STATUS_TIMELEAP_FUTURE ((StbM_TimeBaseStatusType)0x10)
#define STBM_STATUS_TIMELEAP_PAST ((StbM_TimeBaseStatusType)0x20)
/* ================================ [ TYPES     ] ============================================== */
typedef struct StbM_Config_s StbM_ConfigType;

/* @SWS_StbM_91003 */
typedef struct {
  uint32_t nanosecondsHi;
  uint32_t nanosecondsLo;
} StbM_VirtualLocalTimeType;

/* @SWS_StbM_00384 */
typedef struct {
  uint32_t pathDelay;
} StbM_MeasurementType;

/* @SWS_StbM_00142 */
typedef uint16_t StbM_SynchronizedTimeBaseType;

/* @SWS_StbM_00239 */
typedef uint8_t StbM_TimeBaseStatusType;

/* @SWS_StbM_00241 */
typedef struct {
  uint32_t secondsHi;
  uint32_t seconds;
  uint32_t nanoseconds;
  StbM_TimeBaseStatusType timeBaseStatus;
} StbM_TimeStampType;

/* @SWS_StbM_00243 */
typedef struct {
  uint8_t userDataLength; /* range 0 - 3 */
  uint8_t userByte0;
  uint8_t userByte1;
  uint8_t userByte2;
} StbM_UserDataType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_StbM_00052 */
void StbM_Init(const StbM_ConfigType *ConfigPtr);

/* @SWS_StbM_91006 */
Std_ReturnType StbM_GetCurrentVirtualLocalTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                               StbM_VirtualLocalTimeType *localTimePtr);

/* @SWS_StbM_91005 */
Std_ReturnType StbM_BusGetCurrentTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                      StbM_TimeStampType *globalTimePtr,
                                      StbM_VirtualLocalTimeType *localTimePtr,
                                      StbM_UserDataType *userData);

/* @SWS_StbM_00233 */
Std_ReturnType StbM_BusSetGlobalTime(StbM_SynchronizedTimeBaseType timeBaseId,
                                     const StbM_TimeStampType *globalTimePtr,
                                     const StbM_UserDataType *userDataPtr,
                                     const StbM_MeasurementType *measureDataPtr,
                                     const StbM_VirtualLocalTimeType *localTimePtr);

/* @SWS_StbM_00223 */
Std_ReturnType StbM_SetOffset(StbM_SynchronizedTimeBaseType timeBaseId,
                              const StbM_TimeStampType *timeStamp,
                              const StbM_UserDataType *userData);

/* @SWS_StbM_00228 */
Std_ReturnType StbM_GetOffset(StbM_SynchronizedTimeBaseType timeBaseId,
                              StbM_TimeStampType *timeStamp, StbM_UserDataType *userData);

/* @SWS_StbM_00347 */
uint8_t StbM_GetTimeBaseUpdateCounter(StbM_SynchronizedTimeBaseType timeBaseId);
#endif /* __STB_M_H__ */
