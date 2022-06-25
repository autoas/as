/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
#ifndef _OS_AL_H_
#define _OS_AL_H_
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef void (*osal_thread_entry_t)(void *args);

typedef void* osal_thread_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
osal_thread_t osal_thread_create(osal_thread_entry_t entry, void *args);
int osal_thread_join(osal_thread_t thread);
void osal_usleep(uint32_t us);
void osal_start(void);
#ifdef __cplusplus
}
#endif
#endif /* _OS_AL_H_ */
