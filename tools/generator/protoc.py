# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json

__all__ = ['Gen']

def Gen(cfg):
    cwd = os.path.dirname(cfg)
    dir = os.path.join(cwd, 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cmd = 'protoc -I=%s' %(cwd)
    for t in cfg.get('types', ['cpp']):
        cmd += ' --%s_out=%s'%(t, dir)
    cmd += ' %s/%s'%(cwd, cfg['entry'])
    print(cmd)
    r = os.system(cmd)
    assert(0 == r)
    
    