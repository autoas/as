/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.hpp"
#include "Std_Topic.h"
/* ================================ [ MACROS    ] ============================================== */
#define ISOTP_DECLARE_API(x)                                                                       \
  extern isotp_t *isotp_##x##_create(isotp_parameter_t *params);                                   \
  extern int isotp_##x##_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize,          \
                                  uint8_t *rxBuffer, size_t rxSize);                               \
  extern int isotp_##x##_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize);                \
  extern int isotp_##x##_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size);            \
  extern void isotp_##x##_destory(isotp_t *isotp)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
ISOTP_DECLARE_API(can);
ISOTP_DECLARE_API(can_v2);
ISOTP_DECLARE_API(lin);
ISOTP_DECLARE_API(doip);
ISOTP_DECLARE_API(j1939tp);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
isotp_t *isotp_create(isotp_parameter_t *params) {
  isotp_t *isotp = NULL;

  switch (params->protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == params->U.CAN.version) {
      isotp = isotp_can_create(params);
    } else {
      isotp = isotp_can_v2_create(params);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    isotp = isotp_lin_create(params);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    isotp = isotp_j1939tp_create(params);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    isotp = isotp_doip_create(params);
    break;
#endif
  default:
    break;
  }

  return isotp;
}

int isotp_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                   size_t rxSize) {
  int r = -1;

  STD_TOPIC_UDS(&isotp->params, FALSE, (uint32_t)txSize, txBuffer);
  switch (isotp->params.protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->params.U.CAN.version) {
      r = isotp_can_transmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    } else {
      r = isotp_can_v2_transmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    r = isotp_lin_transmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    r = isotp_j1939tp_transmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    r = isotp_doip_transmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    break;
#endif
  default:
    break;
  }

#ifdef USE_STD_TOPIC
  if ((r > 0) && (rxBuffer != NULL)) {
    STD_TOPIC_UDS(&isotp->params, TRUE, (uint32_t)r, rxBuffer);
  }
#endif

  return r;
}

int isotp_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize) {
  int r = -1;
  switch (isotp->params.protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->params.U.CAN.version) {
      r = isotp_can_receive(isotp, rxBuffer, rxSize);
    } else {
      r = isotp_can_v2_receive(isotp, rxBuffer, rxSize);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    r = isotp_lin_receive(isotp, rxBuffer, rxSize);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    r = isotp_j1939tp_receive(isotp, rxBuffer, rxSize);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    r = isotp_doip_receive(isotp, rxBuffer, rxSize);
    break;
#endif
  default:
    break;
  }
#ifdef USE_STD_TOPIC
  if (r > 0) {
    STD_TOPIC_UDS(&isotp->params, TRUE, (uint32_t)r, rxBuffer);
  }
#endif
  return r;
}

int isotp_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size) {
  int r = -1;
  switch (isotp->params.protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->params.U.CAN.version) {
      r = isotp_can_ioctl(isotp, cmd, data, size);
    } else {
      r = isotp_can_v2_ioctl(isotp, cmd, data, size);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    r = isotp_lin_ioctl(isotp, cmd, data, size);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    r = isotp_j1939tp_ioctl(isotp, cmd, data, size);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    r = isotp_doip_ioctl(isotp, cmd, data, size);
    break;
#endif
  default:
    break;
  }
  return r;
}

void isotp_destory(isotp_t *isotp) {
  switch (isotp->params.protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->params.U.CAN.version) {
      isotp_can_destory(isotp);
    } else {
      isotp_can_v2_destory(isotp);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    isotp_lin_destory(isotp);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    isotp_j1939tp_destory(isotp);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    isotp_doip_destory(isotp);
    break;
#endif
  default:
    break;
  }
}
