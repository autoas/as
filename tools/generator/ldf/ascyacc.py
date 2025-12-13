# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021-2024 Parai Wang <parai@foxmail.com>


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
        p[0] = p[1]
    elif len(p) == 3:
        p[0] = p[1]
        p[0].update(p[2])


def p_object(p):
    """object : attributes
    | nodes
    | signals
    | diagnostic_signals
    | frames
    | diagnostic_frames
    | node_attributes
    | schedule_tables
    | signal_encodings
    | signal_representations"""
    if not p[0]:
        p[0] = {}
    p[0].update(p[1])


def p_signal_representations(p):
    """signal_representations : Signal_representation LBRACE signal_representation_list RBRACE"""
    p[0] = p[3]


def p_signal_representation_list(p):
    """signal_representation_list : signal_representation_list signal_representation
    | signal_representation"""
    if len(p) == 2:
        p[0] = {"Signal_representation": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["Signal_representation"].update(p[2])


def p_signal_representation(p):
    """signal_representation : ID COLON ids SEMI"""
    p[0] = {p[1]: p[3]}


def p_signal_encodings(p):
    """signal_encodings : Signal_encoding_types LBRACE signal_encodings_list RBRACE"""
    p[0] = p[3]


def p_signal_encodings_list(p):
    """signal_encodings_list : signal_encodings_list signal_encoding
    | signal_encoding"""
    if len(p) == 2:
        p[0] = {"Signal_encoding_types": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["Signal_encoding_types"].update(p[2])


def p_signal_encoding(p):
    """signal_encoding : ID LBRACE signal_encoding_attr_list RBRACE"""
    p[0] = {p[1]: p[3]}


def p_signal_encoding_attr_list(p):
    """signal_encoding_attr_list : signal_encoding_attr_list signal_encoding_attr
    | signal_encoding_attr"""
    if len(p) == 2:
        p[0] = [p[1]]
    elif len(p) == 3:
        p[0] = p[1] + [p[2]]


def p_signal_encoding_attr(p):
    """signal_encoding_attr : ID COMMA values SEMI"""
    if len(p) == 5:
        p[0] = {p[1]: p[3]}
    elif len(p) == 7:
        p[0] = {p[1]: (p[3], p[5])}


def p_schedule_tables(p):
    """schedule_tables : Schedule_tables LBRACE schtbl_list RBRACE"""
    p[0] = p[3]


def p_schtbl_list(p):
    """schtbl_list : schtbl_list schtbl
    | schtbl"""
    if len(p) == 2:
        p[0] = {"schedule_tables": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["schedule_tables"].update(p[2])


def p_schtbl(p):
    """schtbl : ID LBRACE schentrys RBRACE"""
    p[0] = {p[1]: p[3]}


def p_schentrys(p):
    """schentrys : schentrys schentry
    | schentry"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1] + [p[2]]


def p_schentry(p):
    """schentry : ID delay INTEGER ms SEMI"""
    p[0] = {"name": p[1], "delay": p[3]}


def p_node_attributes(p):
    """node_attributes : Node_attributes LBRACE node_attribute_list RBRACE"""
    p[0] = p[3]


def p_node_attribute_list(p):
    """node_attribute_list : node_attribute_list node_attribute
    | node_attribute"""
    if len(p) == 2:
        p[0] = {"node_attributes": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["node_attributes"].update(p[2])


def p_node_attribute(p):
    """node_attribute : ID LBRACE node_attr_list RBRACE"""
    p[0] = {p[1]: p[3]}


def p_node_attr_list(p):
    """node_attr_list : node_attr_list node_attr
    | node_attr"""
    if len(p) == 2:
        p[0] = p[1]
    elif len(p) == 3:
        p[0] = p[1]
        p[0].update(p[2])


def p_node_attr(p):
    """node_attr : attribute
    | node_frames"""
    p[0] = p[1]


def p_node_frames(p):
    """node_frames : configurable_frames LBRACE ids2 RBRACE"""
    p[0] = {"configurable_frames": p[3]}


def p_diagnostic_frames(p):
    """diagnostic_frames : Diagnostic_frames LBRACE diagnostic_frame_list RBRACE"""
    p[0] = p[3]


def p_diagnostic_frame_list(p):
    """diagnostic_frame_list : diagnostic_frame_list diagnostic_frame
    | diagnostic_frame"""
    if len(p) == 2:
        p[0] = {"diagnostic_frames": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["diagnostic_frames"].update(p[2])


def p_diagnostic_frame(p):
    """diagnostic_frame : ID COLON INTEGER LBRACE frame_signals RBRACE"""
    p[0] = {p[1]: {"id": p[3], "signals": p[5]}}


def p_frames(p):
    """frames : Frames LBRACE frame_list RBRACE"""
    p[0] = p[3]


def p_frame_list(p):
    """frame_list : frame_list frame
    | frame"""
    if len(p) == 2:
        p[0] = {"frames": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["frames"].update(p[2])


def p_frame(p):
    """frame : ID COLON INTEGER COMMA ID COMMA INTEGER LBRACE frame_signals RBRACE"""
    p[0] = {p[1]: {"id": p[3], "publisher": p[5], "dlc": p[7], "signals": p[9]}}


def p_frame_signals(p):
    """frame_signals : frame_signals frame_signal
    | frame_signal"""
    if len(p) == 2:
        p[0] = p[1]
    elif len(p) == 3:
        p[0] = p[1]
        p[0].update(p[2])


def p_frame_signal(p):
    """frame_signal : ID COMMA INTEGER SEMI"""
    p[0] = {p[1]: {"start": p[3]}}


def p_diagnostic_signals(p):
    """diagnostic_signals : Diagnostic_signals LBRACE diagnostic_signal_list RBRACE"""
    p[0] = p[3]


def p_diagnostic_signal_list(p):
    """diagnostic_signal_list : diagnostic_signal_list diagnostic_signal
    | diagnostic_signal"""
    if len(p) == 2:
        p[0] = {"diag_signals": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["diag_signals"].update(p[2])


def p_diagnostic_signal(p):
    """diagnostic_signal : ID COLON INTEGER COMMA INTEGER SEMI"""
    p[0] = {p[1]: {"size": p[3], "initial": p[5]}}


def p_signals(p):
    """signals : Signals LBRACE signals_list RBRACE"""
    p[0] = p[3]


def p_signals_list(p):
    """signals_list : signals_list signal
    | signal"""
    if len(p) == 2:
        p[0] = {"signals": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["signals"].update(p[2])


def p_signal(p):
    """signal : ID COLON INTEGER COMMA INTEGER COMMA ID COMMA ids SEMI
    | ID COLON INTEGER COMMA INTEGER COMMA ID SEMI"""
    if len(p) == 11:
        p[0] = {p[1]: {"size": p[3], "initial": p[5], "publisher": p[7], "subscriber": p[9]}}
    else:
        p[0] = {p[1]: {"size": p[3], "initial": p[5], "publisher": p[7], "subscriber": []}}


def p_nodes(p):
    """nodes : Nodes LBRACE nodes_list RBRACE"""
    p[0] = p[3]


def p_nodes_list(p):
    """nodes_list : nodes_list node
    | node"""
    if len(p) == 2:
        p[0] = {"Nodes": p[1]}
    elif len(p) == 3:
        p[0] = p[1]
        p[0]["Nodes"].update(p[2])


def p_node(p):
    """node : Master COLON ID COMMA INTEGER ms COMMA DIGIT ms SEMI
    | Master COLON ID COMMA INTEGER ms COMMA INTEGER ms SEMI
    | Slaves COLON ids SEMI"""
    if len(p) == 11:
        p[0] = {"master": {"name": p[3], "time_base": p[5], "jitter": p[8]}}
    else:
        p[0] = {"slaves": p[3]}


def p_ids2(p):
    """ids2 : ids2 id2
    | id2"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1] + [p[2]]


def p_id2(p):
    """id2 : ID SEMI"""
    p[0] = p[1]


def p_ids(p):
    """ids : ids COMMA ID
    | ID"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1] + [p[3]]


def p_ints(p):
    """ints : ints COMMA INTEGER
    | INTEGER"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1] + [p[3]]


def p_values(p):
    """values : values COMMA value
    | value"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1] + [p[3]]


def str2value(cstr):
    cstr = str(cstr).replace(" ", "").replace("\n", "")
    if cstr[0:2] == "0x" or cstr[0:2] == "0X":
        return int(cstr, 16)
    elif cstr == "":
        return None
    elif cstr.find(".") != -1:
        return eval(cstr)
    else:
        return int(cstr, 10)


def p_value(p):
    """value : INTEGER
    | DIGIT
    | STR
    | MINUS INTEGER
    | MINUS DIGIT"""
    if len(p) == 2:
        try:
            p[0] = str2value(p[1])
        except:
            p[0] = p[1]
    else:
        p[0] = -str2value(p[2])


def p_attributes(p):
    """attributes : attributes attribute
    | attribute"""
    if len(p) == 2:
        p[0] = p[1]
    elif len(p) == 3:
        p[0] = p[1]
        p[0].update(p[2])


def p_attribute(p):
    """attribute : ID SEMI
    | ID EQUALS STR SEMI
    | ID EQUALS ID SEMI
    | ID EQUALS INTEGER SEMI
    | ID EQUALS ints SEMI
    | ID EQUALS DIGIT kbps SEMI
    | ID EQUALS INTEGER ms SEMI"""
    if len(p) == 3:
        p[0] = {p[1]: ""}
    elif len(p) == 5:
        p[0] = {p[1]: p[3]}
    elif len(p) == 6:
        if p[4] == "kbps":
            p[0] = {p[1]: eval(p[3]) * 1000}
        else:
            p[0] = {p[1]: p[3]}


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
    # exit()
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
