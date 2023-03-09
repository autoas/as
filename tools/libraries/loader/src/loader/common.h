/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef __LOADER_COMMON_H
#define __LOADER_COMMON_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "loader.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int enter_extend_session(loader_t *loader);
int control_dtc_setting_off(loader_t *loader);
int control_dtc_setting_on(loader_t *loader);
int communicaiton_disable(loader_t *loader);
int communicaiton_enable(loader_t *loader);
int enter_program_session(loader_t *loader);
int download_one_section(loader_t *loader, sblk_t *blk);
int download_application(loader_t *loader);
int download_flash_driver(loader_t *loader);
int ecu_reset(loader_t *loader);
int security_extds_access(loader_t *loader);
int security_prgs_access(loader_t *loader);
#endif /* __LOADER_COMMON_H */
