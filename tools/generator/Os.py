# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>
import os
import json
from .helper import *

__all__ = ['Gen']


def fixupRes(prefix, cfg):
    task_list = cfg.get('TaskList', [])
    for res in cfg.get('%sResourceList' % (prefix), []):
        prio = 0
        for tsk in task_list:
            for res2 in tsk.get('ResourceList', []):
                if (res['name'] == res2['name']):
                    res2['Type'] = prefix
                    if (prio < tsk['Priority']):
                        prio = tsk['Priority']
        res['Priority'] = prio


def fixupEvt(cfg):
    evList = cfg.get('EventList', [])
    task_list = cfg.get('TaskList', [])
    for tsk in task_list:
        masks = []
        for ev in tsk.get('EventList', []):
            for ev2 in evList:
                if (ev['name'] == ev2['name']):
                    ev['Mask'] = ev2['Mask']
                    if (ev['Mask'] != 'AUTO'):
                        masks.append(ev['Mask'])
        for ev in tsk.get('EventList', []):
            if (ev['Mask'] == 'AUTO'):
                for id in range(0, 32):
                    mask = 1 << id
                    if mask not in masks:
                        masks.append(mask)
                        ev['Mask'] = mask
                        break


def fixup(cfg):
    fixupRes('', cfg)
    fixupRes('Internal', cfg)
    fixupEvt(cfg)


def Gen_Os(cfg, dir):
    fixup(cfg)
    H = open('%s/Os_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef _OS_CFG_H\n')
    H.write('#define _OS_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write('#ifndef MACROS_ONLY\n')
    H.write('#include "kernel.h"\n')
    H.write('#include "./GEN/TraceOS_Cfg.h"\n')
    H.write('#endif\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    for hn1, hn2 in [('ErrorHook', 'ERROR_HOOK'), ('StartupHook', 'STARTUP_HOOK'),
                     ('ShutdownHook', 'SHUTDOWN_HOOK'), ('PreTaskHook', 'PRETASK_HOOK'),
                     ('PostTaskHook', 'POSTTASK_HOOK')]:
        if cfg.get(hn1, False):
            H.write('#define OS_USE_%s\n' % (hn2))
    if cfg.get('PTHREAD', 0) > 0:
        H.write('#define USE_PTHREAD\n')
    H.write('#define OS_PTHREAD_NUM %s\n' % (cfg.get('PTHREAD', 0)))
    H.write('#define OS_PTHREAD_PRIORITY %s\n' % (cfg.get('PTHREAD_PRIORITY', 0)))
    H.write('#define CPU_CORE_NUMBER %s\n' % (cfg.get('CPU_CORE_NUMBER', 1)))
    H.write('#define OS_STATUS %s\n' % (cfg.get('Status', 'EXTENDED')))
    H.write('\n\n')
    task_list = cfg.get('TaskList', [])
    maxPrio = 0
    multiPrio = False
    multiAct = False
    sumAct = 0
    prioList = []
    prioAct = {}
    maxPrioAct = 0
    for id, task in enumerate(task_list):
        prio = task['Priority']
        act = task.get('Activation', 1)
        sumAct += act
        if prio in prioAct:
            prioAct[prio] += act
        else:
            prioAct[prio] = act
        if (task.get('Activation', 1) > 1):
            multiAct = True
        if prio in prioList:
            multiPrio = True
        else:
            prioList.append(prio)
        if (prio > maxPrio):
            maxPrio = prio
    for prio, act in prioAct.items():
        if (maxPrioAct < act):
            maxPrioAct = act
    maxPrioAct += 1  # in case resource ceiling
    seqMask = 0
    seqShift = 0
    for i in range(1, maxPrioAct+1):
        seqMask |= i
    for i in range(0, 32):
        if ((seqMask >> i) == 0):
            seqShift = i
            break
    H.write('#define PRIORITY_NUM (OS_PTHREAD_PRIORITY+%s)\n' % (maxPrio))
    H.write('#define ACTIVATION_SUM (%s+OS_PTHREAD_NUM)\n' % (sumAct+1))
    if (multiPrio):
        H.write('#define MULTIPLY_TASK_PER_PRIORITY\n')
        H.write('#define SEQUENCE_MASK 0x%Xu\n' % (seqMask))
        H.write('#define SEQUENCE_SHIFT %d\n' % (seqShift))
    if (multiAct):
        H.write('#define MULTIPLY_TASK_ACTIVATION\n')
    H.write('\n\n')
    for id, task in enumerate(task_list):
        H.write('#define TASK_ID_%-32s %-3s /* priority = %s */\n' %
                (task['name'], id, task['Priority']))
    H.write('#define TASK_NUM%-32s %s\n\n' % (' ', id+1))
    alarm_list = cfg.get('AlarmList', [])
    appmode = []
    for id, obj in enumerate(task_list+alarm_list):
        for mode in obj.get('ApplicationModeList', ['OSDEFAULTAPPMODE']):
            if (mode != 'OSDEFAULTAPPMODE'):
                if mode not in appmode:
                    appmode.append(mode)
    for id, mode in enumerate(appmode):
        H.write('#define %s ((AppModeType)(1<<%s))\n' % (mode, id+1))
    withEvt = False
    for task in task_list:
        for ev in task.get('EventList', []):
            withEvt = True
            H.write('#define EVENT_MASK_%-40s %s\n' %
                    ('%s_%s' % (task['name'], ev['name']), ev['Mask']))
    H.write('\n')
    if (withEvt):
        H.write('\n#define EXTENDED_TASK\n\n')
    res_list = cfg.get('ResourceList', [])
    for id, res in enumerate(res_list):
        if (res['name'] == 'RES_SCHEDULER'):
            continue
        H.write('#define RES_ID_%-32s %s\n' % (res['name'], id+1))
    H.write('#define RESOURCE_NUM %s\n\n' % (len(res_list)+1))
    id = -1
    counter_list = cfg.get('CounterList', [])
    for id, counter in enumerate(counter_list):
        H.write('#define COUNTER_ID_%-32s %s\n' % (counter['name'], id))
    H.write('#define COUNTER_NUM%-32s %s\n\n' % (' ', id+1))
    id = -1
    for id, alarm in enumerate(alarm_list):
        H.write('#define ALARM_ID_%-32s %s\n' % (alarm['name'], id))
    H.write('#define ALARM_NUM%-32s %s\n\n' % (' ', id+1))
    isr_list = cfg.get('ISRList', [])
    isr_num = len(isr_list)
    for isr in isr_list:
        if ((isr['Vector']+1) > isr_num):
            isr_num = isr['Vector']+1
    H.write('#define ISR_NUM  %s\n\n' % (isr_num))
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write('#ifndef MACROS_ONLY\n')
    for task in task_list:
        H.write('DeclareTask(%s);\n' % (task['name']))
    H.write('\n')
    for task in task_list:
        for ev in task.get('EventList', []):
            H.write('DeclareEvent(%s);\n' % (ev['name']))
    H.write('\n')
    for res in res_list:
        if (res['name'] == 'RES_SCHEDULER'):
            continue
        H.write('DeclareResource(%s);\n' % (res['name']))
    H.write('\n')
    for counter in counter_list:
        H.write('DeclareCounter(%s);\n' % (counter['name']))
    H.write('\n')
    for alarm in alarm_list:
        H.write('DeclareAlarm(%s);\n' % (alarm['name']))
    H.write('#endif\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#ifndef MACROS_ONLY\n')
    for task in task_list:
        H.write('extern TASK(%s);\n' % (task['name']))
    H.write('\n\n')
    for alarm in alarm_list:
        H.write('extern ALARM(%s);\n' % (alarm['name']))
    H.write('\n\n')
    H.write('#endif\n')
    H.write('#endif /* _OS_CFG_H */\n')
    H.close()

    C = open('%s/Os_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#include "kernel_internal.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write('#ifndef OS_STK_SIZE_SCALER\n#define OS_STK_SIZE_SCALER 1\n#endif\n')
    C.write('#ifndef ISR_ATTR\n#define ISR_ATTR\n#endif\n')
    C.write('#ifndef ISR_ADDR\n#define ISR_ADDR(isr) isr\n#endif\n')
    C.write('#ifdef USE_TRACE\n')
    C.write('#define STD_TRACE_OS2(ev) STD_TRACE_EVENT( &Std_TraceArea_OS, ( ev << TRE_OS_TS_BITS ) | ( TRE_OS_TIMER & TRE_OS_TS_MASK ) )\n')
    C.write('#endif\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    isr_list = cfg.get('ISRList', [])
    for isr in isr_list:
        C.write('extern void ISR_ATTR %s (void);\n' % (isr['name']))
    C.write(
        'void ISR_ATTR __weak default_isr_handle(void) { ShutdownOS(0xEE); }\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    CPU_CORE_NUMBER = cfg.get('CPU_CORE_NUMBER', 1)
    pthnum = cfg.get('PTHREAD', 0)
    pthprio = cfg.get('PTHREAD_PRIORITY', 0)
    task_list = cfg.get('TaskList', [])
    for task in task_list:
        C.write('static uint32_t %s_Stack[(%s*OS_STK_SIZE_SCALER+sizeof(uint32_t)-1)/sizeof(uint32_t)];\n' % (
            task['name'], task.get('StackSize', 512)))
        if (len(task.get('EventList', [])) > 0):
            C.write('static EventVarType %s_EventVar;\n' % (task['name']))
    C.write('#if (OS_STATUS == EXTENDED)\n')
    inres_list = cfg.get('InternalResource', [])
    for task in task_list:
        cstr = ''
        for res in task.get('ResourceList', []):
            skip = False
            for ires in inres_list:
                if (res['name'] == ires['name']):
                    skip = True
            if (not skip):
                cstr += '    case RES_ID_%s:\n' % (res['name'])
        C.write('''static boolean %s_CheckAccess(ResourceType ResID)
{
    boolean bAccess = FALSE;

    switch(ResID)
    {
        case RES_SCHEDULER:
%s            bAccess = TRUE;
        break;
        default:
            break;
    }

    return bAccess;
}\n''' % (task['name'], cstr))
    C.write('#endif\n')
    C.write('const TaskConstType TaskConstArray[TASK_NUM] =\n{\n')
    for task in task_list:
        runPrio = task['Priority']
        if (task.get('Schedule', 'FULL') == 'NON'):
            runPrio = 'PRIORITY_NUM'
        else:
            # generall task should has at most one internal resource
            for res in task.get('ResourceList', []):
                for ires in inres_list:
                    if (res['name'] == ires['name']):
                        if (ires['Priority'] > runPrio):
                            runPrio = res['Priority']
        maxAct = task.get('Activation', 1)
        event = 'NULL'
        if (len(task.get('EventList', [])) > 0):
            if (maxAct > 1):
                raise Exception('Task<%s>: multiple requesting of task activation allowed for basic tasks' % (
                    task['name']))
            maxAct = 1
            event = '&%s_EventVar' % (task['name'])

        def AST(task):
            if (task.get('AutoStart', False)):
                cstr = '(0'
                for appmode in task.get('ApplicationModeList', ['OSDEFAULTAPPMODE']):
                    cstr += ' | (%s)' % (appmode)
                cstr += ')'
                return cstr
            return 0
        C.write('  {\n')
        C.write('    /*.pStack =*/ %s_Stack,\n' % (task['name']))
        C.write('    /*.stackSize =*/ sizeof(%s_Stack),\n' % (task['name']))
        C.write('    /*.entry =*/ TaskMain%s,\n' % (task['name']))
        C.write('    #ifdef EXTENDED_TASK\n')
        C.write('    /*.pEventVar =*/ %s,\n' % (event))
        C.write('    #endif\n')
        C.write('    /*.appModeMask =*/ %s,\n' % (AST(task)))
        C.write('    #if (OS_STATUS == EXTENDED)\n')
        C.write('    /*.CheckAccess =*/ %s_CheckAccess,\n' % (task['name']))
        C.write('    #endif\n')
        C.write('    /*.name =*/ "%s",\n' % (task['name']))
        # for IDLE task, priority is 0.
        if (task['name'][:8] == 'TaskIdle'):
            C.write('    /*.initPriority =*/ 0,\n')
            C.write('    /*.runPriority =*/ 0,\n')
        else:
            C.write('    /*.initPriority =*/ OS_PTHREAD_PRIORITY + %s,\n' % (task['Priority']))
            C.write('    /*.runPriority =*/ OS_PTHREAD_PRIORITY + %s,\n' % (runPrio))
        C.write('    #ifdef MULTIPLY_TASK_ACTIVATION\n')
        C.write('    /*.maxActivation =*/ %s,\n' % (maxAct))
        C.write('    #endif\n')
        if (CPU_CORE_NUMBER > 1):
            cpu = task.get('Cpu', 'OS_ON_ANY_CPU')
            C.write('    /*.cpu = */ %s,\n' % (cpu))
            if ((maxAct > 1) and (cpu == 'OS_ON_ANY_CPU')):
                raise Exception('Task<%s>: must be assigned to one specific CPU as max activation is %s > 1' % (
                    task['name'], maxAct))
        C.write('  },\n')
    C.write('};\n\n')
    C.write('const ResourceConstType ResourceConstArray[RESOURCE_NUM] =\n{\n')
    C.write('  {\n')
    C.write('    /*.ceilPrio =*/ PRIORITY_NUM, /* RES_SCHEDULER */\n')
    C.write('  },\n')
    res_list = cfg.get('ResourceList', [])
    for res in res_list:
        if (res['name'] == 'RES_SCHEDULER'):
            continue
        C.write('  {\n')
        C.write('    /*.ceilPrio =*/ OS_PTHREAD_PRIORITY + %s, /* %s */\n' %
                (res['Priority'], res['name']))
        C.write('  },\n')
    C.write('};\n\n')
    counter_list = cfg.get('CounterList', [])
    if (len(counter_list) > 0):
        C.write('CounterVarType CounterVarArray[COUNTER_NUM];\n')
        C.write('const CounterConstType CounterConstArray[COUNTER_NUM] =\n{\n')
        for counter in counter_list:
            C.write('  {\n')
            C.write('    /*.name=*/"%s",\n' % (counter['name']))
            C.write('    /*.pVar=*/&CounterVarArray[COUNTER_ID_%s],\n' % (counter['name']))
            C.write('    /*.base=*/{\n      /*.maxallowedvalue=*/%s,\n' %
                    (counter.get('MaxAllowed', 65535)))
            C.write('    /*.ticksperbase=*/%s,\n' % (counter.get('TicksPerBase', 1)))
            C.write('    /*.mincycle=*/%s\n    }\n' % (counter.get('MinCycle', 1)))
            C.write('  },\n')
        C.write('};\n\n')
    alarm_list = cfg.get('AlarmList', [])
    if (len(alarm_list) > 0):
        for alarm in alarm_list:
            C.write('static void %s_Action(void)\n{\n' % (alarm['name']))
            if (alarm['Action'].upper() == 'ACTIVATETASK'):
                C.write('  (void)ActivateTask(TASK_ID_%s);\n' % (alarm['Task']))
            elif (alarm['Action'].upper() == 'SETEVENT'):
                C.write('  (void)SetEvent(TASK_ID_%s,EVENT_MASK_%s_%s);\n' %
                        (alarm['Task'], alarm['Task'], alarm['Event']))
            elif (alarm['Action'].upper() == 'CALLBACK'):
                C.write('  extern ALARM(%s);\n  AlarmMain%s();\n' %
                        (alarm['Callback'], alarm['Callback']))
            elif (alarm['Action'].upper() == 'SIGNALCOUNTER'):
                C.write('  (void)SignalCounter(COUNTER_ID_%s);\n' % (alarm['Counter']))
            else:
                assert (0)
            C.write('}\n')
        C.write('AlarmVarType AlarmVarArray[ALARM_NUM];\n')
        C.write('const AlarmConstType AlarmConstArray[ALARM_NUM] =\n{\n')
        for alarm in alarm_list:
            def AST(alarm):
                if (alarm.get('AutoStart', False)):
                    cstr = '(0'
                    for appmode in alarm.get('ApplicationModeList', ['OSDEFAULTAPPMODE']):
                        cstr += ' | (%s)' % (appmode)
                    cstr += ')'
                    return cstr, alarm['StartTime'], alarm['Period']
                return 0, 0, 0
            C.write('  {\n')
            C.write('    /*.name=*/"%s",\n' % (alarm['name']))
            C.write(
                '    /*.pVar=*/&AlarmVarArray[ALARM_ID_%s],\n' % (alarm['name']))
            C.write(
                '    /*.pCounter=*/&CounterConstArray[COUNTER_ID_%s],\n' % (alarm['Driver']))
            C.write('    /*.Action=*/%s_Action,\n' % (alarm['name']))
            appmode, start, period = AST(alarm)
            C.write('    /*.appModeMask=*/%s,\n' % (appmode))
            C.write('    /*.start=*/%s,\n' % (start))
            C.write('    /*.period=*/%s,\n' % (period))
            C.write('  },\n')
        C.write('};\n\n')

    for task in task_list:
        C.write('const TaskType %s = TASK_ID_%s;\n' % (task['name'], task['name']))
    C.write('\n')
    evList = []
    for task in task_list:
        for ev in task.get('EventList', []):
            if ev in evList:
                print('WARNING: %s for %s is with the same name with others' %
                      (ev['name'], task['name']))
            else:
                evList.append(ev['name'])
                C.write('const EventMaskType %s = EVENT_MASK_%s_%s;\n' % (
                    ev['name'], task['name'], ev['name']))
    C.write('\n')
    for res in res_list:
        if (res['name'] == 'RES_SCHEDULER'):
            continue
        C.write('const ResourceType %s = RES_ID_%s;\n' % (res['name'], res['name']))
    C.write('\n')
    for counter in counter_list:
        C.write('const CounterType %s = COUNTER_ID_%s;\n' % (counter['name'], counter['name']))
    C.write('\n')
    for alarm in alarm_list:
        C.write('const AlarmType %s = ALARM_ID_%s;\n' % (alarm['name'], alarm['name']))

    maxPrio = 0
    for task in task_list:
        prio = task['Priority']
        if (prio > maxPrio):
            maxPrio = prio
    if (CPU_CORE_NUMBER > 1):
        perCpu = '[CPU_CORE_NUMBER+1]'
    else:
        perCpu = ''
    C.write('#ifdef USE_SCHED_FIFO\n')
    C.write('#ifdef USE_PTHREAD\n')
    for prio in range(pthprio):
        sumact = pthnum
        C.write('static TaskType ReadyFIFO_pthread_prio%s%s[%s];\n' % (prio, perCpu, sumact))
    C.write('#endif\n')
    for prio in range(maxPrio+1):
        sumact = 3+2  # 2 for the ceiling of resource and one more additional slot
        comments = ''
        for task in task_list:
            prio2 = task['Priority']
            if (prio2 == prio):
                sumact += task.get('Activation', 1)
                comments += '%s(Activation=%s),' % (task['name'], task.get('Activation', 1))
        if (sumact > 5):
            C.write('static TaskType ReadyFIFO_prio%s%s[%s];\n' % (prio, perCpu, sumact))
    cstr = '\nconst ReadyFIFOType ReadyFIFO%s[PRIORITY_NUM+1]=\n{\n' % (perCpu)
    for i in range(CPU_CORE_NUMBER+1):
        if (CPU_CORE_NUMBER > 1):
            perCpu = '[%s]' % (i)
            cstr += '  {\n'
        else:
            perCpu = ''
        cstr += '#ifdef USE_PTHREAD\n'
        for prio in range(pthprio):
            sumact = pthnum
            cstr += '  {\n    /*.max=*/%s,\n    /*.pFIFO=*/ReadyFIFO_pthread_prio%s%s\n  },\n' % (
                sumact, prio, perCpu)
        cstr += '#endif\n'
        for prio in range(maxPrio+1):
            sumact = 3+2  # 2 for the ceiling of resource and one more additional slow
            comments = ''
            for id, task in enumerate(task_list):
                prio2 = task['Priority']
                if (prio2 == prio):
                    sumact += task.get('Activation', 1)
                    comments += '%s(Activation=%s),' % (task['name'], task.get('Activation', 1))
            if (sumact > 5):
                cstr += '  {\n    /*.max=*/%s,/* %s */\n    /*.pFIFO=*/ReadyFIFO_prio%s%s\n  },\n' % (
                    sumact, comments, prio, perCpu)
            else:
                cstr += '  {\n    /*.max=*/0,\n    /*.pFIFO=*/NULL\n  },\n'
        if (CPU_CORE_NUMBER > 1):
            cstr += '  },\n'
        else:
            break
    cstr += '};\n\n'
    C.write(cstr)
    C.write('#endif\n')
    isr_num = len(isr_list)
    for isr in isr_list:
        if ((isr['Vector']+1) > isr_num):
            isr_num = isr['Vector']+1
    if (isr_num > 0):
        C.write('#ifdef __HIWARE__\n#pragma DATA_SEG __NEAR_SEG .vectors\n')
        C.write('const uint16 tisr_pc[ %s ] = {\n' % (isr_num))
        C.write('#else\n')
        C.write('const FP tisr_pc[ %s ] = {\n' % (isr_num))
        C.write('#endif\n')
        for iid in range(isr_num):
            iname = 'default_isr_handle'
            for isr in isr_list:
                if (iid == isr['Vector']):
                    iname = isr['name']
                    break
            C.write('  ISR_ADDR(%s), /* %s */\n' % (iname, iid))
        C.write('};\n\n')
        C.write('#ifdef __HIWARE__\n#pragma DATA_SEG DEFAULT\n#endif\n')
    C.write('#ifdef USE_TRACE\n')
    C.write('static const Std_TraceEventType lOsTraceTask_B[] = {\n')
    idles = []
    for task in task_list:
        if (task['name'][:8] == 'TaskIdle'):
            idles.append(task['name'])
        C.write('  TRE_OS_%s_B,\n' % (toMacro(task['name'])))
    for i in range(cfg.get('PTHREAD', 0)):
        C.write('  TRE_OS_%s_B,\n' % (toMacro('pthread%s' % (i))))
    C.write('};\n')
    C.write('static const Std_TraceEventType lOsTraceTask_E[] = {\n')
    for task in task_list:
        C.write('  TRE_OS_%s_E,\n' % (toMacro(task['name'])))
    for i in range(cfg.get('PTHREAD', 0)):
        C.write('  TRE_OS_%s_E,\n' % (toMacro('pthread%s' % (i))))
    C.write('};\n')
    C.write('#endif\n\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.write('#ifdef USE_TRACE\n')
    C.write('void PreTaskHook(void) {\n')
    C.write('  TaskType tid = 0;\n')
    C.write('  Std_TraceEventType ev;\n')
    C.write('  GetTaskID(&tid);\n')
    if len(idles) > 0:
        C.write('  if ( %s ) { return; }\n' %
                (' || '.join(['(TASK_ID_%s == tid)' % (task) for task in idles])))
    C.write('  ev = lOsTraceTask_B[tid];\n')
    C.write('  STD_TRACE_OS2(ev);\n')
    C.write('}\n')

    C.write('void PostTaskHook(void) {\n')
    C.write('  TaskType tid = 0;\n')
    C.write('  Std_TraceEventType ev;\n')
    C.write('  GetTaskID(&tid);\n')
    if len(idles) > 0:
        C.write('  if ( %s ) { return; }\n' %
                (' || '.join(['(TASK_ID_%s == tid)' % (task) for task in idles])))
    C.write('  ev = lOsTraceTask_E[tid];\n')
    C.write('  STD_TRACE_OS2(ev);\n')
    C.write('}\n')
    C.write('#endif\n\n')
    C.close()


def fromOIL(pdir, cfg):
    oil = cfg['OIL']
    path = cfg.get('PATH', [])
    path += [pdir]
    from .oil import parse
    if not os.path.isfile(oil):
        oil = os.path.join(pdir, oil)
    tgt = '%s/GEN/%s' % (pdir, os.path.basename(oil))
    cmd = 'cp %s %s.h &&' % (oil, tgt)
    cmd += ' gcc -E %s.h -o %s.S' % (tgt, tgt)
    for p in path:
        cmd += ' -I%s' % (p)
    r = os.system(cmd)
    if 0 != r:
        raise Exception('failed to run %s' % (cmd))
    with open('%s.S' % (tgt)) as f:
        fo = open(tgt, 'w')
        for line in f.readlines():
            if not line.startswith('#'):
                fo.write(line)
        fo.close()
    cfg = parse(tgt)
    with open('%s/GEN/OS.json' % (pdir), 'w') as f:
        json.dump(cfg, f, indent=2)
    return cfg


def extract_trace(cfg, dir):
    from .Trace import Gen as TraceGen
    cfg_ = {'class': 'Trace', 'area': 'OS', 'size': cfg.get(
        'trace_size', 1024), 'durations': [], 'events': []}
    for tsk in cfg.get('TaskList', []):
        cfg_['durations'].append(tsk['name'])
        for ev in tsk.get('EventList', []):
            if ev not in cfg_['events']:
                cfg_['events'].append(ev['name'])
    for i in range(cfg.get('PTHREAD', 0)):
        cfg_['durations'].append('pthread%s' % (i))
    with open('%s/Trace.json' % (dir), 'w') as f:
        json.dump(cfg_, f, indent=2)
    TraceGen('%s/Trace.json' % (dir))


def Gen(cfg):
    pdir = os.path.dirname(cfg)
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    if 'OIL' in cfg:
        cfg = fromOIL(pdir, cfg)
    extract_trace(cfg, dir)
    Gen_Os(cfg, dir)
