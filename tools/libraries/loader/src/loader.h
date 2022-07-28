/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LOADER_H
#define LOADER_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "srec.h"
#include "isotp.h"
#include "stdlib.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define L_LOG_DEBUG 0
#define L_LOG_INFO 1
#define L_LOG_WARNING 2
#define L_LOG_ERROR 3
/* ================================ [ TYPES     ] ============================================== */
typedef struct loader_s loader_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
loader_t *loader_create(isotp_t *isotp, srec_t *appSRec, srec_t *flsSRec);
void loader_set_log_level(loader_t *loader, int level);
int loader_poll(loader_t *loader, int *progress, char **msg);
void loader_destory(loader_t *loader);
#ifdef __cplusplus
}
#endif
#endif /* LOADER_H */