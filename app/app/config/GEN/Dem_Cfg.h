/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Generated at Fri Jul 30 09:13:24 2021
 */
#ifndef DEM_CFG_H
#define DEM_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define DEM_USE_NVM
#define DEM_MAX_FREEZE_FRAME_NUMBER 2
#define DEM_MAX_FREEZE_FRAME_DATA_SIZE 13
#define DTC_ENVENT_NUM 5
#ifndef DEM_MAX_FREEZE_FRAME_RECORD
#define DEM_MAX_FREEZE_FRAME_RECORD DTC_ENVENT_NUM
#endif
#define DEM_EVENT_ID_DTC0 0
#define DEM_EVENT_ID_DTC1 1
#define DEM_EVENT_ID_DTC2 2
#define DEM_EVENT_ID_DTC3 3
#define DEM_EVENT_ID_DTC4 4
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* DEM_CFG_H */
