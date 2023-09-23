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
#else
#define __weak
#define __naked
#define __packed
#endif

#if defined(__HIWARE__)
#define inline
#endif

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#ifndef __naked
#define __naked __attribute__((naked))
#endif

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#ifndef __offsetof
#ifdef __compiler_offsetof
#define __offsetof(TYPE, MEMBER) __compiler_offsetof(TYPE, MEMBER)
#else
#define __offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#endif
#endif

#ifndef __containerof
#define __containerof(x, s, m) ((s *)((const volatile char *)(x)-__offsetof(s, m)))
#endif
/* ================================ [ TYPES     ] ============================================== */
#if defined(__HIWARE__) /* for mc9s12 */
typedef void *__far void_ptr_t;
#else
typedef void *void_ptr_t;
#endif
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _STD_COMPILER_H */
