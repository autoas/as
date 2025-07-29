# SSAS - Simple Smart Automotive Software
# Copyright (C) 2024 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_J1939Tp(cfg, dir):
    H = open("%s/J1939Tp_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef J1939TP_CFG_H\n")
    H.write("#define J1939TP_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    TxId = 0
    TxPdusInfo = []
    RxId = 0
    RxPdusInfo = []
    for i, chl in enumerate(cfg.get("TxChannels", [])):
        name = chl["name"]
        H.write("#define J1939TP_CHL_%s_TX %su\n" % (name, i))
        H.write("#define J1939TP_%s_TX %su\n\n" % (name, i))
        H.write("#define J1939TP_%s_CM_TX %su\n" % (name, TxId))
        H.write("#define J1939TP_%s_DT_TX %su\n" % (name, TxId + 1))
        H.write("#define J1939TP_%s_DIRECT_TX %su\n" % (name, TxId + 2))
        TxPdusInfo.append({"TxId": TxId, "chl": chl, "type": "CM", "ctype": "TX"})
        TxPdusInfo.append({"TxId": TxId + 1, "chl": chl, "type": "DT", "ctype": "TX"})
        TxPdusInfo.append({"TxId": TxId + 2, "chl": chl, "type": "DIRECT", "ctype": "TX"})
        TxId = TxId + 3
        protocol = chl["Protocol"]
        if protocol == "CMDT":
            H.write("#define J1939TP_%s_CM_RX %su\n" % (name, RxId))
            RxPdusInfo.append({"RxId": RxId, "chl": chl, "type": "CM", "ctype": "TX"})
            RxId = RxId + 1
        H.write("\n")
    for i, chl in enumerate(cfg.get("RxChannels", [])):
        name = chl["name"]
        H.write("\n#define J1939TP_CHL_%s_RX %su\n\n" % (name, i))
        H.write("\n#define J1939TP_%s_RX %su\n\n" % (name, i))
        H.write("#define J1939TP_%s_CM_RX %su\n" % (name, RxId))
        H.write("#define J1939TP_%s_DT_RX %su\n" % (name, RxId + 1))
        H.write("#define J1939TP_%s_DIRECT_RX %su\n" % (name, RxId + 2))
        RxPdusInfo.append({"RxId": RxId, "chl": chl, "type": "CM", "ctype": "RX"})
        RxPdusInfo.append({"RxId": RxId + 1, "chl": chl, "type": "DT", "ctype": "RX"})
        RxPdusInfo.append({"RxId": RxId + 2, "chl": chl, "type": "DIRECT", "ctype": "RX"})
        RxId = RxId + 3
        protocol = chl["Protocol"]
        if protocol == "CMDT":
            H.write("#define J1939TP_%s_CM_TX %su\n" % (name, TxId))
            TxPdusInfo.append({"TxId": TxId, "chl": chl, "type": "CM", "ctype": "RX"})
            TxId = TxId + 1
        H.write("\n")
    H.write("\n")
    H.write("#ifndef J1939TP_MAIN_FUNCTION_PERIOD\n")
    H.write("#define J1939TP_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define J1939TP_CONVERT_MS_TO_MAIN_CYCLES(x)  \\\n")
    H.write("  ((x + J1939TP_MAIN_FUNCTION_PERIOD - 1u) / J1939TP_MAIN_FUNCTION_PERIOD)\n")
    H.write("%s#define J1939TP_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* J1939TP_CFG_H */\n")
    H.close()

    C = open("%s/J1939Tp_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "CanIf_Cfg.h"\n')
    C.write('#include "PduR_Cfg.h"\n')
    C.write('#include "J1939Tp_Cfg.h"\n')
    C.write('#include "J1939Tp.h"\n')
    C.write('#include "J1939Tp_Priv.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("#ifndef J1939TP_LL_DL\n")
    C.write("#define J1939TP_LL_DL 8\n")
    C.write("#endif\n\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    numRxChls = len(cfg.get("RxChannels", []))
    numTxChls = len(cfg.get("TxChannels", []))
    if numRxChls > 0:
        C.write("static J1939Tp_RxChannelContextType J1939Tp_RxChannelContexts[%s];\n" % (numRxChls))
        for chl in cfg["RxChannels"]:
            C.write("static uint8_t u8Rx%sData[%s];\n" % (chl["name"], chl.get("LL_DL", "J1939TP_LL_DL")))
        C.write("static const J1939Tp_RxChannelType J1939Tp_RxChannels[] = {\n")
        for i, chl in enumerate(cfg["RxChannels"]):
            name = chl["name"]
            C.write("  { /* %s */\n" % (name))
            C.write("    &J1939Tp_RxChannelContexts[%s],\n" % (i))
            C.write("    u8Rx%sData,\n" % (chl["name"]))
            C.write("    /* PduR_RxPduId */ PDUR_%s_RX,\n" % (name))
            protocol = chl["Protocol"]
            if protocol == "CMDT":
                C.write("    /* CanIf_TxCmNPdu */ CANIF_%s_CM_TX,\n" % (name))
            else:
                C.write("    /* CanIf_TxCmNPdu */ (PduIdType)-1,\n")
            C.write("    /* Tr */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("Tr", 200)))
            C.write("    /* T1 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T1", 750)))
            C.write("    /* T2 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T2", 1250)))
            C.write("    /* T3 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T3", 1250)))
            C.write("    /* T4 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T4", 1050)))
            C.write("    /* RxProtocol */ J1939TP_PROTOCOL_%s,\n" % (protocol))
            C.write("    /* RxPacketsPerBlock */ %su,\n" % (chl.get("PacketsPerBlock", 8)))
            C.write("    /* LL_DL */ sizeof(u8Rx%sData),\n" % (chl["name"]))
            C.write("    /* S */ %su,\n" % (chl.get("S", 0)))
            C.write("    /* ADType */ 0u,\n")
            C.write("    %su, /* padding */\n" % (chl.get("padding", 0x55)))
            C.write("  },\n")
        C.write("};\n\n")
    if numTxChls > 0:
        C.write("static J1939Tp_TxChannelContextType J1939Tp_TxChannelContexts[%s];\n" % (numTxChls))
        for chl in cfg["TxChannels"]:
            C.write("static uint8_t u8Tx%sData[%s];\n" % (chl["name"], chl.get("LL_DL", "J1939TP_LL_DL")))
        C.write("static const J1939Tp_TxChannelType J1939Tp_TxChannels[] = {\n")
        for i, chl in enumerate(cfg["TxChannels"]):
            name = chl["name"]
            C.write("  { /* %s */\n" % (name))
            C.write("    &J1939Tp_TxChannelContexts[%s],\n" % (i))
            C.write("    u8Tx%sData,\n" % (chl["name"]))
            C.write("    /* TxPgPGN */ %su,\n" % (chl["PgPGN"]))
            C.write("    /* PduR_TxPduId */ PDUR_%s_TX,\n" % (name))
            C.write("    /* CanIf_TxDirectNPdu */ CANIF_%s_DIRECT_TX,\n" % (name))
            C.write("    /* CanIf_TxCmNPdu */ CANIF_%s_CM_TX,\n" % (name))
            C.write("    /* CanIf_TxDtNPdu */ CANIF_%s_DT_TX,\n" % (name))
            C.write("    /* STMin */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("STMin", 10)))
            C.write("    /* Tr */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("Tr", 200)))
            C.write("    /* T1 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T1", 750)))
            C.write("    /* T2 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T2", 1250)))
            C.write("    /* T3 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T3", 1250)))
            C.write("    /* T4 */ J1939TP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (chl.get("T4", 1050)))
            C.write("    /* TxProtocol */ J1939TP_PROTOCOL_%s,\n" % (chl["Protocol"]))
            C.write("    /* TxPacketsPerBlock */ %su,\n" % (chl.get("PacketsPerBlock", 8)))
            C.write("    /* LL_DL */ sizeof(u8Tx%sData),\n" % (chl["name"]))
            C.write("    /* S */ %su,\n" % (chl.get("S", 0)))
            C.write("    /* ADType */ 0u,\n")
            C.write("    %su, /* padding */\n" % (chl.get("padding", 0x55)))
            C.write("  },\n")
        C.write("};\n\n")
    for pdu in TxPdusInfo + RxPdusInfo:
        if pdu["ctype"] == "TX":
            C.write(
                "static const J1939Tp_TxPduInfoType J1939Tp_PduInfo_%s_%s_%s = {\n"
                % (pdu["chl"]["name"], pdu["type"], pdu["ctype"])
            )
            C.write("  /* TxChannel */ J1939TP_CHL_%s_TX,\n" % (pdu["chl"]["name"]))
            C.write("  /* PacketType */ J1939TP_PACKET_%s,\n" % (pdu["type"]))
            C.write("};\n\n")
        else:
            C.write(
                "static const J1939Tp_RxPduInfoType J1939Tp_PduInfo_%s_%s_%s = {\n"
                % (pdu["chl"]["name"], pdu["type"], pdu["ctype"])
            )
            C.write("    /* RxChannel */ J1939TP_CHL_%s_RX,\n" % (pdu["chl"]["name"]))
            C.write("    /* PacketType */ J1939TP_PACKET_%s,\n" % (pdu["type"]))
            C.write("};\n\n")
    if len(TxPdusInfo) > 0:
        C.write("static const J1939Tp_PduInfoType J1939Tp_TxPduInfos[] = {\n")
        for pdu in TxPdusInfo:
            C.write("  {\n")
            C.write("    { &J1939Tp_PduInfo_%s_%s_%s },\n" % (pdu["chl"]["name"], pdu["type"], pdu["ctype"]))
            C.write("    J1939TP_%s_CHANNEL,\n" % (pdu["ctype"]))
            C.write("  },\n")
        C.write("};\n\n")
    if len(RxPdusInfo) > 0:
        C.write("static J1939Tp_PduInfoType J1939Tp_RxPduInfos[] = {\n")
        for pdu in RxPdusInfo:
            C.write("  {\n")
            C.write("    { &J1939Tp_PduInfo_%s_%s_%s },\n" % (pdu["chl"]["name"], pdu["type"], pdu["ctype"]))
            C.write("    J1939TP_%s_CHANNEL,\n" % (pdu["ctype"]))
            C.write("  },\n")
        C.write("};\n\n")
    C.write("const J1939Tp_ConfigType J1939Tp_Config = {\n")
    if numRxChls > 0:
        C.write("  J1939Tp_RxChannels,\n")
    else:
        C.write("  NULL,\n")
    if numTxChls > 0:
        C.write("  J1939Tp_TxChannels,\n")
    else:
        C.write("  NULL,\n")
    if len(RxPdusInfo) > 0:
        C.write("  J1939Tp_RxPduInfos,\n")
    else:
        C.write("  NULL,\n")
    if len(TxPdusInfo) > 0:
        C.write("  J1939Tp_TxPduInfos,\n")
    else:
        C.write("  NULL,\n")
    C.write("  %s,\n" % (numRxChls))
    C.write("  %s,\n" % (numTxChls))
    C.write("  %s,\n" % (len(RxPdusInfo)))
    C.write("  %s,\n" % (len(TxPdusInfo)))
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_J1939Tp(cfg, dir)
    return ["%s/J1939Tp_Cfg.c" % (dir)]
