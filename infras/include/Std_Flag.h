/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef STD_FLAG_H
#define STD_FLAG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Critical.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef USE_STD_FLAG_CRITICAL
#define sfEnterCritical() EnterCritical()
#define sfExitCritical() ExitCritical()
#else
#define sfEnterCritical()
#define sfExitCritical()
#endif

#define Std_IsFlagSet(flag, mask) ((mask) == ((flag) & (mask)))

#define Std_IsAnyFlagSet(flag, mask) (0 != ((flag) & (mask)))

#define Std_FlagSet(flag, mask)                                                                    \
  do {                                                                                             \
    sfEnterCritical();                                                                             \
    flag |= (mask);                                                                                \
    sfExitCritical();                                                                              \
  } while (0)

#define Std_FlagClear(flag, mask)                                                                  \
  do {                                                                                             \
    sfEnterCritical();                                                                             \
    flag &= ~(mask);                                                                               \
    sfExitCritical();                                                                              \
  } while (0)

#define Std_FlagClearAndSet(flag, maskClear, maskSet)                                              \
  do {                                                                                             \
    sfEnterCritical();                                                                             \
    flag &= ~(maskClear);                                                                          \
    flag |= (maskSet);                                                                             \
    sfExitCritical();                                                                              \
  } while (0)

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* STD_TIMER_H */