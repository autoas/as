# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_LinTp(cfg, dir):
    H = open("%s/LinTp_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef LINTP_CFG_H\n")
    H.write("#define LINTP_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "ComStack_Types.h"\n')
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    for i, chl in enumerate(cfg["channels"]):
        H.write("#define LINTP_%s_RX %s\n" % (chl["name"], i))
        H.write("#define LINTP_%s_TX %s\n" % (chl["name"], i))
    H.write("\n\n")
    if "zero_cost" in cfg:
        H.write("#define PDUR_%s_LINTP_ZERO_COST\n\n" % (cfg["zero_cost"].upper()))
    if cfg.get("GwUserHook", False):
        H.write("#define LINTP_GW_USER_HOOK_RX_IND(id, result) LinTp_GwUserHookRxInd(id, result)\n")
        H.write("#define LINTP_GW_USER_HOOK_TX_CONFIRM(id, result) LinTp_GwUserHookTxConfirm(id, result)\n")
    H.write("#ifndef LINTP_MAIN_FUNCTION_PERIOD\n")
    H.write("#define LINTP_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define LINTP_CONVERT_MS_TO_MAIN_CYCLES(x)  \\\n")
    H.write("  ((x + LINTP_MAIN_FUNCTION_PERIOD - 1u) / LINTP_MAIN_FUNCTION_PERIOD)\n")
    H.write("%s#define LINTP_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    if cfg.get("GwUserHook", False):
        H.write("void LinTp_GwUserHookRxInd(PduIdType id, Std_ReturnType result);\n")
        H.write("void LinTp_GwUserHookTxConfirm(PduIdType id, Std_ReturnType result);\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* LINTP_CFG_H */\n")
    H.close()

    C = open("%s/LinTp_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "LinTp_Cfg.h"\n')
    C.write('#include "LinTp.h"\n')
    C.write('#include "LinTp_Priv.h"\n')
    C.write("#ifdef PDUR_DCM_LINTP_ZERO_COST\n")
    C.write('#include "Dcm_Cfg.h"\n')
    C.write("#else\n")
    C.write('#include "PduR_Cfg.h"\n')
    C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("#ifndef LINTP_LL_DL\n")
    C.write("#define LINTP_LL_DL 8u\n")
    C.write("#endif\n\n")
    C.write("#if defined(_WIN32) || defined(linux)\n")
    C.write("#define L_CONST\n")
    C.write("#else\n")
    C.write("#define L_CONST const\n")
    C.write("#endif\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for chl in cfg["channels"]:
        C.write("static uint8_t u8%sData[%s];\n" % (chl["name"], chl.get("LL_DL", "LINTP_LL_DL")))
    C.write("static L_CONST LinTp_ChannelConfigType LinTpChannelConfigs[] = {\n")
    for i, chl in enumerate(cfg["channels"]):
        C.write("  {\n")
        C.write("    /* %s */\n" % (chl["name"]))
        C.write("    u8%sData,\n" % (chl["name"]))
        if "N_TA" in chl:
            C.write("    CANTP_EXTENDED,\n")
        else:
            C.write("    CANTP_STANDARD,\n")
        C.write("    0,\n")
        C.write("    #ifdef PDUR_DCM_LINTP_ZERO_COST\n")
        C.write("    DCM_%s_RX /* PduR_RxPduId */,\n" % (chl["name"]))
        C.write("    DCM_%s_TX /* PduR_TxPduId */,\n" % (chl["name"]))
        C.write("    #else\n")
        if chl.get("GateWay", False):
            C.write("    PDUR_%s_TX /* PduR_RxPduId */,\n" % (chl["name"]))
            C.write("    PDUR_%s_RX /* PduR_TxPduId */,\n" % (chl["name"]))
        else:
            C.write("    PDUR_%s_RX /* PduR_RxPduId */,\n" % (chl["name"]))
            C.write("    PDUR_%s_TX /* PduR_TxPduId */,\n" % (chl["name"]))
        C.write("    #endif\n")
        C.write("    LINTP_CONVERT_MS_TO_MAIN_CYCLES(%su), /* N_As */\n" % (chl.get("N_As", 25)))
        C.write("    LINTP_CONVERT_MS_TO_MAIN_CYCLES(%su), /* N_Bs */\n" % (chl.get("N_Bs", 1000)))
        C.write("    LINTP_CONVERT_MS_TO_MAIN_CYCLES(%su), /* N_Cr */\n" % (chl.get("N_Cr", 1000)))
        C.write("    %su, /* STmin */\n" % (chl.get("STmin", 0)))
        C.write("    %su, /* BS */\n" % (chl.get("BS", 8)))
        C.write("    %su, /* N_TA */\n" % (chl.get("N_TA", 0)))
        C.write("    %su, /* WftMax */\n" % (chl.get("WftMax", 8)))
        C.write("    %s, /* LL_DL */\n" % (chl.get("LL_DL", "LINTP_LL_DL")))
        C.write("    %su, /* padding */\n" % (chl.get("padding", 0x55)))
        C.write("    LINTP_PHYSICAL, /* comType */\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static LinTp_ChannelContextType LinTpChannelContexts[ARRAY_SIZE(LinTpChannelConfigs)];\n\n")
    C.write("const LinTp_ConfigType LinTp_Config = {\n")
    C.write("  LinTpChannelConfigs,\n")
    C.write("  LinTpChannelContexts,\n")
    C.write("  ARRAY_SIZE(LinTpChannelConfigs),\n")
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("#if defined(_WIN32) || defined(linux)\n")
    C.write("#include <stdlib.h>\n")
    C.write("static void __attribute__((constructor)) _ll_dl_init(void) {\n")
    C.write("  int i;\n")
    C.write('  char *llDlStr = getenv("LL_DL");\n')
    C.write("  if (llDlStr != NULL) {\n")
    C.write("    for( i = 0; i < ARRAY_SIZE(LinTpChannelConfigs); i++ ) {\n")
    C.write("      LinTpChannelConfigs[i].LL_DL = atoi(llDlStr);\n")
    C.write("    }\n")
    C.write("  }\n")
    C.write("}\n")
    C.write("#endif\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_LinTp(cfg, dir)
    return ["%s/LinTp_Cfg.c" % (dir)]
