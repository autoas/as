/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Default Error Tracer AUTOSAR CP R21-11
 */
#ifndef DET_H
#define DET_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#if defined(USE_DET) && defined(DET_THIS_MODULE_ID)
#define DET_VALIDATE(condition, ApiId, ErrorId, ret)                                               \
  do {                                                                                             \
    if (FALSE == (condition)) {                                                                    \
      Det_ReportError(DET_THIS_MODULE_ID, 0, ApiId, ErrorId);                                      \
      ret;                                                                                         \
    }                                                                                              \
  } while (0)
#else
#define DET_VALIDATE(condition, ApiId, ErrorId, ret)
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct Det_Config_s Det_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Det_00008 */
void Det_Init(const Det_ConfigType *ConfigPtr);

/* @SWS_Det_00009 */
Std_ReturnType Det_ReportError(uint16_t ModuleId, uint8_t InstanceId, uint8_t ApiId,
                               uint8_t ErrorId);

/* @SWS_Det_00010 */
void Det_Start(void);

/* @SWS_Det_01001 */
Std_ReturnType Det_ReportRuntimeError(uint16_t ModuleId, uint8_t InstanceId, uint8_t ApiId,
                                      uint8_t ErrorId);

/* @SWS_Det_01003 */
Std_ReturnType Det_ReportTransientFault(uint16_t ModuleId, uint8_t InstanceId, uint8_t ApiId,
                                        uint8_t FaultId);

/* @SWS_Det_00011 */
void Det_GetVersionInfo(Std_VersionInfoType *versioninfo);
#endif /* DET_H */
