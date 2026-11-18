/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef USE_DOIP_CLIENT
#include "isotp.h"
#include "isotp_types.hpp"
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
  DoIp_ClientType *Client;
  DoIp_NodeType *Node;
} IsoTp_DoIpContextType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
IsoTp_Type *IsoTp_DoIpCreate(IsoTp_ParameterType *params) {
  int r = 0;
  IsoTp_Type *isotp = new IsoTp_Type;
  IsoTp_DoIpContextType *doip = (IsoTp_DoIpContextType *)malloc(sizeof(IsoTp_DoIpContextType));

  if ((NULL != isotp) && (NULL != doip)) {
    memset(doip, 0, sizeof(IsoTp_DoIpContextType));
    isotp->Priv = doip;
    isotp->Params = *params;
  } else {
    r = -ENOMEM;
  }

  if (0 == r) {
    doip->Client = DoIp_CreateClient(params->Device, params->Port, NULL);
    if (NULL == doip->Client) {
      ASLOG(DOIPE, ("Failed to clreate doip client <%s:%d>\n", params->Device, params->Port));
      r = -__LINE__;
    }
  }

  if (0 == r) {
    r = DoIp_AwaitVehicleAnnouncement(doip->Client, &doip->Node, 1, 100);

    if (r <= 0) {
      doip->Node = DoIp_Request(doip->Client);
      if (NULL == doip->Node) {
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
    Log::HexDump(Logger::INFO, "  VIN: ", doip->Node->VIN, 17, 17);
    Log::HexDump(Logger::INFO, "  EID: ", doip->Node->EID, 6);
    Log::HexDump(Logger::INFO, "  GID: ", doip->Node->GID, 6);
    ASLOG(DOIP, ("  LA: %X\n", doip->Node->LA));
    r = DoIp_Connect(doip->Node);
    if (0 != r) {
      ASLOG(DOIPE, ("connect failed\n"));
    }
  }

  if (0 == r) {
    r = DoIp_Activate(doip->Node, isotp->Params.U.DoIP.SourceAddress,
                      isotp->Params.U.DoIP.ActivationType, NULL, 0);
    if (0 != r) {
      ASLOG(DOIPE, ("activate failed\n"));
    }
  }

  if (0 != r) {
    if (NULL != isotp) {
      delete isotp;
      isotp = NULL;
    }
    if (NULL != doip->Client) {
      DoIp_DestroyClient(doip->Client);
    }
    if (NULL != doip) {
      free(doip);
    }
  }

  return isotp;
}

int IsoTp_DoIpTransmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize, uint8_t *rxBuffer,
                       size_t rxSize) {
  int r = 0;
  IsoTp_DoIpContextType *doip = (IsoTp_DoIpContextType *)isotp->Priv;
  r = DoIp_Transmit(doip->Node, isotp->Params.U.DoIP.TargetAddress, txBuffer, txSize, rxBuffer,
                    rxSize);
  return r;
}

int IsoTp_DoIpReceive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  IsoTp_DoIpContextType *doip = (IsoTp_DoIpContextType *)isotp->Priv;
  r = DoIp_Receive(doip->Node, rxBuffer, rxSize);
  return r;
}

int IsoTp_DoIpIoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size) {
  return -EACCES;
}

void IsoTp_DoIpDestroy(IsoTp_Type *isotp) {
  IsoTp_DoIpContextType *doip = (IsoTp_DoIpContextType *)isotp->Priv;

  DoIp_DestroyClient(doip->Client);
  free(doip);
  delete isotp;
}
#endif
