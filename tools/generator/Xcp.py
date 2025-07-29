# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def gen_dummy_api(C, service, cfg):
    pass


def gen_dummy_config(C, service, cfg):
    pass


def gen_connect_api(C, service, cfg):
    C.write("Std_ReturnType %s(uint8_t mode,\n" % (service["API"]))
    C.write("                  Xcp_NegativeResponseCodeType *nrc);\n\n")


def gen_connect_config(C, service, cfg):
    C.write("static CONSTANT(Xcp_ConnectConfigType, XCP_CONST) Xcp_ConnectConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("};\n\n")


def gen_disconnect_api(C, service, cfg):
    C.write("Std_ReturnType %s(void);\n\n" % (service["API"]))


def gen_disconnect_config(C, service, cfg):
    C.write("static CONSTANT(Xcp_DisconnectConfigType, XCP_CONST) Xcp_DisconnectConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("};\n\n")


def gen_get_seed_api(C, service, cfg):
    C.write("Std_ReturnType %s(uint8_t mode, uint8_t resource, uint8_t *seed,\n" % (service["API"]))
    C.write("                  uint16_t *seedLen, Xcp_NegativeResponseCodeType *nrc);\n\n")


def gen_get_seed_config(C, service, cfg):
    C.write("static CONSTANT(Xcp_GetSeedConfigType, XCP_CONST) Xcp_GetSeedConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("};\n\n")


def gen_unlock_api(C, service, cfg):
    C.write(
        "Std_ReturnType %s(uint8_t *key, uint16_t keyLen, Xcp_NegativeResponseCodeType *nrc);\n\n" % (service["API"])
    )


def gen_unlock_config(C, service, cfg):
    C.write("static CONSTANT(Xcp_UnlockConfigType, XCP_CONST) Xcp_UnlockConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("};\n\n")


def gen_program_start_api(C, service, cfg):
    C.write("Std_ReturnType %s(Xcp_OpStatusType opStatus, Xcp_NegativeResponseCodeType *nrc);\n\n" % (service["API"]))


def gen_program_start_config(C, service, cfg):
    C.write("static CONSTANT(Xcp_ProgramStartConfigType,XCP_CONST) Xcp_ProgramStartConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("  /* maxBs */ %s,\n" % (service.get("maxBs", "XCP_PACKET_POOL_SIZE")))
    C.write("  /* STmin */ %s,\n" % (service.get("STmin", 10)))
    C.write("  /* queSz */ %s,\n" % (service.get("queSz", "XCP_PACKET_POOL_SIZE")))
    C.write("};\n\n")


def gen_program_reset_api(C, service, cfg):
    C.write("Std_ReturnType %s(Xcp_OpStatusType opStatus, Xcp_NegativeResponseCodeType *nrc);\n\n" % (service["API"]))


def gen_program_reset_config(C, service, cfg):
    C.write("static CONSTANT(Xcp_ProgramResetConfigType,XCP_CONST) Xcp_ProgramResetConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("  XCP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (service.get("delay", 100)))
    C.write("};\n\n")


ServiceMap = {
    0xFF: {
        "name": "STD_CONNECT",
        "API": "Connect",
        "config": gen_connect_config,
        "api": gen_connect_api,
    },
    0xFE: {
        "name": "STD_DISCONNECT",
        "API": "Disconnect",
        "config": gen_disconnect_config,
        "api": gen_disconnect_api,
    },
    0xFD: {
        "name": "STD_GET_STATUS",
        "API": "GetStatus",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xF8: {
        "name": "STD_GET_SEED",
        "API": "GetSeed",
        "config": gen_get_seed_config,
        "api": gen_get_seed_api,
    },
    0xF7: {
        "name": "STD_UNLOCK",
        "API": "Unlock",
        "config": gen_unlock_config,
        "api": gen_unlock_api,
    },
    0xF6: {
        "name": "STD_SET_MTA",
        "API": "SetMTA",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xF5: {
        "name": "STD_UPLOAD",
        "API": "Upload",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xF4: {
        "name": "STD_SHORT_UPLOAD",
        "API": "ShortUpload",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xF0: {
        "name": "CAL_DOWNLOAD",
        "API": "Download",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xEF: {
        "name": "CAL_DOWNLOAD_NEXT",
        "API": "DownloadNext",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD2: {
        "name": "PGM_PROGRAM_START",
        "API": "ProgramStart",
        "config": gen_program_start_config,
        "api": gen_program_start_api,
    },
    0xD1: {
        "name": "PGM_PROGRAM_CLEAR",
        "API": "ProgramClear",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD0: {
        "name": "PGM_PROGRAM",
        "API": "Program",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xCA: {
        "name": "PGM_PROGRAM_NEXT",
        "API": "ProgramNext",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xCF: {
        "name": "PGM_PROGRAM_RESET",
        "API": "ProgramReset",
        "config": gen_program_reset_config,
        "api": gen_program_reset_api,
    },
    0xE3: {
        "name": "DAQ_CLEAR_DAQ_LIST",
        "API": "ClearDAQList",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xE2: {
        "name": "DAQ_SET_DAQ_PTR",
        "API": "SetDAQPtr",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xE1: {
        "name": "DAQ_WRITE_DAQ",
        "API": "WriteDAQ",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xDB: {
        "name": "DAQ_READ_DAQ",
        "API": "ReadDAQ",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xE0: {
        "name": "DAQ_SET_DAQ_LIST_MODE",
        "API": "SetDAQListMode",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xDF: {
        "name": "DAQ_GET_DAQ_LIST_MODE",
        "API": "GetDAQListMode",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xDE: {
        "name": "DAQ_START_STOP_DAQ_LIST",
        "API": "StartStopDAQList",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xDA: {
        "name": "DAQ_GET_DAQ_PROCESSOR_INFO",
        "API": "GetDAQProcessorInfo",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD9: {
        "name": "DAQ_GET_DAQ_RESOLUTION_INFO",
        "API": "GetDAQResolutionInfo",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD8: {
        "name": "DAQ_GET_DAQ_LIST_INFO",
        "API": "GetDAQListInfo",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD7: {
        "name": "DAQ_GET_DAQ_EVENT_INFO",
        "API": "GetDAQEventInfo",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xDD: {
        "name": "DAQ_START_STOP_SYNCH",
        "API": "DAQStartStopSynch",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD6: {
        "name": "DAQ_FREE_DAQ",
        "API": "FreeDAQ",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD5: {
        "name": "DAQ_ALLOC_DAQ",
        "API": "AllocDAQ",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD4: {
        "name": "DAQ_ALLOC_ODT",
        "API": "AllocODT",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0xD3: {
        "name": "DAQ_ALLOC_ODT_ENTRY",
        "API": "AllocODTEntry",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
}


def get_resource(cfg):
    resource = []
    for x in cfg["services"]:
        name = ServiceMap[x["id"]]["name"]
        group = name[:4]
        if group in ["CAL_", "PAG_"]:
            if "XCP_RES_CALPAG" not in resource:
                resource.append("XCP_RES_CALPAG")
        elif group == "DAQ_":
            if "XCP_RES_DAQ" not in resource:
                resource.append("XCP_RES_DAQ")
        elif group == "PGM_":
            if "XCP_RES_PGM" not in resource:
                resource.append("XCP_RES_PGM")
    if len(resource):
        resource = "|".join(resource)
    else:
        resource = 0
    return resource


def preprocess(cfg):
    for x in cfg["services"]:
        x["id"] = toNum(x["id"])
    cfg["services"].sort(key=lambda x: str(x["id"]))


def Gen_Xcp(cfg, dir):
    preprocess(cfg)
    H = open("%s/Xcp_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef XCP_CFG_H\n")
    H.write("#define XCP_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef XCP_PACKET_POOL_SIZE\n")
    H.write("#define XCP_PACKET_POOL_SIZE 8\n")
    H.write("#endif\n\n")

    H.write("#ifndef XCP_MAIN_FUNCTION_PERIOD\n")
    H.write("#define XCP_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define XCP_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + XCP_MAIN_FUNCTION_PERIOD - 1u) / XCP_MAIN_FUNCTION_PERIOD)\n\n")
    for service in cfg["services"]:
        H.write("#define XCP_USE_SERVICE_%s\n" % (ServiceMap[service["id"]]["name"]))
    H.write("%s#define XCP_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", True) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* XCP_CFG_H */\n")
    H.close()

    C = open("%s/Xcp_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Xcp.h"\n')
    C.write('#include "Xcp_Cfg.h"\n')
    C.write('#include "Xcp_Priv.h"\n')
    C.write('#include "CanIf_Cfg.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    for service in cfg["services"]:
        ServiceMap[service["id"]]["api"](C, service, cfg)
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    evChls = cfg.get("EventChannels", [])
    if len(evChls) > 0:
        C.write("static Xcp_EventChannelType Xcp_EventChannels[] = {\n")
        for evChl in evChls:
            C.write("  {\n")
            C.write("    /* TimeCycle */ XCP_CONVERT_MS_TO_MAIN_CYCLES(%su),\n" % (evChl["TimeCycle"]))
            C.write("  },\n")
        C.write("};\n\n")
    staticDaqList = cfg.get("DaqList", {}).get("StaticDaqList", [])
    for daq in staticDaqList:
        daqName = daq["name"]
        for odt in daq["ODTs"]:
            odtName = odt["name"]
            C.write("static CONSTANT(Xcp_OdtEntryType, XCP_CONST) Xcp_%s_%s_OdtEntries[] = {\n" % (daqName, odtName))
            for entry in odt["Entries"]:
                C.write("  {\n")
                C.write("    /* Address */ 0x%x,\n" % (entry["Address"]))
                C.write("    /* BitOffset */ 0x%x,\n" % (entry.get("BitOffset", 0xFF)))
                C.write("    /* Length  */ 0x%x,\n" % (entry["Length"]))
                C.write("    /* Extension */ XCP_MTA_EXT_%s,\n" % (entry.get("Extension", "MEMORY")))
                C.write("  },\n")
            C.write("};\n\n")
    for daq in staticDaqList:
        daqName = daq["name"]
        C.write("static CONSTANT(Xcp_OdtType, XCP_CONST) Xcp_%s_Odts[] = {\n" % (daqName))
        for odt in daq["ODTs"]:
            odtName = odt["name"]
            C.write("  {\n")
            C.write("    (Xcp_OdtEntryType*)Xcp_%s_%s_OdtEntries,\n" % (daqName, odtName))
            C.write("    ARRAY_SIZE(Xcp_%s_%s_OdtEntries),\n" % (daqName, odtName))
            C.write("  },\n")
        C.write("};\n\n")
    for daq in staticDaqList:
        daqName = daq["name"]
        C.write("static CONSTANT(Xcp_DaqListType, XCP_CONST) Xcp_%s_Daq = {\n" % (daqName))
        C.write("  (Xcp_OdtType*)Xcp_%s_Odts,\n" % (daqName))
        C.write("  ARRAY_SIZE(Xcp_%s_Odts),\n" % (daqName))
        C.write("};\n\n")
    dynDaqNo = cfg.get("DaqList", {}).get("DynamicDaqList", {}).get("DaqPoolSize", 0)
    if dynDaqNo > 0:
        for i in range(dynDaqNo):
            C.write("static Xcp_DaqListType Xcp_DynDaq%s;\n\n" % (i))
    dynOdtNo = cfg.get("DaqList", {}).get("DynamicDaqList", {}).get("OdtPoolSize", 0)
    if dynOdtNo > 0:
        C.write("static Xcp_OdtType Xcp_DynOdts[%s];\n\n" % (dynOdtNo))
    dynOdtEntryNo = cfg.get("DaqList", {}).get("DynamicDaqList", {}).get("OdtEntryPoolSize", 0)
    if dynOdtEntryNo > 0:
        C.write("static Xcp_OdtEntryType Xcp_DynOdtEntries[%s];\n\n" % (dynOdtEntryNo))
    if dynDaqNo > 0 or len(staticDaqList) > 0:
        C.write("static Xcp_DaqListContextType Xcp_DaqListContexts[%s];\n" % (dynDaqNo + len(staticDaqList)))
        C.write("static CONSTANT(Xcp_DaqListType*, XCP_CONST) Xcp_DaqList[] = {\n")
        for daq in staticDaqList:
            daqName = daq["name"]
            C.write("  &Xcp_%s_Daq,\n" % (daqName))
        for i in range(dynDaqNo):
            C.write("  &Xcp_DynDaq%s,\n" % (i))
        C.write("};\n\n")

    for service in cfg["services"]:
        ServiceMap[service["id"]]["config"](C, service, cfg)
    C.write("static CONSTANT(Xcp_ServiceType, XCP_CONST) Xcp_Services[] = {\n")
    for service in cfg["services"]:
        serInfo = ServiceMap[service["id"]]
        C.write("  {\n")
        C.write("    Xcp_Dsp%s,\n" % (serInfo["API"]))
        if serInfo["config"] == gen_dummy_config:
            C.write("    NULL,\n")
        else:
            C.write("    (const void XCP_CONST *)&Xcp_%sConfig,\n" % (serInfo["API"]))
        C.write("    XCP_PID_CMD_%s,\n" % (serInfo["name"]))
        res = service.get("resource", get_resource({"services": [service]}))
        C.write("    /* resource */ %s,\n" % (res))
        C.write("  },\n")
    C.write("};\n\n")

    C.write("CONSTANT(Xcp_ConfigType, XCP_CONST) Xcp_Config = {\n")
    C.write("  Xcp_Services,\n")
    if dynDaqNo > 0 or len(staticDaqList) > 0:
        C.write("  Xcp_DaqList,\n")
        C.write("  Xcp_DaqListContexts,\n")
    else:
        C.write("  NULL,\n")
        C.write("  NULL,\n")
    if len(evChls) > 0:
        C.write("  Xcp_EventChannels,\n")
    else:
        C.write("  NULL,\n")
    if dynOdtNo > 0:
        C.write("  Xcp_DynOdts,\n")
    else:
        C.write("  NULL,\n")
    if dynOdtEntryNo > 0:
        C.write("  Xcp_DynOdtEntries,\n")
    else:
        C.write("  NULL,\n")
    C.write("  /* numOfStaticDaqList */ %s,\n" % (len(staticDaqList)))
    C.write("  /* numOfDaqList */ %s,\n" % (dynDaqNo + len(staticDaqList)))
    C.write("  /* numOfDynOdtSlots */ %s,\n" % (dynOdtNo))
    C.write("  /* numOfDynOdtEntrySlots */ %s,\n" % (dynOdtEntryNo))
    C.write("  /* CanIfTxPduId */ CANIF_XCP_ON_CAN_TX,\n")
    C.write("  ARRAY_SIZE(Xcp_Services),\n")
    C.write("  /* numOfEvChls */ %s,\n" % (len(evChls)))
    C.write("  /* resource */ %s,\n" % (get_resource(cfg)))
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_Xcp(cfg, dir)
    return ["%s/Xcp_Cfg.c" % (dir)]

