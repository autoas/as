/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef __CANIF_CFG_H
#define __CANIF_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTP_MAX_CHANNELS
#define CANTP_MAX_CHANNELS 32
#endif

#ifndef J1939TP_MAX_CHANNELS
#define J1939TP_MAX_CHANNELS 32
#endif

#ifndef CANIF_MAIN_FUNCTION_PERIOD
#define CANIF_MAIN_FUNCTION_PERIOD 10
#endif

#ifndef CANIF_RX_PACKET_POOL_SIZE
#define CANIF_RX_PACKET_POOL_SIZE 0u
#endif

#ifndef CANIF_TX_PACKET_POOL_SIZE
#define CANIF_TX_PACKET_POOL_SIZE 0u
#endif

#ifndef CANIF_RX_PACKET_DATA_SIZE
#define CANIF_RX_PACKET_DATA_SIZE 64u
#endif

#ifndef CANIF_TX_PACKET_DATA_SIZE
#define CANIF_TX_PACKET_DATA_SIZE 64u
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* __CANIF_CFG_H */
