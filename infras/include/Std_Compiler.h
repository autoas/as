/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 * ref: AUTOSAR_SWS_CompilerAbstraction
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
#elif defined(__CC_ARM)
#else
#define __weak
#define __naked
#define __packed
#define __constructor
#endif

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define INITIALIZER2_(f, p)                                                                        \
  static void f(void);                                                                             \
  __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f;                                         \
  __pragma(comment(linker, "/include:" p #f "_")) static void f(void)
#ifdef _WIN64
#define INITIALIZER(f) INITIALIZER2_(f, "")
#else
#define INITIALIZER(f) INITIALIZER2_(f, "_")
#endif
#else
#define INITIALIZER(f)                                                                             \
  static void f(void) __attribute__((constructor));                                                \
  static void f(void)
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

#ifndef __constructor
#define __constructor __attribute__((constructor))
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

/* @SWS_COMPILER_00046 */
#ifndef AUTOMATIC
#define AUTOMATIC
#endif

/* @SWS_COMPILER_00059 */
#ifndef TYPEDEF
#define TYPEDEF
#endif

/* @SWS_COMPILER_00001 */
#ifndef FUNC
#define FUNC(rettype, memclass) memclass rettype
#endif

/* @SWS_COMPILER_00023 */
#ifndef CONSTANT
#define CONSTANT(consttype, memclass) const consttype memclass
#endif

/* @SWS_COMPILER_00013 */
#ifndef P2CONST
#define P2CONST(ptrtype, memclass, ptrclass) const ptrtype memclass *ptrclass
#endif

/* @SWS_COMPILER_00031 */
#ifndef CONSTP2VAR
#define CONSTP2VAR(ptrtype, memclass, ptrclass) ptrtype memclass *const ptrclass
#endif

/* @SWS_COMPILER_00032 */
#ifndef CONSTP2CONST
#define CONSTP2CONST(ptrtype, memclass, ptrclass) const ptrtype memclass *const ptrclass
#endif

/* @SWS_COMPILER_00026 */
#ifndef VAR
#define VAR(vartype, memclass) memclass vartype
#endif

/* @SWS_COMPILER_00006 */
#ifndef P2VAR
#define P2VAR(ptrtype, memclass, ptrclass) ptrtype memclass *ptrclass
#endif

/* @SWS_COMPILER_00039 */
#ifndef P2FUNC
#define P2FUNC(rettype, ptrclass, fctname) rettype(*ptrclass fctname)
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
