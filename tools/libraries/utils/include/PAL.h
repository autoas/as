/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 * Platform Abstraction Layer
 */
#ifndef _PAL_H_
#define _PAL_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void *PAL_DlOpen(const char *path);
void *PAL_DlSym(void *dll, const char *symbol);
void PAL_DlClose(void *dll);

bool PAL_FileExists(const char *file);

/* get a system timestamp in us */
uint64_t PAL_Timestamp(void);
#ifdef __cplusplus
}
#endif
#endif /* _PAL_H_ */
