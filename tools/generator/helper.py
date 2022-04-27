# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import time


def toMacro(s):
    words = []
    word = None
    for a in s:
        if a.isupper() or a.isdigit():
            if word != None:
                a_ = word[-1]
                if a_.isupper() or a_.isdigit():
                    word += a
                    continue
                else:
                    words.append(word)
            word = a
        else:
            if word == None:
                word = a
            else:
                word += a
    words.append(word)
    sss = '_'.join(s.upper() for s in words)
    for s, r in [('CAN_TP', 'CANTP'), ('LIN_TP', 'LINTP'), ('PDU_R', 'PDUR'), ('CAN_IF', 'CANIF'),
                 ('__', '_')]:
        sss = sss.replace(s, r)
    return sss


def toPduSymbol(names):
    return '_'.join([toMacro(n) for n in names])

def GenHeader(f):
    GenTime = time.asctime(time.localtime(time.time()))
    f.write('/**\n')
    f.write(' * SSAS - Simple Smart Automotive Software\n')
    f.write(' * Copyright (C) 2021 Parai Wang <parai@foxmail.com>\n')
    f.write(' *\n')
    f.write(' * Generated at %s\n' % (GenTime))
    f.write(' */\n')
    print('GEN %s' % (os.path.abspath(f.name)))


TypeInfoMap = {
    'uint8': {'size': 1, 'IsArray': False, 'ctype': 'uint8_t'},
    'int8': {'size': 1, 'IsArray': False, 'ctype': 'int8_t'},
    'uint16': {'size': 2, 'IsArray': False, 'ctype': 'uint16_t'},
    'int16': {'size': 2, 'IsArray': False, 'ctype': 'int16_t'},
    'uint32': {'size': 4, 'IsArray': False, 'ctype': 'uint32_t'},
    'int32': {'size': 4, 'IsArray': False, 'ctype': 'int32_t'},
    'uint64': {'size': 8,  'IsArray': False, 'ctype': 'uint64_t'},
    'int164': {'size': 8, 'IsArray': False, 'ctype': 'uint64_t'},
    'float': {'size': 4, 'IsArray': False, 'ctype': 'float'},
    'double': {'size': 8, 'IsArray': False, 'ctype': 'double'},
    'uint8_n': {'size': 1, 'IsArray': True, 'ctype': 'uint8_t'},
    'int8_n': {'size': 1, 'IsArray': True, 'ctype': 'int8_t'},
    'uint16_n': {'size': 2, 'IsArray': True, 'ctype': 'uint16_t'},
    'int16_n': {'size': 2, 'IsArray': True, 'ctype': 'int16_t'},
    'uint32_n': {'size': 4, 'IsArray': True, 'ctype': 'uint32_t'},
    'int32_n': {'size': 4, 'IsArray': True, 'ctype': 'int32_t'},
    'uint64_n': {'size': 8, 'IsArray': True, 'ctype': 'uint64_t'},
    'int64_n': {'size': 8, 'IsArray': True, 'ctype': 'int64_t'},
}


def GetDataSize(data):
    if data['type'] == 'struct':
        size = 0
        for sd in data['data']:
            size += GetDataSize(sd)
        return size
    return TypeInfoMap[data['type']]['size']*data.get('size', 1)*data.get('repeat', 1)
