# SSAS - Simple Smart Automotive Software
# Copyright (C) 2024 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_CanSM(cfg, dir):
    H = open("%s/CanSM_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef CANSM_CFG_H\n")
    H.write("#define CANSM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef CANSM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define CANSM_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define CANSM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + CANSM_MAIN_FUNCTION_PERIOD - 1u) / CANSM_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("%s#define CANSM_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* CANSM_CFG_H */\n")
    H.close()

    C = open("%s/CanSM_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "CanSM.h"\n')
    C.write('#include "CanSM_Cfg.h"\n')
    C.write('#include "CanSM_Priv.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("static const CanSM_NetworkConfigType CanSM_NetworkConfigs[] = {\n")
    for idx, network in enumerate(cfg["networks"]):
        C.write("  {\n")
        C.write("    %s, /* UserGetBusOffDelayFnc */\n" % ("NULL"))
        C.write("    CANSM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* BorTimeL1 */\n" % (network["BorTimeL1"]))
        C.write("    CANSM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* BorTimeL2 */\n" % (network["BorTimeL2"]))
        C.write("    CANSM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* BorTimeTxEnsured */\n" % (network["BorTimeTxEnsured"]))
        C.write("    CANSM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* TxRecoveryTime */\n" % (network["TxRecoveryTime"]))
        C.write("    %su, /* BorCounterL1ToL2 */\n" % (network["BorCounterL1ToL2"]))
        C.write("    %su, /* ControllerId */\n" % (idx))
        C.write("    %su, /* TransceiverId */\n" % (idx))
        C.write("    %su, /* ComMHandle */\n" % (idx))
        C.write(
            "    %s, /* BorTxConfirmationPolling */\n" % (str(network.get("BorTxConfirmationPolling", False)).upper())
        )
        C.write("    %s, /* CanTrcvPnEnabled */\n" % (str(network.get("CanTrcvPnEnabled", False)).upper()))
        C.write("  },\n")
    C.write("};\n\n")

    C.write("static CanSM_ChannelContextType CanSm_ChannelContexts[ARRAY_SIZE(CanSM_NetworkConfigs)];\n")

    C.write("const CanSM_ConfigType CanSM_Config = {\n")
    C.write("  CanSM_NetworkConfigs,\n")
    C.write("  CanSm_ChannelContexts,\n")
    C.write("  ARRAY_SIZE(CanSM_NetworkConfigs),\n")
    C.write("  %su, /* ModeRequestRepetitionMax */\n" % (cfg.get("ModeRequestRepetitionMax", 10)))
    C.write(
        "  CANSM_CONVERT_MS_TO_MAIN_CYCLES(%su), /* RequestRepetitionTime */\n"
        % (cfg.get("RequestRepetitionTime", 1000))
    )
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_CanSM(cfg, dir)
    return ["%s/CanSM_Cfg.c" % (dir)]
