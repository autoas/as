# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

from . import ascyacc
import sys,os

__all__ = ['ASCP']

__self_node_name__ = 'AS'

def get_period(p,msg):
    id = msg['id']
    if('baList' not in p):
        return None
    for ba in p['baList']:
        if((ba[1]=='"GenMsgCycleTime"') and (ba[2]=='BO_')):
            if(ba[3]==id):
                return ba[4]
    return None

def get_init(p,sig):
    name = sig['name']
    if('baList' not in p):
        return None
    for ba in p['baList']:
        if((ba[1]=='"GenSigStartValue"') and (ba[2]=='SG_')):
            if(ba[4]==name):
                return ba[5]
    return None

def get_comment(p,sig):
    name = sig['name']
    if('cmList' not in p):
        return None
    for cm in p['cmList']:
        if((cm[1]=='SG_') and (cm[3]==name)):
            return cm[4]
    return None

def post_process_period(p):
    for msg in p['messages']:
        period = get_period(p,msg)
        if(period!=None):
            msg.update({'CycleTime':period})

def post_process_init(p):
    for msg in p['messages']:
        for sig in msg['signals']:
            init = get_init(p,sig)
            if(init!=None):
                sig.update({'InitValue':init})
            comment = get_comment(p,sig)
            if(comment!=None):
                sig.update({'comment':comment})

def parse(file):
    fp = open(file,'r')
    data = fp.read()
    fp.close()
    p = ascyacc.parse(data)
    post_process_period(p)
    post_process_init(p)
    return p

def dbc(path):
    p = parse(path)
    return p['messages']