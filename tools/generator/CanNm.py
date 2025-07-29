# SSAS - Simple Smart Automotive Software
# Copyright (C) 2024 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_CanNm(cfg, dir):
    H = open("%s/CanNm_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef CANNM_CFG_H\n")
    H.write("#define CANNM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef CANNM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define CANNM_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define CANNM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + CANNM_MAIN_FUNCTION_PERIOD - 1u) / CANNM_MAIN_FUNCTION_PERIOD)\n\n")

    H.write("/* @ECUC_CanNm_00055 */\n")
    H.write("%s#define CANNM_REMOTE_SLEEP_IND_ENABLED\n\n" % ("" if cfg.get("RemoteSleepIndEnabled", True) else "// "))

    H.write("/* @ECUC_CanNm_00010 */\n")
    H.write("// #define CANNM_PASSIVE_MODE_ENABLED\n\n")

    H.write("/* @ECUC_CanNm_00080 */\n")
    H.write("%s#define CANNM_COORDINATOR_SYNC_SUPPORT\n\n" % ("" if cfg.get("CoordinatorSyncSupport", True) else "// "))

    H.write("/* @ECUC_CanNm_00086 */\n")
    H.write("%s#define CANNM_GLOBAL_PN_SUPPORT\n\n" % ("" if cfg.get("GlobalPnSupport", True) else "// "))

    H.write("%s#define CANNM_CAR_WAKEUP_SUPPORT\n\n" % ("" if cfg.get("CarWakeupSupport", True) else "// "))

    H.write("%s#define CANNM_PASSIVE_STARTUP_REPEAT_ENABLED\n\n" % ("" if cfg.get("PassiveStartupRepeatEnabled", False) else "// "))

    H.write("%s#define CANNM_REQUEST_BIT_AUTO_SET\n\n" % ("" if cfg.get("RequestBitAutoSet", False) else "// "))

    H.write("#if defined(USE_PDUR) && defined(USE_COM)\n")
    H.write("%s#define CANNM_COM_USER_DATA_SUPPORT\n" % ("" if cfg.get("ComUserDataSupport", True) else "// "))
    H.write("#define PDUR_CANNM_COM_ZERO_COST\n")
    H.write("#endif\n")
    H.write("%s#define CANNM_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* CANNM_CFG_H */\n")
    H.close()

    C = open("%s/CanNm_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "CanNm.h"\n')
    C.write('#include "CanNm_Cfg.h"\n')
    C.write('#include "CanNm_Priv.h"\n')
    C.write('#include "Std_Debug.h"\n')
    C.write('#include "CanIf.h"\n')
    C.write("#ifdef _WIN32\n")
    C.write("#include <stdlib.h>\n")
    C.write("#endif\n")
    C.write("#ifdef USE_CANIF\n")
    C.write('#include "CanIf_Cfg.h"\n')
    C.write("#endif\n")
    C.write("#ifdef USE_PDUR\n")
    C.write('#include "PduR_Cfg.h"\n')
    C.write("#endif\n")
    C.write("#ifdef USE_COM\n")
    C.write('#include "Com_Cfg.h"\n')
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
    C.write("#ifdef CANNM_GLOBAL_PN_SUPPORT\n")
    for idx, network in enumerate(cfg["networks"]):
        if network.get("PnEnabled", False):
            C.write("static const uint8_t nm%sPnFilterMaskByte[] = {%s};\n" % (idx, ", ".join(network["PnFilterMask"])))
    C.write("#endif\n")
    C.write("static L_CONST CanNm_ChannelConfigType CanNm_ChannelConfigs[] = {\n")
    for idx, network in enumerate(cfg["networks"]):
        C.write("  {\n")
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* ImmediateNmCycleTime */\n" % (network["ImmediateNmCycleTime"]))
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* MsgCycleOffset */\n" % (network["MsgCycleOffset"]))
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* MsgCycleTime */\n" % (network["MsgCycleTime"]))
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* MsgReducedTime */\n" % (network["MsgReducedTime"]))
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* MsgTimeoutTime */\n" % (network["MsgTimeoutTime"]))
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* RepeatMessageTime */\n" % (network["RepeatMessageTime"]))
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* NmTimeoutTime */\n" % (network["NmTimeoutTime"]))
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* WaitBusSleepTime */\n" % (network["WaitBusSleepTime"]))
        C.write("    #ifdef CANNM_REMOTE_SLEEP_IND_ENABLED\n")
        C.write("    CANNM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* RemoteSleepIndTime */\n" % (network["RemoteSleepIndTime"]))
        C.write("    #endif\n")
        C.write("    #ifdef CANNM_COM_USER_DATA_SUPPORT\n")
        C.write("    %s, /* UserDataTxPdu */\n" % (network["UserDataTxPdu"]))
        C.write("    #endif\n")
        C.write("    #ifdef USE_CANIF\n")
        C.write("    CANIF_%s_TX, /* TxPdu */\n" % (network["name"]))
        C.write("    #else\n")
        C.write("    %su, /* TxPdu */\n" % (idx))
        C.write("    #endif\n")
        C.write("    %su, /* NodeId */\n" % (network["NodeId"]))
        C.write("    %su, /* nmNetworkHandle */\n" % (idx))
        C.write("    %su, /* ImmediateNmTransmissions */\n" % (network["ImmediateNmTransmissions"]))
        C.write("    CANNM_PDU_BYTE_%s, /* PduCbvPosition */\n" % (network.get("PduCbvPosition", 1)))
        C.write("    CANNM_PDU_BYTE_%s, /* PduNidPosition */\n" % (network.get("PduNidPosition", 0)))
        C.write("    %s, /* ActiveWakeupBitEnabled */\n" % (str(network.get("ActiveWakeupBitEnabled", False)).upper()))
        C.write("    %s, /* PassiveModeEnabled */\n" % (str(network.get("PassiveModeEnabled", False)).upper()))
        C.write("    %s, /* RepeatMsgIndEnabled */\n" % (str(network.get("RepeatMsgIndEnabled", False)).upper()))
        C.write("    %s, /* NodeDetectionEnabled */\n" % (str(network.get("NodeDetectionEnabled", False)).upper()))
        C.write("    #ifdef CANNM_GLOBAL_PN_SUPPORT\n")
        C.write("    %s, /* PnEnabled */\n" % (str(network.get("PnEnabled", False)).upper()))
        C.write("    %s, /* AllNmMessagesKeepAwake */\n" % (str(network.get("AllNmMessagesKeepAwake", True)).upper()))
        if network.get("PnEnabled", False):
            C.write("    %su, /* PnInfoOffset */\n" % (network["PnInfoOffset"]))
            C.write("    sizeof(nm%sPnFilterMaskByte), /* PnInfoLength */\n" % (idx))
            C.write("    nm%sPnFilterMaskByte, /* PnFilterMaskByte */\n" % (idx))
        else:
            C.write("    %s, /* PnInfoOffset */\n" % (0))
            C.write("    %s, /* PnInfoLength */\n" % (0))
            C.write("    %s, /* PnFilterMaskByte */\n" % ("NULL"))
        C.write("    #endif\n")
        C.write("    #ifdef CANNM_CAR_WAKEUP_SUPPORT\n")
        C.write("    %s, /* CarWakeUpRxEnabled */\n" % (str(network.get("CarWakeUpRxEnabled", False)).upper()))
        if network.get("CarWakeUpRxEnabled", False):
            C.write("    %su, /* NodeMask */\n" % (network["NodeMask"]))
            C.write("    %su, /* CarWakeUpBytePosition */\n" % (network["CarWakeUpBytePosition"]))
            C.write("    %su, /* CarWakeUpBitPosition */\n" % (network["CarWakeUpBitPosition"]))
            C.write(
                "    %s, /* CarWakeUpFilterEnabled */\n" % (str(network.get("CarWakeUpFilterEnabled", False)).upper())
            )
            if network.get("CarWakeUpFilterEnabled", False):
                C.write("    %su, /* CarWakeUpFilterNodeId */\n" % (network["CarWakeUpFilterNodeId"]))
            else:
                C.write("    %su, /* CarWakeUpFilterNodeId */\n" % (0))
        else:
            C.write("    %s, /* NodeMask */\n" % (0))
            C.write("    %s, /* CarWakeUpBytePosition */\n" % (0))
            C.write("    %s, /* CarWakeUpBitPosition */\n" % (0))
            C.write("    %s, /* CarWakeUpFilterEnabled */\n" % ("FALSE"))
            C.write("    %s, /* CarWakeUpFilterNodeId */\n" % (0))
        C.write("    #endif\n")
        C.write("  },\n")
    C.write("};\n\n")

    C.write("static CanNm_ChannelContextType CanNm_ChannelContexts[ARRAY_SIZE(CanNm_ChannelConfigs)];\n\n")

    C.write("const CanNm_ConfigType CanNm_Config = {\n")
    C.write("  CanNm_ChannelConfigs,\n")
    C.write("  CanNm_ChannelContexts,\n")
    C.write("  ARRAY_SIZE(CanNm_ChannelConfigs),\n")
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write(
        """#ifdef _WIN32
static void __attribute__((constructor)) _cannm_start(void) {
  char *nodeStr = getenv("CANNM_NODE_ID");
  if (nodeStr != NULL) {
    CanNm_ChannelConfigs[0].NodeId = atoi(nodeStr);
    CanNm_ChannelConfigs[0].MsgReducedTime =
      CANNM_CONVERT_MS_TO_MAIN_CYCLES(500 + (CanNm_ChannelConfigs[0].NodeId * 100));
    ASLOG(INFO, ("CanNm NodeId=%d, ReduceTime=%d\\n", CanNm_ChannelConfigs[0].NodeId,
                 CanNm_ChannelConfigs[0].MsgReducedTime));
#ifdef USE_CANIF
    CanIf_SetDynamicTxId(CanNm_ChannelConfigs[0].TxPdu, 0x500 + CanNm_ChannelConfigs[0].NodeId);
#endif
  }
}
#endif\n"""
    )
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write(
        """#ifndef USE_CANIF
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr) {
  CanNm_RxIndication(Mailbox->ControllerId, PduInfoPtr);
}

void CanIf_TxConfirmation(PduIdType CanTxPduId) {
  CanNm_TxConfirmation(CanTxPduId, E_OK);
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  Can_PduType canPdu;

  canPdu.swPduHandle = TxPduId;
  canPdu.length = PduInfoPtr->SduLength;
  canPdu.sdu = PduInfoPtr->SduDataPtr;
  canPdu.id = 0x500 + CanNm_ChannelConfigs[0].NodeId;
  return Can_Write(0, &canPdu);
}
#endif\n\n"""
    )
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_CanNm(cfg, dir)
    return ["%s/CanNm_Cfg.c" % (dir)]
