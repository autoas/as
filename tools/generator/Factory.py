# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ['Gen']

def Gen_Factory(dir, cfg):
    factoryName = cfg['name']
    H = open('%s/%s_Factory.h' % (dir, factoryName), 'w')
    GenHeader(H)
    H.write('#ifndef _%s_FACTORY_H\n' % (factoryName.upper()))
    H.write('#define _%s_FACTORY_H\n' % (factoryName.upper()))
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write('#include "factory.h"\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    for id, event in enumerate(cfg['events']):
        H.write('#define %s_EVENT_%s %s\n' %
                (factoryName.upper(), event.upper(), id))
    H.write('\n')

    for id, machine in enumerate(cfg['machines']):
        machineName = machine['name']
        H.write('#define %s_MACHINE_%s %s\n' %
                (factoryName.upper(), machineName.upper(), id))
    H.write('\n')

    for machine in cfg['machines']:
        machineName = machine['name']
        for id, nodeName in enumerate(machine['nodes']):
            H.write('#define %s_NODE_%s_%s %s\n' %
                    (factoryName.upper(), machineName.upper(), toMacro(nodeName), id))
        H.write('\n')
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    for machine in cfg['machines']:
        machineName = machine['name']
        for nodeName in machine['nodes']:
            H.write('Std_ReturnType %s_%s_%s_Main(void);\n' %
                    (factoryName, machineName, nodeName))
            for event in cfg['events']:
                H.write('Std_ReturnType %s_%s_%s_%s(void);\n' %
                        (factoryName, machineName, nodeName, event))
            H.write('\n')
    H.write('void %s_FactoryStateNotification(uint8_t machineId, machine_state_t state);\n\n' %
            (factoryName))
    H.write('extern const factory_t %s_Factory;\n' % (factoryName))
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /* _%s_FACTORY_H*/\n' % (factoryName.upper()))
    H.close()

    C = open('%s/%s_Factory.c' % (dir, factoryName), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "%s_Factory.h"\n' % (factoryName))
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    for machine in cfg['machines']:
        machineName = machine['name']
        for nodeName in machine['nodes']:
            C.write('static const factory_event_t %s_%s_%s_Events[] = {\n' %
                    (factoryName, machineName, nodeName))
            for event in cfg['events']:
                C.write('  %s_%s_%s_%s,\n' %
                        (factoryName, machineName, nodeName, event))
            C.write('};\n\n')
    for machine in cfg['machines']:
        machineName = machine['name']
        C.write('static const machine_node_t %s_%s_Nodes[] = {\n' % (
            factoryName, machineName))
        for nodeName in machine['nodes']:
            C.write('  {\n')
            C.write('    "%s",\n' % (nodeName))
            C.write('    %s_%s_%s_Main,\n' %
                    (factoryName, machineName, nodeName))
            C.write('    %s_%s_%s_Events,\n' %
                    (factoryName, machineName, nodeName))
            C.write('    ARRAY_SIZE(%s_%s_%s_Events),\n' %
                    (factoryName, machineName, nodeName))
            C.write('  },\n')
        C.write('};\n\n')

    C.write(
        'static const machine_t %s_FactoryMachines[] = {\n' % (factoryName))
    for machine in cfg['machines']:
        machineName = machine['name']
        C.write('  {\n')
        C.write('    "%s",\n' % (machineName))
        C.write('    %s_%s_Nodes,\n' % (factoryName, machineName))
        C.write('    ARRAY_SIZE(%s_%s_Nodes),\n' % (factoryName, machineName))
        C.write('  },\n')
    C.write('};\n\n')

    C.write('static factory_context_t %s_FactoryContext;\n' % (factoryName))
    C.write('const factory_t %s_Factory = {\n' % (factoryName))
    C.write('  "%s",\n' % (factoryName))
    C.write('  &%s_FactoryContext,\n' % (factoryName))
    C.write('  %s_FactoryMachines,\n' % (factoryName))
    C.write('  ARRAY_SIZE(%s_FactoryMachines),\n' % (factoryName))
    C.write('  %s_FactoryStateNotification,\n' % (factoryName))
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
    Gen_Factory(dir, cfg)
