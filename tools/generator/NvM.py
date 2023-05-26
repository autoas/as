# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ['Gen']

def GetName(node):
    if node['name'][-2:] == '{}':
        return node['name'][:-2]
    return node['name']


def GetBlockSize(block):
    size = 0
    for data in block['data']:
        size += GetDataSize(data)*block.get('size', 1)
    return size


def GetMaxDataSize(cfg):
    maxSize = 0
    for block in cfg['blocks']:
        size = 0
        for data in block['data']:
            size += TypeInfoMap[data['type']]['size'] * \
                data.get('size', 1)*data.get('repeat', 1)
        if size > maxSize:
            maxSize = size
    return maxSize


def GenTypes(H, cfg):
    for block in cfg['blocks']:
        H.write('typedef struct {\n')
        for data in block['data']:
            dinfo = TypeInfoMap[data['type']]
            cstr = '  %s %s' % (dinfo['ctype'], GetName(data))
            if data['name'][-2:] == '{}':
                cstr += '[%s]' % (data['repeat'])
            if dinfo['IsArray']:
                cstr += '[%s]' % (data['size'])
            cstr += ';\n'
            H.write(cstr)
        H.write('} %sType;\n\n' % (GetName(block)))


def GenConstants(C, cfg):
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(1, repeat):
            name = block['name'].format(i)
            C.write('#define %s_Rom %s_Rom\n' %
                    (name, block['name'].format(0)))
        name = block['name'].format(0)
        cstr = 'static const %sType %s_Rom = { ' % (GetName(block), name)
        for data in block['data']:
            dft = data['default']
            if type(dft) == str:
                cstr += '%s, ' % (str(eval(dft)
                                      ).replace('[', '{').replace(']', '}'))
            else:
                cstr += '%s, ' % (dft)
        cstr += '};\n\n'
        C.write(cstr)


def Gen_Ea(cfg, dir):
    H = open('%s/Ea_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef _EA_CFG_H\n')
    H.write('#define _EA_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write('#ifndef EA_PAGE_SIZE\n')
    H.write('#define EA_PAGE_SIZE 1\n')
    H.write('#endif\n\n')
    H.write('#ifndef EA_SECTOR_SIZE\n')
    H.write('#define EA_SECTOR_SIZE 4\n')
    H.write('#endif\n\n')
    H.write('#define EA_MAX_DATA_SIZE %d\n\n' % (GetMaxDataSize(cfg)))
    Number = 1
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(repeat):
            name = block['name'].format(i)
            H.write('#define EA_NUMBER_%s %s\n' % (name, Number))
            Number += 1
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    GenTypes(H, cfg)
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /* _EA_CFG_H */\n')
    H.close()

    C = open('%s/Ea_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "Ea.h"\n')
    C.write('#include "Ea_Cfg.h"\n')
    C.write('#include "Ea_Priv.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write('void NvM_JobEndNotification(void);\n')
    C.write('void NvM_JobErrorNotification(void);\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    C.write('static const Ea_BlockConfigType Ea_BlockConfigs[] = {\n')
    Cnt = 0
    Address = 0
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(repeat):
            name = block['name'].format(i)
            size = GetBlockSize(block)
            NumberOfWriteCycles = block.get('NumberOfWriteCycles', 10000000)
            C.write('  { EA_NUMBER_%s, %s, %s+2, %s },\n' %
                    (name, Address, size, NumberOfWriteCycles))
            Address += int(((size+2+3)/4))*4
            Cnt += 1
    C.write('};\n\n')

    C.write('const Ea_ConfigType Ea_Config = {\n')
    C.write('  NvM_JobEndNotification,\n')
    C.write('  NvM_JobErrorNotification,\n')
    C.write('  Ea_BlockConfigs,\n')
    C.write('  ARRAY_SIZE(Ea_BlockConfigs),\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def Gen_Fee(cfg, dir):
    H = open('%s/Fee_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef FEE_CFG_H\n')
    H.write('#define FEE_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write('#define FEE_MAX_DATA_SIZE %d\n\n' % (GetMaxDataSize(cfg)))
    Number = 1
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(repeat):
            name = block['name'].format(i)
            H.write('#define FEE_NUMBER_%s %s\n' % (name, Number))
            Number += 1
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    GenTypes(H, cfg)
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /* FEE_CFG_H */\n')
    H.close()

    C = open('%s/Fee_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "Fee.h"\n')
    C.write('#include "Fee_Cfg.h"\n')
    C.write('#include "Fee_Priv.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write('#ifndef FLS_BASE_ADDRESS\n')
    C.write('#define FLS_BASE_ADDRESS 0\n')
    C.write('#endif\n\n')

    C.write('#ifndef FEE_MAX_JOB_RETRY\n')
    C.write('#define FEE_MAX_JOB_RETRY 0xFF\n')
    C.write('#endif\n\n')

    C.write('#ifndef FEE_MAX_ERASED_NUMBER\n')
    C.write('#define FEE_MAX_ERASED_NUMBER 1000000\n')
    C.write('#endif\n\n')

    maxSize = GetMaxDataSize(cfg)
    # need at least 3*sizeof(Fee_BankAdminType), if page size is 8, that is 3*32 = 96
    if maxSize < 128:
        maxSize = 128
    maxSize = int((maxSize+31)/32)*32
    C.write('#ifndef FEE_WORKING_AREA_SIZE\n')
    C.write('#define FEE_WORKING_AREA_SIZE %s\n' % (maxSize))
    C.write('#endif\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write('void NvM_JobEndNotification(void);\n')
    C.write('void NvM_JobErrorNotification(void);\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    GenConstants(C, cfg)
    C.write('static const Fee_BlockConfigType Fee_BlockConfigs[] = {\n')
    Cnt = 0
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(repeat):
            name = block['name'].format(i)
            size = GetBlockSize(block)
            NumberOfWriteCycles = block.get('NumberOfWriteCycles', 10000000)
            C.write('  { FEE_NUMBER_%s, %s, %s, &%s_Rom },\n' %
                    (name, size, NumberOfWriteCycles, name))
            Cnt += 1
    C.write('};\n\n')
    C.write('static uint32_t Fee_BlockDataAddress[%s];\n' % (Cnt))
    C.write('static const Fee_BankType Fee_Banks[] = {\n')
    C.write('  {FLS_BASE_ADDRESS, FLS_BASE_ADDRESS + 64 * 1024},\n')
    C.write('  {FLS_BASE_ADDRESS + 64 * 1024, FLS_BASE_ADDRESS + 128 * 1024},\n')
    C.write('};\n\n')

    C.write(
        'static uint32_t Fee_WorkingArea[FEE_WORKING_AREA_SIZE/sizeof(uint32_t)];\n')
    C.write('const Fee_ConfigType Fee_Config = {\n')
    C.write('  NvM_JobEndNotification,\n')
    C.write('  NvM_JobErrorNotification,\n')
    C.write('  Fee_BlockDataAddress,\n')
    C.write('  Fee_BlockConfigs,\n')
    C.write('  ARRAY_SIZE(Fee_BlockConfigs),\n')
    C.write('  Fee_Banks,\n')
    C.write('  ARRAY_SIZE(Fee_Banks),\n')
    C.write('  (uint8_t*)Fee_WorkingArea,\n')
    C.write('  sizeof(Fee_WorkingArea),\n')
    C.write('  FEE_MAX_JOB_RETRY,\n')
    C.write('  FEE_MAX_DATA_SIZE,\n')
    C.write('  FEE_MAX_ERASED_NUMBER,\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def Gen_NvM(cfg, dir):
    H = open('%s/NvM_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef NVM_CFG_H\n')
    H.write('#define NVM_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write('#include "Std_Types.h"\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    target = cfg.get('target', 'Ea')
    if target != 'Fee':
        H.write('#define NVM_BLOCK_USE_CRC\n')
    else:
        H.write('/* NVM target is FEE, CRC is not used */\n')
    H.write('#define MEMIF_ZERO_COST_%s\n' % (target.upper()))
    Number = 2
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(repeat):
            name = block['name'].format(i)
            H.write('#define NVM_BLOCKID_%s %s\n' % (name, Number))
            Number += 1
    H.write('#define NVM_BLOCK_NUMBER %d\n' % (Number-1))
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    GenTypes(H, cfg)
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /* NVM_CFG_H */\n')
    H.close()

    C = open('%s/NvM_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "NvM.h"\n')
    C.write('#include "NvM_Cfg.h"\n')
    C.write('#include "NvM_Priv.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    if target != 'Fee':
        maxSize = GetMaxDataSize(cfg)
        maxSize = int((maxSize+4+3)/4)*4
        C.write('#ifndef NVM_WORKING_AREA_SIZE\n')
        C.write('#define NVM_WORKING_AREA_SIZE %s\n' % (maxSize))
        C.write('#endif\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    if target != 'Fee':
        GenConstants(C, cfg)
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(repeat):
            name = block['name'].format(i)
            C.write('%sType %s_Ram;\n' % (GetName(block), name))
    C.write(
        'static const NvM_BlockDescriptorType NvM_BlockDescriptors[] = {\n')
    Number = 1
    for block in cfg['blocks']:
        repeat = block.get('repeat', 1)
        for i in range(repeat):
            name = block['name'].format(i)
            if target != 'Fee':
                C.write('  { &%s_Ram, %s, sizeof(%sType), NVM_CRC16, &%s_Rom },\n' % (
                    name, Number, GetName(block), name))
            else:
                C.write('  { &%s_Ram, %s, sizeof(%sType) },\n' %
                        (name, Number, GetName(block)))
            Number += 1
    C.write('};\n\n')
    C.write('static uint16_t NvM_JobReadMasks[(NVM_BLOCK_NUMBER+15)/16];\n')
    C.write('static uint16_t NvM_JobWriteMasks[(NVM_BLOCK_NUMBER+15)/16];\n')
    if target != 'Fee':
        C.write(
            'static uint8_t NvM_WorkingArea[NVM_WORKING_AREA_SIZE];\n')
    C.write('const NvM_ConfigType NvM_Config = {\n')
    C.write('  NvM_BlockDescriptors,\n')
    C.write('  ARRAY_SIZE(NvM_BlockDescriptors),\n')
    C.write('  NvM_JobReadMasks,\n')
    C.write('  NvM_JobWriteMasks,\n')
    if target != 'Fee':
        C.write('  NvM_WorkingArea,\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    target = cfg.get('target', 'Ea')
    if target == 'Fee':
        Gen_Fee(cfg, dir)
    elif target == 'Ea':
        Gen_Ea(cfg, dir)
    else:
        raise
    Gen_NvM(cfg, dir)
