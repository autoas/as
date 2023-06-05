/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 - 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef STD_CRITICAL_H
#define STD_CRITICAL_H
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* spin lock need to be used for MCU with 2 or more CPUs */
#ifndef DISABLE_STD_CRITICAL_DEF
#define EnterCritical() do { imask_t imask = Std_EnterCritical()
#define InterLeaveCritical() Std_ExitCritical(imask)
#define InterEnterCritical() imask = Std_EnterCritical()
#define ExitCritical() Std_ExitCritical(imask); } while(0)
#endif


/* ================================ [ TYPES     ] ============================================== */
typedef unsigned int imask_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
imask_t Std_EnterCritical(void);
void Std_ExitCritical(imask_t);
#ifdef __cplusplus
}
#endif
#endif /* STD_CRITICAL_H */
