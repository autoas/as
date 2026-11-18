# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import time
import datetime


def toNum(v):
    if type(v) is str:
        return eval(v)
    return v


def toMacro(s):
    words = []
    word = None
    for a in s:
        if a.isupper() or a.isdigit():
            if word != None:
                a_ = word[-1]
                # concat: Aa, AA, aa, 11, A1, 1A
                if (
                    (a_.isupper() and a.islower())
                    or (a_.isupper() and a.isupper())
                    or (a_.islower() and a.islower())
                    or (a_.isdigit() and a.isdigit())
                    or (a_.isupper() and a.isdigit())
                    or (a_.isdigit() and a.isupper())
                ):
                    word += a
                    continue
                else:
                    words.append(word)
            word = a
        else:
            if word == None:
                word = a
            else:
                word += a
    words.append(word)
    sss = "_".join(s.upper() for s in words)
    for s, r in [("CAN_TP", "CANTP"), ("LIN_TP", "LINTP"), ("PDU_R", "PDUR"), ("CAN_IF", "CANIF"), ("__", "_")]:
        sss = sss.replace(s, r)
    return sss


def toPduSymbol(names):
    return "_".join([toMacro(n) for n in names])


def GenHeader(f):
    currentDateTime = datetime.datetime.now()
    date = currentDateTime.date()
    year = date.strftime("%Y")
    GenTime = time.asctime(time.localtime(time.time()))
    f.write("/**\n")
    f.write(" * SSAS - Simple Smart Automotive Software\n")
    f.write(" * Copyright (C) 2021-%s Parai Wang <parai@foxmail.com>\n" % (year))
    f.write(" *\n")
    f.write(" * Generated at %s\n" % (GenTime))
    f.write(" */\n")
    print("GEN %s" % (os.path.abspath(f.name)))


TypeInfoMap = {
    "bool": {"Size": 1, "IsArray": False, "ctype": "boolean"},
    "string": {"Size": 1, "IsArray": True, "VariableArray": True, "ctype": "uint8_t"},
    "uint8": {"Size": 1, "IsArray": False, "ctype": "uint8_t"},
    "int8": {"Size": 1, "IsArray": False, "ctype": "int8_t"},
    "uint16": {"Size": 2, "IsArray": False, "ctype": "uint16_t"},
    "int16": {"Size": 2, "IsArray": False, "ctype": "int16_t"},
    "uint32": {"Size": 4, "IsArray": False, "ctype": "uint32_t"},
    "int32": {"Size": 4, "IsArray": False, "ctype": "int32_t"},
    "uint64": {"Size": 8, "IsArray": False, "ctype": "uint64_t"},
    "int64": {"Size": 8, "IsArray": False, "ctype": "uint64_t"},
    "float": {"Size": 4, "IsArray": False, "ctype": "float"},
    "double": {"Size": 8, "IsArray": False, "ctype": "double"},
    "bool_n": {"Size": 1, "IsArray": True, "ctype": "boolean"},
    "uint8_n": {"Size": 1, "IsArray": True, "ctype": "uint8_t"},
    "int8_n": {"Size": 1, "IsArray": True, "ctype": "int8_t"},
    "uint16_n": {"Size": 2, "IsArray": True, "ctype": "uint16_t"},
    "int16_n": {"Size": 2, "IsArray": True, "ctype": "int16_t"},
    "uint32_n": {"Size": 4, "IsArray": True, "ctype": "uint32_t"},
    "int32_n": {"Size": 4, "IsArray": True, "ctype": "int32_t"},
    "uint64_n": {"Size": 8, "IsArray": True, "ctype": "uint64_t"},
    "int64_n": {"Size": 8, "IsArray": True, "ctype": "int64_t"},
    "float_n": {"Size": 4, "IsArray": True, "ctype": "float"},
    "double_n": {"Size": 8, "IsArray": True, "ctype": "double"},
}


def GetDataSize(data):
    if data["Type"] == "struct":
        size = 0
        for sd in data["Data"]:
            size += GetDataSize(sd)
        return size
    return TypeInfoMap[data["Type"]]["Size"] * data.get("Size", 1) * data.get("Repeat", 1)
