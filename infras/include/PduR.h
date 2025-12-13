/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of PDU Router AUTOSAR CP Release 4.4.0
 */
#ifndef PDUR_H
#define PDUR_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_PduR_00100 */
#define PDUR_E_PDU_ID_INVALID 0x02
#define PDUR_E_ROUTING_PATH_GROUP_ID_INVALID 0x08
#define PDUR_E_PARAM_POINTER 0x09
/* ================================ [ TYPES     ] ============================================== */
typedef struct PduR_Config_s PduR_ConfigType;

/* @SWS_PduR_00654 */
typedef uint16_t PduR_RoutingPathGroupIdType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_PduR_00334 */
void PduR_Init(const PduR_ConfigType *ConfigPtr);

/* @SWS_PduR_00615 */
void PduR_EnableRouting(PduR_RoutingPathGroupIdType id);

/* @SWS_PduR_00617 */
void PduR_DisableRouting(PduR_RoutingPathGroupIdType id, boolean initialize);

/* @SWS_PduR_00338 */
void PduR_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* PDUR_H */
