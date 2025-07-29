# SSAS - Simple Smart Automotive Software
# Copyright (C) 2025 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_E2E(cfg, dir):
    H = open("%s/E2E_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef E2E_CFG_H\n")
    H.write("#define E2E_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    if len(cfg.get("ProtectP11", [])):
        H.write("#define E2E_USE_PROTECT_P11\n")
    for idx, profile in enumerate(cfg.get("ProtectP11", [])):
        H.write("#define E2E_PROTECT_P11_%s %su\n" % (toMacro(profile["name"]), idx))
    if len(cfg.get("CheckP11", [])):
        H.write("\n#define E2E_USE_CHECK_P11\n")
    for idx, profile in enumerate(cfg.get("CheckP11", [])):
        H.write("#define E2E_CHECK_P11_%s %su\n" % (toMacro(profile["name"]), idx))

    if len(cfg.get("ProtectP22", [])):
        H.write("\n#define E2E_USE_PROTECT_P22\n")
    for idx, profile in enumerate(cfg.get("ProtectP22", [])):
        H.write("#define E2E_PROTECT_P22_%s %su\n" % (toMacro(profile["name"]), idx))
    if len(cfg.get("CheckP22", [])):
        H.write("\n#define E2E_USE_CHECK_P22\n")
    for idx, profile in enumerate(cfg.get("CheckP22", [])):
        H.write("#define E2E_CHECK_P22_%s %su\n" % (toMacro(profile["name"]), idx))

    if len(cfg.get("ProtectP44", [])):
        H.write("\n#define E2E_USE_PROTECT_P44\n")
    for idx, profile in enumerate(cfg.get("ProtectP44", [])):
        H.write("#define E2E_PROTECT_P44_%s %su\n" % (toMacro(profile["name"]), idx))
    if len(cfg.get("CheckP44", [])):
        H.write("\n#define E2E_USE_CHECK_P44\n")
    for idx, profile in enumerate(cfg.get("CheckP44", [])):
        H.write("#define E2E_CHECK_P44_%s %su\n" % (toMacro(profile["name"]), idx))

    if len(cfg.get("ProtectP05", [])):
        H.write("\n#define E2E_USE_PROTECT_P05\n")
    for idx, profile in enumerate(cfg.get("ProtectP05", [])):
        H.write("#define E2E_PROTECT_P05_%s %su\n" % (toMacro(profile["name"]), idx))
    if len(cfg.get("CheckP44", [])):
        H.write("\n#define E2E_USE_CHECK_P05\n")
    for idx, profile in enumerate(cfg.get("CheckP05", [])):
        H.write("#define E2E_CHECK_P05_%s %su\n" % (toMacro(profile["name"]), idx))

    H.write("\n%s#define E2E_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* E2E_CFG_H */\n")
    H.close()

    C = open("%s/E2E_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "E2E.h"\n')
    C.write('#include "E2E_Cfg.h"\n')
    C.write('#include "E2E_Priv.h"\n')
    C.write('#include "Det.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    if len(cfg.get("ProtectP11", [])) > 0:
        for profile in cfg.get("ProtectP11", []):
            C.write("static E2E_ProtectProfile11ContextType E2E_ProtectP11_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_ProtectProfile11ConfigType E2E_ProtectProfile11Configs[] = {\n")
        for profile in cfg.get("ProtectP11", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_ProtectP11_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P11 */\n")
            C.write("      %su, /* DataID */\n" % (profile["DataID"]))
            C.write("      %su, /* CRCOffset */\n" % (profile.get("CRCOffset", 0)))
            C.write("      %su, /* CounterOffset */\n" % (profile.get("CounterOffset", 8)))
            C.write("      %su, /* DataIDNibbleOffset */\n" % (profile.get("DataIDNibbleOffset", 12)))
            C.write("      E2E_P11_DATAID_%s,\n" % (profile.get("DataIDMode", "NIBBLE")))
            C.write("    }\n")
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("CheckP11", [])) > 0:
        for profile in cfg.get("CheckP11", []):
            C.write("static E2E_CheckProfile11ContextType E2E_CheckP11_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_CheckProfile11ConfigType E2E_CheckProfile11Configs[] = {\n")
        for profile in cfg.get("CheckP11", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_CheckP11_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P11 */\n")
            C.write("      %su, /* DataID */\n" % (profile["DataID"]))
            C.write("      %su, /* CRCOffset */\n" % (profile.get("CRCOffset", 0)))
            C.write("      %su, /* CounterOffset */\n" % (profile.get("CounterOffset", 8)))
            C.write("      %su, /* DataIDNibbleOffset */\n" % (profile.get("DataIDNibbleOffset", 12)))
            C.write("      E2E_P11_DATAID_%s,\n" % (profile.get("DataIDMode", "NIBBLE")))
            C.write("    },\n")
            C.write("    %su, /* MaxDeltaCounter */\n" % (profile.get("MaxDeltaCounter", 1)))
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("ProtectP22", [])) > 0:
        for profile in cfg.get("ProtectP22", []):
            C.write("static E2E_ProtectProfile22ContextType E2E_ProtectP22_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_ProtectProfile22ConfigType E2E_ProtectProfile22Configs[] = {\n")
        for profile in cfg.get("ProtectP22", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_ProtectP22_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P22 */\n")
            C.write("      %su, /* Offset */\n" % (profile.get("Offset", 0)))
            dl = profile["DataIDList"]
            dl = eval(str(dl))
            dl = ", ".join(["0x%02xu" % (x) for x in dl])
            C.write("      { %s }, /* DataIDList */\n" % (dl))
            C.write("    }\n")
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("CheckP22", [])) > 0:
        for profile in cfg.get("CheckP22", []):
            C.write("static E2E_CheckProfile22ContextType E2E_CheckP22_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_CheckProfile22ConfigType E2E_CheckProfile22Configs[] = {\n")
        for profile in cfg.get("CheckP22", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_CheckP22_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P22 */\n")
            C.write("      %su, /* Offset */\n" % (profile.get("Offset", 0)))
            dl = profile["DataIDList"]
            dl = eval(str(dl))
            dl = ", ".join(["0x%02xu" % (x) for x in dl])
            C.write("      { %s }, /* DataIDList */\n" % (dl))
            C.write("    },\n")
            C.write("    %su, /* MaxDeltaCounter */\n" % (profile.get("MaxDeltaCounter", 1)))
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("ProtectP44", [])) > 0:
        for profile in cfg.get("ProtectP44", []):
            C.write("static E2E_ProtectProfile44ContextType E2E_ProtectP44_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_ProtectProfile44ConfigType E2E_ProtectProfile44Configs[] = {\n")
        for profile in cfg.get("ProtectP44", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_ProtectP44_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P44 */\n")
            C.write("      %su, /* DataID */\n" % (profile["DataID"]))
            C.write("      %su, /* Offset */\n" % (profile.get("Offset", 0)))
            C.write("    }\n")
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("CheckP44", [])) > 0:
        for profile in cfg.get("CheckP44", []):
            C.write("static E2E_CheckProfile44ContextType E2E_CheckP44_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_CheckProfile44ConfigType E2E_CheckProfile44Configs[] = {\n")
        for profile in cfg.get("CheckP44", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_CheckP44_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P44 */\n")
            C.write("      %su, /* DataID */\n" % (profile["DataID"]))
            C.write("      %su, /* Offset */\n" % (profile.get("Offset", 0)))
            C.write("    },\n")
            C.write("    %su, /* MaxDeltaCounter */\n" % (profile.get("MaxDeltaCounter", 1)))
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("ProtectP05", [])) > 0:
        for profile in cfg.get("ProtectP05", []):
            C.write("static E2E_ProtectProfile05ContextType E2E_ProtectP05_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_ProtectProfile05ConfigType E2E_ProtectProfile05Configs[] = {\n")
        for profile in cfg.get("ProtectP05", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_ProtectP05_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P05 */\n")
            C.write("      %su, /* DataID */\n" % (profile["DataID"]))
            C.write("      %su, /* Offset */\n" % (profile.get("Offset", 0)))
            C.write("    }\n")
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("CheckP05", [])) > 0:
        for profile in cfg.get("CheckP05", []):
            C.write("static E2E_CheckProfile05ContextType E2E_CheckP05_%s_Context;\n" % (profile["name"]))
        C.write("static const E2E_CheckProfile05ConfigType E2E_CheckProfile05Configs[] = {\n")
        for profile in cfg.get("CheckP05", []):
            C.write("  { /* %s */\n" % (profile["name"]))
            C.write("    &E2E_CheckP05_%s_Context,\n" % (profile["name"]))
            C.write("    { /* P05 */\n")
            C.write("      %su, /* DataID */\n" % (profile["DataID"]))
            C.write("      %su, /* Offset */\n" % (profile.get("Offset", 0)))
            C.write("    },\n")
            C.write("    %su, /* MaxDeltaCounter */\n" % (profile.get("MaxDeltaCounter", 1)))
            C.write("  },\n")
        C.write("};\n\n")
    C.write("const E2E_ConfigType E2E_Config = {\n")
    if len(cfg.get("ProtectP11", [])) > 0:
        C.write("  E2E_ProtectProfile11Configs,\n")
    if len(cfg.get("CheckP11", [])) > 0:
        C.write("  E2E_CheckProfile11Configs,\n")
    if len(cfg.get("ProtectP22", [])) > 0:
        C.write("  E2E_ProtectProfile22Configs,\n")
    if len(cfg.get("CheckP22", [])) > 0:
        C.write("  E2E_CheckProfile22Configs,\n")
    if len(cfg.get("ProtectP44", [])) > 0:
        C.write("  E2E_ProtectProfile44Configs,\n")
    if len(cfg.get("CheckP44", [])) > 0:
        C.write("  E2E_CheckProfile44Configs,\n")
    if len(cfg.get("ProtectP05", [])) > 0:
        C.write("  E2E_ProtectProfile05Configs,\n")
    if len(cfg.get("CheckP05", [])) > 0:
        C.write("  E2E_CheckProfile05Configs,\n")
    if len(cfg.get("ProtectP11", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_ProtectProfile11Configs),\n")
    if len(cfg.get("CheckP11", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_CheckProfile11Configs),\n")
    if len(cfg.get("ProtectP22", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_ProtectProfile22Configs),\n")
    if len(cfg.get("CheckP22", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_CheckProfile22Configs),\n")
    if len(cfg.get("ProtectP44", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_ProtectProfile44Configs),\n")
    if len(cfg.get("CheckP44", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_CheckProfile44Configs),\n")
    if len(cfg.get("ProtectP05", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_ProtectProfile05Configs),\n")
    if len(cfg.get("CheckP05", [])) > 0:
        C.write("  ARRAY_SIZE(E2E_CheckProfile05Configs),\n")
    C.write("};\n\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")

    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_E2E(cfg, dir)
    return ["%s/E2E_Cfg.c" % (dir)]
