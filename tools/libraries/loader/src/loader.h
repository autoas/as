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
/* ================================ [ TYPES     ] ============================================== */
typedef struct loader_s loader_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
loader_t *loader_create(isotp_t *isotp, srec_t *appSRec, srec_t *flsSRec);
int loader_poll(loader_t *loader, int *progress, char **msg);
void loader_destory(loader_t *loader);
#ifdef __cplusplus
}
#endif
#endif /* LOADER_H */
