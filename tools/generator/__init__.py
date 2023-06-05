# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
import pickle
import hashlib

from .NvM import Gen as NvMGen
from .Dem import Gen as DemGen
from .Com import Gen as ComGen
from .Net import Gen as NetGen
from .Factory import Gen as FactoryGen
from .MemCluster import Gen as MCGen
from .PduR import Gen as PduRGen
from .CanTp import Gen as CanTpGen
from .CanIf import Gen as CanIfGen
from .Rte import Gen as RteGen
from .protoc import Gen as ProtocGen
from .Os import Gen as OsGen
from .Dcm import Gen as DcmGen
from .Trace import Gen as TraceGen

def DummyGen(cfg):
    pass

__GEN__ = {
    'NvM': NvMGen,
    'Dem': DemGen,
    'Com': ComGen,
    'Net': NetGen,
    'Factory': FactoryGen,
    'MemCluster': MCGen,
    'PduR': PduRGen,
    'CanTp': CanTpGen,
    'CanIf': CanIfGen,
    'Rte': RteGen,
    'protoc': ProtocGen,
    'OS': OsGen,
    'Dcm': DcmGen,
    'Trace': TraceGen,
    'EcuC': DummyGen,
}

RootDir = os.path.abspath(os.path.dirname(__file__) + '/../..')


def GetGen(cfg):
    if cfg.endswith('.json'):
        with open(cfg) as f:
            con = json.load(f)
            if 'class' in con:
                cls = con['class']
                if cls in __GEN__:
                    return __GEN__[cls]
                else:
                    raise Exception('class %s is not supported' % (cls))
            else:
                raise
    else:
        raise


def Generate(cfgs, force=False):
    db = {}
    dbpath = '%s/.gendb.pkl' % (RootDir)
    if os.path.isfile(dbpath):
        db = pickle.load(open(dbpath, 'rb'))
    if type(cfgs) is str:
        cfgs = [cfgs]
    for cfg in cfgs:
        gen = GetGen(cfg)
        if cfg in db:
            old_hash = db[cfg]
        else:
            old_hash = None
        md5 = hashlib.md5()
        md5.update(open(cfg, 'rb').read())
        new_hash = md5.hexdigest()
        if new_hash != old_hash or force:
            gen(cfg)
        db[cfg] = new_hash
    pickle.dump(db, open(dbpath, 'wb'))
