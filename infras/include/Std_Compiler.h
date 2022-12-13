/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _STD_COMPILER_H
#define _STD_COMPILER_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#if defined(__GNUC__)
#elif defined(__CWCC__)
#elif defined(__DCC__)
#elif defined(__ICCHCS12__)
#elif defined(__ICCARM__)
#elif defined(__HIWARE__)
#else
#define __weak
#define __naked
#endif

#ifndef __weak
# define __weak			__attribute__((weak))
#endif

#ifndef __naked
# define __naked			__attribute__((naked))
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _STD_COMPILER_H */
