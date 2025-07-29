# SSAS - Simple Smart Automotive Software
# Copyright (C) 2024 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_OsekNm(cfg, dir):
    H = open("%s/OsekNm_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef OSEKNM_CFG_H\n")
    H.write("#define OSEKNM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef OSEKNM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define OSEKNM_MAIN_FUNCTION_PERIOD %s\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define OSEKNM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + OSEKNM_MAIN_FUNCTION_PERIOD - 1) / OSEKNM_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* OSEKNM_CFG_H */\n")
    H.close()

    C = open("%s/OsekNm_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "OsekNm.h"\n')
    C.write('#include "OsekNm_Cfg.h"\n')
    C.write('#include "OsekNm_Priv.h"\n')
    C.write('#include "CanIf.h"\n')
    C.write('#include "Nm.h"\n')
    C.write("#ifdef USE_CANIF\n")
    C.write('#include "CanIf_Cfg.h"\n')
    C.write("#endif\n")
    C.write("#ifdef _WIN32\n")
    C.write("#include <stdlib.h>\n")
    C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("#ifdef _WIN32\n")
    C.write("#define L_CONST\n")
    C.write("#else\n")
    C.write("#define L_CONST const\n")
    C.write("#endif\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("static L_CONST OsekNm_ChannelConfigType OsekNm_ChannelConfigs[] = {\n")
    for idx, network in enumerate(cfg["networks"]):
        C.write("  {\n")
        C.write("    OSEKNM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* tTx */\n" % (network["tTx"]))
        C.write("    OSEKNM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* tTyp */\n" % (network["tTyp"]))
        C.write("    OSEKNM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* tMax */\n" % (network["tMax"]))
        C.write("    OSEKNM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* tError */\n" % (network["tError"]))
        C.write("    OSEKNM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* tWbs */\n" % (network["tWbs"]))
        C.write("    %s, /* NodeMask */\n" % (network["NodeMask"]))
        C.write("    %s, /* NodeId */\n" % (network["NodeId"]))
        C.write("    %s, /* rx_limit */\n" % (network["rx_limit"]))
        C.write("    %s, /* tx_limit */\n" % (network["tx_limit"]))
        C.write("    #ifdef USE_CANIF\n")
        C.write("    CANIF_%s_TX, /* txPduId */\n" % (network["name"]))
        C.write("    #else\n")
        C.write("    %s, /* txPduId */\n" % (idx))
        C.write("    #endif\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static OsekNm_ChannelContextType OsekNm_ChannelContexts[ARRAY_SIZE(OsekNm_ChannelConfigs)];\n\n")
    C.write("const OsekNm_ConfigType OsekNm_Config = {\n")
    C.write("  OsekNm_ChannelConfigs,\n")
    C.write("  OsekNm_ChannelContexts,\n")
    C.write("  ARRAY_SIZE(OsekNm_ChannelConfigs),\n")
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write(
        """#ifdef _WIN32
static void __attribute__((constructor)) _oseknm_start(void) {
  char *nodeStr = getenv("OSEKNM_NODE_ID");
  if (nodeStr != NULL) {
    OsekNm_ChannelConfigs[0].NodeId = atoi(nodeStr);
#ifdef USE_CANIF
    CanIf_SetDynamicTxId(CANIF_%s_TX, 0x400 + OsekNm_ChannelConfigs[0].NodeId);
#endif
  }
}
#endif\n"""
        % (cfg["networks"][0]["name"])
    )
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write("""
void OsekNm_D_Init(NetworkHandleType NetId, OsekNm_RoutineRefType Routine) {
}

void OsekNm_D_Offline(NetworkHandleType NetId) {
#ifdef USE_NM
  Nm_PrepareBusSleepMode(NetId);
#endif
}

void OsekNm_D_Online(NetworkHandleType NetId) {
#ifdef USE_NM
  Nm_NetworkMode(NetId);
#endif
}

#ifndef USE_CANIF
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  if (0x400 == (Mailbox->CanId & 0xFFFFFF00)) {
    OsekNm_RxIndication(Mailbox->ControllerId, PduInfoPtr);
  }
}

void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  OsekNm_TxConfirmation((NetworkHandleType)CanTxPduId, E_OK);
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Can_PduType canPdu;

  canPdu.swPduHandle = TxPduId;
  canPdu.length = PduInfoPtr->SduLength;
  canPdu.sdu = PduInfoPtr->SduDataPtr;
  canPdu.id = 0x400 + OsekNm_ChannelConfigs[0].NodeId;
  return Can_Write(0, &canPdu);
}
#endif
\n""")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_OsekNm(cfg, dir)
    return ["%s/OsekNm_Cfg.c" % (dir)]
