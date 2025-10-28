# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

__all__ = ["dbc"]

from . import ascyacc


def get_period(p, msg):
    CycleTime, FirstTime = None, None
    id = msg["id"]
    for ba in p.get("baList", []):
        if (ba[1] == '"GenMsgCycleTime"') and (ba[2] == "BO_"):
            if ba[3] == id:
                CycleTime = ba[4]
        if (ba[1] == '"GenMsgDelayTime"') and (ba[2] == "BO_"):
            if ba[3] == id:
                FirstTime = ba[4]
    return CycleTime, FirstTime


def get_init(p, sig):
    name = sig["name"]
    for ba in p.get("baList", []):
        if (ba[1] == '"GenSigStartValue"') and (ba[2] == "SG_"):
            if ba[4] == name:
                return ba[5]
    return None


def get_comment(p, sig):
    name = sig["name"]
    for cm in p.get("cmList", []):
        if (cm[1] == "SG_") and (cm[3] == name):
            return cm[4]
    return None


def post_process_period(p):
    for msg in p["messages"]:
        CycleTime, FirstTime = get_period(p, msg)
        if CycleTime != None:
            msg.update({"CycleTime": CycleTime})
        if FirstTime != None:
            msg.update({"FirstTime": FirstTime})

def post_process_init(p):
    for msg in p["messages"]:
        for sig in msg["signals"]:
            init = get_init(p, sig)
            if init != None:
                sig.update({"InitValue": init})
            comment = get_comment(p, sig)
            if comment != None:
                sig.update({"comment": comment})


def post_process_id_type(p):
    for msg in p["messages"]:
        id = msg["id"]
        if id > 0x7FF:
            id |= 0x80000000
            msg["id"] = id


def parse(file):
    print(f"DBC: {file}")
    fp = open(file, "r")
    data = fp.read()
    fp.close()
    p = ascyacc.parse(data)
    post_process_period(p)
    post_process_init(p)
    post_process_id_type(p)
    return p


def dbc(path):
    p = parse(path)
    return p["messages"]
