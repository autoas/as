# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *
from .MemMap import *

__all__ = ["Gen"]


def GetName(node):
    if node["name"][-2:] == "{}":
        return node["name"][:-2]
    return node["name"]


def GetBlockSize(block):
    size = 0
    for data in block["data"]:
        size += GetDataSize(data) * block.get("size", 1)
    return size


def GetMaxDataSize(cfg):
    maxSize = 0
    for block in cfg["blocks"]:
        size = 0
        for data in block["data"]:
            size += TypeInfoMap[data["type"]]["size"] * data.get("size", 1) * data.get("repeat", 1)
        if size > maxSize:
            maxSize = size
    return maxSize


def GenTypes(H, cfg):
    for block in cfg["blocks"]:
        H.write("typedef struct {\n")
        for data in block["data"]:
            dinfo = TypeInfoMap[data["type"]]
            cstr = "  %s %s" % (dinfo["ctype"], GetName(data))
            if data["name"][-2:] == "{}":
                cstr += "[%s]" % (data["repeat"])
            if dinfo["IsArray"]:
                cstr += "[%s]" % (data["size"])
            cstr += ";\n"
            H.write(cstr)
        H.write("} %sType;\n\n" % (GetName(block)))


def GenConstants(C, cfg, module):
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(1, repeat):
            name = block["name"].format(i)
            C.write("#define %s_Rom %s_Rom\n" % (name, block["name"].format(0)))
        name = block["name"].format(0)
        cstr = "static CONSTANT(%sType, %s_CONST) %s_Rom = { " % (GetName(block), module, name)
        for data in block["data"]:
            dft = data["default"]
            if type(dft) == str:
                cstr += "%s, " % (str(eval(dft)).replace("[", "{").replace("]", "}"))
            else:
                cstr += "%s, " % (dft)
        cstr += "};\n\n"
        C.write(cstr)


def Gen_Ea(cfg, dir):
    H = open("%s/Ea_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef _EA_CFG_H\n")
    H.write("#define _EA_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef EA_PAGE_SIZE\n")
    H.write("#define EA_PAGE_SIZE 1\n")
    H.write("#endif\n\n")
    H.write("#ifndef EA_SECTOR_SIZE\n")
    H.write("#define EA_SECTOR_SIZE 4\n")
    H.write("#endif\n\n")
    H.write("#define EA_MAX_DATA_SIZE %d\n\n" % (GetMaxDataSize(cfg)))
    Number = 1
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            H.write("#define EA_NUMBER_%s %s\n" % (name, Number))
            Number += 1
    H.write("#ifndef EA_CONST\n")
    H.write("#define EA_CONST\n")
    H.write("#endif\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    GenTypes(H, cfg)
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* _EA_CFG_H */\n")
    H.close()

    C = open("%s/Ea_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Ea.h"\n')
    C.write('#include "Ea_Cfg.h"\n')
    C.write('#include "Ea_Priv.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("void NvM_JobEndNotification(void);\n")
    C.write("void NvM_JobErrorNotification(void);\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#define EA_START_SEC_CONST\n")
    C.write('#include "Ea_MemMap.h"\n')
    C.write("static CONSTANT(Ea_BlockConfigType, EA_CONST) Ea_BlockConfigs[] = {\n")
    Cnt = 0
    Address = 0
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            size = GetBlockSize(block)
            NumberOfWriteCycles = block.get("NumberOfWriteCycles", 10000000)
            C.write("  { EA_NUMBER_%s, %s, %s+2, %s },\n" % (name, Address, size, NumberOfWriteCycles))
            Address += int(((size + 2 + 3) / 4)) * 4
            Cnt += 1
    C.write("};\n\n")

    C.write("CONSTANT(Ea_ConfigType, EA_CONST) Ea_Config = {\n")
    C.write("  NvM_JobEndNotification,\n")
    C.write("  NvM_JobErrorNotification,\n")
    C.write("  Ea_BlockConfigs,\n")
    C.write("  ARRAY_SIZE(Ea_BlockConfigs),\n")
    C.write("};\n")
    C.write("#define EA_STOP_SEC_CONST\n")
    C.write('#include "Ea_MemMap.h"\n')
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen_Fee(cfg, dir):
    H = open("%s/Fee_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef FEE_CFG_H\n")
    H.write("#define FEE_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#define FEE_MAX_DATA_SIZE %d\n\n" % (GetMaxDataSize(cfg)))
    Number = 1
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            H.write("#define FEE_NUMBER_%s %s\n" % (name, Number))
            Number += 1
    H.write("#ifndef FEE_CONST\n")
    H.write("#define FEE_CONST\n")
    H.write("#endif\n")
    H.write("%s#define FEE_USE_CONTEXT_CRC\n\n" % ("" if cfg.get("UseContextCrc", True) else "// "))
    H.write("%s#define FEE_USE_FAULTS\n\n" % ("" if cfg.get("UseFaults", True) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    GenTypes(H, cfg)
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* FEE_CFG_H */\n")
    H.close()

    C = open("%s/Fee_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Fee.h"\n')
    C.write('#include "Fee_Cfg.h"\n')
    C.write('#include "Fee_Priv.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    banks = cfg.get("banks", [])
    if len(banks) == 0:
        C.write("#ifndef FLS_BANK0_ADDRESS\n")
        C.write("#define FLS_BANK0_ADDRESS 0\n")
        C.write("#endif\n\n")

        C.write("#ifndef FLS_BANK0_SIZE\n")
        C.write("#define FLS_BANK0_SIZE (32 * 1024)\n")
        C.write("#endif\n\n")

        C.write("#ifndef FLS_BANK1_ADDRESS\n")
        C.write("#define FLS_BANK1_ADDRESS (FLS_BANK0_ADDRESS + FLS_BANK0_SIZE)\n")
        C.write("#endif\n\n")

        C.write("#ifndef FLS_BANK1_SIZE\n")
        C.write("#define FLS_BANK1_SIZE (32 * 1024)\n")
        C.write("#endif\n\n")

    C.write("#ifndef FEE_MAX_JOB_RETRY\n")
    C.write("#define FEE_MAX_JOB_RETRY 0xFF\n")
    C.write("#endif\n\n")

    C.write("#ifndef FEE_MAX_ERASED_NUMBER\n")
    C.write("#define FEE_MAX_ERASED_NUMBER 1000000\n")
    C.write("#endif\n\n")

    maxSize = GetMaxDataSize(cfg)
    # need at least 3*sizeof(Fee_BankAdminType), if page size is 8, that is 3*32 = 96
    if maxSize < 128:
        maxSize = 128
    maxSize = int((maxSize + 31 + 32) / 32) * 32
    C.write("#ifndef FEE_WORKING_AREA_SIZE\n")
    C.write("#define FEE_WORKING_AREA_SIZE %s\n" % (maxSize))
    C.write("#endif\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("void NvM_JobEndNotification(void);\n")
    C.write("void NvM_JobErrorNotification(void);\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#define FEE_START_SEC_CONST\n")
    C.write('#include "Fee_MemMap.h"\n')
    GenConstants(C, cfg, "FEE")
    C.write("static CONSTANT(Fee_BlockConfigType, FEE_CONST) Fee_BlockConfigs[] = {\n")
    Cnt = 0
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            size = GetBlockSize(block)
            NumberOfWriteCycles = block.get("NumberOfWriteCycles", 10000000)
            C.write("  { FEE_NUMBER_%s, %s, %s, (const uint8_t*)&%s_Rom },\n" % (name, size, NumberOfWriteCycles, name))
            Cnt += 1
    C.write("};\n\n")
    C.write("static Fee_BlockContextType Fee_BlockContexts[%s];\n" % (Cnt))

    C.write("static CONSTANT(Fee_BankType, FEE_CONST) Fee_Banks[] = {\n")
    if len(banks) == 0:
        C.write("  {FLS_BANK0_ADDRESS, FLS_BANK0_ADDRESS + FLS_BANK0_SIZE},\n")
        C.write("  {FLS_BANK1_ADDRESS, FLS_BANK1_ADDRESS + FLS_BANK1_SIZE},\n")
    else:
        for bank in banks:
            addr = toNum(bank["address"])
            size = toNum(bank["size"])
            for i in range(bank.get("repeat", 1)):
                C.write("  {0x%x, 0x%x},\n" % (addr + i * size, addr + (i + 1) * size))
    C.write("};\n\n")

    C.write("static uint32_t Fee_WorkingArea[FEE_WORKING_AREA_SIZE/sizeof(uint32_t)];\n")
    C.write("CONSTANT(Fee_ConfigType, FEE_CONST) Fee_Config = {\n")
    C.write("  NvM_JobEndNotification,\n")
    C.write("  NvM_JobErrorNotification,\n")
    C.write("  Fee_BlockContexts,\n")
    C.write("  Fee_BlockConfigs,\n")
    C.write("  ARRAY_SIZE(Fee_BlockConfigs),\n")
    C.write("  Fee_Banks,\n")
    C.write("  ARRAY_SIZE(Fee_Banks),\n")
    C.write("  (uint8_t*)Fee_WorkingArea,\n")
    C.write("  sizeof(Fee_WorkingArea),\n")
    C.write("  FEE_MAX_JOB_RETRY,\n")
    C.write("  FEE_MAX_DATA_SIZE,\n")
    C.write("  FEE_MAX_ERASED_NUMBER,\n")
    C.write("};\n")
    C.write("#define FEE_STOP_SEC_CONST\n")
    C.write('#include "Fee_MemMap.h"\n')
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen_NvM(cfg, dir):
    H = open("%s/NvM_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef NVM_CFG_H\n")
    H.write("#define NVM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "Std_Types.h"\n')
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    target = os.getenv("NVM_TARGET", cfg.get("target", "Ea"))
    if target != "Fee":
        H.write("#define NVM_BLOCK_USE_CRC\n")
    else:
        H.write("/* NVM target is FEE, CRC is not used */\n")
    H.write("#define NVM_BLOCK_USE_STATUS\n")
    H.write("#define MEMIF_ZERO_COST_%s\n" % (target.upper()))
    Number = 2
    NvMTestInfo = {"blocks": []}
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            H.write("#define NVM_BLOCKID_%s %s\n" % (name, Number))
            NvMTestInfo["blocks"].append({"name": name, "number": Number, "size": GetBlockSize(block)})
            Number += 1
    with open("%s/NvMTest.json" % (dir), "w") as f:
        json.dump(NvMTestInfo, f, indent=2)
    H.write("#define NVM_BLOCK_NUMBER %d\n" % (Number - 1))
    H.write("#ifndef NVM_CONST\n")
    H.write("#define NVM_CONST\n")
    H.write("#endif\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    GenTypes(H, cfg)
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* NVM_CFG_H */\n")
    H.close()

    C = open("%s/NvM_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "NvM.h"\n')
    C.write('#include "NvM_Cfg.h"\n')
    C.write('#include "NvM_Priv.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    if target != "Fee":
        maxSize = GetMaxDataSize(cfg)
        maxSize = int((maxSize + 4 + 3) / 4) * 4
        C.write("#ifndef NVM_WORKING_AREA_SIZE\n")
        C.write("#define NVM_WORKING_AREA_SIZE %s\n" % (maxSize))
        C.write("#endif\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#define NVM_START_SEC_CONST\n")
    C.write('#include "NvM_MemMap.h"\n')
    if target != "Fee":
        GenConstants(C, cfg, "NVM")
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            C.write("%sType %s_Ram;\n" % (GetName(block), name))
    C.write("static CONSTANT(NvM_BlockDescriptorType, NVM_CONST) NvM_BlockDescriptors[] = {\n")
    Number = 1
    for block in cfg["blocks"]:
        repeat = block.get("repeat", 1)
        for i in range(repeat):
            name = block["name"].format(i)
            if target != "Fee":
                C.write(
                    "  { &%s_Ram, %s, sizeof(%sType), NVM_CRC16, &%s_Rom },\n" % (name, Number, GetName(block), name)
                )
            else:
                C.write("  { &%s_Ram, %s, sizeof(%sType) },\n" % (name, Number, GetName(block)))
            Number += 1
    C.write("};\n\n")
    C.write("static uint16_t NvM_JobReadMasks[(NVM_BLOCK_NUMBER+15)/16];\n")
    C.write("static uint16_t NvM_JobWriteMasks[(NVM_BLOCK_NUMBER+15)/16];\n")
    C.write("#ifdef NVM_BLOCK_USE_STATUS\n")
    C.write("static NvM_RequestResultType NvM_BlockStatus[NVM_BLOCK_NUMBER];\n")
    C.write("#endif\n")
    if target != "Fee":
        C.write("static uint8_t NvM_WorkingArea[NVM_WORKING_AREA_SIZE];\n")
    C.write("CONSTANT(NvM_ConfigType, NVM_CONST) NvM_Config = {\n")
    C.write("  NvM_BlockDescriptors,\n")
    C.write("  ARRAY_SIZE(NvM_BlockDescriptors),\n")
    C.write("  NvM_JobReadMasks,\n")
    C.write("  NvM_JobWriteMasks,\n")
    C.write("  #ifdef NVM_BLOCK_USE_STATUS\n")
    C.write("  NvM_BlockStatus,\n")
    C.write("  #endif\n")
    if target != "Fee":
        C.write("  NvM_WorkingArea,\n")
    C.write("};\n")
    C.write("#define NVM_STOP_SEC_CONST\n")
    C.write('#include "NvM_MemMap.h"\n')
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    source = {"NvM": ["%s/NvM_Cfg.c" % (dir)]}
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    target = os.getenv("NVM_TARGET", cfg.get("target", "Ea"))
    if target == "Fee":
        Gen_Fee(cfg, dir)
        GenMemMap("Fee", dir)
        source["Fee"] = ["%s/Fee_Cfg.c" % (dir)]
    elif target == "Ea":
        Gen_Ea(cfg, dir)
        GenMemMap("Ea", dir)
        source["Ea"] = ["%s/Fee_Cfg.c" % (dir)]
    else:
        raise
    Gen_NvM(cfg, dir)
    GenMemMap("NvM", dir)
    return source
