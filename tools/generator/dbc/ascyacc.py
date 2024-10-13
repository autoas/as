# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>


from ply import *
from .asclex import *

precedence = (("left", "PLUS", "MINUS"), ("left", "TIMES", "DIVIDE"), ("left", "POWER"))


def str2int(cstr):
    cstr = str(cstr).replace(" ", "").replace("\n", "")
    if cstr[0:2] == "0x" or cstr[0:2] == "0X":
        return int(cstr, 16)
    elif cstr == "":
        return None
    elif cstr.find(".") != -1:
        return eval(cstr)
    else:
        return int(cstr, 10)


def p_objectList(p):
    """objectList : objectList object
    | object"""
    if len(p) == 2:
        if not p[0]:
            p[0] = {}
        p[0].update(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = {}
        p[0].update(p[2])


def p_object(p):
    """object :  version
    | ns
    | bs
    | bu
    | boList
    | evList
    | cmList
    | baList
    | badefList
    | badefDefList"""
    if not p[0]:
        p[0] = {}
    p[0].update(p[1])


def p_version(p):
    """version : VERSION STR END"""
    p[0] = {"version": p[2]}


def p_ns(p):
    """ns : NS_ COLON EOL nsList END"""
    p[0] = {"ns": p[4]}


def p_bs(p):
    """bs : BS_ COLON idList END"""
    p[0] = {"bs": p[3]}


def p_bu(p):
    """bu : BU_ COLON idList END"""
    p[0] = {"bu": p[3]}


def p_boList(p):
    """boList : boList bo
    | bo"""
    if len(p) == 2:
        if not p[0]:
            p[0] = {"messages": []}
        p[0]["messages"].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = {"messages": []}
        p[0]["messages"].append(p[2])


def p_bo(p):
    """bo : BO_ digit ID COLON digit ID EOL sgList END
    |  BO_ digit ID COLON digit ID END"""
    if len(p) == 8:
        p[0] = {"id": p[2], "name": p[3], "dlc": p[5], "node": p[6], "signals": []}
    else:
        p[0] = {"id": p[2], "name": p[3], "dlc": p[5], "node": p[6], "signals": p[8]}


def p_evList(p):
    """evList : evList ev
    | ev"""
    if len(p) == 2:
        if not p[0]:
            p[0] = {"evList": []}
        p[0]["evList"].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = {"evList": []}
        p[0]["evList"].append(p[2])


def p_ev(p):
    """ev : EV_ ID COLON digit LBK digit  OR digit RBK STR digit digit idList SEMI END"""
    p[0] = p[1]


def p_baList(p):
    """baList : baList ba
    | ba"""
    if len(p) == 2:
        if not p[0]:
            p[0] = {"baList": []}
        p[0]["baList"].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = {"baList": []}
        p[0]["baList"].append(p[2])


def p_ba(p):
    """ba : BA_ STR  digit SEMI EOL
    | BA_ STR  BU_ ID digit SEMI EOL
    | BA_ STR  BU_ ID STR SEMI EOL
    | BA_ STR  BO_ digit digit SEMI EOL
    | BA_ STR  BO_ digit STR SEMI EOL
    | BA_ STR  SG_ digit ID digit SEMI EOL
    | BA_ STR  SG_ digit ID STR SEMI EOL
    | BA_ STR STR SEMI EOL"""
    p[0] = []
    for i in range(1, len(p)):
        p[0].append(p[i])


def p_cmList(p):
    """cmList : cmList cm
    | cm"""
    if len(p) == 2:
        if not p[0]:
            p[0] = {"cmList": []}
        p[0]["cmList"].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = {"cmList": []}
        p[0]["cmList"].append(p[2])


def p_cm(p):
    """cm : CM_ SG_ digit ID STR SEMI EOL
    | CM_ BO_ INTEGER STR SEMI EOL
    | CM_ STR SEMI EOL"""
    p[0] = []
    for i in range(1, len(p)):
        p[0].append(p[i])


def p_badefList(p):
    """badefList : badefList badef
    | badef"""
    if len(p) == 2:
        if not p[0]:
            p[0] = {"badefList": []}
        p[0]["badefList"].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = {"badefList": []}
        p[0]["badefList"].append(p[2])


def p_badef(p):
    """badef : BA_DEF_ BU_ STR INT digit digit SEMI EOL
    | BA_DEF_ BO_ STR INT digit digit SEMI EOL
    | BA_DEF_ EV_ STR INT digit digit SEMI EOL
    | BA_DEF_ SG_ STR INT digit digit SEMI EOL
    | BA_DEF_ BU_ STR FLOAT digit digit SEMI EOL
    | BA_DEF_ BO_ STR FLOAT digit digit SEMI EOL
    | BA_DEF_ EV_ STR FLOAT digit digit SEMI EOL
    | BA_DEF_ SG_ STR HEX digit digit SEMI EOL
    | BA_DEF_ BU_ STR HEX digit digit SEMI EOL
    | BA_DEF_ BO_ STR HEX digit digit SEMI EOL
    | BA_DEF_ EV_ STR HEX digit digit SEMI EOL
    | BA_DEF_ SG_ STR FLOAT digit digit SEMI EOL
    | BA_DEF_ STR FLOAT digit digit SEMI EOL
    | BA_DEF_ BU_ STR ENUM strList SEMI EOL
    | BA_DEF_ BO_ STR ENUM strList SEMI EOL
    | BA_DEF_ EV_ STR ENUM strList SEMI EOL
    | BA_DEF_ SG_ STR ENUM strList SEMI EOL
    | BA_DEF_ BU_ STR STRING SEMI EOL
    | BA_DEF_ BO_ STR STRING SEMI EOL
    | BA_DEF_ EV_ STR STRING SEMI EOL
    | BA_DEF_ SG_ STR STRING SEMI EOL
    | BA_DEF_ STR HEX digit digit SEMI EOL
    | BA_DEF_ STR STRING SEMI EOL
    | BA_DEF_ STR INT digit digit SEMI EOL
    """
    p[0] = []
    for i in range(1, len(p)):
        p[0].append(p[i])


def p_badefDefList(p):
    """badefDefList : badefDefList badefDef
    | badefDef"""
    if len(p) == 2:
        if not p[0]:
            p[0] = {"badefDefList": []}
        p[0]["badefDefList"].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = {"badefDefList": []}
        p[0]["badefDefList"].append(p[2])


def p_badefDef(p):
    """badefDef : BA_DEF_DEF_ STR STR   SEMI EOL
    | BA_DEF_DEF_ STR digit SEMI EOL"""
    p[0] = (p[2], p[3])


def p_strList(p):
    """strList : strList COMMA STR
    | STR"""
    if len(p) == 2:
        if not p[0]:
            p[0] = []
        p[0].append(p[1])
    else:
        p[0] = p[1]
        if not p[0]:
            p[0] = []
        p[0].append(p[3])


def p_sgList(p):
    """sgList : sgList sg
    | sg"""
    if len(p) == 2:
        if not p[0]:
            p[0] = []
        p[0].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = []
        p[0].append(p[2])


def p_sg(p):
    """sg : SG_ ID COLON digit OR digit AT digit PLUS  LPAREN digit COMMA digit RPAREN LBK digit OR digit RBK STR rxNodeList EOL
    |   SG_ ID COLON digit OR digit AT digit MINUS LPAREN digit COMMA digit RPAREN LBK digit OR digit RBK STR idList EOL
    """
    p[0] = {
        "name": p[2],
        "start": p[4],
        "size": p[6],
        "endian": "big" if p[8] == 0 else "little",
        "sign": p[9],
        "factor": p[11],
        "offset": p[13],
        "min": p[16],
        "max": p[18],
        "unit": p[20],
        "node": p[21],
    }


def p_idList(p):
    """idList : idList ID
    | ID
    | empty"""
    if len(p) == 2:
        if not p[0]:
            p[0] = []
        p[0].append(p[1])
    elif len(p) == 3:
        p[0] = p[1]
        if not p[0]:
            p[0] = []
        p[0].append(p[2])


def p_rxNodeList(p):
    """rxNodeList : rxNodeList COMMA ID
    | ID
    | empty"""
    if len(p) == 2:
        if not p[0]:
            p[0] = []
        p[0].append(p[1])
    elif len(p) == 4:
        p[0] = p[1]
        if not p[0]:
            p[0] = []
        p[0].append(p[3])


def p_nsList(p):
    """nsList : nsList ID EOL
    | ID EOL
    | nsList BA_ EOL
    | BA_ EOL
    | nsList CM_ EOL
    | CM_ EOL
    | nsList BA_DEF_ EOL
    | BA_DEF_ EOL
    | nsList BA_DEF_DEF_ EOL
    | BA_DEF_DEF_ EOL"""
    if len(p) == 3:
        if not p[0]:
            p[0] = []
        p[0].append(p[1])
    elif len(p) == 4:
        p[0] = p[1]
        if not p[0]:
            p[0] = []
        p[0].append(p[2])


def p_digit(p):
    """digit : INTEGER
    | DIGIT
    | MINUS INTEGER
    | MINUS DIGIT"""
    if len(p) == 2:
        p[0] = str2int(p[1])
    else:
        p[0] = -str2int(p[2])


def p_END(p):
    """END : END EOL
    | EOL"""
    if len(p) == 3:
        p[0] = p[2]
    else:
        p[0] = p[1]


def p_empty(p):
    """empty :"""


bparser = None


#### Catastrophic error handler
def p_error(p):
    if not p:
        raise Exception("SYNTAX ERROR AT EOF")
    else:
        try:
            print("SYNTAX ERROR AT LINE(%s) %s" % (p.lineno, p))
        except:
            print("SYNTAX ERROR AT LINE(%s) %s" % (int(p), p))
    # sys.exit()
    while 1:
        tok = bparser.token()  # Get the next token
        if not tok or tok.type == "EOL":
            break
    bparser.errok()

    return tok


def parse(data, debug=0):
    global bparser
    if debug == 1:
        print(data)
    lex.lex(debug=debug)
    bparser = yacc.yacc()
    bparser.error = 0
    p = bparser.parse(data, debug=debug)
    if bparser.error:
        return None
    return p
