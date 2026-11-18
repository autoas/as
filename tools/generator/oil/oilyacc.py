# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>


from ply import *
from . import oillex
import sys

tokens = oillex.tokens

precedence = (("left", "PLUS", "MINUS"), ("left", "TIMES", "DIVIDE"), ("left", "POWER"))


def p_osek(p):
    """osek : ID ID LBRACE objectList RBRACE END"""
    p[0] = p[4]


def p_END(p):
    """END : SEMI
    | empty"""


def p_empty(p):
    """empty :"""


def p_objectList(p):
    """objectList : objectList object
    | object"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1] + [p[2]]


def p_object(p):
    """object : os
    | com
    | task
    | alarm
    | counter
    | appmode
    | event
    | resource
    | isr"""
    p[0] = p[1]


def p_os(p):
    """os : OS ID LBRACE attributes RBRACE END"""
    p[0] = {"Name": p[2], "Type": "OS"}
    p[0].update(p[4])


def p_com(p):
    """com : COM ID LBRACE attributes RBRACE END"""
    p[0] = {"Name": p[2], "Type": "COM"}
    p[0].update(p[4])


def p_task(p):
    """task : TASK ID LBRACE attributes RBRACE END"""
    p[0] = {"Name": p[2], "Type": "TASK"}
    p[0].update(p[4])


def p_alarm(p):
    """alarm : ALARM ID LBRACE attributes RBRACE END"""
    p[0] = {"Name": p[2], "Type": "ALARM"}
    p[0].update(p[4])


def p_counter(p):
    """counter : COUNTER ID LBRACE attributes RBRACE END"""
    p[0] = {"Name": p[2], "Type": "COUNTER"}
    p[0].update(p[4])


def p_appmode(p):
    """appmode : APPMODE ID SEMI"""
    p[0] = {"Name": p[2], "Type": "APPMODE"}


def p_event(p):
    """event : EVENT ID SEMI"""
    p[0] = {"Name": p[2], "Type": "EVENT"}


def p_resource(p):
    """resource : RESOURCE ID SEMI"""
    p[0] = {"Name": p[2], "Type": "RESOURCE"}


def p_isr(p):
    """isr : ISR ID LBRACE attributes RBRACE END"""
    p[0] = {"Name": p[2], "Type": "ISR"}
    p[0].update(p[4])


def p_attributes(p):
    """attributes : attributes attribute
    | attribute"""
    if len(p) == 2:
        p[0] = {}
        extra = p[1]
    else:
        p[0] = p[1]
        extra = p[2]
    for k, v in extra.items():
        if k == "EVENT":
            evt = {"Name": v, "Mask": "AUTO"}
            k = "EventList"
            if k not in p[0]:
                p[0][k] = []
            p[0][k].append(evt)
        elif k == "RESOURCE":
            rst = {"Name": v}
            k = "ResourceList"
            if k not in p[0]:
                p[0][k] = []
            p[0][k].append(rst)
        elif k not in p[0].keys():
            p[0][k] = v
        else:
            raise Exception("%s already has %s" % (p[1], p[2]))


def p_attribute(p):
    """attribute : ID EQUALS value SEMI
    | TASK EQUALS value SEMI
    | COUNTER EQUALS value SEMI
    | APPMODE EQUALS value SEMI
    | EVENT EQUALS value SEMI
    | RESOURCE EQUALS value SEMI
    | ID EQUALS value LBRACE attributes RBRACE END"""
    if p[3] == "TRUE":
        p[3] = True
    if p[3] == "FALSE":
        p[3] = False
    if len(p) == 5:
        p[0] = {p[1]: p[3]}
    else:
        p[0] = {p[1]: (p[3], p[5])}


def p_value(p):
    """value : ID
    | STR
    | int
    | float"""
    p[0] = p[1]


def p_int(p):
    """int : INTEGER"""
    p[0] = eval(p[1])


def p_float(p):
    """float : DIGIT"""
    p[0] = eval(p[1])


# Catastrophic error handler


def p_error(p):
    if not p:
        print("SYNTAX ERROR AT EOF")
        exit()
    else:
        try:
            print("SYNTAX ERROR AT LINE(%s) %s" % (p.lineno, p))
        except:
            print("SYNTAX ERROR AT LINE(%s) %s" % (int(p), p))
    sys.exit()
    while 1:
        tok = bparser.token()  # Get the next token
        if not tok or tok.type == "END":
            break
    bparser.errok()

    return tok


bparser = yacc.yacc()


def parse(data, debug=0):
    bparser.error = 0
    p = bparser.parse(data, debug=debug)
    if bparser.error:
        return None
    return p
