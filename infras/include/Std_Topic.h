/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef _STD_TOPIC_H_
#define _STD_TOPIC_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifdef USE_STD_TOPIC
#define STD_TOPIC_ISOTP(cid, isRx, id, dlc, data) Std_TopicIsoTpPut(cid, isRx, id, dlc, data)
#define STD_TOPIC_UDS(isotp, isRx, dlc, data) Std_TopicUdsPut(isotp, isRx, dlc, data)
#define STD_TOPIC_CAN(busid, isRx, canid, dlc, data) Std_TopicCanPut(busid, isRx, canid, dlc, data)
#define STD_TOPIC_LIN(busid, isRx, id, dlc, data) Std_TopicLinPut(busid, isRx, id, dlc, data)
#else
#define STD_TOPIC_ISOTP(cid, isRx, id, dlc, data)
#define STD_TOPIC_UDS(isotp, isRx, dlc, data)
#define STD_TOPIC_CAN(busid, isRx, canid, dlc, data)
#define STD_TOPIC_LIN(busid, isRx, id, dlc, data)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Std_TopicIsoTpPut(uint8_t channleId, int isRx, uint32_t id, uint32_t dlc, const uint8_t *data);
void Std_TopicUdsPut(void *isotp, int isRx, uint32_t dlc, const uint8_t *data);
void Std_TopicCanPut(int busid, int isRx, uint32_t canid, uint32_t dlc, const uint8_t *data);
void Std_TopicLinPut(int busid, int isRx, uint32_t id, uint32_t dlc, const uint8_t *data);
#ifdef __cplusplus
}
#endif
#endif /* _STD_TOPIC_H_ */
