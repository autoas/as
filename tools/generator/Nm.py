# SSAS - Simple Smart Automotive Software
# Copyright (C) 2024 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_Nm(cfg, dir):
    H = open("%s/Nm_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef NM_CFG_H\n")
    H.write("#define NM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef NM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define NM_MAIN_FUNCTION_PERIOD %s\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define NM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + NM_MAIN_FUNCTION_PERIOD - 1) / NM_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* NM_CFG_H */\n")
    H.close()

    C = open("%s/Nm_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Nm.h"\n')
    C.write('#include "Nm_Cfg.h"\n')
    C.write('#include "Nm_Priv.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("static const Nm_ChannelConfigType Nm_ChannelConfigs[] = {\n")
    for idx, network in enumerate(cfg["networks"]):
        C.write("  {\n")
        C.write("    NM_BUSNM_%s, /* ImmediateNmCycleTime */\n" % (network.get("type", "CANNM")))
        C.write("    %s, /* handle */\n" % (idx))
        C.write("    %s, /* ComMHandle */\n" % (idx))
        C.write("  },\n")
    C.write("};\n\n")

    C.write("const Nm_ConfigType Nm_Config = {\n")
    C.write("  Nm_ChannelConfigs,\n")
    C.write("  ARRAY_SIZE(Nm_ChannelConfigs),\n")
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_Nm(cfg, dir)
    return ["%s/Nm_Cfg.c" % (dir)]
