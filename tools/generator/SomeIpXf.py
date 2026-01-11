# SSAS - Simple Smart Automotive Software
# Copyright (C) 2026 Parai Wang <parai@foxmail.com>

from .helper import *


def GetTypeInfo(data, structs={}):
    typ = data["type"]
    if typ in TypeInfoMap:
        return TypeInfoMap[typ]
    if typ in structs:
        return {"IsArray": "size" in data, "IsStruct": True, "ctype": "%s_Type" % (typ)}
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