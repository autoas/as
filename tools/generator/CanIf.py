# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

from .Com import get_messages

__all__ = ["Gen"]


def Gen_CanIf(cfg, dir):
    modules = []
    for network in cfg["Networks"]:
        for pdu in network["RxPdus"] + network["TxPdus"]:
            if pdu["Up"] not in modules:
                modules.append(pdu["Up"])
    H = open("%s/CanIf_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef CANIF_CFG_H\n")
    H.write("#define CANIF_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    ID = 0
    for network in cfg["Networks"]:
        H.write("#define CANIF_CHL_%s %su\n" % (network["Name"], ID))
        ID += 1
    H.write("\n")
    ID = 0
    for network in cfg["Networks"]:
        for pdu in network["RxPdus"]:
            H.write("#define CANIF_%s %su /* %s id=0x%x */\n" % (pdu["Name"], ID, network["Name"], toNum(pdu["Id"])))
            ID += 1
    H.write("\n")
    ID = 0
    for network in cfg["Networks"]:
        for pdu in network["TxPdus"]:
            H.write("#define CANIF_%s %su /* %s id=0x%x */\n" % (pdu["Name"], ID, network["Name"], toNum(pdu["Id"])))
            ID += 1
    H.write("#ifndef CANIF_MAIN_FUNCTION_PERIOD\n")
    H.write("#define CANIF_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define CANIF_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + CANIF_MAIN_FUNCTION_PERIOD - 1u) / CANIF_MAIN_FUNCTION_PERIOD)\n\n")
    for netId, network in enumerate(cfg["Networks"]):
        if network.get("TxTimeout", 100):
            H.write("#define CANIF_USE_TX_TIMEOUT\n\n")
            break
    H.write("%s#define CANIF_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("%s#define CANIF_USE_TX_CALLOUT\n\n" % ("" if cfg.get("UseTxCallout", False) else "// "))
    H.write("%s#define CANIF_USE_RX_CALLOUT\n\n" % ("" if cfg.get("UseRxCallout", False) else "// "))
    H.write("#ifndef CANIF_RX_PACKET_POOL_SIZE\n")
    H.write("#define CANIF_RX_PACKET_POOL_SIZE %su\n" % (cfg.get("RxPacketPoolSize", 0)))
    H.write("#endif\n\n")
    H.write("#ifndef CANIF_TX_PACKET_POOL_SIZE\n")
    H.write("#define CANIF_TX_PACKET_POOL_SIZE %su\n" % (cfg.get("TxPacketPoolSize", 0)))
    H.write("#endif\n\n")
    H.write("#ifndef CANIF_RX_PACKET_DATA_SIZE\n")
    H.write("#define CANIF_RX_PACKET_DATA_SIZE %su\n" % (cfg.get("RxPacketDataSize", 64)))
    H.write("#endif\n\n")
    H.write("#ifndef CANIF_TX_PACKET_DATA_SIZE\n")
    H.write("#define CANIF_TX_PACKET_DATA_SIZE %su\n" % (cfg.get("TxPacketDataSize", 64)))
    H.write("#endif\n\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* CANIF_CFG_H */\n")
    H.close()

    C = open("%s/CanIf_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "CanIf.h"\n')
    C.write('#include "CanIf_Cfg.h"\n')
    C.write('#include "CanIf_Priv.h"\n')
    for mod in modules:
        if mod == "PduR":
            C.write('#include "PduR_CanIf.h"\n')
        elif mod[:4] == "User":
            pass
        else:
            C.write('#include "%s.h"\n' % (mod))
        if mod[:4] == "User":
            pass
        elif mod not in ["OsekNm", "CanNm", "CanTSyn", "Xcp"]:
            C.write('#include "%s_Cfg.h"\n' % (mod))
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    rxinds = []
    txinds = []
    for netId, network in enumerate(cfg["Networks"]):
        for pdu in network["RxPdus"]:
            if pdu["Up"][:4] == "User" and pdu["Up"] not in rxinds:
                rxinds.append(pdu["Up"])
                C.write("void %s_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);\n" % (pdu["Up"]))
    for netId, network in enumerate(cfg["Networks"]):
        for pdu in network["TxPdus"]:
            if pdu["Up"][:4] == "User" and pdu["Up"] not in txinds:
                txinds.append(pdu["Up"])
                C.write("void %s_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);\n" % (pdu["Up"]))
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for netId, network in enumerate(cfg["Networks"]):
        if len(network["RxPdus"]) == 0:
            continue
        C.write("static const CanIf_RxPduType CanIf_RxPdus_%s[] = {\n" % (network["Name"]))
        for pdu in network["RxPdus"]:
            for pdu_ in network["RxPdus"]:
                if pdu_ == pdu:
                    break
                canid = toNum(pdu["Id"]) & 0x1FFFFFFF
                mask = toNum(pdu.get("Mask", "0xFFFFFFFF")) & 0x1FFFFFFF
                canid_ = toNum(pdu_["Id"]) & 0x1FFFFFFF
                mask_ = toNum(pdu_.get("Mask", "0xFFFFFFFF")) & 0x1FFFFFFF
                if (canid & mask_) == (canid_ & mask_):
                    raise Exception(f"Duplicate Rx Pdus: {pdu} vs {pdu_}")
                if (canid & mask) == (canid_ & mask):
                    raise Exception(f"Duplicate Rx Pdus: {pdu} vs {pdu_}")
            C.write("  {\n")
            if pdu["Up"] in ["PduR", "Xcp"]:
                C.write("    %s_CanIfRxIndication,\n" % (pdu["Up"]))
            else:
                C.write("    %s_RxIndication,\n" % (pdu["Up"]))
            if pdu["Up"] in ["OsekNm", "CanNm", "CanTSyn", "Xcp"]:
                C.write("    %s, /* NetId */\n" % (netId))
            elif pdu["Up"][:4] == "User":
                C.write("    CANIF_%s, /* RxPduId */\n" % (pdu["Name"]))
            else:
                C.write("    %s_%s,\n" % (pdu["Up"].upper(), pdu["Name"]))
            C.write("    0x%x, /* CanId */\n" % (toNum(pdu["Id"]) & 0x1FFFFFFF))
            C.write("    0x%x, /* Mask */\n" % (toNum(pdu.get("Mask", "0xFFFFFFFF")) & 0x1FFFFFFF))
            C.write("    %s, /* Hoh */\n" % (pdu.get("Hoh", 0)))
            C.write("  },\n")
        C.write("};\n\n")
    for netId, network in enumerate(cfg["Networks"]):
        for pdu in network["TxPdus"]:
            if pdu.get("Dynamic", False):
                C.write("static Can_IdType canidOf%s = %s;\n" % (pdu["Name"], pdu["Id"]))
    C.write("static const CanIf_TxPduType CanIf_TxPdus[] = {\n")
    for netId, network in enumerate(cfg["Networks"]):
        for pdu in network["TxPdus"]:
            C.write("  {\n")
            if pdu["Up"] in ["PduR", "Xcp"]:
                C.write("    %s_CanIfTxConfirmation,\n" % (pdu["Up"]))
            else:
                C.write("    %s_TxConfirmation,\n" % (pdu["Up"]))
            if pdu["Up"] in ["OsekNm", "CanNm", "CanTSyn", "Xcp"]:
                C.write("    %s, /* NetId */\n" % (netId))
            elif pdu["Up"][:4] == "User":
                C.write("    CANIF_%s, /* TxPduId */\n" % (pdu["Name"]))
            else:
                C.write("    %s_%s,\n" % (pdu["Up"].upper(), pdu["Name"]))
            C.write("    0x%x, /* CanId */\n" % (toNum(pdu["Id"])))
            if pdu.get("Dynamic", False):
                C.write("    &canidOf%s, /* CanIdPtr */\n" % (pdu["Name"]))
            else:
                C.write("    NULL, /* CanIdPtr */\n")
            C.write("    %s, /* Hoh */\n" % (pdu.get("Hoh", 0)))
            C.write("    %s, /* ControllerId */\n" % (netId))
            C.write("    #if CANIF_TX_PACKET_POOL_SIZE > 0\n")
            C.write("    %s, /* UseTxPool */\n" % (str(pdu.get("UseTxPool", False)).upper()))
            C.write("    #endif\n")
            C.write("  },\n")
    C.write("};\n\n")
    C.write("static CanIf_CtrlContextType CanIf_CtrlContexts[%s];\n" % (len(cfg["Networks"])))

    C.write("static const CanIf_CtrlConfigType CanIf_CtrlConfigs[] = {\n")
    for netId, network in enumerate(cfg["Networks"]):
        C.write("  {\n")
        if len(network["RxPdus"]) == 0:
            C.write("    NULL,\n")
            C.write("    0u,\n")
        else:
            C.write("    CanIf_RxPdus_%s,\n" % (network["Name"]))
            C.write("    ARRAY_SIZE(CanIf_RxPdus_%s),\n" % (network["Name"]))
        C.write("    #if defined(CANIF_USE_TX_TIMEOUT) && defined(USE_CANSM)\n")
        C.write("    CANIF_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (network.get("TxTimeout", 100)))
        C.write("    #endif\n")
        C.write("  },\n")
    C.write("};\n")

    C.write("const CanIf_ConfigType CanIf_Config = {\n")
    C.write("  CanIf_TxPdus,\n")
    C.write("  CanIf_CtrlContexts,\n")
    C.write("  CanIf_CtrlConfigs,\n")
    C.write("  ARRAY_SIZE(CanIf_TxPdus),\n")
    C.write("  ARRAY_SIZE(CanIf_CtrlContexts),\n")
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def extract(cfg, dir):
    cfg_ = dict(cfg)
    cfg_["Networks"] = []
    bNew = False
    hrh = 0
    hth = 0
    for network in cfg["Networks"]:
        E2E = network.get("E2E", [])
        NumHrh = network.get("NumHrh", 1)
        NumHth = network.get("NumHth", 1)
        ignore = network.get("Ignore", [])
        if "Dbc" in network:
            network_ = dict(network)
            path = network["Dbc"]
            del network_["Dbc"]
            me = network["Me"]
            name = network["Name"]
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, "..", path))
            if not os.path.isfile(path):
                raise Exception("File %s not exists" % (path))
            messages = get_messages(path)
            for msg in messages:
                if msg["Name"] in ignore:
                    continue
                rn = "%s_%s" % (name, toMacro(msg["Name"]))
                rn = rn.upper()
                if msg["Node"] == me:
                    if "TX" not in rn:
                        rn += "_TX"
                    kl = "TxPdus"
                    UseTxPool = msg["Name"] in E2E
                    pdu = {"Name": rn, "Id": msg["Id"], "Hoh": hth, "Up": "PduR", "UseTxPool": UseTxPool}
                else:
                    if "RX" not in rn:
                        rn += "_RX"
                    kl = "RxPdus"
                    pdu = {"Name": rn, "Id": msg["Id"], "Hoh": hrh, "Up": "PduR"}
                if kl not in network_:
                    network_[kl] = []
                network_[kl].append(pdu)
            cfg_["Networks"].append(network_)
            bNew = True
        else:
            cfg_["Networks"].append(network)
        hrh += NumHrh
        hth += NumHth
    if bNew:
        with open("%s/CanIf.json" % (dir), "w") as f:
            json.dump(cfg_, f, indent=2)
    hrh = 0
    hth = 0
    for network in cfg_["Networks"]:
        NumHrh = network.get("NumHrh", 1)
        NumHth = network.get("NumHth", 1)
        network["RxPdus"].sort(key=lambda x: toNum(x["Id"]) & 0x1FFFFFFF)
        network["TxPdus"].sort(key=lambda x: toNum(x["Id"]) & 0x1FFFFFFF)
        for pdu in network["RxPdus"]:
            hh = pdu.get("Hoh", hrh)
            if hh < hrh or hh > (hrh + NumHrh):
                raise Exception("Invalid hoh for: %s" % (pdu))
            pdu["Hoh"] = hh
        for pdu in network["TxPdus"]:
            hh = pdu.get("Hoh", hth)
            if hh < hth or hh > (hth + NumHth):
                raise Exception("Invalid hoh for: %s" % (pdu))
            pdu["Hoh"] = hh
        hrh += NumHrh
        hth += NumHth
    return cfg_


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cfg_ = extract(cfg, dir)
    Gen_CanIf(cfg_, dir)
    return ["%s/CanIf_Cfg.c" % (dir)]
