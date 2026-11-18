# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *
from .DoIp import Gen_DoIp
from .SomeIp import Gen_SomeIp
from .SoAd import Gen_SoAd

__all__ = ["Gen"]

__GENNET__ = {
    "DoIp": Gen_DoIp,
    "SomeIp": Gen_SomeIp,
    "SoAd": Gen_SoAd,
}


def Gen_Net(cfg, dir):
    source = {}
    for mod in cfg["Modules"]:
        if mod["Class"] in __GENNET__:
            ret = __GENNET__[mod["Class"]](mod, dir)
            source.update(ret)
        else:
            raise Exception("unknown class %s" % (mod["Class"]))
    return source


def ProcDoIp(cfg, mod):
    sock = {
        "Name": "DOIP_UDP",
        "Server": mod["Discovery"],
        "Protocol": "UDP",
        "Multicast": True,
        "Up": "DoIP",
        "RxPduId": "DOIP_RX_PID_UDP",
    }
    cfg["Sockets"].append(sock)
    sock = {
        "Name": "DOIP_TCP",
        "Server": "NULL:13400",
        "Protocol": "TCP",
        "Up": "DoIP",
        "Listen": mod["MaxConnections"],
        "RxPduId": "DOIP_RX_PID_TCP",
    }
    if mod.get("EnableTLS", False):
        sock["Up"] = "TLS"
        sock["RxPduId"] = "TLS_RX_PID_DOIP_TCP"
    cfg["Sockets"].append(sock)


def ProcSomeIp(cfg, mod):
    sock = {
        "Name": "SD_MULTICAST",
        "Server": mod["SD"]["Multicast"] + ":30490",
        "Multicast": True,
        "Protocol": "UDP",
        "Up": "SD",
        "RxPduId": "SD_RX_PID_MULTICAST",
    }
    cfg["Sockets"].append(sock)
    sock = {"Name": "SD_UNICAST", "Server": "NULL:30490", "Protocol": "UDP", "Up": "SD", "RxPduId": "SD_RX_PID_UNICAST"}
    cfg["Sockets"].append(sock)
    for service in mod.get("Servers", []):
        name = "SOMEIP_%s" % (toMacro(service["Name"]))
        if "Reliable" in service:
            protocol = "TCP"
            port = service.get("Reliable", 0)
        else:
            protocol = "UDP"
            port = service.get("Unreliable", 0)
        sock = {
            "Name": name,
            "Server": "NULL:%s" % (port),
            "Protocol": protocol,
            "Up": "SOMEIP",
            "RxPduId": "SOMEIP_RX_PID_%s" % (name),
        }
        if "Reliable" in service:
            sock["Listen"] = service["Listen"] if "Listen" in service else 1
        cfg["Sockets"].append(sock)
        for eg in service.get("EventGroups", []):
            if "Multicast" in eg:
                sock = {
                    "Name": "_".join([name, eg["Name"]]),
                    "Server": "%s" % (eg["Multicast"].get("Addr", "NULL:0")),
                    "Multicast": True,
                    "Protocol": "UDP",
                    "Up": "SOMEIP",
                    "ModeChg": "Sd",
                    "RxPduId": "SOMEIP_RX_PID_%s" % (name),
                }
                cfg["Sockets"].append(sock)
    for service in mod.get("Clients", []):
        name = "SOMEIP_%s" % (toMacro(service["Name"]))
        if "Protocol" in service:
            protocol = service["Protocol"]
            if protocol == "TCP":
                port = service.get("Reliable", 0)
            else:
                port = service.get("Unreliable", 0)
        else:
            if "Reliable" in service:
                protocol = "TCP"
                port = service.get("Reliable", 0)
            else:
                protocol = "UDP"
                port = service.get("Unreliable", 0)
        sock = {
            "Name": name,
            "Client": "NULL:%s" % (port),
            "Protocol": protocol,
            "Up": "SOMEIP",
            "RxPduId": "SOMEIP_RX_PID_%s" % (name),
        }
        cfg["Sockets"].append(sock)
        for eg in service.get("EventGroups", []):
            if "Multicast" in eg:
                sock = {
                    "Name": "_".join([name, eg["Name"]]),
                    "Client": "%s" % (eg["Multicast"].get("Addr", "NULL:0")),
                    "Multicast": True,
                    "Protocol": "UDP",
                    "Up": "SOMEIP",
                    "ModeChg": "Sd",
                    "RxPduId": "SOMEIP_RX_PID_%s" % (name),
                }
                cfg["Sockets"].append(sock)


def ProcSoAd(netCfg, dir):
    cfg = None
    for mod in netCfg["Modules"]:
        if mod["Class"] == "SoAd":
            cfg = mod
    if cfg == None:
        cfg = {"Class": "SoAd", "Sockets": []}
        netCfg["Modules"].append(cfg)
    for mod in netCfg["Modules"]:
        if mod["Class"] == "DoIp":
            ProcDoIp(cfg, mod)
        elif mod["Class"] == "SomeIp":
            ProcSomeIp(cfg, mod)
    with open("%s/Network.json" % (dir), "w") as f:
        json.dump(netCfg, f, indent=2)


RandomPort = 60000


def GenAntiTestForSomeIp(cfgj):
    global RandomPort
    dir = os.path.join(os.path.dirname(cfgj), "GENT")
    os.makedirs(dir, exist_ok=True)
    with open(cfgj) as f:
        cfg = json.load(f)
    someIpCfg = None
    for mod in cfg["Modules"]:
        if mod["Class"] == "SomeIp":
            someIpCfg = mod
            servers = []
            clients = []
            if "Servers" in mod:
                servers = mod["Servers"]
                del mod["Servers"]
            if "Clients" in mod:
                clients = mod["Clients"]
                del mod["Clients"]
            for ser in servers:
                if "Unreliable" in ser:
                    ser["Unreliable"] = RandomPort
                    RandomPort += 1
            mod["Clients"] = servers
            mod["Servers"] = clients
    if someIpCfg:
        cfg["Modules"] = [someIpCfg]
    ProcSoAd(cfg, dir)
    return Gen_Net(cfg, dir)


def Gen(cfgj):
    dir = os.path.join(os.path.dirname(cfgj), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfgj) as f:
        cfg = json.load(f)
    ProcSoAd(cfg, dir)
    source = Gen_Net(cfg, dir)
    if cfg.get("AntiTest", False):
        sourceAnti = GenAntiTestForSomeIp(cfgj)
        for name, src in sourceAnti.items():
            source[name + "_T"] = src
    return source
