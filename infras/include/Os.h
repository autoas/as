/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Operating System AUTOSAR CP Release 4.4.0
 */
#ifndef OS_H
#define OS_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define DONOTCARE ((AppModeType)0)
#define OSDEFAULTAPPMODE ((AppModeType)1)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Os_91007 */
typedef uint32 AppModeType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void StartOS(AppModeType Mode);
#endif /* OS_H */
