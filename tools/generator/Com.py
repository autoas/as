# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen", "get_messages"]


def gen_rx_sig_cfg(sig, C):
    if "Group" in sig:
        return
    C.write("static Com_SignalRxContextType Com_SignalRxContext_%s;\n" % (sig["Name"]))
    C.write("static const Com_SignalRxConfigType Com_SignalRxConfig_%s = {\n" % (sig["Name"]))
    InvalidNotification = sig.get("InvalidNotification", "NULL")
    RxNotification = sig.get("RxNotification", "NULL")
    RxTOut = sig.get("RxTOut", "NULL")
    FirstTimeout = sig.get("FirstTimeout", 0)
    Timeout = sig.get("Timeout", 0)
    DataInvalidAction = sig.get("DataInvalidAction", "NOTIFY")
    RxDataTimeoutAction = sig.get("RxDataTimeoutAction", "NONE")
    C.write("  &Com_SignalRxContext_%s,\n" % (sig["Name"]))
    C.write("  #ifdef COM_USE_SIGNAL_RX_INVALID_NOTIFICATION\n")
    C.write("  %s, /* InvalidNotification */\n" % (InvalidNotification))
    C.write("  #endif\n")
    C.write("  #ifdef COM_USE_SIGNAL_RX_NOTIFICATION\n")
    C.write("  %s, /* RxNotification */\n" % (RxNotification))
    C.write("  #endif\n")
    C.write("  #ifdef COM_USE_SIGNAL_RX_TIMEOUT\n")
    C.write("  %s, /* RxTOut */\n" % (RxTOut))
    C.write("  #endif\n")
    if "SUBSTITUTE" == RxDataTimeoutAction:
        t0, t1, nBytes = get_signal_info(sig)
        C.write("  %s%s_TimeoutSubstitutionValue,\n" % ("" if t0 in ["UINT8N", "SINT8N"] else "&", sig["Name"]))
    else:
        C.write("  NULL, /* TimeoutSubstitutionValue */\n")
    C.write("  %s, /* FirstTimeout */\n" % (FirstTimeout))
    C.write("  %s, /* Timeout */\n" % (Timeout))
    C.write("  COM_ACTION_%s, /* DataInvalidAction */\n" % (DataInvalidAction))
    C.write("  COM_ACTION_%s, /* RxDataTimeoutAction */\n" % (RxDataTimeoutAction))
    C.write("};\n\n")


def gen_tx_sig_cfg(sig, C):
    if "Group" in sig:
        return
    C.write("static const Com_SignalTxConfigType Com_SignalTxConfig_%s = {\n" % (sig["Name"]))
    ErrorNotification = sig.get("ErrorNotification", "NULL")
    TxNotification = sig.get("TxNotification", "NULL")
    C.write("  #ifdef COM_USE_SIGNAL_TX_ERROR_NOTIFICATION\n")
    C.write("  %s, /* ErrorNotification */\n" % (ErrorNotification))
    C.write("  #endif\n")
    C.write("  #ifdef COM_USE_SIGNAL_TX_NOTIFICATION\n")
    C.write("  %s, /* TxNotification */\n" % (TxNotification))
    C.write("  #endif\n")
    C.write("  #if !defined(COM_USE_SIGNAL_TX_ERROR_NOTIFICATION) && !defined(COM_USE_SIGNAL_TX_NOTIFICATION)\n")
    C.write("  0,\n")
    C.write("  #endif\n")
    C.write("};\n\n")


def get_signal_info(sig):
    if sig.get("IsGroup", False):
        return "UINT8N", "uint8_t", int(sig["Size"] / 8)
    if sig["Size"] <= 8:
        t = "INT8"
        t1 = "int8_t"
    elif sig["Size"] <= 16:
        t = "INT16"
        t1 = "int16_t"
    elif sig["Size"] <= 32:
        t = "INT32"
        t1 = "int32_t"
    else:
        t = "INT8N"
        t1 = "int8_t"
    if sig.get("Sign", "+") == "+":
        t = "U%s" % (t)
        t1 = "u%s" % (t1)
    else:
        t = "S%s" % (t)
    return t, t1, int((sig["Size"] + 7) / 8)


def get_signal(msg, name):
    for sig in msg["Signals"]:
        if sig["Name"] == name:
            return sig
    raise Exception("sinal %s not found in msg %s" % (name, msg["Name"]))


def gen_signal_init_value(sig, C):
    if sig.get("IsGroup", False):
        return
    t0, t1, nBytes = get_signal_info(sig)
    if t0 in ["UINT8N", "SINT8N"]:
        InitialValue = sig.get("InitialValue", "[0]")
        cstr = ""
        values = eval(InitialValue)
        if type(values) not in [list, tuple]:
            values = [values]
        for x in values:
            cstr += "0x%x, " % (x)
        C.write("static const %s %s_InitialValue[%s] = { %s };\n" % (t1, sig["Name"], nBytes, cstr))
    else:
        InitialValue = sig.get("InitialValue", 0)
        C.write("static const %s %s_InitialValue = %s;\n" % (t1, sig["Name"], InitialValue))


def gen_signal_timeout_value(sig, C):
    if "SUBSTITUTE" != sig.get("RxDataTimeoutAction", "NONE"):
        return
    if "Group" not in sig:
        t0, t1, nBytes = get_signal_info(sig)
        if t0 in ["UINT8N", "SINT8N"]:
            TimeoutSubstitutionValue = sig.get("TimeoutSubstitutionValue", "[0]")
            cstr = ""
            for x in eval(TimeoutSubstitutionValue):
                cstr += "0x%x, " % (x)
            C.write("static const %s %s_TimeoutSubstitutionValue[%s] = { %s };\n" % (t1, sig["Name"], nBytes, cstr))
        else:
            TimeoutSubstitutionValue = sig.get("TimeoutSubstitutionValue", 0)
            C.write("static const %s %s_TimeoutSubstitutionValue = %s;\n" % (t1, sig["Name"], TimeoutSubstitutionValue))


def gen_sig(network, sig, msg, C, isTx):
    if msg["Name"].startswith(network["Name"]):
        name = toMacro(msg["Name"])
    else:
        name = toPduSymbol((network["Name"], msg["Name"]))
    C.write("  {\n")
    C.write("#ifdef USE_SHELL\n")
    C.write('    "%s",\n' % (sig["Name"]))
    C.write("#endif\n")
    if "Group" in sig:
        gsig = get_signal(msg, sig["Group"])
        offset = int(gsig["Start"] / 8)
        C.write("    &Com_GrpsData_%s[%s], /* Ptr */\n" % (sig["Group"], int(sig["Start"] / 8) - offset))
    else:
        C.write("    &Com_PduData_%s[%s], /* Ptr */\n" % (msg["Name"], int(sig["Start"] / 8)))
    t0, t1, nBytes = get_signal_info(sig)
    if sig.get("IsGroup", False):
        C.write("    Com_GrpsData_%s, /* Ptr */\n" % (sig["Name"]))
    else:
        C.write("    %s%s_InitialValue, /* InitPtr */\n" % ("" if t0 in ["UINT8N", "SINT8N"] else "&", sig["Name"]))
    C.write("#ifdef COM_USE_SIGNAL_CONFIG\n")
    if "Group" in sig:
        C.write("    NULL, /* RxConfig */\n")
        C.write("    NULL, /* TxConfig */\n")
    elif isTx:
        C.write("    NULL, /* RxConfig */\n")
        C.write("    &Com_SignalTxConfig_%s, /* TxConfig */\n" % (sig["Name"]))
    else:
        C.write("    &Com_SignalRxConfig_%s, /* RxConfig */\n" % (sig["Name"]))
        C.write("    NULL, /* TxConfig */\n")
    C.write("#endif\n")
    C.write("    COM_%sID_%s, /* HandleId */\n" % ("G" if sig.get("IsGroup", False) else "S", sig["Name"]))
    C.write("    COM_%s, /* PduId */\n" % (name))
    C.write("    %s, /* BitPosition */\n" % (sig["Start"] & 7))
    C.write("    %s, /* BitSize */\n" % (sig["Size"]))
    C.write("#ifdef COM_USE_SIGNAL_UPDATE_BIT\n")
    UpdateBit = sig.get("UpdateBit", "COM_UPDATE_BIT_NOT_USED")
    if type(UpdateBit) is int:
        assert UpdateBit > sig["Start"]
        UpdateBit = UpdateBit - int(sig["Start"] / 8) * 8
    C.write("    %s, /* UpdateBit */\n" % (UpdateBit))
    C.write("#endif\n")
    if sig.get("Dyn", False):
        t0 = "UINT8_DYN"
    C.write("    COM_%s, /* Type */\n" % (t0))
    C.write("    COM_%s_ENDIAN, /* Endianness */\n" % (sig["Endian"].upper()))
    C.write("    %s,\n" % (str(sig.get("IsGroup", False)).upper()))
    C.write("  },\n")


def gen_rx_msg_cfg(network, msg, C):
    C.write("static const Com_IPduRxConfigType Com_IPduRxConfig_%s = {\n" % (msg["Name"]))
    RxIpduCallout = msg.get("RxIpduCallout", "NULL")
    RxNotification = msg.get("RxNotification", "NULL")
    RxTOut = msg.get("RxTOut", "NULL")
    FirstTimeout = msg.get("FirstTimeout", 0)
    Timeout = msg.get("Timeout", 0)
    C.write("  &Com_IPduRxContext_%s,\n" % (msg["Name"]))
    C.write("  #ifdef COM_USE_RX_NOTIFICATION\n")
    C.write("  %s, /* RxNotification */\n" % (RxNotification))
    C.write("  #endif\n")
    C.write("  #ifdef COM_USE_RX_TIMEOUT\n")
    C.write("  %s, /* RxTOut */\n" % (RxTOut))
    C.write("  #endif\n")
    C.write("  #ifdef COM_USE_RX_IPDU_CALLOUT\n")
    C.write("  %s, /* RxIpduCallout */\n" % (RxIpduCallout))
    C.write("  #endif\n")
    C.write("  COM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* FirstTimeout */\n" % (FirstTimeout))
    C.write("  COM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* Timeout */\n" % (Timeout))
    C.write("};\n\n")


def gen_tx_msg_cfg(network, msg, C):
    if msg["Name"].startswith(network["Name"]):
        name = toMacro(msg["Name"])
    else:
        name = toPduSymbol((network["Name"], msg["Name"]))
    C.write("static const Com_IPduTxConfigType Com_IPduTxConfig_%s = {\n" % (msg["Name"]))
    ErrorNotification = msg.get("ErrorNotification", "NULL")
    TxNotification = msg.get("TxNotification", "NULL")
    TxIpduCallout = msg.get("TxIpduCallout", "NULL")
    FirstTime = msg.get("FirstTime", 0)
    CycleTime = msg.get("CycleTime", 0)
    if network["Network"] in ["LIN"]:
        C.write("  NULL,\n")
    else:
        C.write("  &Com_IPduTxContext_%s,\n" % (msg["Name"]))
    C.write("  #ifdef COM_USE_TX_ERROR_NOTIFICATION\n")
    C.write("  %s, /* ErrorNotification */\n" % (ErrorNotification))
    C.write("  #endif\n")
    C.write("  #ifdef COM_USE_TX_NOTIFICATION\n")
    C.write("  %s, /* TxNotification */\n" % (TxNotification))
    C.write("  #endif\n")
    C.write("  #ifdef COM_USE_TX_IPDU_CALLOUT\n")
    C.write("  %s, /* TxIpduCallout */\n" % (TxIpduCallout))
    C.write("  #endif\n")
    C.write("  COM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* FirstTime */\n" % (FirstTime))
    C.write("  COM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* CycleTime */\n" % (CycleTime))
    C.write("#ifdef USE_PDUR\n")
    if msg.get("Trigger", False) or network["Network"] in ["LIN"]:
        C.write("  (PduIdType)-1 /* trigger transmit */,\n")
    else:
        C.write("  PDUR_%s,\n" % (name.upper()))
    C.write("#else\n")
    C.write("  COM_ECUC_PDUID_OFFSET + COM_%s,\n" % (name.upper()))
    C.write("#endif\n")
    C.write("};\n\n")


def gen_msg(msg, C, network):
    isTx = msg["Node"] == network["Me"]
    C.write("  {\n")
    C.write("#ifdef USE_SHELL\n")
    C.write('    "%s",\n' % (msg["Name"]))
    C.write("#endif\n")
    C.write("    Com_PduData_%s, /* Ptr */\n" % (msg["Name"]))
    dynLen = msg["Signals"][-1].get("Dyn", False)
    if dynLen:
        C.write("    &%s_dynLen, /* DynLen */\n" % (msg["Name"]))
    else:
        C.write("    NULL, /* DynLen */\n")
    C.write("    Com_IPduSignals_%s, /* Signals */\n" % (msg["Name"]))
    if isTx:
        C.write("    NULL, /* RxConfig */\n")
        # if network["Network"] in ["LIN"]:
        #     C.write("    NULL, /* TxConfig */\n")
        # else:
        C.write("    &Com_IPduTxConfig_%s, /* TxConfig */\n" % (msg["Name"]))
    else:
        C.write("    &Com_IPduRxConfig_%s, /* RxConfig */\n" % (msg["Name"]))
        C.write("    NULL, /* TxConfig */\n")
    C.write("    Com_IPdu%s_GroupRefMask,\n" % (msg["Name"]))
    C.write("    sizeof(Com_PduData_%s), /* Length */\n" % (msg["Name"]))
    C.write("    ARRAY_SIZE(Com_IPduSignals_%s), /* NumOfSignals */\n" % (msg["Name"]))
    C.write("  },\n")


def Gen_Com(cfg, dir):
    H = open("%s/Com_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef COM_CFG_H\n")
    H.write("#define COM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef COM_CONST\n")
    H.write("#define COM_CONST\n")
    H.write("#endif\n\n")
    H.write("#ifndef COM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define COM_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define COM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + COM_MAIN_FUNCTION_PERIOD - 1u) / COM_MAIN_FUNCTION_PERIOD)\n\n")
    NTs = []
    for network in cfg["Networks"]:
        if network["Network"] not in NTs:
            NTs.append(network["Network"])
    for nt in NTs:
        H.write("#define COM_USE_%s\n" % (nt))
    H.write("#define COM_USE_SIGNAL_CONFIG\n")
    H.write("#define COM_USE_SIGNAL_UPDATE_BIT\n")
    H.write("\n")
    for network in cfg["Networks"]:
        H.write("#define COM_RX_FOR_%s(id, PduInfoPtr) \\\n" % (network["Name"]))
        IF = "if"
        for msg in network["Messages"]:
            if msg["Node"] != network["Me"]:
                if msg["Name"].startswith(network["Name"]):
                    name = toMacro(msg["Name"])
                else:
                    name = toPduSymbol((network["Name"], msg["Name"]))
                H.write("  %s (0x%X == id) { \\\n" % (IF, msg["Id"]))
                H.write("    Com_RxIndication(COM_%s, PduInfoPtr); \\\n" % (name.upper()))
                H.write("  }")
                IF = "else if"
        H.write("\n\n")
    H.write("#ifndef COM_ECUC_PDUID_OFFSET\n")
    H.write("#define COM_ECUC_PDUID_OFFSET 0\n")
    H.write("#endif\n\n")
    last_end = "COM_ECUC_PDUID_OFFSET"
    for idx, network in enumerate(cfg["Networks"]):
        H.write("/* NOTE: manually modify to fix it to the right HTH */\n")
        H.write("#define COM_ECUC_%s_PDUID_MIN %s\n" % (network["Name"], last_end))
        H.write("#define COM_ECUC_%s_PDUID_MAX %s + %s\n" % (network["Name"], last_end, len(network["Messages"])))
        last_end = "COM_ECUC_%s_PDUID_MAX" % (network["Name"])
        H.write("#define COM_TX_FOR_%s(TxPduId, dlPdu, PduInfoPtr, ret) \\\n" % (network["Name"]))
        IF = "if"
        for msg in network["Messages"]:
            if msg["Node"] == network["Me"]:
                if msg["Name"].startswith(network["Name"]):
                    name = toMacro(msg["Name"])
                else:
                    name = toPduSymbol((network["Name"], msg["Name"]))
                H.write("  %s ((COM_%s+COM_ECUC_PDUID_OFFSET) == TxPduId) { \\\n" % (IF, name.upper()))
                if network["Network"] == "CAN":
                    H.write("    dlPdu.id = 0x%X; \\\n" % (msg["Id"]))
                    H.write("    ret = Can_Write(%s, &dlPdu); \\\n" % (idx))
                elif network["Network"] == "LIN":
                    pass
                else:
                    raise
                H.write("  }")
                IF = "else if"
        H.write("\n\n")
    PDU_ID = 0
    for network in cfg["Networks"]:
        H.write("/* messages for network %s */\n" % (network["Name"]))
        for msg in network["Messages"]:
            if msg["Name"].startswith(network["Name"]):
                name = toMacro(msg["Name"])
            else:
                name = toPduSymbol((network["Name"], msg["Name"]))
            H.write("#define COM_%s %s\n" % (name.upper(), PDU_ID))
            PDU_ID += 1
        H.write("\n")
    SIG_ID = 0
    for network in cfg["Networks"]:
        H.write("/* signals for network %s */\n" % (network["Name"]))
        for msg in network["Messages"]:
            H.write(
                "/* signals for network %s message %s: id=0x%x dlc=%d, dir=%s */\n"
                % (network["Name"], msg["Name"], msg["Id"], msg["Dlc"], "TX" if msg["Node"] == network["Me"] else "RX")
            )
            for sig in msg["Signals"]:
                H.write(
                    "#define COM_%sID_%s %s /* %s %s@%s */\n"
                    % (
                        "G" if sig.get("IsGroup", False) else "S",
                        sig["Name"],
                        SIG_ID,
                        sig["Endian"],
                        sig["Size"],
                        sig["Start"],
                    )
                )
                SIG_ID += 1
            H.write("\n")
        H.write("\n")
    H.write("/* NOTE: manually modify to create more groups */\n")
    for id, network in enumerate(cfg["Networks"]):
        H.write("#define COM_GROUP_ID_%s %s\n" % (network["Name"], id))
        for msg in network["Messages"]:
            H.write("#define Com_IPdu%s_GroupRefMask (1<<COM_GROUP_ID_%s)\n" % (msg["Name"], network["Name"]))
    H.write("\n#define COM_GROUP_ID_MAX %s\n" % (len(cfg["Networks"])))
    H.write("\n#define COM_USE_MAIN_FAST\n\n")
    bHasTxErrorNotification = False
    bHasTxNotification = False
    bHasTxIpduCallout = False
    bHasSignalTxErrorNotification = False
    bHasSignalTxNotification = False
    bHasRxNotification = False
    bHasRxTOut = False
    bHasRxIpduCallout = False
    bHasSignalRxInvalidNotification = False
    bHasSignalRxRxNotification = False
    bHasSignalRxRxTOut = False
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            if msg["Node"] == network["Me"]:
                ErrorNotification = msg.get("ErrorNotification", "NULL")
                TxNotification = msg.get("TxNotification", "NULL")
                TxIpduCallout = msg.get("TxIpduCallout", "NULL")
                if ErrorNotification != "NULL":
                    bHasTxErrorNotification = True
                if TxNotification != "NULL":
                    bHasTxNotification = True
                if TxIpduCallout != "NULL":
                    bHasTxIpduCallout = True
                for sig in msg["Signals"]:
                    ErrorNotification = sig.get("ErrorNotification", "NULL")
                    TxNotification = sig.get("TxNotification", "NULL")
                    if ErrorNotification != "NULL":
                        bHasSignalTxErrorNotification = True
                    if TxNotification != "NULL":
                        bHasSignalTxNotification = True
            else:
                RxIpduCallout = msg.get("RxIpduCallout", "NULL")
                RxNotification = msg.get("RxNotification", "NULL")
                RxTOut = msg.get("RxTOut", "NULL")
                if RxNotification != "NULL":
                    bHasRxNotification = True
                if RxTOut != "NULL":
                    bHasRxTOut = True
                if RxIpduCallout != "NULL":
                    bHasRxIpduCallout = True
                for sig in msg["Signals"]:
                    InvalidNotification = sig.get("InvalidNotification", "NULL")
                    RxNotification = sig.get("RxNotification", "NULL")
                    RxTOut = sig.get("RxTOut", "NULL")
                    if InvalidNotification != "NULL":
                        bHasSignalRxInvalidNotification = True
                    if RxNotification != "NULL":
                        bHasSignalRxRxNotification = True
                    if RxTOut != "NULL":
                        bHasSignalRxRxTOut = True
    if bHasTxErrorNotification:
        H.write("#define COM_USE_TX_ERROR_NOTIFICATION\n")
    if bHasTxNotification:
        H.write("#define COM_USE_TX_NOTIFICATION\n")
    if bHasTxIpduCallout:
        H.write("#define COM_USE_TX_IPDU_CALLOUT\n")
    if bHasSignalTxErrorNotification:
        H.write("#define COM_USE_SIGNAL_TX_ERROR_NOTIFICATION\n")
    if bHasSignalTxNotification:
        H.write("#define COM_USE_SIGNAL_TX_NOTIFICATION\n")
    if bHasRxNotification:
        H.write("#define COM_USE_RX_NOTIFICATION\n")
    if bHasRxTOut:
        H.write("#define COM_USE_RX_TIMEOUT\n")
    if bHasRxIpduCallout:
        H.write("#define COM_USE_RX_IPDU_CALLOUT\n")
    if bHasSignalRxInvalidNotification:
        H.write("#define COM_USE_SIGNAL_RX_INVALID_NOTIFICATION\n")
    if bHasSignalRxRxNotification:
        H.write("#define COM_USE_SIGNAL_RX_NOTIFICATION\n")
    if bHasSignalRxRxTOut:
        H.write("#define COM_USE_SIGNAL_RX_TIMEOUT\n")
    H.write("%s#define COM_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* COM_CFG_H */\n")
    H.close()

    C = open("%s/Com_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Com_Cfg.h"\n')
    C.write('#include "Com.h"\n')
    C.write('#include "Com_Priv.h"\n')
    C.write("#ifdef USE_PDUR\n")
    C.write('#include "PduR_Cfg.h"\n')
    C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            if msg["Node"] == network["Me"]:
                ErrorNotification = msg.get("ErrorNotification", "NULL")
                TxNotification = msg.get("TxNotification", "NULL")
                TxIpduCallout = msg.get("TxIpduCallout", "NULL")
                if ErrorNotification != "NULL":
                    C.write("extern void %s(void);\n" % (ErrorNotification))
                if TxNotification != "NULL":
                    C.write("extern void %s(void);\n" % (TxNotification))
                if TxIpduCallout != "NULL":
                    C.write("extern boolean %s(PduIdType PduId, const PduInfoType *PduInfoPtr);\n" % (TxIpduCallout))
                for sig in msg["Signals"]:
                    ErrorNotification = sig.get("ErrorNotification", "NULL")
                    TxNotification = sig.get("TxNotification", "NULL")
                    if ErrorNotification != "NULL":
                        C.write("extern void %s(void);\n" % (ErrorNotification))
                    if TxNotification != "NULL":
                        C.write("extern void %s(void);\n" % (TxNotification))
            else:
                RxIpduCallout = msg.get("RxIpduCallout", "NULL")
                RxNotification = msg.get("RxNotification", "NULL")
                RxTOut = msg.get("RxTOut", "NULL")
                if RxNotification != "NULL":
                    C.write("extern void %s(void);\n" % (RxNotification))
                if RxTOut != "NULL":
                    C.write("extern void %s(void);\n" % (RxTOut))
                if RxIpduCallout != "NULL":
                    C.write("extern boolean %s(PduIdType PduId, const PduInfoType *PduInfoPtr);\n" % (RxIpduCallout))
                for sig in msg["Signals"]:
                    InvalidNotification = sig.get("InvalidNotification", "NULL")
                    RxNotification = sig.get("RxNotification", "NULL")
                    RxTOut = sig.get("RxTOut", "NULL")
                    if InvalidNotification != "NULL":
                        C.write("extern void %s(void);\n" % (InvalidNotification))
                    if RxNotification != "NULL":
                        C.write("extern void %s(void);\n" % (RxNotification))
                    if RxTOut != "NULL":
                        C.write("extern void %s(void);\n" % (RxTOut))
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            for sig in msg["Signals"]:
                gen_signal_init_value(sig, C)
                if msg["Node"] != network["Me"]:  # isRx
                    gen_signal_timeout_value(sig, C)
    C.write("\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            C.write("static uint8_t Com_PduData_%s[%s];\n" % (msg["Name"], msg["Dlc"]))
            groups = []
            for sig in msg["Signals"]:
                if "Group" in sig:
                    if sig["Group"] not in groups:
                        groups.append(sig["Group"])
            for sig in msg["Signals"]:
                if sig["Name"] in groups:
                    C.write("static uint8_t Com_GrpsData_%s[%s];\n" % (sig["Name"], int(sig["Size"] / 8)))

    C.write("\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            dynLen = msg["Signals"][-1].get("Dyn", False)
            if dynLen:
                C.write("static Com_DataLengthType %s_dynLen = %s;\n" % (msg["Name"], msg["Dlc"]))
            if msg["Node"] == network["Me"]:
                if network["Network"] in ["LIN"]:
                    continue
                C.write("static Com_IPduTxContextType Com_IPduTxContext_%s;\n" % (msg["Name"]))
            else:
                C.write("static Com_IPduRxContextType Com_IPduRxContext_%s;\n" % (msg["Name"]))
    C.write("\n")
    C.write("#ifdef COM_USE_SIGNAL_CONFIG\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            gen_cfg = gen_rx_sig_cfg
            if msg["Node"] == network["Me"]:  # is Tx message
                gen_cfg = gen_tx_sig_cfg
            for sig in msg["Signals"]:
                gen_cfg(sig, C)
    C.write("#endif /* COM_USE_SIGNAL_CONFIG */\n")
    C.write("static const Com_SignalConfigType Com_SignalConfigs[] = {\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            for sig in msg["Signals"]:
                gen_sig(network, sig, msg, C, msg["Node"] == network["Me"])
    C.write("};\n\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            C.write("static const Com_SignalConfigType* Com_IPduSignals_%s[] = {\n" % (msg["Name"]))
            for sig in msg["Signals"]:
                C.write(
                    "  &Com_SignalConfigs[COM_%sID_%s],\n" % ("G" if sig.get("IsGroup", False) else "S", sig["Name"])
                )
            C.write("};\n\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            gen_cfg = gen_rx_msg_cfg
            if msg["Node"] == network["Me"]:  # is Tx message
                gen_cfg = gen_tx_msg_cfg
                # if network["Network"] in ["LIN"]:
                #     continue
            gen_cfg(network, msg, C)
    C.write("static const Com_IPduConfigType Com_IPduConfigs[] = {\n")
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            gen_msg(msg, C, network)
    C.write("};\n\n")
    C.write("static Com_GlobalContextType Com_GlobalContext;\n")
    C.write("const Com_ConfigType Com_Config = {\n")
    C.write("  Com_IPduConfigs,\n")
    C.write("  Com_SignalConfigs,\n")
    C.write("  &Com_GlobalContext,\n")
    C.write("  ARRAY_SIZE(Com_IPduConfigs),\n")
    C.write("  ARRAY_SIZE(Com_SignalConfigs),\n")
    C.write("  %s /* NumOfGroups */,\n" % (len(cfg["Networks"])))
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write("#ifdef USE_E2E\n")
    C.write('#include "E2E.h"\n')
    C.write('#include "E2E_Cfg.h"\n')
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            if "E2E" not in msg:
                continue
            name = msg["E2E"]["Name"]
            profile = msg["E2E"]["Profile"]
            if msg["Node"] == network["Me"]:
                TxIpduCallout = msg.get("TxIpduCallout", "NULL")
                if TxIpduCallout != "NULL":
                    C.write("boolean %s(PduIdType PduId, const PduInfoType *PduInfoPtr) {\n" % (TxIpduCallout))
                    C.write("  Std_ReturnType ret;\n")
                    C.write(
                        "  ret = E2E_%sProtect(E2E_PROTECT_%s_%s, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);\n"
                        % (profile, profile, toMacro(name))
                    )
                    C.write("  return (E_OK == ret);\n")
                    C.write("}\n\n")
            else:
                RxIpduCallout = msg.get("RxIpduCallout", "NULL")
                if RxIpduCallout != "NULL":
                    C.write("boolean %s(PduIdType PduId, const PduInfoType *PduInfoPtr) {\n" % (RxIpduCallout))
                    C.write("  Std_ReturnType ret;\n")
                    C.write(
                        "  ret = E2E_%sCheck(E2E_CHECK_%s_%s, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);\n"
                        % (profile, profile, toMacro(name))
                    )
                    C.write("  return ( (E_OK == ret) || (E2E_E_OK_SOME_LOST == ret));\n")
                    C.write("}\n\n")
    C.write("#endif /* USE_E2E */\n\n")
    C.close()


def get_messages(path):
    from .dbc import dbc

    p = dbc(path)
    return p


def get_signals(signalNames, network):
    signals = []
    for msg in network["Messages"]:
        for sig in msg["Signals"]:
            if sig["Name"] in signalNames:
                signals.append(sig)
    return signals


def handle_groups(network):
    if "Groups" not in network:
        return
    for group in network["Groups"]:
        if "Name" in group:
            # { "Name": "SystemTime", "Signals": ["year", ...] }
            items = [(group["Name"], group.get("Signals", []))]
        else:
            # legacy form: { "SystemTime": ["year", ...] }
            items = group.items()
        for groupName, signalNames in items:
            signals = get_signals(signalNames, network)
            for sig in signals:
                sig["Group"] = groupName
    del network["Groups"]


def add_group_signal(msg):
    _bebm = []
    for i in range(msg["Dlc"]):
        for j in range(8):
            _bebm.append(i * 8 + 7 - j)
    group_signals = {}
    for sig in msg["Signals"]:
        if "Group" in sig:
            if sig["Group"] not in group_signals:
                gsig = {
                    "Name": sig["Group"],
                    "Endian": sig["Endian"],
                    "Start": sig["Start"],
                    "Size": sig["Size"],
                    "IsGroup": True,
                }
                group_signals[sig["Group"]] = gsig
            else:
                # group signal must be UINT8N
                gsig = group_signals[sig["Group"]]
                if gsig["Endian"] == "big":
                    index1 = _bebm.index(sig["Start"])
                    index2 = _bebm.index(gsig["Start"])
                    start = min(index1, index2)
                    lsbIndex1 = ((sig["Start"] ^ 0x7) + sig["Size"] - 1) ^ 7
                    lsbIndex2 = ((gsig["Start"] ^ 0x7) + gsig["Size"] - 1) ^ 7
                    index1 = _bebm.index(lsbIndex1)
                    index2 = _bebm.index(lsbIndex2)
                    end = max(index1, index2)
                    start = int(start / 8) * 8
                    end = end | 0x07
                    gsig["Start"] = _bebm[start]
                    gsig["Size"] = end - start + 1
                else:
                    start = min(sig["Start"], gsig["Start"])
                    end = max(gsig["Start"] + gsig["Size"], gsig["Start"] + gsig["Size"])
                    start = int(start / 8) * 8
                    end = end | 0x07
                    gsig["Start"] = start
                    gsig["Size"] = end - start + 1
    msg["Signals"].extend([sig for _, sig in group_signals.items()])


def post(cfg, e2eCfg={}):
    for network in cfg["Networks"]:
        trigger = network.get("Trigger", [])
        for msg in network["Messages"]:
            if msg["Name"] in trigger:
                msg["Trigger"] = True
            if any("Group" in sig for sig in msg["Signals"]):
                add_group_signal(msg)
        E2E = network.get("E2E", [])
        for e2e in E2E:
            for msg in network["Messages"]:
                if msg["Name"] == e2e["Name"]:
                    if msg["Node"] == network["Me"]:
                        typ = "Protect"
                    else:
                        typ = "Check"
                    name, profile = e2e["Name"], e2e["Profile"]
                    ecfg = None
                    for ec in e2eCfg.get("%s%s" % (typ, profile), []):
                        if name == ec["Name"]:
                            ecfg = ec
                    if ecfg == None:
                        raise Exception("E2E profile %s %s for %s is not configured" % (typ, profile, name))
                    for key, value in ecfg.items():
                        if key in ["DataID", "DataIDList"]:
                            e2e[key] = eval(str(value))
                        else:
                            e2e[key] = value
                    msg["E2E"] = e2e
    # make sure TX fist then RX
    for network in cfg["Networks"]:
        txmsgs, rxmsgs = [], []
        for msg in network["Messages"]:
            msg["Id"] = toNum(msg["Id"])
            if msg["Node"] == network["Me"]:
                if "TX" not in toMacro(msg["Name"]):
                    msg["Name"] += "_TX"
                txmsgs.append(msg)
            else:
                if "RX" not in toMacro(msg["Name"]):
                    msg["Name"] += "_RX"
                rxmsgs.append(msg)
        network["Messages"] = txmsgs + rxmsgs
    messages = {}
    # post process to ensure message has unique name
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            if msg["Name"] not in messages:
                messages[msg["Name"]] = [(network, msg)]
            else:
                messages[msg["Name"]].append((network, msg))
    for msgName, L in messages.items():
        if len(L) > 1:
            for network, msg in L:
                msg["Name"] = "_".join([network["Name"], msg["Name"]])
    # post process to ensure signal has unique name
    signals = {}
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            for sig in msg["Signals"]:
                if sig["Name"] not in signals:
                    signals[sig["Name"]] = [(network, msg, sig)]
                else:
                    signals[sig["Name"]].append((network, msg, sig))
    for sigName, L in signals.items():
        if len(L) > 1:
            sigNames = []
            bAddNetworkName = False
            for network, msg, sig in L:
                sig["Name"] = "_".join([msg["Name"], sig["Name"]])
                if sig["Name"] not in sigNames:
                    sigNames.append(sig["Name"])
                else:
                    bAddNetworkName = True
            if bAddNetworkName:
                for network, msg, sig in L:
                    sig["Name"] = "_".join([network["Name"], sig["Name"]])
    # auto adjust timeout_factor
    for network in cfg["Networks"]:
        if "TimeoutFactor" in network:
            for msg in network["Messages"]:
                if msg["Node"] == network["Me"]:
                    continue  # do nothing for TxMsg
                if "CycleTime" in msg and "Timeout" not in msg:
                    msg["Timeout"] = network["TimeoutFactor"] * msg["CycleTime"]
    # auto add message/signal rx and timeout notification
    for network in cfg["Networks"]:
        enabledTOut = network.get("EnableMessageRxTimeoutNotification", False)
        enabledRx = network.get("EnableMessageRxNotification", False)
        enabledSigTOut = network.get("EnableSignalRxTimeoutNotification", False)
        enabledSigRx = network.get("EnableSignalRxNotification", False)
        for msg in network["Messages"]:
            if msg["Node"] == network["Me"]:
                continue  # do nothing for TxMsg
            CycleTime = msg.get("CycleTime", 0)
            if enabledTOut and "RxTOut" not in msg and 0 != CycleTime:
                msg["RxTOut"] = "%s_RxTimeout" % (msg["Name"])
                if network["Name"] not in msg["Name"]:
                    msg["RxTOut"] = "%s_%s" % (network["Name"], msg["RxTOut"])
            if enabledRx and "RxNotification" not in msg:
                msg["RxNotification"] = "%s_RxNotification" % (msg["Name"])
                if network["Name"] not in msg["Name"]:
                    msg["RxNotification"] = "%s_%s" % (network["Name"], msg["RxNotification"])
            for signal in msg["Signals"]:
                if enabledSigTOut and "RxTOut" not in signal and 0 != CycleTime:
                    signal["RxTOut"] = "%s_RxTimeout" % (signal["Name"])
                    if msg["Name"] not in signal["Name"]:
                        signal["RxTOut"] = "%s_%s" % (msg["Name"], signal["RxTOut"])
                    if network["Name"] not in signal["Name"]:
                        signal["RxTOut"] = "%s_%s" % (network["Name"], signal["RxTOut"])
                if enabledSigRx and "RxNotification" not in signal:
                    signal["RxNotification"] = "%s_RxNotification" % (signal["Name"])
                    if msg["Name"] not in signal["Name"]:
                        signal["RxNotification"] = "%s_%s" % (msg["Name"], signal["RxNotification"])
                    if network["Name"] not in signal["Name"]:
                        signal["RxNotification"] = "%s_%s" % (network["Name"], signal["RxNotification"])
    # auto add message tx callout
    for network in cfg["Networks"]:
        enabledTxCallout = network.get("EnableMessageTxCallout", False)
        enabledRxCallout = network.get("EnableMessageRxCallout", False)
        for msg in network["Messages"]:
            if msg["Node"] != network["Me"]:
                if enabledRxCallout and "RxIpduCallout" not in msg:
                    msg["RxIpduCallout"] = "%s_RxIpduCallout" % (msg["Name"])
                    if network["Name"] not in msg["Name"]:
                        msg["RxIpduCallout"] = "%s_%s" % (network["Name"], msg["RxIpduCallout"])
            else:
                if enabledTxCallout and "TxIpduCallout" not in msg:
                    msg["TxIpduCallout"] = "%s_TxIpduCallout" % (msg["Name"])
                    if network["Name"] not in msg["Name"]:
                        msg["TxIpduCallout"] = "%s_%s" % (network["Name"], msg["TxIpduCallout"])


def ldf2dbc(ldfPath, dbcPath):
    from .ldf import ldf

    ldf(ldfPath, dbcPath)


def extract(cfg, dir):
    e2eCfg = {}
    if "E2E" in cfg:
        path = cfg["E2E"]
        if not os.path.isfile(path):
            path = os.path.abspath(os.path.join(dir, "..", path))
            if not os.path.isfile(path):
                raise Exception("File %s not exists" % (path))
        with open(path) as f:
            e2eCfg = json.load(f)
    cfg_ = {"Class": "Com", "Networks": []}
    for network in cfg["Networks"]:
        if "Ldf" in network:
            path = network["Ldf"]
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, "..", path))
            if not os.path.isfile(path):
                raise Exception("File %s not exists" % (path))
            dbc = os.path.join(dir, os.path.basename(path)[:-4] + ".dbc")
            ldf2dbc(path, dbc)
            del network["Ldf"]
            network["Dbc"] = dbc
        if "Dbc" in network:
            network_ = dict(network)
            if "Messages" not in network:
                network_["Messages"] = []
            path = network["Dbc"]
            del network_["Dbc"]
            if not os.path.isfile(path):
                path = os.path.abspath(os.path.join(dir, "..", path))
            if not os.path.isfile(path):
                raise Exception("File %s not exists" % (path))
            network_["Messages"].extend(get_messages(path))
            handle_groups(network_)
            cfg_["Networks"].append(network_)
        else:
            cfg_["Networks"].append(network)
    post(cfg_, e2eCfg)
    with open("%s/Com.json" % (dir), "w") as f:
        json.dump(cfg_, f, indent=2)
    return cfg_


def GenRTE(cfg, dir):
    fp = open("%s/bswcom.py" % (dir), "w")
    fp.write("from generator import asar\n\n")
    fp.write('NAMESPACE = "Default"\n\n')
    sigL = []
    for network in cfg["Networks"]:
        for msg in network["Messages"]:
            for sig in msg["Signals"]:
                if sig.get("IsGroup", False):
                    continue
                t0, t1, nBytes = get_signal_info(sig)
                sig[".type"] = t0
                if t0 in ["UINT8N", "SINT8N"]:
                    InitialValue = sig.get("InitialValue", [0])
                    continue  # TODO: has issue
                else:
                    InitialValue = sig.get("InitialValue", 0)
                sig[".init"] = InitialValue
                sigL.append(sig)
    for sig in sigL:
        fp.write("{0}_IV = asar.factory.ConstantTemplate('{0}_IV', NAMESPACE, {1})\n".format(sig["Name"], sig[".init"]))
    fp.write("\n")
    for sig in sigL:
        fp.write(
            "{0}_I = asar.factory.SenderReceiverInterfaceTemplate('{0}_I', NAMESPACE, asar.platform.ImplementationTypes.{1})\n".format(
                sig["Name"], sig[".type"].lower()
            )
        )
    fp.write("COM_D = []\n")
    for sig in sigL:
        fp.write("COM_D.append('{0}')\n" .format (sig["Name"]))
    fp.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    cfg_ = extract(cfg, dir)
    Gen_Com(cfg_, dir)
    GenRTE(cfg_, dir)
    return ["%s/Com_Cfg.c" % (dir)]
