/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LIN_CFG_H
#define LIN_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Lin.h"
#include "Std_Timer.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef LIN_CHANNEL_NUM
#define LIN_CHANNEL_NUM 4
#endif

#define LIN_USE_CTRL_AC_CONTEXT_TYPE

#ifndef LIN_MAX_DATA_SIZE
#define LIN_MAX_DATA_SIZE 64
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  LIN_STATE_IDLE,
  LIN_STATE_ONLINE,
  LIN_STATE_ONLY_HEADER_TRANSMITTING,
  LIN_STATE_HEADER_TRANSMITTING,
  LIN_STATE_FULL_TRANSMITTING,
  LIN_STATE_WAITING_RESPONSE,
  LIN_STATE_ONLY_HEADER_TRANSMITTED,
  LIN_STATE_RESPONSE_TRANSMITTED,
  LIN_STATE_FULL_TRANSMITTED,
  LIN_STATE_RESPONSE_RECEIVED,
  LIN_STATE_WAITING_DATA,
  LIN_STATE_DATA_TRANSMITTING
} Lin_StateType;

typedef struct {
  Lin_FrameCsModelType Cs;
  uint8_t type;
  Lin_FramePidType pid;
  uint8_t dlc;
  uint8_t data[LIN_MAX_DATA_SIZE];
  uint8_t checksum;
} Lin_FrameType;

typedef struct {
  int fd;
  Std_TimerType timer;
  Lin_FrameType frame;
  Lin_StateType state;
} Lin_CtrlAcContextType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* LIN_CFG_H */
