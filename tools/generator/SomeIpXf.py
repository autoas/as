# SSAS - Simple Smart Automotive Software
# Copyright (C) 2026 Parai Wang <parai@foxmail.com>

from .helper import *

def GetArgs(cfg, name):
    for args in cfg.get("args", []):
        if name == args["name"]:
            return args["args"]
    raise Exception(f"args {name} not found")

def GetTypeInfo(data, structs={}):
    if type(data) is str:
        typ = data
    else:
        typ = data["type"]
    if typ in TypeInfoMap:
        return TypeInfoMap[typ]
    if typ in structs:
        dinfo = {"IsArray": "size" in data, "IsStruct": True, "ctype": "%s_Type" % (typ)}
        if dinfo["IsArray"]:
            dinfo["size"] = data["size"]
        return dinfo
    else:
        raise Exception("unknown data type: %s" % (data))


def GetStructDataSize(data, structs={}):
    size = 0
    typ = data["type"]
    if typ in TypeInfoMap:
        dinfo = TypeInfoMap[data["type"]]
        sz = TypeInfoMap[data["type"]]["size"] * data.get("size", 1)
        size += sz
    elif typ in structs:
        sz = GetStructSize(structs[typ], structs)
        sz = sz * data.get("size", 1)
        size += sz
    else:
        raise
    return size


def GetStructSize(struct, structs={}):
    size = 0
    for data in struct["data"]:
        dinfo = GetTypeInfo(data, structs)
        sz = GetStructDataSize(data, structs)
        if data.get("variable_array", False) or dinfo.get("variable_array", False):
            if sz < 256:
                sz += 1
            elif sz < 65536:
                sz += 2
            else:
                sz += 4
        if struct.get("with_tag", False):
            sz += 2
        size += sz
    return size


def GetStructs(cfg):
    structs = {}
    for st in cfg.get("structs", []):
        structs[st["name"]] = st
    return structs


def GetStructDataPayloadSize(data, structs={}):
    size = 0
    typ = data["type"]
    if typ in TypeInfoMap:
        dinfo = TypeInfoMap[data["type"]]
        sz = TypeInfoMap[data["type"]]["size"] * data.get("size", 1)
        size += sz
    elif typ in structs:
        sz = GetStructPayloadSize(structs[typ], structs)
        sz = sz * data.get("size", 1)
        size += sz
    else:
        raise
    return size


def GetStructPayloadSize(struct, structs={}):
    payloadSize = 0
    for data in struct["data"]:
        dinfo = GetTypeInfo(data, structs)
        sz = GetStructDataPayloadSize(data, structs)
        if data.get("variable_array", False) or dinfo.get("variable_array", False):
            if sz < 256:
                sz += 1
            elif sz < 65536:
                sz += 2
            else:
                sz += 4
        if struct.get("with_tag", False):
            sz += 2
        payloadSize += sz
    if struct.get("with_length", False):
        if payloadSize < 256:
            payloadSize += 1
        elif payloadSize < 65536:
            payloadSize += 2
        else:
            payloadSize += 4
    return payloadSize


def GetTypePayloadSize(data, structs={}):
    if type(data) is str:
        typ = data
    else:
        typ = data["type"]
    if typ == "void":
        return 0
    if typ in TypeInfoMap:
        return TypeInfoMap[typ]["size"]
    elif typ in structs:
        return GetStructPayloadSize(structs[typ], structs)
    else:
        raise Exception("unknown type: %s" % (typ))


def GetTypeCatlog(data, structs={}):
    dinfo = GetTypeInfo(data, structs)
    if dinfo["ctype"] in ["boolean", "uint8_t", "int8_t"]:
        dtype = "Byte"
    elif dinfo["ctype"] in ["uint16_t", "int16_t"]:
        dtype = "Short"
    elif dinfo["ctype"] in ["uint32_t", "int32_t", "float"]:
        dtype = "Long"
    elif dinfo["ctype"] in ["uint64_t", "int64_t", "double"]:
        dtype = "LongLong"
    else:
        dtype = "Struct"
    if dinfo["IsArray"]:
        dtype += "Array"
    return dtype


def GetXfCType(data, structs={}):
    dinfo = GetTypeInfo(data, structs)
    dtype = dinfo["ctype"]
    if dinfo["IsArray"]:
        dtype += f"[{dinfo['size']}]"
    return dtype


def GetArgTypeC(data, structs):
    ptr = ""
    dinfo = GetTypeInfo(data, structs)
    dtype = dinfo["ctype"]
    if dinfo.get("IsArray", False):
        dtype += f"[{dinfo['size']}]"
        ptr = "*"
    if dinfo.get("IsStruct", False):
        ptr = "*"
    return f"{dtype}{ptr}"


def GetArgRefC(data, structs):
    ref = ""
    dinfo = GetTypeInfo(data, structs)
    dtype = dinfo["ctype"]
    if dinfo.get("IsArray", False):
        dtype += f"[{dinfo['size']}]"
        ref = "&"
    if dinfo.get("IsStruct", False):
        ref = "&"
    return ref


def SomeIpXfDecode(typ, structs, buffer, bufferSize, data, **kwargs):
    if type(typ) is str:
        typ_ = typ
    else:
        typ_ = typ["type"]
    typCatlog = GetTypeCatlog(typ, structs)
    if typCatlog in ["Byte", "Short", "Long", "LongLong"]:
        return f"SomeIpXf_Decode{typCatlog}({buffer}, {bufferSize}, &{data})"
    elif typCatlog in ["ByteArray", "ShortArray", "LongArray", "LongLongArray"]:
        return f"SomeIpXf_Decode{typCatlog}({buffer}, {bufferSize}, {data}, sizeof({data})/sizeof({data}[0]))"
    elif typCatlog in ["Struct"]:
        return f"SomeIpXf_Decode{typCatlog}({buffer}, {bufferSize}, &{data}, &SomeIpXf_Struct{typ_}Def)"
    elif typCatlog in ["StructArray"]:
        return f"SomeIpXf_Decode{typCatlog}({buffer}, {bufferSize}, &{data}, &SomeIpXf_Struct{typ_}Def), &{kwargs['length']})"
    raise Exception("unsupported type catlog: %s" % (typCatlog))


def SomeIpXfEncode(typ, structs, buffer, bufferSize, data, **kwargs):
    if type(typ) is str:
        typ_ = typ
    else:
        typ_ = typ["type"]
    stype = kwargs.get("stype", "C++")
    if stype == "C":
        ref = ""
    else:
        ref = "&"
    typCatlog = GetTypeCatlog(typ, structs)
    if typCatlog in ["Byte", "Short", "Long", "LongLong"]:
        return f"SomeIpXf_Encode{typCatlog}({buffer}, {bufferSize}, {data})"
    elif typCatlog in ["ByteArray", "ShortArray", "LongArray", "LongLongArray"]:
        return f"SomeIpXf_Encode{typCatlog}({buffer}, {bufferSize}, {data}, sizeof({data})/sizeof({data}[0]))"
    elif typCatlog in ["Struct"]:
        return f"SomeIpXf_Encode{typCatlog}({buffer}, {bufferSize}, {ref}{data}, &SomeIpXf_Struct{typ_}Def)"
    elif typCatlog in ["StructArray"]:
        return f"SomeIpXf_Encode{typCatlog}({buffer}, {bufferSize}, {ref}{data}, &SomeIpXf_Struct{typ_}Def), sizeof({data})/sizeof({data}[0])"
    raise Exception("unsupported type catlog: %s" % (typCatlog))
