# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>

import os
import sys
import json
import importlib.util
from building import *

__all__ = ['Gen']

def Gen(cfg):
    pkgArxml = Package('https://github.com/autoas/arxml.git')
    pkgCfile = Package('https://github.com/autoas/cfile.git', version='v0.1.4')
    sys.path.append(pkgArxml)
    sys.path.append(pkgCfile)
    bsw = os.path.dirname(cfg)
    bswName = os.path.basename(bsw)
    genDir = os.path.join(bsw, 'GEN')
    with open(cfg) as f:
        cfg = json.load(f)
    for inc in cfg.get('includes', []):
        p = os.path.abspath(os.path.join(bsw, inc))
        print('inc', p)
        sys.path.append(p)
    rte =os.path.join(bsw, cfg['py'])
    os.makedirs(genDir, exist_ok=True)
    spec = importlib.util.spec_from_file_location(bswName, rte)
    rteM = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(rteM)
    rteM.main(genDir)