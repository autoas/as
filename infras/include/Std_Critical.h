/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef STD_CRITICAL_H
#define STD_CRITICAL_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define EnterCritical() do { imask_t imask = Std_EnterCritical()
#define ExitCritical() Std_ExitCritical(imask); } while(0)

/* ================================ [ TYPES     ] ============================================== */
typedef unsigned int imask_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
imask_t Std_EnterCritical(void);
void Std_ExitCritical(imask_t);
#endif /* STD_CRITICAL_H */
