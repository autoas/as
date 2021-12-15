/**
 * DLL Config - Data Link Layer Configuration of LVDS project
 * Copyright (C) 2021  Parai Wang <parai@foxmail.com>
 */
#ifndef _DLL_CONFIG_H_
#define _DLL_CONFIG_H_
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#define DLL_MAX_DATA_LENGHT 90

/* @TM_Protocol.0212(1) */
#define WRITE_FRAME_LEN_MAX 90
/* @TM_Protocol.0213(1) */
#define READ_FRAME_LEN_MAX 65

/* @TM_Protocol.0009(1) */
#define DLL_V_SYNC 0x79

#ifdef _WIN32
/* my windows LIN Bus simulator real time ability is too bad */
#define DLL_TIMIEOUT_US 100000
#else
#define DLL_TIMIEOUT_US 10000
#endif

#define DLL_CHL0 0

#define DLL_CHL0_SCH_TABLE0 0

//#define DLL_USE_RINGBUFFER
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _DLL_CONFIG_H_ */
