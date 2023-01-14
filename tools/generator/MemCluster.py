# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ['Gen']

def Gen_Macros(cfg, H):
    mps = cfg['clusters']
    mps.sort(key=lambda x: eval(str(x['size'])))
    MaxSize = 0
    for mp in mps:
        tsz = eval(str(mp['size']))
        H.write('#define MEMPOOL_%s_%s_SIZE %s\n' % (
            cfg['name'].upper(), mp['name'].upper(), tsz))
        if tsz > MaxSize:
            MaxSize = tsz
    H.write('#define MEMPOOL_%s_MAX_SIZE %s\n\n' %
            (cfg['name'].upper(), MaxSize))

def Gen_Defs(cfg, C):
    mps = cfg['clusters']
    mps.sort(key=lambda x: eval(str(x['size'])))
    for mp in mps:
        tsz = eval(str(mp['size'])) * eval(str(mp['number']))
        C.write('static uint32_t MC_%s_%s_Buffer[(%s+sizeof(uint32_t) - 1)/sizeof(uint32_t)];\n' % (
            cfg['name'], mp['name'], tsz))
    C.write('static const mem_cluster_cfg_t MC_%sCfgs[] = {\n' % (cfg['name']))
    for mp in mps:
        C.write('  {\n')
        C.write('    (uint8_t*)MC_%s_%s_Buffer,\n' % (cfg['name'], mp['name']))
        C.write('    %s,\n' % (mp['size']))
        C.write('    %s,\n' % (mp['number']))
        C.write('  },\n')
    C.write('};\n\n')
    C.write('static mempool_t MC_%sPools[%s];\n' % (
        cfg['name'], len(mps)))
    C.write('static const mem_cluster_t MC_%s = {\n' % (cfg['name']))
    C.write('  MC_%sPools,\n' % (cfg['name']))
    C.write('  MC_%sCfgs,\n' % (cfg['name']))
    C.write('  %s,\n' % (len(mps)))
    C.write('};\n\n')

def Gen_MC(cfg, dir):
    H = open('%s/%sMem.h' % (dir, cfg['name']), 'w')
    GenHeader(H)
    H.write('#ifndef %s_MEM_H\n' % (cfg['name'].upper()))
    H.write('#define %s_MEM_H\n' % (cfg['name'].upper()))
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write('#include "mempool.h"\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    Gen_Macros(cfg, H)
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('void %s_MemInit(void);\n' % (cfg['name']))
    H.write('void* %s_MemAlloc(uint32_t size);\n' % (cfg['name']))
    H.write('void* %s_MemGet(uint32_t* size);\n' % (cfg['name']))
    H.write('void %s_MemFree(void* buffer);\n' % (cfg['name']))
    H.write('#endif /* %s_MEM_H */\n' % (cfg['name'].upper()))
    H.close()

    C = open('%s/%sMem.c' % (dir, cfg['name']), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "%sMem.h"\n' % (cfg['name']))
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    Gen_Defs(cfg, C)
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.write('void %s_MemInit(void) {\n' % (cfg['name']))
    C.write('  mc_init(&MC_%s);\n' % (cfg['name']))
    C.write('}\n\n')
    C.write('void* %s_MemAlloc(uint32_t size) {\n' % (cfg['name']))
    C.write('  return (void*)mc_alloc(&MC_%s, size);\n' % (cfg['name']))
    C.write('}\n\n')
    C.write('void* %s_MemGet(uint32_t *size) {\n' % (cfg['name']))
    C.write('  return (void*)mc_get(&MC_%s, size);\n' % (cfg['name']))
    C.write('}\n\n')
    C.write('void %s_MemFree(void* buffer) {\n' % (cfg['name']))
    C.write('  mc_free(&MC_%s, (uint8_t*)buffer);\n' % (cfg['name']))
    C.write('}\n\n')

    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_MC(cfg, dir)
