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
    for network in cfg["networks"]:
        for pdu in network["RxPdus"] + network["TxPdus"]:
            if pdu["up"] not in modules:
                modules.append(pdu["up"])
    H = open("%s/CanIf_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef CANIF_CFG_H\n")
    H.write("#define CANIF_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    ID = 0
    for network in cfg["networks"]:
        H.write("#define CANIF_CHL_%s %su\n" % (network["name"], ID))
        ID += 1
    H.write("\n")
    ID = 0
    for network in cfg["networks"]:
        for pdu in network["RxPdus"]:
            H.write("#define CANIF_%s %su /* %s id=0x%x */\n" % (pdu["name"], ID, network["name"], toNum(pdu["id"])))
            ID += 1
    H.write("\n")
    ID = 0
    for network in cfg["networks"]:
        for pdu in network["TxPdus"]:
            H.write("#define CANIF_%s %su /* %s id=0x%x */\n" % (pdu["name"], ID, network["name"], toNum(pdu["id"])))
            ID += 1
    H.write("#ifndef CANIF_MAIN_FUNCTION_PERIOD\n")
    H.write("#define CANIF_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define CANIF_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + CANIF_MAIN_FUNCTION_PERIOD - 1u) / CANIF_MAIN_FUNCTION_PERIOD)\n\n")
    for netId, network in enumerate(cfg["networks"]):
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
    for netId, network in enumerate(cfg["networks"]):
        for pdu in network["RxPdus"]:
            if pdu["up"][:4] == "User" and pdu["up"] not in rxinds:
                rxinds.append(pdu["up"])
                C.write("void %s_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);\n" % (pdu["up"]))
    for netId, network in enumerate(cfg["networks"]):
        for pdu in network["TxPdus"]:
            if pdu["up"][:4] == "User" and pdu["up"] not in txinds:
                txinds.append(pdu["up"])
                C.write("void %s_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);\n" % (pdu["up"]))
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for netId, network in enumerate(cfg["networks"]):
        C.write("static const CanIf_RxPduType CanIf_RxPdus_%s[] = {\n" % (network["name"]))
        for pdu in network["RxPdus"]:
            C.write("  {\n")
            if pdu["up"] in ["PduR", "Xcp"]:
                C.write("    %s_CanIfRxIndication,\n" % (pdu["up"]))
            else:
                C.write("    %s_RxIndication,\n" % (pdu["up"]))
            if pdu["up"] in ["OsekNm", "CanNm", "CanTSyn", "Xcp"]:
                C.write("    %s, /* NetId */\n" % (netId))
            elif pdu["up"][:4] == "User":
                C.write("    CANIF_%s, /* rxPduId */\n" % (pdu["name"]))
            else:
                C.write("    %s_%s,\n" % (pdu["up"].upper(), pdu["name"]))
            C.write("    0x%x, /* canid */\n" % (toNum(pdu["id"]) & 0x1FFFFFFF))
            C.write("    0x%x, /* mask */\n" % (toNum(pdu.get("mask", "0xFFFFFFFF")) & 0x1FFFFFFF))
            C.write("    %s, /* hoh */\n" % (pdu.get("hoh", 0)))
            C.write("  },\n")
        C.write("};\n\n")
    for netId, network in enumerate(cfg["networks"]):
        for pdu in network["TxPdus"]:
            if pdu.get("dynamic", False):
                C.write("static Can_IdType canidOf%s = %s;\n" % (pdu["name"], pdu["id"]))
    C.write("static const CanIf_TxPduType CanIf_TxPdus[] = {\n")
    for netId, network in enumerate(cfg["networks"]):
        for pdu in network["TxPdus"]:
            C.write("  {\n")
            if pdu["up"] in ["PduR", "Xcp"]:
                C.write("    %s_CanIfTxConfirmation,\n" % (pdu["up"]))
            else:
                C.write("    %s_TxConfirmation,\n" % (pdu["up"]))
            if pdu["up"] in ["OsekNm", "CanNm", "CanTSyn", "Xcp"]:
                C.write("    %s, /* NetId */\n" % (netId))
            elif pdu["up"][:4] == "User":
                C.write("    CANIF_%s, /* txPduId */\n" % (pdu["name"]))
            else:
                C.write("    %s_%s,\n" % (pdu["up"].upper(), pdu["name"]))
            C.write("    0x%x, /* canid */\n" % (toNum(pdu["id"])))
            if pdu.get("dynamic", False):
                C.write("    &canidOf%s, /* p_canid */\n" % (pdu["name"]))
            else:
                C.write("    NULL, /* p_canid */\n")
            C.write("    %s, /* hoh */\n" % (pdu.get("hoh", 0)))
            C.write("    %s, /* ControllerId */\n" % (netId))
            C.write("    #if CANIF_TX_PACKET_POOL_SIZE > 0\n")
            C.write("    %s, /* bUseTxPool */\n" % (str(pdu.get("UseTxPool", False)).upper()))
            C.write("    #endif\n")
            C.write("  },\n")
    C.write("};\n\n")
    C.write("static CanIf_CtrlContextType CanIf_CtrlContexts[%s];\n" % (len(cfg["networks"])))

    C.write("static const CanIf_CtrlConfigType CanIf_CtrlConfigs[] = {\n")
    for netId, network in enumerate(cfg["networks"]):
        C.write("  {\n")
        C.write("    CanIf_RxPdus_%s,\n" % (network["name"]))
        C.write("    ARRAY_SIZE(CanIf_RxPdus_%s),\n" % (network["name"]))
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
    cfg_["networks"] = []
    bNew = False
    hrh = 0
    hth = 0
    for network in cfg["networks"]:
        E2E = network.get("E2E", [])
        NumHrh = network.get("NumHrh", 1)
        NumHth = network.get("NumHth", 1)
        ignore = network.get("ignore", [])
        if "dbc" in network:
            network_ = dict(network)
            path = network["dbc"]
            del network_["dbc"]
            me = network["me"]
            name = network["name"]
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, "..", path))
            if not os.path.isfile(path):
                raise Exception("File %s not exists" % (path))
            messages = get_messages(path)
            for msg in messages:
                if msg["name"] in ignore:
                    continue
                rn = "%s_%s" % (name, toMacro(msg["name"]))
                rn = rn.upper()
                if msg["node"] == me:
                    if "TX" not in rn:
                        rn += "_TX"
                    kl = "TxPdus"
                    UseTxPool = msg["name"] in E2E
                    pdu = {"name": rn, "id": msg["id"], "hoh": hth, "up": "PduR", "UseTxPool": UseTxPool}
                else:
                    if "RX" not in rn:
                        rn += "_RX"
                    kl = "RxPdus"
                    pdu = {"name": rn, "id": msg["id"], "hoh": hrh, "up": "PduR"}
                if kl not in network_:
                    network_[kl] = []
                network_[kl].append(pdu)
            cfg_["networks"].append(network_)
            bNew = True
        else:
            cfg_["networks"].append(network)
        hrh += NumHrh
        hth += NumHth
    if bNew:
        with open("%s/CanIf.json" % (dir), "w") as f:
            json.dump(cfg_, f, indent=2)
    hrh = 0
    hth = 0
    for network in cfg_["networks"]:
        NumHrh = network.get("NumHrh", 1)
        NumHth = network.get("NumHth", 1)
        network["RxPdus"].sort(key=lambda x: toNum(x["id"]) & 0x1FFFFFFF)
        network["TxPdus"].sort(key=lambda x: toNum(x["id"]) & 0x1FFFFFFF)
        for pdu in network["RxPdus"]:
            hh = pdu.get("hoh", hrh)
            if hh < hrh or hh > (hrh + NumHrh):
                raise Exception("Invalid hoh for: %s" % (pdu))
            pdu["hoh"] = hh
        for pdu in network["TxPdus"]:
            hh = pdu.get("hoh", hth)
            if hh < hth or hh > (hth + NumHth):
                raise Exception("Invalid hoh for: %s" % (pdu))
            pdu["hoh"] = hh
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
