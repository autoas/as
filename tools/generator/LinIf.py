# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ["Gen"]


def CalcParity(pid):
    id_ = pid & 0x3F  # Extract 6-bit ID

    # Calculate parity bit P0 (bit 6)
    p0 = (pid ^ (pid >> 1) ^ (pid >> 2) ^ (pid >> 4)) << 6
    p0 = p0 & 0x40  # Isolate bit 6

    # Calculate parity bit P1 (bit 7)
    p1 = (~((pid >> 1) ^ (pid >> 3) ^ (pid >> 4) ^ (pid >> 5))) << 7
    p1 = p1 & 0x80  # Isolate bit 7

    # Combine ID with parity bits
    return id_ | p0 | p1


def Gen_LinIf(cfg, dir):
    H = open("%s/LinIf_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef LINIF_CFG_H\n")
    H.write("#define LINIF_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    tblId = 0
    bHasMaster = False
    bHasSlave = False
    for network in cfg["networks"]:
        netName = network["name"]
        if network["mode"] == "master":
            for schtbl in network["ScheduleTables"]:
                H.write("#define LINIF_SCHTBL_%s_%s %s\n" % (netName, schtbl["name"], tblId))
                tblId += 1
            bHasMaster = True
        else:
            H.write("#define LINIF_SCHTBL_%s %s\n" % (netName, tblId))
            tblId += 1
            bHasSlave = True
    H.write("\n")
    if bHasMaster and bHasSlave:
        H.write("#define LINIF_VARIANT LINIF_VARIANT_BOTH\n")
    elif bHasMaster:
        H.write("#define LINIF_VARIANT LINIF_VARIANT_MASTER\n")
    else:
        H.write("#define LINIF_VARIANT LINIF_VARIANT_SLAVE\n")
    H.write("#ifndef LINIF_MAIN_FUNCTION_PERIOD\n")
    H.write("#define LINIF_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define LINIF_CONVERT_MS_TO_MAIN_CYCLES(x)  \\\n")
    H.write("  ((x + LINIF_MAIN_FUNCTION_PERIOD - 1u) / LINIF_MAIN_FUNCTION_PERIOD)\n")
    H.write("%s#define LINIF_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("#define LINIF_SCHED_MODE_%s\n\n" % (cfg.get("SchedMode", "POLLING")))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* LINIF_CFG_H */\n")
    H.close()

    C = open("%s/LinIf_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "LinIf_Priv.h"\n')
    C.write('#include "LinIf.h"\n')
    C.write('#include "LinTp.h"\n')
    C.write("#ifdef USE_COM\n")
    C.write('#include "Com.h"\n')
    C.write('#include "Com_Cfg.h"\n')
    C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("#ifndef LINIF_DELAY_UINT\n")
    C.write("#ifdef _WIN32\n")
    C.write("#define LINIF_DELAY_UINT 10 /* 10 ms */\n")
    C.write("#else\n")
    C.write("#define LINIF_DELAY_UINT 1 /* 1 ms */\n")
    C.write("#endif\n")
    C.write("#endif\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    PduMap = {}
    for network in cfg["networks"]:
        for pdu in network["RxPdus"] + network["TxPdus"]:
            PduMap[pdu["name"]] = pdu
            C.write(
                "static Std_ReturnType LinIf_UserCallback_%s(uint8_t channel, Lin_PduType *frame,Std_ReturnType notifyResult);\n\n"
                % (pdu["name"])
            )
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("static LinIf_ChannelContextType LinIf_ChannelContexts[%s];\n\n" % (len(cfg["networks"])))
    schtbls = []
    ProcessParity = cfg.get("ProcessParity", False)
    for network in cfg["networks"]:
        netName = network["name"]
        if network["mode"] == "master":
            for schtbl in network["ScheduleTables"]:
                C.write(
                    "static const LinIf_ScheduleTableEntryType %s_%s_SchTblEnts[] = {\n" % (netName, schtbl["name"])
                )
                for entry in schtbl["tables"]:
                    name = entry["name"]
                    pdu = PduMap[name]
                    drc = name[-2:]
                    C.write("  { /* %s */\n" % (name))
                    if ProcessParity:
                        C.write("    /* id */ 0x%x,\n" % (CalcParity(toNum(pdu["id"]))))
                    else:
                        C.write("    /* id */ 0x%x,\n" % (toNum(pdu["id"])))
                    C.write("    /* dlc */ %s,\n" % (pdu["dlc"]))
                    C.write("    /* type */ LINIF_%s,\n" % (pdu["type"]))
                    C.write("    /* Cs */ LIN_%s_CS,\n" % (pdu.get("checksum", "enhanced").upper()))
                    C.write("    /* Drc */ LIN_FRAMERESPONSE_%s,\n" % (drc))
                    C.write("    /* Callback */ LinIf_UserCallback_%s,\n" % (name))
                    C.write("    LINIF_CONVERT_MS_TO_MAIN_CYCLES(%su * LINIF_DELAY_UINT),\n" % (toNum(entry["delay"])))
                    C.write("  },\n")
                C.write("};\n\n")
                schtbls.append("%s_%s_SchTblEnts" % (netName, schtbl["name"]))
        else:
            pdus = network["RxPdus"] + network["TxPdus"]
            pdus.sort(key=lambda x: toNum(x["id"]))
            C.write("static const LinIf_ScheduleTableEntryType %s_SchTblEnts[] = {\n" % (netName))
            for pdu in pdus:
                name = pdu["name"]
                pdu = PduMap[name]
                drc = name[-2:]
                C.write("  { /* %s */\n" % (name))
                if ProcessParity:
                    C.write("    /* id */ 0x%x,\n" % (CalcParity(toNum(pdu["id"]))))
                else:
                    C.write("    /* id */ 0x%x,\n" % (toNum(pdu["id"])))
                C.write("    /* dlc */ %s,\n" % (pdu["dlc"]))
                C.write("    /* type */ LINIF_%s,\n" % (pdu["type"]))
                C.write("    /* Cs */ LIN_%s_CS,\n" % (pdu.get("checksum", "enhanced").upper()))
                C.write("    /* Drc */ LIN_FRAMERESPONSE_%s,\n" % (drc))
                C.write("    /* Callback */ LinIf_UserCallback_%s,\n" % (name))
                C.write("    /* delay us */ 0,\n")
                C.write("  },\n")
            C.write("};\n\n")
            schtbls.append("%s_SchTblEnts" % (netName))
    C.write("static const LinIf_ScheduleTableType LinIf_SchTbls[] = {\n")
    for x in schtbls:
        C.write("  {\n")
        C.write("    %s,\n" % (x))
        C.write("    ARRAY_SIZE(%s),\n" % (x))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("const LinIf_ChannelConfigType LinIf_ChannelConfigs[] = {\n")
    for i, network in enumerate(cfg["networks"]):
        netName = network["name"]
        C.write("  { /* %s */\n" % (netName))
        C.write("    #if (LINIF_VARIANT & LINIF_VARIANT_SLAVE) == LINIF_VARIANT_SLAVE\n")
        if network["mode"] == "master":
            C.write("    NULL,\n")
        else:
            C.write("    &LinIf_SchTbls[LINIF_SCHTBL_%s],\n" % (netName))
        C.write("    #endif\n")
        C.write(
            "    /* timeout */ LINIF_CONVERT_MS_TO_MAIN_CYCLES(%su * LINIF_DELAY_UINT),\n"
            % (toNum(network.get("timeout", 100)))
        )
        C.write("    /* linChannel */ %su,\n" % (i))
        C.write("    #if LINIF_VARIANT == LINIF_VARIANT_BOTH\n")
        C.write("    /* nodeType */ LINIF_%s,\n" % (network["mode"].upper()))
        C.write("    #endif\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("const LinIf_ConfigType LinIf_Config = {\n")
    C.write("  #if (LINIF_VARIANT & LINIF_VARIANT_MASTER) == LINIF_VARIANT_MASTER\n")
    C.write("  LinIf_SchTbls,\n")
    C.write("  ARRAY_SIZE(LinIf_SchTbls),\n")
    C.write("  #endif\n")
    C.write("  LinIf_ChannelConfigs,\n")
    C.write("  LinIf_ChannelContexts,\n")
    C.write("  ARRAY_SIZE(LinIf_ChannelConfigs),\n")
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    for i, network in enumerate(cfg["networks"]):
        for pdu in network["RxPdus"] + network["TxPdus"]:
            name = pdu["name"]
            drc = name[-2:]
            C.write(
                "static Std_ReturnType LinIf_UserCallback_%s(uint8_t channel, Lin_PduType *frame,Std_ReturnType notifyResult) {\n"
                % (name)
            )
            if "RX" == drc:
                if "Com" == pdu["up"]:
                    C.write("  Std_ReturnType r = LINIF_R_NOT_OK;\n")
                    C.write("  PduInfoType pduInfo;\n")
                    C.write("  if (LINIF_R_RECEIVED_OK == notifyResult) {\n")
                    C.write("    pduInfo.SduDataPtr = frame->SduPtr;\n")
                    C.write("    pduInfo.SduLength = frame->Dl;\n")
                    C.write("    Com_RxIndication(COM_%s, &pduInfo);\n" % (name))
                    C.write("    r = LINIF_R_OK;\n")
                    C.write("  }\n")
                    C.write("  return r;\n")
                elif "LinTp" == pdu["up"]:
                    C.write("  Std_ReturnType r = LINIF_R_NOT_OK;\n")
                    C.write("  PduInfoType pduInfo;\n")
                    C.write("  if (LINIF_R_RECEIVED_OK == notifyResult) {\n")
                    C.write("    pduInfo.SduDataPtr = frame->SduPtr;\n")
                    C.write("    pduInfo.SduLength = frame->Dl;\n")
                    C.write("    LinTp_RxIndication(%s, &pduInfo);\n" % (i))
                    C.write("    r = LINIF_R_OK;\n")
                    C.write("  }\n")
                    C.write("  return r;\n")
                else:
                    raise
            elif "TX" == drc:
                if "Com" == pdu["up"]:
                    C.write("  Std_ReturnType r = LINIF_R_NOT_OK;\n")
                    C.write("  Std_ReturnType ret;\n")
                    C.write("  PduInfoType pduInfo;\n")
                    C.write("  if (LINIF_R_TRIGGER_TRANSMIT == notifyResult) {\n")
                    C.write("    pduInfo.SduDataPtr = frame->SduPtr;\n")
                    C.write("    pduInfo.SduLength = frame->Dl;\n")
                    C.write("    ret = Com_TriggerTransmit(COM_%s, &pduInfo);\n" % (name))
                    C.write("    if (E_OK == ret) {\n")
                    C.write("       r = LINIF_R_OK;\n")
                    C.write("    }\n")
                    C.write("  }\n")
                    C.write("  return r;\n")
                elif "LinTp" == pdu["up"]:
                    C.write("  Std_ReturnType r = LINIF_R_NOT_OK;\n")
                    C.write("  Std_ReturnType ret;\n")
                    C.write("  PduInfoType pduInfo;\n")
                    C.write("  if (LINIF_R_TRIGGER_TRANSMIT == notifyResult) {\n")
                    C.write("    pduInfo.SduDataPtr = frame->SduPtr;\n")
                    C.write("    pduInfo.SduLength = frame->Dl;\n")
                    C.write("    ret = LinTp_TriggerTransmit(%s, &pduInfo);\n" % (i))
                    C.write("    if (E_OK == ret) {\n")
                    C.write("       r = LINIF_R_OK;\n")
                    C.write("    }\n")
                    C.write("  }\n")
                    C.write("  return r;\n")
                else:
                    raise
            else:
                raise
            C.write("}\n\n")
    C.close()


def extract(cfg, dir):
    cfg_ = dict(cfg)
    cfg_["networks"] = []
    bNew = False
    for network in cfg["networks"]:
        if "ldf" in network:
            from .ldf import ldf

            network_ = dict(network)
            path = network["ldf"]
            del network_["ldf"]
            me = network["me"]
            netName = network["name"]
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, "..", path))
            if not os.path.isfile(path):
                raise Exception("File %s not exists" % (path))
            p = ldf(path)
            master = p["Nodes"]["master"]["name"]
            if master == me:
                network_["mode"] = "master"
            else:
                network_["mode"] = "slave"
            messages = p["messages"]
            pduMap = {}
            for kl in ["TxPdus", "RxPdus"]:
                if kl not in network_:
                    network_[kl] = []
            for msg in messages:
                rn = "%s_%s" % (netName, toMacro(msg["name"]))
                rn = rn.upper()
                if msg["node"] == me:
                    if "TX" not in rn:
                        rn += "_TX"
                    kl = "TxPdus"
                else:
                    if "RX" not in rn:
                        rn += "_RX"
                    kl = "RxPdus"
                pdu = {
                    "name": rn,
                    "id": msg["id"],
                    "type": "UNCONDITIONAL",
                    "checksum": "enhanced",
                    "dlc": msg["dlc"],
                    "up": "Com",
                }
                network_[kl].append(pdu)
                pduMap[msg["name"]] = pdu
            schtbl_diag = []
            if "diagnostic_frames" in p:
                reqId = p["diagnostic_frames"]["MasterReq"]["id"]
                resId = p["diagnostic_frames"]["SlaveResp"]["id"]
                if master == me:  # for master, general CANTP gateway to LINTP
                    pdu = {
                        "name": "%s_DIAG_REQ_TX" % (netName),
                        "id": reqId,
                        "type": "DIAG_MRF",
                        "checksum": "classic",
                        "dlc": 8,
                        "up": "LinTp",
                    }
                    network_["TxPdus"].append(pdu)
                    pdu = {
                        "name": "%s_DIAG_RES_RX" % (netName),
                        "id": resId,
                        "type": "DIAG_SRF",
                        "checksum": "classic",
                        "dlc": 8,
                        "up": "LinTp",
                    }
                    network_["RxPdus"].append(pdu)
                    schtbl_diag = [
                        {
                            "name": "DIAG_REQ",
                            "tables": [
                                {
                                    "name": "%s_DIAG_REQ_TX" % (netName),
                                    "delay": "20",
                                }
                            ],
                        },
                        {
                            "name": "DIAG_RES",
                            "tables": [
                                {
                                    "name": "%s_DIAG_RES_RX" % (netName),
                                    "delay": "20",
                                }
                            ],
                        },
                    ]
                else:
                    pdu = {
                        "name": "%s_DIAG_REQ_RX" % (netName),
                        "id": reqId,
                        "type": "DIAG_MRF",
                        "checksum": "classic",
                        "dlc": 8,
                        "up": "LinTp",
                    }
                    network_["RxPdus"].append(pdu)
                    pdu = {
                        "name": "%s_DIAG_RES_TX" % (netName),
                        "id": resId,
                        "type": "DIAG_SRF",
                        "checksum": "classic",
                        "dlc": 8,
                        "up": "LinTp",
                    }
                    network_["TxPdus"].append(pdu)
            if master == me:
                # only for master, it has schedule table, for slave just 1 table with all PDU configured
                schtbls = p["schedule_tables"]
                if "ScheduleTables" not in network_:
                    network_["ScheduleTables"] = []
                for sname, schtbl in schtbls.items():
                    schtbl_ = {"name": sname, "tables": []}
                    for entry in schtbl:
                        pdu = pduMap[entry["name"]]
                        entry_ = {
                            "name": pdu["name"],
                            "delay": entry["delay"],
                        }
                        schtbl_["tables"].append(entry_)
                    network_["ScheduleTables"].append(schtbl_)
                network_["ScheduleTables"].extend(schtbl_diag)
            cfg_["networks"].append(network_)
            bNew = True
        else:
            cfg_["networks"].append(network)
    if bNew:
        with open("%s/LinIf.json" % (dir), "w") as f:
            json.dump(cfg_, f, indent=2)
    for network in cfg_["networks"]:
        network["RxPdus"].sort(key=lambda x: toNum(x["id"]) & 0x1FFFFFFF)
        network["TxPdus"].sort(key=lambda x: toNum(x["id"]) & 0x1FFFFFFF)
    return cfg_


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cfg_ = extract(cfg, dir)
    Gen_LinIf(cfg_, dir)
    return ["%s/LinIf_Cfg.c" % (dir)]
