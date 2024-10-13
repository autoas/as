# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>


from ply import *

keywords = (
    "VERSION",
    "NS_",
    "BS_",
    "BU_",
    "BO_",
    "SG_",
    "EV_",
    "BA_",
    "CM_",
    "BA_DEF_",
    "BA_DEF_DEF_",
    "INT",
    "ENUM",
    "STRING",
    "HEX",
    "FLOAT",
)

tokens = keywords + (
    "EQUALS",
    "PLUS",
    "MINUS",
    "TIMES",
    "DIVIDE",
    "POWER",  # =, +, -, *, /, ^
    "LPAREN",
    "RPAREN",
    "LBRACE",
    "RBRACE",
    "LBK",
    "RBK",  # (, ), {, }, [, ],
    "LT",
    "LE",
    "GT",
    "GE",
    "NE",  # <, <=, >, >=, !=
    "COLON",
    "COMMA",
    "SEMI",
    "OR",
    "AND",
    "AT",  # :, ,, ;, |, &, @
    "INTEGER",
    "DIGIT",
    "STR",
    "ID",
    "EOL",
)

t_EQUALS = r"="
t_PLUS = r"\+"
t_MINUS = r"-"
t_TIMES = r"\*"
t_POWER = r"\^"
t_DIVIDE = r"/"
t_LPAREN = r"\("
t_RPAREN = r"\)"
t_LBRACE = r"\{"
t_RBRACE = r"\}"
t_LBK = r"\["  # bracket
t_RBK = r"\]"
t_LT = r"<"
t_LE = r"<="
t_GT = r">"
t_GE = r">="
t_NE = r"<>"
t_COLON = r":"
t_COMMA = r"\,"
t_SEMI = r";"
t_OR = r"\|"
t_AND = r"\&"
t_AT = r"\@"
t_INTEGER = r"(0(x|X)[0-9a-fA-F]+)|(\d+)"
t_DIGIT = r"((\d*\.\d+)(E[\+-]?\d+)?|([1-9]\d*E[\+-]?\d+))"
t_STR = r'"(.|\n)*?"'


def t_ID(t):
    r"[_A-Za-z][_A-Za-z0-9]*"
    if t.value in keywords:
        t.type = t.value
    return t


def t_EOL(t):
    r"\n"
    t.lexer.lineno += 1
    return t


def t_SPACE(t):
    r"\s"


def t_error(t):
    print("Illegal character '%s'" % t.value[0])
    t.lexer.skip(1)
