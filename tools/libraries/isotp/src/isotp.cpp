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
  extern IsoTp_Type *IsoTp_##x##Create(IsoTp_ParameterType *params);                               \
  extern int IsoTp_##x##Transmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize,        \
                                 uint8_t *rxBuffer, size_t rxSize);                                \
  extern int IsoTp_##x##Receive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize);              \
  extern int IsoTp_##x##IoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size);          \
  extern void IsoTp_##x##Destroy(IsoTp_Type *isotp)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
ISOTP_DECLARE_API(Can);
ISOTP_DECLARE_API(CanV2);
ISOTP_DECLARE_API(Lin);
ISOTP_DECLARE_API(DoIp);
ISOTP_DECLARE_API(J1939Tp);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
IsoTp_Type *IsoTp_Create(IsoTp_ParameterType *params) {
  IsoTp_Type *isotp = NULL;

  switch (params->Protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == params->U.CAN.Version) {
      isotp = IsoTp_CanCreate(params);
    } else {
      isotp = IsoTp_CanV2Create(params);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    isotp = IsoTp_LinCreate(params);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    isotp = IsoTp_J1939TpCreate(params);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    isotp = IsoTp_DoIpCreate(params);
    break;
#endif
  default:
    break;
  }

  return isotp;
}

int IsoTp_Transmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                   size_t rxSize) {
  int r = -1;

  STD_TOPIC_UDS(&isotp->Params, FALSE, (uint32_t)txSize, txBuffer);
  switch (isotp->Params.Protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->Params.U.CAN.Version) {
      r = IsoTp_CanTransmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    } else {
      r = IsoTp_CanV2Transmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    r = IsoTp_LinTransmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    r = IsoTp_J1939TpTransmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    r = IsoTp_DoIpTransmit(isotp, txBuffer, txSize, rxBuffer, rxSize);
    break;
#endif
  default:
    break;
  }

#ifdef USE_STD_TOPIC
  if ((r > 0) && (rxBuffer != NULL)) {
    STD_TOPIC_UDS(&isotp->Params, TRUE, (uint32_t)r, rxBuffer);
  }
#endif

  return r;
}

int IsoTp_Receive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize) {
  int r = -1;
  switch (isotp->Params.Protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->Params.U.CAN.Version) {
      r = IsoTp_CanReceive(isotp, rxBuffer, rxSize);
    } else {
      r = IsoTp_CanV2Receive(isotp, rxBuffer, rxSize);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    r = IsoTp_LinReceive(isotp, rxBuffer, rxSize);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    r = IsoTp_J1939TpReceive(isotp, rxBuffer, rxSize);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    r = IsoTp_DoIpReceive(isotp, rxBuffer, rxSize);
    break;
#endif
  default:
    break;
  }
#ifdef USE_STD_TOPIC
  if (r > 0) {
    STD_TOPIC_UDS(&isotp->Params, TRUE, (uint32_t)r, rxBuffer);
  }
#endif
  return r;
}

int IsoTp_IoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size) {
  int r = -1;
  switch (isotp->Params.Protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->Params.U.CAN.Version) {
      r = IsoTp_CanIoCtl(isotp, cmd, data, size);
    } else {
      r = IsoTp_CanV2IoCtl(isotp, cmd, data, size);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    r = IsoTp_LinIoCtl(isotp, cmd, data, size);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    r = IsoTp_J1939TpIoCtl(isotp, cmd, data, size);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    r = IsoTp_DoIpIoCtl(isotp, cmd, data, size);
    break;
#endif
  default:
    break;
  }
  return r;
}

void IsoTp_Destroy(IsoTp_Type *isotp) {
  switch (isotp->Params.Protocol) {
  case ISOTP_OVER_CAN:
    if (ISOTP_CAN_V1 == isotp->Params.U.CAN.Version) {
      IsoTp_CanDestroy(isotp);
    } else {
      IsoTp_CanV2Destroy(isotp);
    }
    break;
#ifdef USE_LINTP
  case ISOTP_OVER_LIN:
    IsoTp_LinDestroy(isotp);
    break;
#endif
  case ISOTP_OVER_J1939TP:
    IsoTp_J1939TpDestroy(isotp);
    break;
#ifdef USE_DOIP_CLIENT
  case ISOTP_OVER_DOIP:
    IsoTp_DoIpDestroy(isotp);
    break;
#endif
  default:
    break;
  }
}
