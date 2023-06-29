# SSAS - Simple Smart Automotive Software
# Copyright (C) 2023 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ['Gen']


def Gen_Trace(cfg, dir):
    area = cfg['area']
    H = open('%s/Trace%s_Cfg.h' % (dir, area), 'w')
    GenHeader(H)
    H.write('#ifndef __STD_TRACE_%s_CFG_H\n' % (area.upper()))
    H.write('#define __STD_TRACE_%s_CFG_H\n' % (area.upper()))
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write('#include "Std_Timer.h"\n')
    H.write('#include "Std_Trace.h"\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write('#ifndef TRE_%s_TIMER\n' % (area.upper()))
    H.write('#define TRE_%s_TIMER Std_GetTime()\n' % (area.upper()))
    H.write('#endif\n\n')
    for index, ev in enumerate(cfg['events']):
        H.write('#define TRE_%s_%s %s\n' % (area.upper(), toMacro(ev), index))
    H.write('#define TRE_%s_MAX %s\n\n' % (area.upper(), len(cfg['events'])))
    # let's support maximum 4096 events
    nBits = None
    for i in range(1, 13):
        if ((1 << i) > len(cfg['events'])):
            nBits = i
            break
    if nBits is None:
        raise Exception("too much events")
    H.write('#define TRE_%s_ID_BITS %s\n' % (area.upper(), nBits))
    H.write('#define TRE_%s_TS_BITS %s\n' % (area.upper(), 32-nBits))
    H.write('#define TRE_%s_TS_MASK %s\n' % (area.upper(), hex((1 << (32-nBits))-1)))
    H.write('\n')
    H.write('#ifdef USE_TRACE_%s\n'%(area.upper()))
    H.write(
        '#define STD_TRACE_{0}(ev) STD_TRACE_EVENT( &Std_TraceArea_{0}, ( TRE_{0}_##ev << TRE_{0}_TS_BITS ) | ( TRE_{0}_TIMER & TRE_{0}_TS_MASK ) )\n\n'.format(area.upper()))
    H.write(
        '#define STD_TRACE_{0}_MAIN() STD_TRACE_MAIN( &Std_TraceArea_{0} )\n'.format(area.upper()))
    H.write('#else\n')
    H.write('#define STD_TRACE_%s(ev)\n'%(area.upper()))
    H.write('#define STD_TRACE_%s_MAIN()\n'%(area.upper()))
    H.write('#endif\n')

    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write('extern const Std_TraceAreaType Std_TraceArea_%s;\n' % (area.upper()))
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /*__STD_TRACE_%s_CFG_H */\n' % (area.upper()))
    H.close()

    C = open('%s/Trace%s_Cfg.c' % (dir, area), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "Std_Trace.h"\n')
    C.write('#ifdef USE_SHELL\n')
    C.write('#include "shell.h"\n')
    C.write('#endif\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    C.write('RB_DECLARE(TraceArea%s, Std_TraceEventType, %s);\n' % (area, cfg.get('size', 1024)))
    C.write('const Std_TraceAreaType Std_TraceArea_%s = {\n' % (area.upper()))
    C.write('  &rb_TraceArea%s,\n' % (area))
    C.write('};\n\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.write('#ifdef USE_SHELL\n')
    C.write('static int Shell_Trace%s(int argc, const char *argv[]) {\n' % (area))
    C.write('  Std_TraceDump(&Std_TraceArea_%s);\n' % (area.upper()))
    C.write('  return 0;\n')
    C.write('}\n')
    C.write('SHELL_REGISTER(trace_%s, "trace_%s\\n", Shell_Trace%s)\n' %
            (area.lower(), area.lower(), area))
    C.write('#endif\n')
    C.close()


def extract(cfg, dir):
    cfg_ = {}
    for k, v in cfg.items():
        if k not in ['durations']:
            cfg_[k] = v
    if 'events' not in cfg_:
        cfg_['events'] = []
    for dur in cfg.get('durations', []):
        cfg_['events'].append('%s_B' % (dur))
        cfg_['events'].append('%s_E' % (dur))
    with open('%s/Trace.json' % (dir), 'w') as f:
        json.dump(cfg_, f, indent=2)
    return cfg_


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cfg_ = extract(cfg, dir)
    Gen_Trace(cfg_, dir)
