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
    return node["name"]


def GenTypes(H, cfg):
    for block in cfg["blocks"]:
        H.write("typedef struct {\n")
        for data in block["data"]:
            dinfo = TypeInfoMap[data["type"]]
            cstr = "  %s %s" % (dinfo["ctype"], GetName(data))
            if dinfo["IsArray"]:
                cstr += "[%s]" % (data["size"])
            cstr += ";\n"
            H.write(cstr)
        H.write("} RoD_%sType;\n\n" % (GetName(block)))


def PlaceToRaw(raw, value, type, endian):
    if type in ["uint8", "uint8_n"]:
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


def GenConstants(C, cfg):
    endian = cfg.get("enidan", "little")
    for block in cfg["blocks"]:
        name = block["name"]
        cstr = "static CONSTANT(RoD_%sType, ROD_CONST) RoD_%s = { " % (name, name)
        raw = []
        for data in block["data"]:
            value = data["value"]
            if data["type"] == "string":
                cstr += '"%s", ' % (value)
                for c in value:
                    raw.append(ord(c))
                i = len(value)
                while i < data["size"]:
                    raw.append(0)
                    i = i + 1
            elif type(value) == str:
                value = eval(value)
                cstr += "%s, " % (str(value).replace("[", "{").replace("]", "}"))
                if type(value) in [list, tuple]:
                    for v in value:
                        PlaceToRaw(raw, v, data["type"], endian)
                    i = len(value)
                    while i < data["size"]:
                        PlaceToRaw(raw, 0, data["type"], endian)
                        i = i + 1
                else:
                    PlaceToRaw(raw, value, data["type"], endian)
            else:
                cstr += "%s, " % (value)
                PlaceToRaw(raw, value, data["type"], endian)
        block["crc"] = Calcaulte_Crc16(raw)
        cstr += "};\n\n"
        C.write(cstr)


def Gen_RoD(cfg, dir):
    H = open("%s/RoD_%sCfg.h" % (dir, cfg["name"]), "w")
    GenHeader(H)
    H.write("#ifndef ROD_%s_CFG_H\n" % (cfg["name"].upper()))
    H.write("#define ROD_%s_CFG_H\n" % (cfg["name"].upper()))
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "Std_Types.h"\n')
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    Number = 0
    for block in cfg["blocks"]:
        name = block["name"]
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
    H.write("#endif /* ROD_%s_CFG_H */\n" % (cfg["name"].upper()))
    H.close()

    C = open("%s/RoD_%sCfg.c" % (dir, cfg["name"]), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "RoD.h"\n')
    C.write('#include "RoD_%sCfg.h"\n' % (cfg["name"]))
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#define ROD_%s_START_SEC_CONST\n" % (cfg["name"].upper()))
    C.write('#include "RoD_%s_MemMap.h"\n' % (cfg["name"]))
    GenConstants(C, cfg)
    C.write("static CONSTANT(RoD_DataType, ROD_CONST) RoD_%sDatas[] = {\n" % (cfg["name"]))
    for block in cfg["blocks"]:
        name = block["name"]
        crc = block["crc"]
        invCrc = ~crc & 0xFFFF
        C.write("  { &RoD_%s, sizeof(RoD_%s), 0x%04X, 0x%04X },\n" % (name, name, crc, invCrc))
    C.write("};\n\n")
    C.write("#define ROD_%s_STOP_SEC_CONST\n" % (cfg["name"].upper()))
    C.write('#include "RoD_%s_MemMap.h"\n' % (cfg["name"]))

    C.write("#define ROD_%s_ENTRY_START_SEC_CONST\n" % (cfg["name"].upper()))
    C.write('#include "RoD_%s_Entry_MemMap.h"\n' % (cfg["name"]))
    C.write("CONSTANT(RoD_ConfigType, ROD_CONST_ENTRY) RoD_%sConfig = {\n" % (cfg["name"]))
    C.write("  RoD_%sDatas,\n" % (cfg["name"]))
    C.write("  ROD_MAGIC_NUMBER,\n")
    C.write("  ~ROD_MAGIC_NUMBER,\n")
    C.write("  ARRAY_SIZE(RoD_%sDatas),\n" % (cfg["name"]))
    C.write("  (uint16_t)~ARRAY_SIZE(RoD_%sDatas),\n" % (cfg["name"]))
    C.write("};\n\n")
    C.write("#define ROD_%s_ENTRY_STOP_SEC_CONST\n" % (cfg["name"].upper()))
    C.write('#include "RoD_%s_Entry_MemMap.h"\n' % (cfg["name"]))
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def UpdateAppVersion(blk):
    now = time.localtime(time.time())
    for data in blk["data"]:
        if data["name"] == "year":
            data["value"] = now.tm_year
        if data["name"] == "month":
            data["value"] = now.tm_mon
        if data["name"] == "day":
            data["value"] = now.tm_mday
        if data["name"] == "hour":
            data["value"] = now.tm_hour
        if data["name"] == "minute":
            data["value"] = now.tm_min
        if data["name"] == "second":
            data["value"] = now.tm_sec


def post(cfg, dir):
    for blk in cfg["blocks"]:
        if blk["name"] == "AppVersion":
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
    GenMemMap("RoD_%s" % (cfg["name"]), dir)
    GenMemMap("RoD_%s_Entry" % (cfg["name"]), dir)
    return ["%s/RoD_%sCfg.c" % (dir, cfg["name"])]
