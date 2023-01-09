# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ['Gen']


def GetSnapshotSize(cfg):
    size = 0
    for data in cfg['Environments']:
        size += GetDataSize(data)
    return size


def get_shell_basic_data_print(C, data, offset):
    C.write('  {\n')
    if data['type'] in ['uint8', 'uint16', 'uint32', ]:
        C.write('    %s_t *itsValue = (%s_t*)(data+%d);\n' % (data['type'], data['type'], offset))
        C.write('    printf("  %s = %%u (0x%%x)\\n", (uint32_t)*itsValue, (uint32_t)*itsValue);\n' %
                (data['name']))
    elif data['type'] in ['int8', 'int16', 'int32']:
        C.write('    %s_t *itsValue = (%s_t*)(data+%d);\n' % (data['type'], data['type'], offset))
        C.write('    printf("  %s = %%d (0x%%x)\\n", (int32_t)*itsValue, (int32_t)*itsValue);\n' %
                (data['name']))
    else:
        raise
    C.write('  }\n')


def gen_shell_data_print(C, data):
    C.write('#ifdef USE_SHELL\n')
    C.write('static void Dem_FFD_Print%s(uint8_t* data) {\n' % (data['name']))
    if data['type'] == 'struct':
        offset = 0
        for d in data['data']:
            get_shell_basic_data_print(C, d, offset)
            offset += GetDataSize(d)
    else:
        get_shell_basic_data_print(C, data, 0)
    C.write('}\n')
    C.write('#endif\n')


def Gen_Dem(cfg, dir):
    H = open('%s/Dem_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef DEM_CFG_H\n')
    H.write('#define DEM_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write('#define DEM_USE_NVM\n')
    H.write('#define DEM_MAX_FREEZE_FRAME_NUMBER 2\n')
    H.write('#define DEM_MAX_FREEZE_FRAME_DATA_SIZE %d\n' %
            (GetSnapshotSize(cfg)+1))
    H.write('#define DTC_ENVENT_NUM %d\n' % (len(cfg['DTCs'])))
    H.write('#ifndef DEM_MAX_FREEZE_FRAME_RECORD\n')
    H.write('#define DEM_MAX_FREEZE_FRAME_RECORD DTC_ENVENT_NUM\n')
    H.write('#endif\n')
    for i, data in enumerate(cfg['DTCs']):
        H.write('#define DEM_EVENT_ID_%s %s\n' % (data['name'], i))
    conditions = []
    for dtc in cfg['DTCs']:
        for c in dtc.get('conditions', []):
            if c not in conditions:
                conditions.append(c)
    if len(conditions) > 0:
        H.write('\n#define DEM_USE_ENABLE_CONDITION\n')
        H.write('#define DEM_NUM_OF_ENABLE_CONDITION %s\n\n' % (len(conditions)))
        for i, cond in enumerate(conditions):
            H.write('#define DEM_CONTIDION_%s (1<<%s)\n' % (toMacro(cond), i))
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
    H.write('#endif /* DEM_CFG_H */\n')
    H.close()

    C = open('%s/Dem_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "Dem_Priv.h"\n')
    C.write('#include "NvM_Cfg.h"\n')
    C.write('#ifdef USE_SHELL\n')
    C.write('#include "shell.h"\n')
    C.write('#endif\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    for i, data in enumerate(cfg['Environments']):
        C.write('#define DEM_FFD_%s %s\n' % (data['name'], i))
    C.write('\n')
    for i, data in enumerate(cfg['ExtendedDatas']):
        C.write('#define DEM_EXTD_%s %s\n' % (data['name'], i))
    C.write('\n')
    for i, data in enumerate(cfg['ExtendedDatas']):
        C.write('#define DEM_EXTD_%s_NUMBER %s\n' % (data['name'], hex(i+1)))
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    for i, data in enumerate(cfg['Environments']):
        C.write(
            'Std_ReturnType Dem_FFD_Get%s(Dem_EventIdType EventId, uint8_t *data);\n' % (data['name']))
        gen_shell_data_print(C, data)
    C.write('\n')
    for i, data in enumerate(cfg['ExtendedDatas']):
        C.write(
            'Std_ReturnType Dem_EXTD_Get%s(Dem_EventIdType EventId, uint8_t *data);\n' % (data['name']))
    C.write('\n')
    for i, data in enumerate(cfg['DTCs']):
        C.write('extern Dem_EventStatusRecordType Dem_NvmEventStatusRecord%s_Ram; /* %s */\n' %
                (i, data['name']))
    for i, data in enumerate(cfg['DTCs']):
        C.write('#if DEM_MAX_FREEZE_FRAME_RECORD > %s\n' % (i))
        C.write(
            'extern Dem_FreezeFrameRecordType Dem_NvmFreezeFrameRecord%s_Ram;\n' % (i))
        C.write('#endif\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    C.write(
        'static Dem_FreezeFrameRecordType* const Dem_NvmFreezeFrameRecord[DEM_MAX_FREEZE_FRAME_RECORD] = {\n')
    for i, data in enumerate(cfg['DTCs']):
        C.write('#if DEM_MAX_FREEZE_FRAME_RECORD > %s\n' % (i))
        C.write('  &Dem_NvmFreezeFrameRecord%s_Ram,\n' % (i))
        C.write('#endif\n')
    C.write('};\n\n')
    C.write('#ifdef DEM_USE_NVM\n')
    C.write(
        'static const uint16_t Dem_NvmFreezeFrameNvmBlockIds[DEM_MAX_FREEZE_FRAME_RECORD] = {\n')
    for i, data in enumerate(cfg['DTCs']):
        C.write('#if DEM_MAX_FREEZE_FRAME_RECORD > %s\n' % (i))
        C.write('  NVM_BLOCKID_Dem_NvmFreezeFrameRecord%s,\n' % (i))
        C.write('#endif\n')
    C.write('};\n')
    C.write('#endif\n\n')
    C.write(
        'static const Dem_FreeFrameDataConfigType FreeFrameDataConfigs[] = {\n')
    for data in cfg['Environments']:
        C.write('  {')
        C.write('    Dem_FFD_Get%s,\n' % (data['name']))
        C.write('    %s,\n' % (data['id']))
        C.write('    %s,\n' % (GetDataSize(data)))
        C.write('#ifdef USE_SHELL\n')
        C.write('    Dem_FFD_Print%s,\n' % (data['name']))
        C.write('#endif\n')
        C.write('  },')
    C.write('};\n\n')
    C.write(
        'static const Dem_ExtendedDataConfigType ExtendedDataConfigs[] = {\n')
    for i, data in enumerate(cfg['ExtendedDatas']):
        C.write('  {Dem_EXTD_Get%s, DEM_EXTD_%s_NUMBER, %s},\n' %
                (data['name'], data['name'], GetDataSize(data)))
    C.write('};\n\n')
    C.write(
        'static const Dem_DebounceCounterBasedConfigType Dem_DebounceCounterBasedDefault = {\n')
    C.write('  DEM_DEBOUNCE_FREEZE,\n')
    C.write('  /* DebounceCounterDecrementStepSize */ 2,\n')
    C.write('  /* DebounceCounterFailedThreshold */ 10,\n')
    C.write('  /* DebounceCounterIncrementStepSize */ 1,\n')
    C.write('  /* DebounceCounterJumpDown */ FALSE,\n')
    C.write('  /* DebounceCounterJumpDownValue */ 0,\n')
    C.write('  /* DebounceCounterJumpUp */ TRUE,\n')
    C.write('  /* DebounceCounterJumpUpValue */ 0,\n')
    C.write('  /* DebounceCounterPassedThreshold */ -10,\n')
    C.write('};\n\n')
    C.write('/* each Event can have different environment data that cares about */\n')
    C.write('static const uint16_t Dem_FreezeFrameDataIndexDefault[] = {\n')
    for data in cfg['Environments']:
        C.write('  DEM_FFD_%s,\n' % (data['name']))
    C.write('};\n\n')
    C.write('/* each Event can have different extended data that cares about*/\n')
    C.write('static const uint8_t Dem_ExtendedDataNumberIndexDefault[] = {\n')
    for data in cfg['ExtendedDatas']:
        C.write('  DEM_EXTD_%s,\n' % (data['name']))
    C.write('};\n\n')
    C.write(
        'static const Dem_FreezeFrameRecordClassType Dem_FreezeFrameRecordClassDefault = {\n')
    C.write('  Dem_FreezeFrameDataIndexDefault,\n')
    C.write('  ARRAY_SIZE(Dem_FreezeFrameDataIndexDefault),\n')
    C.write('};\n\n')
    C.write(
        'static const Dem_ExtendedDataClassType Dem_ExtendedDataClassDefault = {\n')
    C.write('  Dem_ExtendedDataNumberIndexDefault,\n')
    C.write('  ARRAY_SIZE(Dem_ExtendedDataNumberIndexDefault),\n')
    C.write('};\n\n')
    for i, data in enumerate(cfg['DTCs']):
        C.write('static const uint8_t Dem_FreezeFrameRecNumsFor%s[] = {%s, %s};\n' % (
            data['name'], 2*i+1, 2*i+2))
    C.write(
        'static const Dem_FreezeFrameRecNumClassType Dem_FreezeFrameRecNumClass[] = {\n')
    for i, data in enumerate(cfg['DTCs']):
        C.write('  {\n')
        C.write('    Dem_FreezeFrameRecNumsFor%s,\n' % (data['name']))
        C.write('    ARRAY_SIZE(Dem_FreezeFrameRecNumsFor%s),\n' %
                (data['name']))
        C.write('  },\n')
    C.write('};\n\n')
    C.write(
        'static const Dem_DTCAttributesType Dem_DTCAttributesDefault = {\n')
    C.write('  /* AgingAllowed */ TRUE,\n')
    C.write('  /* AgingCycleCounterThreshold */ 2,\n')
    C.write('  /* OperationCycleRef */ DEM_OPERATION_CYCLE_IGNITION,\n')
    C.write('  /* EventConfirmationThreshold */ 1,\n')
    C.write('  /* OccurrenceCounterProcessing */ DEM_PROCESS_OCCCTR_TF,\n')
    C.write('  /* FreezeFrameRecordTrigger */ DEM_TRIGGER_ON_TEST_FAILED,\n')
    C.write('  /* DebounceAlgorithmClass */ DEM_DEBOUNCE_COUNTER_BASED,\n')
    C.write('  &Dem_DebounceCounterBasedDefault,\n')
    C.write('  &Dem_ExtendedDataClassDefault,\n')
    C.write('  &Dem_FreezeFrameRecordClassDefault,\n')
    C.write('};\n\n')
    C.write(
        'static const Dem_EventConfigType EventConfigs[DTC_ENVENT_NUM] = {\n')
    for i, data in enumerate(cfg['DTCs']):
        C.write('  {\n')
        C.write('    %s,\n' % (data['number']))
        C.write('    &Dem_NvmEventStatusRecord%s_Ram,\n' % (i))
        C.write('    &Dem_FreezeFrameRecNumClass[%s],\n' % (i))
        C.write('    &Dem_DTCAttributesDefault,\n')
        C.write('    %s,\n' % (data['priority']))
        C.write('#ifdef DEM_USE_NVM\n')
        C.write('    NVM_BLOCKID_Dem_NvmEventStatusRecord%s,\n' % (i))
        C.write('#endif\n')
        C.write('#ifdef DEM_USE_ENABLE_CONDITION\n')
        mask = '0'
        for cond in data.get('conditions', []):
            mask += '|DEM_CONTIDION_%s' % (toMacro(cond))
        C.write('    %s,\n' % (mask))
        C.write('#endif\n')
        C.write('  },\n')
    C.write('};\n\n')
    C.write('static Dem_EventContextType EventContexts[DTC_ENVENT_NUM];\n')
    C.write(
        'static Dem_OperationCycleStateType Dem_OperationCycleStates[2];\n')
    C.write('const Dem_ConfigType Dem_Config = {\n')
    C.write('  FreeFrameDataConfigs,\n')
    C.write('  ARRAY_SIZE(FreeFrameDataConfigs),\n')
    C.write('  ExtendedDataConfigs,\n')
    C.write('  ARRAY_SIZE(ExtendedDataConfigs),\n')
    C.write('  Dem_NvmFreezeFrameRecord,\n')
    C.write('#ifdef DEM_USE_NVM\n')
    C.write('  Dem_NvmFreezeFrameNvmBlockIds,\n')
    C.write('#endif\n')
    C.write('  ARRAY_SIZE(Dem_NvmFreezeFrameRecord),\n')
    C.write('  EventConfigs,\n')
    C.write('  EventContexts,\n')
    C.write('  ARRAY_SIZE(EventConfigs),\n')
    C.write('  Dem_OperationCycleStates,\n')
    C.write('  ARRAY_SIZE(Dem_OperationCycleStates),\n')
    C.write('  /* TypeOfFreezeFrameRecordNumeration */ DEM_FF_RECNUM_CONFIGURED,\n')
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
    Gen_Dem(cfg, dir)
