/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_StandardTypes.pdf
 */
/* @SWS_Std_00014 */
#ifndef STD_TYPES_H
#define STD_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include "Std_Compiler.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* @SWS_Std_00006 */
#ifndef STATUSTYPEDEFINED
#define STATUSTYPEDEFINED
#define E_OK 0x00u
typedef unsigned char StatusType; /* OSEK compliance */
#endif
#define E_NOT_OK 0x01u

/* @ SWS_Std_00007 */
#define STD_HIGH 0x01u /* Physical state 5V or 3.3V */
#define STD_LOW 0x00u  /* Physical state 0V */

/* @SWS_Std_00013 */
#define STD_ACTIVE 0x01u /* Logical state active */
#define STD_IDLE 0x00u   /* Logical state idle */

/* @SWS_Std_00010 */
#ifndef STD_ON
#define STD_ON 0x01u
#endif
#ifndef STD_OFF
#define STD_OFF 0x00u
#endif


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Std_00005 */
typedef uint8_t Std_ReturnType;

typedef uint8_t boolean;

typedef int8_t sint8_t;
typedef int16_t sint16_t;
typedef int32_t sint32_t;

typedef int8_t sint8;
typedef uint8_t uint8;
typedef int16_t sint16;
typedef uint16_t uint16;
typedef int32_t sint32;
typedef uint32_t uint32;

typedef uint_fast8_t uint8f;
typedef uint_fast16_t uint16f;
typedef uint_fast32_t uint32f;

typedef int_fast8_t sint8f;
typedef int_fast16_t sint16f;
typedef int_fast32_t sint32f;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* STD_TYPES_H */
