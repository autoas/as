# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>

from . import oilyacc
import re


def extract(p):
    km = {'STARTUPHOOK': 'StartupHook', 'ERRORHOOK': 'ErrorHook',
          'PRETASKHOOK': 'PreTaskHook', 'POSTTASKHOOK': 'PostTaskHook',
          'SHUTDOWNHOOK': 'ShutdownHook', 'STATUS': 'Status', 'TYPE': 'Type',
          'USERESSCHEDULER': 'UseResScheduler', 'USEGETSERVICEID': 'UseGetServiceID',
          'USEPARAMETERACCESS': 'UseParameterAccess', 'STACK': 'StackSize',
          'PRIORITY': 'Priority', 'ACTIVATION': 'Activation', 'AUTOSTART': 'AutoStart',
          'ACTION': 'Action', 'COUNTER': 'Counter', 'TASK': 'Task', 'SCHEDULE': 'Schedule',
          'ALARMTIME': 'StartTime', 'CYCLETIME': 'Period', 'EVENT': 'Event',
          'MAXALLOWEDVALUE': 'MaxAllowed', 'TICKSPERBASE': 'TicksPerBase', 'MINCYCLE': 'MinCycle'}
    cfg = {}
    def add(obj, tag):
        if tag not in cfg:
            cfg[tag] = []
        bFound = False
        for x in cfg[tag]:
            if x['Name'] == obj['Name']:
                x.update(obj)
                bFound = True
        if bFound == False:
            cfg[tag].append(obj)
    for obj in p:
        if obj['type'] == 'OS':
            for k, v in obj.items():
                if k in km:
                    k = km[k]
                cfg[k] = v
            if 'SystemTimer' in cfg:
                counter = {'Name': 'SystemTimer', 'Hardware':cfg['SystemTimer']}
                add(counter, 'CounterList')
        elif obj['type'] == 'COM':
            pass
        elif obj['type'] == 'APPMODE':
            pass
        elif obj['type'] == 'EVENT':
            pass
        elif obj['type'] == 'RESOURCE':
            resource = {'Name': obj['name']}
            add(resource, 'ResourceList')
        elif obj['type'] == 'ISR':
            pass
        elif obj['type'] == 'TASK':
            task = {'Name': obj['name']}
            for k, v in obj.items():
                if k in km:
                    k = km[k]
                if k == 'AutoStart':
                    if type(v) is tuple:
                        task[k] = v[0]
                        ApplicationModeList = []
                        for k1, v1 in v[1].items():
                            if k1 in km:
                                k1 = km[k1]
                            if k1 == 'APPMODE':
                                ApplicationModeList.append(v1)
                            else:
                                task[k1] = v1
                        task['ApplicationModeList'] = ApplicationModeList
                    else:
                        task[k] = v
                elif k not in ['name', 'type']:
                    task[k] = v
            add(task, 'TaskList')
        elif obj['type'] == 'ALARM':
            alarm = {'Name': obj['name']}
            for k, v in obj.items():
                if k in km:
                    k = km[k]
                if k == 'Action':
                    alarm[k] = v[0]
                    for k1, v1 in v[1].items():
                        if k1 in km:
                            k1 = km[k1]
                        if k1 == 'EventList':
                            alarm['Event'] = v1[0]['Name']
                        else:
                            alarm[k1] = v1
                elif k == 'AutoStart':
                    if type(v) is tuple:
                        alarm[k] = v[0]
                        ApplicationModeList = []
                        for k1, v1 in v[1].items():
                            if k1 in km:
                                k1 = km[k1]
                            if k1 == 'APPMODE':
                                ApplicationModeList.append(v1)
                            else:
                                alarm[k1] = v1
                        alarm['ApplicationModeList'] = ApplicationModeList
                    else:
                        alarm[k] = v
                elif k not in ['name', 'type']:
                    alarm[k] = v
            add(alarm, 'AlarmList')
        elif obj['type'] == 'COUNTER':
            counter = {'Name': obj['name']}
            for k, v in obj.items():
                if k in km:
                    k = km[k]
                if k not in ['name', 'type']:
                    counter[k] = v
            add(counter, 'CounterList')
        else:
            raise Exception('unknown type for: %s' % (obj))
    return cfg


def parse(file):
    print('OIL parse', file)
    fp = open(file, 'r')
    data = ''
    comment = False
    reBegin = re.compile(r'/\*(.*)')
    reEnd = re.compile(r'(.*)\*/')
    reOneLine = re.compile(r'/\*(.*)\*/')
    for l in fp.readlines():
        if False == comment and reOneLine.search(l):
            cm = reOneLine.search(l).groups()[0]
            data += l.replace('/*%s*/' % (cm), '')
            continue
        if reBegin.search(l):
            cm = reBegin.search(l).groups()[0]
            data += l.replace('/*%s' % (cm), '')
            comment = True
        if False == comment:
            data += l
        if reEnd.search(l):
            cm = reEnd.search(l).groups()[0]
            data += l.replace('%s*/' % (cm), '')
            comment = False
    fp.close()
    p = oilyacc.parse(data)
    return extract(p)
