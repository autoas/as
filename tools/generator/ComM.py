# SSAS - Simple Smart Automotive Software
# Copyright (C) 2024 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_ComM(cfg, dir):
    H = open("%s/ComM_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef COMM_CFG_H\n")
    H.write("#define COMM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef COMM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define COMM_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define COMM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + COMM_MAIN_FUNCTION_PERIOD - 1u) / COMM_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("%s#define COMM_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* COMM_CFG_H */\n")
    H.close()

    C = open("%s/ComM_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "ComM.h"\n')
    C.write('#include "ComM_Cfg.h"\n')
    C.write('#include "ComM_Priv.h"\n')
    C.write('#include "Com_Cfg.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    chlIdMap = {}
    for idc, chanel in enumerate(cfg["chanels"]):
        chlIdMap[chanel["name"]] = str(idc)
        user_ids = []
        for idu, user in enumerate(cfg["users"]):
            if chanel["name"] in user["chanels"]:
                user_ids.append(str(idu))
        C.write("static const uint8_t ComM_Chl%sUsers[] = { %s };\n" % (idc, ",".join(user_ids)))
        IpduGroupIds = chanel.get("IpduGroupIds", [])
        if len(IpduGroupIds) > 0:
            C.write(
                "static const Com_IpduGroupIdType Com_Chl%sIpduGroupIds[] = { %s };\n"
                % (idc, ",".join([str(x) for x in IpduGroupIds]))
            )
    C.write("static const ComM_ChannelConfigType ComM_ChannelConfigs[] = {\n")
    for idc, chanel in enumerate(cfg["chanels"]):
        IpduGroupIds = chanel.get("IpduGroupIds", [])

        C.write("  {\n")
        C.write("    ComM_Chl%sUsers,\n" % (idc))
        if len(IpduGroupIds) > 0:
            C.write("    Com_Chl%sIpduGroupIds,\n" % (idc))
        else:
            C.write("    NULL, /* ComIpduGroupIds */\n")
        C.write("    #ifdef COMM_USE_VARIANT_LIGHT\n")
        C.write(
            "    COMM_CONVERT_MS_TO_MAIN_CYCLES(%s), /* NmLightTimeout */ \n" % (chanel.get("NmLightTimeout", 5000))
        )
        C.write("    #endif\n")
        C.write("    %s, /* smHandle */\n" % (idc))
        C.write("    COMM_BUS_TYPE_%s,\n" % (chanel.get("type", "CAN")))
        C.write("    ARRAY_SIZE(ComM_Chl%sUsers),\n" % (idc))
        if len(IpduGroupIds) > 0:
            C.write("    ARRAY_SIZE(Com_Chl%sIpduGroupIds),\n" % (idc))
        else:
            C.write("    0, /* numOfComIpduGroups */\n")
        C.write("    COMM_NM_VARIANT_%s,\n" % (chanel.get("variant", "FULL")))
        C.write("    %s, /* nmHandle */\n" % (chanel.get("nmHandle", idc)))
        C.write("  },\n")
    C.write("};\n\n")

    for idu, user in enumerate(cfg["users"]):
        C.write(
            "static const uint8_t ComM_User%sChannels[] = { %s };\n"
            % (idu, ",".join([chlIdMap[x] for x in user["chanels"]]))
        )
    
    C.write("static const ComM_UserConfigType ComM_UserConfigs[] = {\n")
    for idu, user in enumerate(cfg["users"]):
        C.write("  {\n")
        C.write("    ComM_User%sChannels,\n" % (idu))
        C.write("    ARRAY_SIZE(ComM_User%sChannels),\n" % (idu))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static ComM_ChannelContextType ComM_ChannelContexts[ARRAY_SIZE(ComM_ChannelConfigs)];\n\n")

    C.write("static ComM_UserContextType ComM_UserContexts[ARRAY_SIZE(ComM_UserConfigs)];\n\n")

    C.write("const ComM_ConfigType ComM_Config = {\n")
    C.write("  ComM_ChannelConfigs,\n")
    C.write("  ComM_ChannelContexts,\n")
    C.write("  ComM_UserConfigs,\n")
    C.write("  ComM_UserContexts,\n")
    C.write("  ARRAY_SIZE(ComM_ChannelConfigs),\n")
    C.write("  ARRAY_SIZE(ComM_UserConfigs),\n")
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_ComM(cfg, dir)
    return ["%s/ComM_Cfg.c" % (dir)]
