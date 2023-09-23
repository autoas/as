/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.h"
#include "doip_client.h"
#include "Std_Debug.h"
#include "Log.hpp"
#include <string.h>
using namespace as;
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DOIP AS_LOG_INFO
#define AS_LOG_DOIPE AS_LOG_ERROR
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  doip_client_t *client;
  doip_node_t *node;
} isotp_doip_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
extern "C" isotp_t *isotp_doip_create(isotp_parameter_t *params) {
  int r = 0;
  isotp_t *isotp = (isotp_t *)malloc(sizeof(isotp_t));
  isotp_doip_t *doip = (isotp_doip_t *)malloc(sizeof(isotp_doip_t));

  if ((NULL != isotp) && (NULL != doip)) {
    memset(isotp, 0, sizeof(isotp_t));
    memset(doip, 0, sizeof(isotp_doip_t));
    isotp->priv = doip;
    isotp->params = *params;
  } else {
    r = -ENOMEM;
  }

  if (0 == r) {
    doip->client = doip_create_client(params->device, params->port);
    if (NULL == doip->client) {
      ASLOG(DOIPE, ("Failed to clreate doip client <%s:%d>\n", params->device, params->port));
      r = -__LINE__;
    }
  }

  if (0 == r) {
    r = doip_await_vehicle_announcement(doip->client, &doip->node, 1, 3000);

    if (r <= 0) {
      doip->node = doip_request(doip->client);
      if (NULL == doip->node) {
        r = -__LINE__;
        ASLOG(DOIPE, ("target is not reachable\n"));
      } else {
        r = 0;
      }
    } else {
      r = 0;
    }
  }

  if (0 == r) {
    ASLOG(DOIP, ("Found Vehicle:\n"));
    Log::hexdump(Logger::INFO, "  VIN: ", doip->node->VIN, 17, 17);
    Log::hexdump(Logger::INFO, "  EID: ", doip->node->EID, 6);
    Log::hexdump(Logger::INFO, "  GID: ", doip->node->GID, 6);
    ASLOG(DOIP, ("  LA: %X\n", doip->node->LA));
    r = doip_connect(doip->node);
    if (0 != r) {
      ASLOG(DOIPE, ("connect failed\n"));
    }
  }

  if (0 == r) {
    r = doip_activate(doip->node, isotp->params.U.DoIP.sourceAddress,
                      isotp->params.U.DoIP.activationType, NULL, 0);
    if (0 != r) {
      ASLOG(DOIPE, ("activate failed\n"));
    }
  }

  if (0 != r) {
    if (NULL != isotp) {
      free(isotp);
      isotp = NULL;
    }
    if (NULL != doip->client) {
      doip_destory_client(doip->client);
    }
    if (NULL != doip) {
      free(doip);
    }
  }

  return isotp;
}

extern "C" int isotp_doip_transmit(isotp_t *isotp, const uint8_t *txBuffer, size_t txSize,
                                   uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  isotp_doip_t *doip = (isotp_doip_t *)isotp->priv;
  r = doip_transmit(doip->node, isotp->params.U.DoIP.targetAddress, txBuffer, txSize, rxBuffer,
                    rxSize);
  return r;
}

extern "C" int isotp_doip_receive(isotp_t *isotp, uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  isotp_doip_t *doip = (isotp_doip_t *)isotp->priv;
  r = doip_receive(doip->node, rxBuffer, rxSize);
  return r;
}

extern "C" int isotp_doip_ioctl(isotp_t *isotp, int cmd, const void *data, size_t size) {
  return -EACCES;
}

extern "C" void isotp_doip_destory(isotp_t *isotp) {
  isotp_doip_t *doip = (isotp_doip_t *)isotp->priv;

  doip_destory_client(doip->client);
  free(doip);
  free(isotp);
}
