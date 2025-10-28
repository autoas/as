# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>

import os
import sys
import json
import importlib.util
from building import *

__all__ = ["Gen"]


def Gen(cfg):
    pkgArxml = Package("https://github.com/autoas/arxml.git", version="v5.0")
    pkgCfile = Package("https://github.com/autoas/cfile.git", version="7ed5d2584e4c711d2fb219dfdf228f100f609186")
    sys.path.append(pkgArxml + "/src")
    sys.path.append(pkgArxml + "/examples/template")
    sys.path.append(pkgCfile + "/src")
    bsw = os.path.dirname(cfg)
    bswName = os.path.basename(bsw)
    genDir = os.path.join(bsw, "GEN")
    os.makedirs(genDir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    if "py" in cfg:
        for inc in cfg.get("includes", []):
            p = os.path.abspath(os.path.join(bsw, inc))
            sys.path.append(p)
        rte = os.path.join(bsw, cfg["py"])
        spec = importlib.util.spec_from_file_location(bswName, rte)
        rteM = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(rteM)
        rteM.main(genDir)
    else:
        from generator import asar

        arxmls = []
        if type(cfg["arxml"]) is str:
            arxmls_ = [cfg["arxml"]]
        else:
            arxmls_ = cfg["arxml"]
        for arxml in arxmls_:
            arxml = os.path.join(bsw, arxml)
            if os.path.isfile(arxml):
                arxmls.append(arxml)
            elif os.path.isdir(arxml):
                arxmls.extend(glob.glob(f"{arxml}/*.arxml"))
            else:
                arxmls.extend(glob.glob(arxml))
        asar.GenFromArxml(arxmls, genDir)
