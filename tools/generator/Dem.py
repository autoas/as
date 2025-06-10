# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *
from .MemMap import *

__all__ = ["Gen"]


def GetSnapshotSize(cfg):
    size = 0
    for data in cfg["Environments"]:
        size += GetDataSize(data)
    return size


def GetExtendedDataSize(cfg):
    size = 0
    for data in cfg["ExtendedDatas"]:
        size += GetDataSize(data)
    return size


def GetProp(first, second, attr, dft):
    return first.get(attr, second.get(attr, dft))


def get_shell_basic_data_print(C, data, offset):
    C.write("  {\n")
    if data["type"] in [
        "uint8",
        "uint16",
        "uint32",
    ]:
        C.write("    %s_t *itsValue = (%s_t*)(data+%d);\n" % (data["type"], data["type"], offset))
        C.write('    PRINTF("  %s = %%u (0x%%x)\\n", (uint32_t)*itsValue, (uint32_t)*itsValue);\n' % (data["name"]))
    elif data["type"] in ["int8", "int16", "int32"]:
        C.write("    %s_t *itsValue = (%s_t*)(data+%d);\n" % (data["type"], data["type"], offset))
        C.write('    PRINTF("  %s = %%d (0x%%x)\\n", (int32_t)*itsValue, (int32_t)*itsValue);\n' % (data["name"]))
    else:
        raise
    C.write("  }\n")


def gen_shell_data_print(C, data):
    C.write("#ifdef USE_SHELL\n")
    C.write("static void Dem_FFD_Print%s(uint8_t* data) {\n" % (data["name"]))
    if data["type"] == "struct":
        offset = 0
        for d in data["data"]:
            get_shell_basic_data_print(C, d, offset)
            offset += GetDataSize(d)
    else:
        get_shell_basic_data_print(C, data, 0)
    C.write("}\n")
    C.write("#endif\n")


def Gen_Dem(cfg, dir):
    general = cfg.get("general", {})
    envMap = {}
    envsMapByName = {data["name"]: data for data in cfg["Environments"]}
    envsAll = [data["name"] for data in cfg["Environments"]]
    envsAll.sort(key=lambda x: toNum(envsMapByName[x]["id"]))
    envsKey = ".".join(envsAll)
    envMap[envsKey] = "Default"
    H = open("%s/Dem_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef DEM_CFG_H\n")
    H.write("#define DEM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef DEM_VAR\n")
    H.write("#define DEM_VAR\n")
    H.write("#endif\n")
    H.write("#ifndef DEM_CONST\n")
    H.write("#define DEM_CONST\n")
    H.write("#endif\n\n")
    H.write("#define DEM_USE_NVM\n")
    MaxFreezeFrameNum = cfg.get("MaxFreezeFrameNum", 2)
    H.write("#define DEM_MAX_FREEZE_FRAME_NUMBER %su\n" % (MaxFreezeFrameNum))
    H.write("#define DEM_MAX_FREEZE_FRAME_DATA_SIZE %du\n" % (GetSnapshotSize(cfg) + 1))
    H.write("#define DEM_MAX_EXTENDED_DATA_SIZE %du\n" % (GetExtendedDataSize(cfg) + 1))
    idx = 0
    for i, dtc in enumerate(cfg["DTCs"]):
        events = dtc.get("events", [dtc])
        for event in events:
            H.write("#define DEM_EVENT_ID_%s %su\n" % (event["name"], idx))
            idx += 1
    H.write("#define DTC_ENVENT_NUM %du\n" % (idx))
    H.write("#ifndef DEM_MAX_FREEZE_FRAME_RECORD\n")
    H.write("#define DEM_MAX_FREEZE_FRAME_RECORD DTC_ENVENT_NUM\n")
    H.write("#endif\n")
    H.write("#ifndef DEM_MAX_EXTENDED_DATA_RECORD\n")
    H.write("#define DEM_MAX_EXTENDED_DATA_RECORD DTC_ENVENT_NUM\n")
    H.write("#endif\n")
    conditions = []
    for dtc in cfg["DTCs"]:
        for c in dtc.get("conditions", []):
            if c not in conditions:
                conditions.append(c)
    if len(conditions) > 0:
        H.write("\n#define DEM_USE_ENABLE_CONDITION\n")
        H.write("#define DEM_NUM_OF_ENABLE_CONDITION %su\n\n" % (len(conditions)))
        for i, cond in enumerate(conditions):
            H.write("#define DEM_CONTIDION_%s (1u<<%s)\n" % (toMacro(cond), i))
    H.write("\n/* #define DEM_STATUS_BIT_STORAGE_TEST_FAILED */\n\n")
    H.write("#define DEM_RESET_CONFIRMED_BIT_ON_OVERFLOW\n\n")
    H.write("#define DEM_STATUS_BIT_HANDLING_TEST_FAILED_SINCE_LAST_CLEAR DEM_STATUS_BIT_AGING_AND_DISPLACEMENT\n\n")
    H.write("#define DEM_USE_NVM_EXTENDED_DATA\n\n")
    memories = cfg.get("Memories", [{"name": "Primary", "origin": "0x0001"}])
    for memory in memories:
        if toNum(memory["origin"]) == 0x0002:
            H.write("#define DEM_USE_MIRROR_MEMORY\n\n")
    H.write("%s#define DEM_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* DEM_CFG_H */\n")
    H.close()

    C = open("%s/Dem_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Dem_Priv.h"\n')
    C.write('#include "NvM_Cfg.h"\n')
    C.write("#ifdef USE_SHELL\n")
    C.write('#include "shell.h"\n')
    C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    for i, data in enumerate(cfg["Environments"]):
        C.write("#define DEM_FFD_%s %s\n" % (data["name"], i))
    C.write("\n")
    for i, data in enumerate(cfg["ExtendedDatas"]):
        C.write("#define DEM_EXTD_%s %s\n" % (data["name"], i))
    C.write("\n")
    for i, data in enumerate(cfg["ExtendedDatas"]):
        C.write("#define DEM_EXTD_%s_NUMBER %s\n" % (data["name"], hex(i + 1)))
    for i, memory in enumerate(memories):
        C.write("#define DEM_MEMORY_ID_%s %s\n" % (memory["name"], i))
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    for i, data in enumerate(cfg["Environments"]):
        C.write(
            "Std_ReturnType Dem_FFD_Get%s(Dem_DtcIdType EventId, uint8_t *data, Dem_DTCOriginType DTCOrigin);\n"
            % (data["name"])
        )
        gen_shell_data_print(C, data)
    C.write("\n")
    for i, data in enumerate(cfg["ExtendedDatas"]):
        C.write(
            "Std_ReturnType Dem_EXTD_Get%s(Dem_DtcIdType EventId, uint8_t *data, Dem_DTCOriginType DTCOrigin);\n"
            % (data["name"])
        )
    C.write("\n")
    idx = 0
    for i, dtc in enumerate(cfg["DTCs"]):
        events = dtc.get("events", [dtc])
        for event in events:
            C.write(
                "extern Dem_EventStatusRecordType Dem_NvmEventStatusRecord%s_Ram; /* %s */\n" % (idx, event["name"])
            )
            idx += 1
    for i, dtc in enumerate(cfg["DTCs"]):
        for memory in memories:
            C.write(
                "extern Dem_DtcStatusRecordType Dem_Nvm%sDtcStatusRecord%s_Ram; /* %s */\n"
                % (memory["name"], i, dtc["name"])
            )
    for i, dtc in enumerate(cfg["DTCs"]):
        C.write("#if DEM_MAX_FREEZE_FRAME_RECORD > %s\n" % (i))
        for memory in memories:
            C.write("extern Dem_FreezeFrameRecordType Dem_Nvm%sFreezeFrameRecord%s_Ram;\n" % (memory["name"], i))
        C.write("#endif\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        C.write("#if (DEM_MAX_EXTENDED_DATA_RECORD > %s) && defined(DEM_USE_NVM_EXTENDED_DATA)\n" % (i))
        for memory in memories:
            C.write("extern Dem_ExtendedDataRecordType Dem_Nvm%sExtendedDataRecord%s_Ram;\n" % (memory["name"], i))
        C.write("#endif\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#define DEM_START_SEC_CONST\n")
    C.write('#include "Dem_MemMap.h"\n')
    idx = 0
    C.write("static CONSTP2VAR(Dem_EventStatusRecordType, DEM_VAR, DEM_CONST) Dem_NvmEventStatusRecords[] = {\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        events = dtc.get("events", [dtc])
        for event in events:
            C.write("  &Dem_NvmEventStatusRecord%s_Ram, /* %s */\n" % (idx, event["name"]))
            idx += 1
    C.write("};\n\n")
    idx = 0
    C.write("static CONSTANT(uint16_t, DEM_CONST) Dem_NvmEventStatusRecordNvmBlockIds[] = {\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        events = dtc.get("events", [dtc])
        for event in events:
            C.write("  NVM_BLOCKID_Dem_NvmEventStatusRecord%s, /* %s */\n" % (idx, event["name"]))
            idx += 1
    C.write("};\n\n")
    for memory in memories:
        C.write(
            "static CONSTP2VAR(Dem_DtcStatusRecordType, DEM_VAR, DEM_CONST) Dem_Nvm%sDtcStatusRecord[] = {\n"
            % (memory["name"])
        )
        for i, dtc in enumerate(cfg["DTCs"]):
            C.write("  &Dem_Nvm%sDtcStatusRecord%s_Ram,\n" % (memory["name"], i))
        C.write("};\n\n")
        C.write("#ifdef DEM_USE_NVM\n")
        C.write("static CONSTANT(uint16_t, DEM_CONST) Dem_Nvm%sDtcStatusRecordNvmBlockIds[] = {\n" % (memory["name"]))
        for i, data in enumerate(cfg["DTCs"]):
            C.write("  NVM_BLOCKID_Dem_Nvm%sDtcStatusRecord%s,\n" % (memory["name"], i))
        C.write("};\n")
        C.write("#endif\n\n")
    for memory in memories:
        C.write(
            "static CONSTP2VAR(Dem_FreezeFrameRecordType, DEM_VAR, DEM_CONST) Dem_Nvm%sFreezeFrameRecord[] = {\n"
            % (memory["name"])
        )
        for i, dtc in enumerate(cfg["DTCs"]):
            C.write("#if DEM_MAX_FREEZE_FRAME_RECORD > %s\n" % (i))
            C.write("  &Dem_Nvm%sFreezeFrameRecord%s_Ram,\n" % (memory["name"], i))
            C.write("#endif\n")
        C.write("};\n\n")
        C.write("#ifdef DEM_USE_NVM\n")
        C.write("static CONSTANT(uint16_t, DEM_CONST) Dem_Nvm%sFreezeFrameNvmBlockIds[] = {\n" % (memory["name"]))
        for i, data in enumerate(cfg["DTCs"]):
            C.write("#if DEM_MAX_FREEZE_FRAME_RECORD > %s\n" % (i))
            C.write("  NVM_BLOCKID_Dem_Nvm%sFreezeFrameRecord%s,\n" % (memory["name"], i))
            C.write("#endif\n")
        C.write("};\n")
        C.write("#endif\n\n")
    for memory in memories:
        C.write("#ifdef DEM_USE_NVM_EXTENDED_DATA\n")
        C.write(
            "static CONSTP2VAR(Dem_ExtendedDataRecordType, DEM_VAR, DEM_CONST) Dem_Nvm%sExtendedDataRecord[] = {\n"
            % (memory["name"])
        )
        for i, dtc in enumerate(cfg["DTCs"]):
            C.write("#if DEM_MAX_EXTENDED_DATA_RECORD > %s\n" % (i))
            C.write("  &Dem_Nvm%sExtendedDataRecord%s_Ram,\n" % (memory["name"], i))
            C.write("#endif\n")
        C.write("};\n")
        C.write("#ifdef DEM_USE_NVM\n")
        C.write("static CONSTANT(uint16_t, DEM_CONST) Dem_Nvm%sExtendedDataNvmBlockIds[] = {\n" % (memory["name"]))
        for i, data in enumerate(cfg["DTCs"]):
            C.write("#if DEM_MAX_EXTENDED_DATA_RECORD > %s\n" % (i))
            C.write("  NVM_BLOCKID_Dem_Nvm%sExtendedDataRecord%s,\n" % (memory["name"], i))
            C.write("#endif\n")
        C.write("};\n")
        C.write("#endif\n")
        C.write("#endif\n\n")
    C.write("#ifndef DEM_USE_NVM\n")
    for memory in memories:
        C.write(
            "static uint8_t Dem_Nvm%sDtcStatusRecordDirty[(ARRAY_SIZE(Dem_Nvm%sDtcStatusRecord)+7)/8];\n"
            % (memory["name"], memory["name"])
        )
        C.write(
            "static uint8_t Dem_Nvm%sFreezeFrameRecordDirty[(ARRAY_SIZE(Dem_Nvm%sFreezeFrameRecord)+7)/8];\n"
            % (memory["name"], memory["name"])
        )
        C.write("#ifdef DEM_USE_NVM_EXTENDED_DATA\n")
        C.write(
            "static uint8_t Dem_Nvm%sExtendedDataDirty[(ARRAY_SIZE(Dem_Nvm%sExtendedDataRecord)+7)/8];\n"
            % (memory["name"], memory["name"])
        )
        C.write("#endif\n")
    C.write("#endif\n\n")
    C.write("static CONSTANT(Dem_MemoryDestinationType, DEM_CONST) Dem_MemoryDestination[] = {\n")
    for memory in memories:
        C.write("  {\n")
        C.write("    Dem_Nvm%sDtcStatusRecord,\n" % (memory["name"]))
        C.write("    Dem_Nvm%sFreezeFrameRecord,\n" % (memory["name"]))
        C.write("    #ifdef DEM_USE_NVM_EXTENDED_DATA\n")
        C.write("    Dem_Nvm%sExtendedDataRecord,\n" % (memory["name"]))
        C.write("    #endif\n")
        C.write("    #ifndef DEM_USE_NVM\n")
        C.write("    Dem_Nvm%sDtcStatusRecordDirty,\n" % (memory["name"]))
        C.write("    Dem_Nvm%sFreezeFrameRecordDirty,\n" % (memory["name"]))
        C.write("    #ifdef DEM_USE_NVM_EXTENDED_DATA\n")
        C.write("    Dem_Nvm%sExtendedDataDirty,\n" % (memory["name"]))
        C.write("    #endif\n")
        C.write("    #else\n")
        C.write("    Dem_Nvm%sDtcStatusRecordNvmBlockIds,\n" % (memory["name"]))
        C.write("    Dem_Nvm%sFreezeFrameNvmBlockIds,\n" % (memory["name"]))
        C.write("    #ifdef DEM_USE_NVM_EXTENDED_DATA\n")
        C.write("    Dem_Nvm%sExtendedDataNvmBlockIds,\n" % (memory["name"]))
        C.write("    #endif\n")
        C.write("    #endif\n")
        C.write("    ARRAY_SIZE(Dem_Nvm%sDtcStatusRecord),\n" % (memory["name"]))
        C.write("    ARRAY_SIZE(Dem_Nvm%sFreezeFrameRecord),\n" % (memory["name"]))
        C.write("    #ifdef DEM_USE_NVM_EXTENDED_DATA\n")
        C.write("    ARRAY_SIZE(Dem_Nvm%sExtendedDataRecord),\n" % (memory["name"]))
        C.write("    #endif\n")
        C.write("    %s/* DTCOrigin */\n" % (memory["origin"]))
        C.write("  },\n")
    C.write("};\n\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        C.write(
            "static CONSTP2CONST(Dem_MemoryDestinationType, DEM_CONST, DEM_CONST) Dem_Dtc%sMemoryDestinations[]= {\n"
            % (i)
        )
        for memory in dtc.get("destination", ["Primary"]):
            C.write("  &Dem_MemoryDestination[DEM_MEMORY_ID_%s],\n" % (memory))
        C.write("};\n\n")
    C.write("static CONSTANT(Dem_FreeFrameDataConfigType, DEM_CONST) FreeFrameDataConfigs[] = {\n")
    for x in envsAll:
        data = envsMapByName[x]
        C.write("  {\n")
        C.write("    Dem_FFD_Get%s,\n" % (data["name"]))
        C.write("    %s,\n" % (data["id"]))
        C.write("    %s,\n" % (GetDataSize(data)))
        C.write("#ifdef USE_SHELL\n")
        C.write("    Dem_FFD_Print%s,\n" % (data["name"]))
        C.write("#endif\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dem_ExtendedDataConfigType, DEM_CONST) ExtendedDataConfigs[] = {\n")
    for i, data in enumerate(cfg["ExtendedDatas"]):
        C.write("  {Dem_EXTD_Get%s, DEM_EXTD_%s_NUMBER, %s},\n" % (data["name"], data["name"], GetDataSize(data)))
    C.write("};\n\n")
    dbncs = {}
    dbnc_dfts = {
        "DebounceCounterDecrementStepSize": 2,
        "DebounceCounterFailedThreshold": 10,
        "DebounceCounterIncrementStepSize": 1,
        "DebounceCounterJumpDown": False,
        "DebounceCounterJumpDownValue": 0,
        "DebounceCounterJumpUp": True,
        "DebounceCounterJumpUpValue": 0,
        "DebounceCounterPassedThreshold": -10,
    }
    for dtc in cfg["DTCs"]:
        dbnc = {key: GetProp(dtc, general, key, dft) for key, dft in dbnc_dfts.items()}
        vp = "-".join([str(v) for k, v in dbnc.items()])
        if vp not in dbncs:
            dbnc["id"] = len(dbncs.keys())
            dbncs[vp] = dbnc
    for vp, dbnc in dbncs.items():
        idx = dbnc["id"]
        C.write(
            "static CONSTANT(Dem_DebounceCounterBasedConfigType, DEM_CONST) Dem_DebounceCounterBasedConfig%s = {\n"
            % (idx)
        )
        C.write("  DEM_DEBOUNCE_FREEZE,\n")
        C.write("  /* DebounceCounterDecrementStepSize */ %s,\n" % (dbnc["DebounceCounterDecrementStepSize"]))
        C.write("  /* DebounceCounterFailedThreshold */ %s,\n" % (dbnc["DebounceCounterFailedThreshold"]))
        C.write("  /* DebounceCounterIncrementStepSize */ %s,\n" % (dbnc["DebounceCounterIncrementStepSize"]))
        C.write("  /* DebounceCounterJumpDown */ %s,\n" % (str(dbnc["DebounceCounterJumpDown"]).upper()))
        C.write("  /* DebounceCounterJumpDownValue */ %s,\n" % (dbnc["DebounceCounterJumpDownValue"]))
        C.write("  /* DebounceCounterJumpUp */ %s,\n" % (str(dbnc["DebounceCounterJumpUp"]).upper()))
        C.write("  /* DebounceCounterJumpUpValue */ %s,\n" % (dbnc["DebounceCounterJumpUpValue"]))
        C.write("  /* DebounceCounterPassedThreshold */ %s,\n" % (dbnc["DebounceCounterPassedThreshold"]))
        C.write("};\n\n")
    C.write("/* each Event can have different environment data that cares about */\n")
    for dtc in cfg["DTCs"]:
        envs = dtc.get("Environments", [])
        if len(envs) > 0:
            envs.sort(key=lambda x: toNum(envsMapByName[x]["id"]))
            envsKey = ".".join(envs)
            if envsKey not in envMap:
                idx = len(envMap)
                C.write("static CONSTANT(uint16_t, DEM_CONST) Dem_FreezeFrameDataIndex%s[] = {\n" % (idx))
                for env in envs:
                    C.write("  DEM_FFD_%s,\n" % (env))
                C.write("};\n\n")
                envMap[envsKey] = idx
                C.write(
                    "static CONSTANT(Dem_FreezeFrameRecordClassType, DEM_CONST) Dem_FreezeFrameRecordClass%s = {\n"
                    % (idx)
                )
                C.write("  Dem_FreezeFrameDataIndex%s,\n" % (idx))
                C.write("  ARRAY_SIZE(Dem_FreezeFrameDataIndex%s),\n" % (idx))
                C.write("};\n\n")
    C.write("static CONSTANT(uint16_t, DEM_CONST) Dem_FreezeFrameDataIndexDefault[] = {\n")
    for x in envsAll:
        data = envsMapByName[x]
        C.write("  DEM_FFD_%s,\n" % (data["name"]))
    C.write("};\n\n")
    C.write("/* each Event can have different extended data that cares about*/\n")
    C.write("static CONSTANT(uint8_t, DEM_CONST) Dem_ExtendedDataNumberIndexDefault[] = {\n")
    for data in cfg["ExtendedDatas"]:
        C.write("  DEM_EXTD_%s,\n" % (data["name"]))
    C.write("};\n\n")
    C.write("static CONSTANT(Dem_FreezeFrameRecordClassType, DEM_CONST) Dem_FreezeFrameRecordClassDefault = {\n")
    C.write("  Dem_FreezeFrameDataIndexDefault,\n")
    C.write("  ARRAY_SIZE(Dem_FreezeFrameDataIndexDefault),\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dem_ExtendedDataRecordClassType, DEM_CONST) Dem_ExtendedDataRecordClassDefault = {\n")
    C.write("  Dem_ExtendedDataNumberIndexDefault,\n")
    C.write("  ARRAY_SIZE(Dem_ExtendedDataNumberIndexDefault),\n")
    C.write("  /* ExtendedDataRecordNumber */ 1,\n")
    C.write("};\n\n")
    C.write(
        "static CONSTP2CONST(Dem_ExtendedDataRecordClassType, DEM_CONST, DEM_CONST) Dem_ExtendedDataRecordClassRefsDefault[] = {\n"
    )
    C.write("  &Dem_ExtendedDataRecordClassDefault,\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dem_ExtendedDataClassType, DEM_CONST) Dem_ExtendedDataClassDefault = {\n")
    C.write("  Dem_ExtendedDataRecordClassRefsDefault,\n")
    C.write("  ARRAY_SIZE(Dem_ExtendedDataRecordClassRefsDefault),\n")
    C.write("};\n\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        FreezeFrameRecNums = dtc.get("FreezeFrameRecNums", [str(i + 1) for i in range(MaxFreezeFrameNum)])
        C.write(
            "static CONSTANT(uint8_t, DEM_CONST) Dem_FreezeFrameRecNumsFor%s[] = {%s};\n"
            % (dtc["name"], ", ".join(FreezeFrameRecNums))
        )
    C.write("static CONSTANT(Dem_FreezeFrameRecNumClassType, DEM_CONST) Dem_FreezeFrameRecNumClass[] = {\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        C.write("  {\n")
        C.write("    Dem_FreezeFrameRecNumsFor%s,\n" % (dtc["name"]))
        C.write("    ARRAY_SIZE(Dem_FreezeFrameRecNumsFor%s),\n" % (dtc["name"]))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dem_DTCAttributesType, DEM_CONST) Dem_DTCAttributes[] = {\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        envs = dtc.get("Environments", [])
        if len(envs) > 0:
            envs.sort(key=lambda x: toNum(envsMapByName[x]["id"]))
            envsKey = ".".join(envs)
            FFRef = envMap[envsKey]
        else:
            FFRef = "Default"
        C.write("  {\n")
        C.write("    &Dem_ExtendedDataClassDefault,\n")
        C.write("    &Dem_FreezeFrameRecordClass%s,\n" % (FFRef))
        C.write("    &Dem_FreezeFrameRecNumClass[%s],\n" % (i))
        C.write("    Dem_Dtc%sMemoryDestinations,\n" % (i))
        C.write("    ARRAY_SIZE(Dem_Dtc%sMemoryDestinations),\n" % (i))
        C.write("    /* Priority */ %s,\n" % (dtc["priority"]))
        C.write("    /* AgingAllowed */ TRUE,\n")
        C.write("    /* AgingCycleCounterThreshold */ %s,\n" % (GetProp(dtc, general, "AgingCycleCounterThreshold", 2)))
        C.write(
            "    /* OccurrenceCounterProcessing */ DEM_PROCESS_OCCCTR_%s,\n"
            % (GetProp(dtc, general, "OccurrenceCounterProcessing", "TF"))
        )
        C.write(
            "    /* FreezeFrameRecordTrigger */ DEM_TRIGGER_ON_%s,\n"
            % (GetProp(dtc, general, "FreezeFrameRecordTrigger", "TEST_FAILED"))
        )
        C.write("    #ifdef DEM_USE_NVM_EXTENDED_DATA\n")
        C.write(
            "    /* ExtendedDataRecordTrigger */ DEM_TRIGGER_ON_%s,\n"
            % (GetProp(dtc, general, "ExtendedDataRecordTrigger", "TEST_FAILED"))
        )
        C.write("    #endif\n")
        C.write("    /* DebounceAlgorithmClass */ DEM_DEBOUNCE_COUNTER_BASED,\n")
        C.write("    /* EnvironmentDataCapture */ DEM_CAPTURE_SYNCHRONOUS_TO_REPORTING,\n")
        C.write("  },\n")
    C.write("};\n\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        C.write("static CONSTANT(Dem_EventIdType, DEM_CONST) Dem_Dtc%sEventRefs[] = {\n" % (i))
        events = dtc.get("events", [dtc])
        for event in events:
            C.write("  DEM_EVENT_ID_%s,\n" % (event["name"]))
        C.write("};\n\n")
    C.write("\n")
    C.write("static CONSTANT(Dem_DTCType, DEM_CONST) Dem_Dtcs[] = {\n")
    for i, dtc in enumerate(cfg["DTCs"]):
        C.write("  {\n")
        C.write("    &Dem_DTCAttributes[%s],\n" % (i))
        C.write("    Dem_Dtc%sEventRefs,\n" % (i))
        C.write("    %s,\n" % (dtc["number"]))
        C.write("    %s, /* DtcId */\n" % (i))
        C.write("    ARRAY_SIZE(Dem_Dtc%sEventRefs),\n" % (i))
        C.write("    #if 0\n")
        C.write("    0, /* FunctionalUnit */\n")
        C.write("    DEM_SEVERITY_NO_SEVERITY, /* Severity */\n")
        C.write("    DEM_NV_STORE_IMMEDIATE_AT_FIRST_OCCURRENCE, /* NvStorageStrategy */\n")
        C.write("    #endif\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dem_EventConfigType, DEM_CONST) Dem_EventConfigs[DTC_ENVENT_NUM] = {\n")
    idx = 0
    for i, dtc in enumerate(cfg["DTCs"]):
        vp = "-".join([str(GetProp(dtc, general, key, dft)) for key, dft in dbnc_dfts.items()])
        dbnc = dbncs[vp]
        events = dtc.get("events", [dtc])
        for event in events:
            C.write("  {\n")
            C.write("    &Dem_Dtcs[%s],\n" % (i))
            C.write("    /* DemCallbackInitMForE */ NULL,\n")
            C.write("    &Dem_DebounceCounterBasedConfig%s,\n" % (dbnc["id"]))
            C.write("    #ifdef DEM_USE_ENABLE_CONDITION\n")
            mask = "0"
            for cond in dtc.get("conditions", []):
                mask += "|DEM_CONTIDION_%s" % (toMacro(cond))
            C.write("    %s,\n" % (mask))
            C.write("    #endif\n")
            C.write("    /* ConfirmationThreshold */ %s,\n" % (GetProp(event, general, "ConfirmationThreshold", 1)))
            C.write(
                "    /* OperationCycleRef */ DEM_OPERATION_CYCLE_%s,\n"
                % (GetProp(event, general, "OperationCycleRef", "IGNITION"))
            )
            C.write("    /* RecoverableInSameOperationCycle */ FALSE,\n")
            C.write("  },\n")
            idx += 1
    C.write("};\n\n")
    C.write("#ifndef DEM_USE_NVM\n")
    C.write("static uint8_t Dem_EventStatusDirty[(ARRAY_SIZE(Dem_EventConfigs)+7)/8];\n")
    C.write("#endif\n")
    C.write("static Dem_EventContextType Dem_EventContexts[DTC_ENVENT_NUM];\n")
    C.write("static Dem_OperationCycleStateType Dem_OperationCycleStates[2];\n")
    C.write("CONSTANT(Dem_ConfigType, DEM_CONST) Dem_Config = {\n")
    C.write("  FreeFrameDataConfigs,\n")
    C.write("  ExtendedDataConfigs,\n")
    C.write("  Dem_EventConfigs,\n")
    C.write("  Dem_EventContexts,\n")
    C.write("  Dem_NvmEventStatusRecords,\n")
    C.write("#ifdef DEM_USE_NVM\n")
    C.write("  Dem_NvmEventStatusRecordNvmBlockIds,\n")
    C.write("#else\n")
    C.write("  Dem_EventStatusDirty,\n")
    C.write("#endif\n")
    C.write("  Dem_Dtcs,\n")
    C.write("  Dem_MemoryDestination,\n")
    C.write("  Dem_OperationCycleStates,\n")
    C.write("  ARRAY_SIZE(FreeFrameDataConfigs),\n")
    C.write("  ARRAY_SIZE(Dem_EventConfigs),\n")
    C.write("  ARRAY_SIZE(Dem_Dtcs),\n")
    C.write("  ARRAY_SIZE(Dem_MemoryDestination),\n")
    C.write("  ARRAY_SIZE(ExtendedDataConfigs),\n")
    C.write("  ARRAY_SIZE(Dem_OperationCycleStates),\n")
    C.write("  /* TypeOfFreezeFrameRecordNumeration */ DEM_FF_RECNUM_CONFIGURED,\n")
    C.write("  /* StatusAvailabilityMask */ 0x%02x,\n" % (toNum(cfg.get("StatusAvailabilityMask", 0x7F))))
    C.write("};\n")
    C.write("#define DEM_STOP_SEC_CONST\n")
    C.write('#include "Dem_MemMap.h"\n')
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_Dem(cfg, dir)
    GenMemMap("Dem", dir)
    return ["%s/Dem_Cfg.c" % (dir)]
