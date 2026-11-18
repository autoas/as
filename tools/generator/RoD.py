# SSAS - Simple Smart Automotive Software
# Copyright (C) 20214Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *
from .MemMap import *
from pycrc.algorithms import Crc

__all__ = ["Gen"]


def Calcaulte_Crc16(data):
    CrcEng = Crc(width=16, poly=0x1021, reflect_in=False, xor_in=0xFFFF, reflect_out=False, xor_out=0x0000)
    crc = CrcEng.table_driven(data)
    return crc


def GetName(node):
    return node["Name"]


def GenTypes(H, cfg):
    def CollectFunctions(data, functions):
        for sd in data:
            if sd["Type"] == "struct":
                CollectFunctions(sd["Data"], functions)
            elif sd["Type"] == "function" and "Value" in sd:
                func_name = sd["Value"]
                if func_name and func_name != "NULL" and func_name not in functions:
                    functions[func_name] = sd

    all_functions = {}
    for block in cfg["Blocks"]:
        CollectFunctions(block["Data"], all_functions)

    for func_name, func_info in all_functions.items():
        ret_type = func_info.get("Return", "void")
        args = ", ".join(func_info.get("Args", []))
        H.write("extern %s %s(%s);\n" % (ret_type, func_name, args))
    if all_functions:
        H.write("\n")

    def GenStructDef(name, data, all_structs, fnc_types):
        struct_def = ""
        for sd in data:
            if sd["Type"] == "struct":
                nested_name = GetName(sd)
                if nested_name not in all_structs:
                    all_structs[nested_name] = sd["Data"]
                    struct_def += GenStructDef(nested_name, sd["Data"], all_structs, fnc_types)

        struct_def += "typedef struct {\n"
        for sd in data:
            if sd["Type"] == "struct":
                cstr = "  RoD_%sType %s;\n" % (GetName(sd), GetName(sd))
                struct_def += cstr
            elif sd["Type"] == "function":
                ret_type = sd.get("Return", "void")
                func_name = sd.get("Value", "")
                if func_name:
                    fnc_type_name = "%sFncType" % func_name
                    if fnc_type_name not in fnc_types:
                        fnc_types[fnc_type_name] = True
                        H.write("typedef %s (*%s)(%s);\n\n" % (ret_type, fnc_type_name, ", ".join(sd.get("Args", []))))
                    struct_def += "  %s %s;\n" % (fnc_type_name, GetName(sd))
                else:
                    func_type = "%s (*%s)(%s);\n" % (ret_type, GetName(sd), ", ".join(sd.get("Args", [])))
                    struct_def += "  " + func_type
            else:
                dinfo = TypeInfoMap[sd["Type"]]
                cstr = "  %s %s" % (dinfo["ctype"], GetName(sd))
                if dinfo["IsArray"]:
                    cstr += "[%s]" % (sd.get("Size", 1))
                cstr += ";\n"
                struct_def += cstr
        struct_def += "} RoD_%sType;\n\n" % (name)
        return struct_def

    for block in cfg["Blocks"]:
        all_structs = {}
        fnc_types = {}
        struct_def = GenStructDef(GetName(block), block["Data"], all_structs, fnc_types)
        H.write(struct_def)


def PlaceToRaw(raw, value, type, data_node, endian):
    if type == "struct":
        for sd in data_node["Data"]:
            sv = sd.get("Value", 0)
            PlaceToRaw(raw, sv, sd["Type"], sd, endian)
    elif type == "function":
        if not value or value == "NULL":
            raw.extend([0, 0, 0, 0])
    elif type in ["uint8", "uint8_n"]:
        raw.append(value)
    elif type in ["uint16", "uint16_n"]:
        if endian == "little":
            raw.extend([(value >> 0) & 0xFF, (value >> 8) & 0xFF])
        else:
            raw.extend([(value >> 8) & 0xFF, (value >> 0) & 0xFF])
    elif type in ["uint32", "uint32_n"]:
        if endian == "little":
            raw.extend([(value >> 0) & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF, (value >> 24) & 0xFF])
        else:
            raw.extend([(value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, (value >> 0) & 0xFF])


def GenStructInit(data_node):
    cstr = "{ "
    for i, sd in enumerate(data_node["Data"]):
        if sd["Type"] == "struct":
            cstr += GenStructInit(sd)
        elif sd["Type"] == "function":
            func_name = sd.get("Value", "NULL")
            cstr += func_name
        else:
            value = sd.get("Value", 0)
            if type(value) == str:
                value = eval(value)
            if type(value) in [list, tuple]:
                cstr += "{" + ", ".join(str(v) for v in value) + "}"
            else:
                cstr += str(value)
        if i < len(data_node["Data"]) - 1:
            cstr += ", "
    cstr += " }"
    return cstr


def GenConstants(C, cfg):
    endian = cfg.get("Enidan", "little")
    for block in cfg["Blocks"]:
        name = block["Name"]
        cstr = "static CONSTANT(RoD_%sType, ROD_CONST) RoD_%s = { " % (name, name)
        raw = []

        for i, data in enumerate(block["Data"]):
            if data["Type"] == "struct":
                struct_init = GenStructInit(data)
                cstr += struct_init
                PlaceToRaw(raw, None, "struct", data, endian)
            elif data["Type"] == "function":
                func_name = data.get("Value", "NULL")
                cstr += func_name
                if not func_name or func_name == "NULL":
                    raw.extend([0, 0, 0, 0])
            elif data["Type"] == "string":
                value = data["Value"]
                cstr += '"%s"' % (value)
                for c in value:
                    raw.append(ord(c))
                n = len(value)
                while n < data.get("Size", 1):
                    raw.append(0)
                    n = n + 1
            else:
                value = data.get("Value", 0)
                if type(value) == str:
                    value = eval(value)
                cstr += str(value).replace("[", "{").replace("]", "}")

                if type(value) in [list, tuple]:
                    for v in value:
                        PlaceToRaw(raw, v, data["Type"], data, endian)
                    n = len(value)
                    while n < data.get("Size", 1):
                        PlaceToRaw(raw, 0, data["Type"], data, endian)
                        n = n + 1
                else:
                    PlaceToRaw(raw, value, data["Type"], data, endian)

            if i < len(block["Data"]) - 1:
                cstr += ", "

        if "Crc" not in block:
            block["Crc"] = Calcaulte_Crc16(raw)
        else:
            block["Crc"] = toNum(block["Crc"])
        cstr += "};\n\n"
        C.write(cstr)


def Gen_RoD(cfg, dir):
    H = open("%s/RoD_%sCfg.h" % (dir, cfg["Name"]), "w")
    GenHeader(H)
    H.write("#ifndef ROD_%s_CFG_H\n" % (cfg["Name"].upper()))
    H.write("#define ROD_%s_CFG_H\n" % (cfg["Name"].upper()))
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "Std_Types.h"\n')
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    Number = 0
    for block in cfg["Blocks"]:
        name = block["Name"]
        H.write("#define ROD_NUMBER_%s %s\n" % (name, Number))
        Number += 1
    H.write("\n")
    H.write("#ifndef ROD_CONST\n")
    H.write("#define ROD_CONST\n")
    H.write("#endif\n")
    H.write("#ifndef ROD_CONST_ENTRY\n")
    H.write("#define ROD_CONST_ENTRY\n")
    H.write("#endif\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    GenTypes(H, cfg)
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* ROD_%s_CFG_H */\n" % (cfg["Name"].upper()))
    H.close()

    C = open("%s/RoD_%sCfg.c" % (dir, cfg["Name"]), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "RoD.h"\n')
    C.write('#include "RoD_%sCfg.h"\n' % (cfg["Name"]))
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#define ROD_%s_START_SEC_CONST\n" % (cfg["Name"].upper()))
    C.write('#include "RoD_%s_MemMap.h"\n' % (cfg["Name"]))
    GenConstants(C, cfg)
    C.write("static CONSTANT(RoD_DataType, ROD_CONST) RoD_%sDatas[] = {\n" % (cfg["Name"]))
    for block in cfg["Blocks"]:
        name = block["Name"]
        crc = block["Crc"]
        invCrc = ~crc & 0xFFFF
        C.write("  { &RoD_%s, sizeof(RoD_%s), 0x%04X, 0x%04X },\n" % (name, name, crc, invCrc))
    C.write("};\n\n")
    C.write("#define ROD_%s_STOP_SEC_CONST\n" % (cfg["Name"].upper()))
    C.write('#include "RoD_%s_MemMap.h"\n' % (cfg["Name"]))

    C.write("#define ROD_%s_ENTRY_START_SEC_CONST\n" % (cfg["Name"].upper()))
    C.write('#include "RoD_%s_Entry_MemMap.h"\n' % (cfg["Name"]))
    C.write("CONSTANT(RoD_ConfigType, ROD_CONST_ENTRY) RoD_%sConfig = {\n" % (cfg["Name"]))
    C.write("  RoD_%sDatas,\n" % (cfg["Name"]))
    C.write("  ROD_MAGIC_NUMBER,\n")
    C.write("  ~ROD_MAGIC_NUMBER,\n")
    C.write("  ARRAY_SIZE(RoD_%sDatas),\n" % (cfg["Name"]))
    C.write("  (uint16_t)~ARRAY_SIZE(RoD_%sDatas),\n" % (cfg["Name"]))
    C.write("};\n\n")
    C.write("#define ROD_%s_ENTRY_STOP_SEC_CONST\n" % (cfg["Name"].upper()))
    C.write('#include "RoD_%s_Entry_MemMap.h"\n' % (cfg["Name"]))
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def UpdateAppVersion(blk):
    now = time.localtime(time.time())
    for data in blk["Data"]:
        if data["Name"] == "year":
            data["Value"] = now.tm_year
        if data["Name"] == "month":
            data["Value"] = now.tm_mon
        if data["Name"] == "day":
            data["Value"] = now.tm_mday
        if data["Name"] == "hour":
            data["Value"] = now.tm_hour
        if data["Name"] == "minute":
            data["Value"] = now.tm_min
        if data["Name"] == "second":
            data["Value"] = now.tm_sec


def post(cfg, dir):
    for blk in cfg["Blocks"]:
        if blk["Name"] == "AppVersion":
            UpdateAppVersion(blk)
    with open("%s/RoD.json" % (dir), "w") as f:
        json.dump(cfg, f, indent=2)


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    post(cfg, dir)
    Gen_RoD(cfg, dir)
    GenMemMap("RoD_%s" % (cfg["Name"]), dir)
    GenMemMap("RoD_%s_Entry" % (cfg["Name"]), dir)
    return ["%s/RoD_%sCfg.c" % (dir, cfg["Name"])]
