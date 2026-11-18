/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef LOADER_COMMON_H
#define LOADER_COMMON_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "loader.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int Loader_EnterExtendSession(Loader_Type *loader);
int Loader_ControlDtcSettingOff(Loader_Type *loader);
int Loader_ControlDtcSettingOn(Loader_Type *loader);
int Loader_CommunicationDisable(Loader_Type *loader);
int Loader_CommunicationEnable(Loader_Type *loader);
int Loader_EnterProgramSession(Loader_Type *loader);
int Loader_DownloadOneSection(Loader_Type *loader, Srec_BlockType *blk);
int Loader_DownloadApplication(Loader_Type *loader);
int Loader_DownloadFlashDriver(Loader_Type *loader);
int Loader_EcuReset(Loader_Type *loader);
int Loader_SecurityExtdsAccess(Loader_Type *loader);
int Loader_SecurityPrgsAccess(Loader_Type *loader);
#ifdef __cplusplus
}
#endif
#endif /* LOADER_COMMON_H */
