# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
import pickle
import hashlib

from .NvM import Gen as NvMGen
from .RoD import Gen as RoDGen
from .Dem import Gen as DemGen
from .Com import Gen as ComGen
from .Net import Gen as NetGen
from .Factory import Gen as FactoryGen
from .MemCluster import Gen as MCGen
from .PduR import Gen as PduRGen
from .CanTp import Gen as CanTpGen
from .LinTp import Gen as LinTpGen
from .CanIf import Gen as CanIfGen
from .CanNm import Gen as CanNmGen
from .OsekNm import Gen as OsekNmGen
from .Nm import Gen as NmGen
from .CanSM import Gen as CanSMGen
from .ComM import Gen as ComMGen
from .Rte import Gen as RteGen
from .protoc import Gen as ProtocGen
from .Os import Gen as OsGen
from .Dcm import Gen as DcmGen
from .Trace import Gen as TraceGen
from .Xcp import Gen as XcpGen
from .LinIf import Gen as LinIfGen
from .J1939Tp import Gen as J1939TpGen
from .Csm import Gen as CsmGen
from .SecOC import Gen as SecOCGen
from .E2E import Gen as E2EGen

def DummyGen(cfg):
    pass


__GEN__ = {
    "NvM": NvMGen,
    "RoD": RoDGen,
    "Dem": DemGen,
    "Com": ComGen,
    "Net": NetGen,
    "Factory": FactoryGen,
    "MemCluster": MCGen,
    "PduR": PduRGen,
    "CanTp": CanTpGen,
    "LinTp": LinTpGen,
    "CanIf": CanIfGen,
    "Rte": RteGen,
    "protoc": ProtocGen,
    "OS": OsGen,
    "Dcm": DcmGen,
    "Trace": TraceGen,
    "EcuC": DummyGen,
    "Xcp": XcpGen,
    "LinIf": LinIfGen,
    "J1939Tp": J1939TpGen,
    "CanNm": CanNmGen,
    "OsekNm": OsekNmGen,
    "Nm": NmGen,
    "CanSM": CanSMGen,
    "ComM": ComMGen,
    "Csm": CsmGen,
    "SecOC": SecOCGen,
    "E2E": E2EGen,
}

RootDir = os.path.abspath(os.path.dirname(__file__) + "/../..")


def GetGen(cfg):
    if cfg.endswith(".json"):
        with open(cfg) as f:
            con = json.load(f)
            if "class" in con:
                cls = con["class"]
                if cls in __GEN__:
                    return cls, __GEN__[cls]
                else:
                    raise Exception("class %s is not supported" % (cls))
            else:
                raise
    else:
        raise


def CalcHash(js):
    L = [js]
    with open(js) as f:
        cfg = json.load(f)
        for cls in ["Com", "CanIf", "PduR"]:
            if cfg.get("class", None) == cls:
                for n in cfg.get("networks", []):
                    for x in ["dbc", "ldf"]:
                        path = n.get(x, None)
                        if path != None:
                            if not os.path.isfile(path):
                                path = os.path.join(os.path.dirname(js), path)
                            L.append(path)
        for cls, key in [("Dcm", "excel"), ("Rte", "py"), ("protoc", "entry")]:
            if cfg.get("class", None) == cls:
                path = cfg.get(key, None)
                if path != None:
                    if not os.path.isfile(path):
                        path = os.path.join(os.path.dirname(js), path)
                        L.append(path)
    md5 = hashlib.md5()
    for f in L:
        md5.update(open(f, "rb").read())
    new_hash = md5.hexdigest()
    return new_hash


def Generate(cfgs, force=False):
    source = {}
    db = {}
    dbpath = "%s/.gendb.pkl" % (RootDir)
    if os.path.isfile(dbpath):
        db = pickle.load(open(dbpath, "rb"))
    if type(cfgs) is str:
        cfgs = [cfgs]
    for cfg in cfgs:
        cls, gen = GetGen(cfg)
        if cfg in db:
            old_hash, src = db[cfg]
        else:
            old_hash, src = None, []
        new_hash = CalcHash(cfg)
        if new_hash != old_hash or force:
            src = gen(cfg)
        if src != None:
            if type(src) is list:
                if cls not in source:
                    source[cls] = src
                else:
                    source[os.path.basename(cfg)[:-5]] = src
            else:
                source.update(src)
        db[cfg] = new_hash, src
    pickle.dump(db, open(dbpath, "wb"))
    return source
