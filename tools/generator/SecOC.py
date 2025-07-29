# SSAS - Simple Smart Automotive Software
# Copyright (C) 2025 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def GetFV(cfg, name):
    for fv in cfg["FreshnessValueManager"]:
        if fv["name"] == name:
            return fv
    raise Exception("FV %s not found" % (name))


def Gen_Csm(cfg, dir):
    H = open("%s/SecOC_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef SECOC_CFG_H\n")
    H.write("#define SECOC_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("\n#ifndef SECOC_MAIN_FUNCTION_PERIOD\n")
    H.write("#define SECOC_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define SECOC_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + SECOC_MAIN_FUNCTION_PERIOD - 1u) / SECOC_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("%s#define SECOC_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    for idx, TxPdu in enumerate(cfg.get("TxProc", [])):
        H.write("#define SECOC_%s %su\n" % (TxPdu["name"], idx))
        H.write("#define SECOC_FW_%s %su\n" % (TxPdu["name"], idx))
    for idx, RxPdu in enumerate(cfg.get("RxProc", [])):
        H.write("#define SECOC_%s %su\n" % (RxPdu["name"], idx))
        H.write("#define SECOC_FW_%s %su\n" % (RxPdu["name"], idx))
    if cfg.get("SELF_TEST", False):
        H.write("#define SECOC_SELF_TEST\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* SECOC_CFG_H */\n")
    H.close()

    C = open("%s/SecOC_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "SecOC.h"\n')
    C.write('#include "Csm_Cfg.h"\n')
    C.write('#include "PduR_Cfg.h"\n')
    C.write('#include "SecOC_Cfg.h"\n')
    C.write('#include "SecOC_Priv.h"\n')
    C.write('#include <string.h>\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    for idx, fv in enumerate(cfg["FreshnessValueManager"]):
        C.write("#define FV_%s %su\n" % (toMacro(fv["name"]), idx))
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    for fv in cfg["FreshnessValueManager"]:
        C.write(
            "Std_ReturnType SecOC_GetFreshness_%s(uint8_t *FreshnessValue,uint32_t *FreshnessValueLength);\n"
            % (fv["name"])
        )
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("static const SecOC_FreshnessValueType SecOC_FreshnessValues[] = {\n")
    for fv in cfg["FreshnessValueManager"]:
        C.write("  {\n")
        C.write("    SecOC_GetFreshness_%s,\n" % (fv["name"]))
        C.write("    %su, /* length in bits */\n" % (fv["length"]))
        C.write("  },\n")
    C.write("};\n\n")
    if len(cfg.get("TxProc", [])) > 0:
        C.write("static SecOC_TxPduProcContextType SecOC_TxPduProcContexts[%s];\n" % (len(cfg.get("TxProc", []))))
        for idx, TxPdu in enumerate(cfg.get("TxProc", [])):
            AuthPduOffset = max(TxPdu.get("AuthPduHeaderLength", 0), 2)
            fv = GetFV(cfg, TxPdu["FreshnessValueId"])
            length = AuthPduOffset + (TxPdu["AuthInfoLength"] // 8) + TxPdu["AuthenticIPduLength"] + (fv["length"] // 8)
            C.write("static uint8_t SecOC_TxPduProc_%s_Buffer[%s];\n" % (TxPdu["name"], length))
        C.write("static const SecOC_TxPduProcessingType SecOC_TxPduProcs[] = {\n")
        for idx, TxPdu in enumerate(cfg.get("TxProc", [])):
            AuthPduHeaderLength = TxPdu.get("AuthPduHeaderLength", 0)
            AuthPduOffset = max(AuthPduHeaderLength, 2)
            fv = GetFV(cfg, TxPdu["FreshnessValueId"])
            C.write("  {\n")
            C.write("    &SecOC_TxPduProcContexts[%s],\n" % (idx))
            C.write("    SecOC_TxPduProc_%s_Buffer,\n" % (TxPdu["name"]))
            C.write("    CSM_JOB_%s,\n" % (TxPdu["TxAuthServiceConfigRef"]))
            C.write("    PDUR_FW_%s, /* FwTxPduId */\n" % (TxPdu["name"]))
            C.write("    PDUR_%s,  /* UpTxPduId */\n" % (TxPdu["name"]))
            C.write("    ARRAY_SIZE(SecOC_TxPduProc_%s_Buffer),\n" % (TxPdu["name"]))
            C.write("    %su, /* AuthInfoLength */\n" % (TxPdu["AuthInfoLength"]))
            C.write("    %su, /* AuthInfoTruncLength */\n" % (TxPdu["AuthInfoTruncLength"]))
            C.write("    %su, /* DataId */\n" % (TxPdu["DataId"]))
            C.write("    FV_%s, /* FreshnessValueId */\n" % (toMacro(TxPdu["FreshnessValueId"])))
            C.write("    %su, /* FreshnessValueLength */\n" % (fv["length"]))
            C.write("    %su, /* FreshnessValueTruncLength */\n" % (TxPdu["FreshnessValueTruncLength"]))
            C.write("    0x%xu, /* TxPduUnusedAreasDefault */\n" % (toNum(TxPdu.get("TxPduUnusedAreasDefault", 0x55))))
            C.write("    %su, /* AuthPduHeaderLength */\n" % (AuthPduHeaderLength))
            C.write("    %su, /* AuthPduOffset */\n" % (AuthPduOffset))
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("RxProc", [])) > 0:
        C.write("static SecOC_RxPduProcContextType SecOC_RxPduProcContexts[%s];\n" % (len(cfg.get("RxProc", []))))
        for idx, RxPdu in enumerate(cfg.get("RxProc", [])):
            fv = GetFV(cfg, RxPdu["FreshnessValueId"])
            length = (
                2
                + RxPdu.get("AuthPduHeaderLength", 0)
                + (RxPdu["AuthInfoLength"] // 8)
                + RxPdu["AuthenticIPduLength"]
                + (fv["length"] // 8)
            )
            C.write("static uint8_t SecOC_RxPduProc_%s_Buffer[%s];\n" % (RxPdu["name"], length))
        C.write("static const SecOC_RxPduProcessingType SecOC_RxPduProcs[] = {\n")
        for idx, RxPdu in enumerate(cfg.get("RxProc", [])):
            AuthPduHeaderLength = RxPdu.get("AuthPduHeaderLength", 0)
            fv = GetFV(cfg, RxPdu["FreshnessValueId"])
            C.write("  {\n")
            C.write("    &SecOC_RxPduProcContexts[%s],\n" % (idx))
            C.write("    SecOC_RxPduProc_%s_Buffer,\n" % (RxPdu["name"]))
            C.write("    CSM_JOB_%s,\n" % (RxPdu["RxAuthServiceConfigRef"]))
            C.write("    PDUR_%s,  /* UpRxPduId */\n" % (RxPdu["name"]))
            C.write("    ARRAY_SIZE(SecOC_RxPduProc_%s_Buffer),\n" % (RxPdu["name"]))
            if RxPdu.get("UseAuthDataFreshness", False):
                C.write("    %su, /* AuthDataFreshnessLen */\n" % (RxPdu["AuthDataFreshnessLen"]))
                C.write("    %su, /* AuthDataFreshnessStartPosition */\n" % (RxPdu["AuthDataFreshnessStartPosition"]))
            else:
                C.write("    0u, /* AuthDataFreshnessLen */\n")
                C.write("    0u, /* AuthDataFreshnessStartPosition */\n")
            C.write("    %su, /* AuthInfoLength */\n" % (RxPdu["AuthInfoLength"]))
            C.write("    %su, /* AuthInfoTruncLength */\n" % (RxPdu["AuthInfoTruncLength"]))
            C.write("    %su, /* DataId */\n" % (RxPdu["DataId"]))
            C.write("    FV_%s, /* FreshnessValueId */\n" % (toMacro(RxPdu["FreshnessValueId"])))
            C.write("    %su, /* FreshnessValueLength */\n" % (fv["length"]))
            C.write("    %su, /* FreshnessValueTruncLength */\n" % (RxPdu["FreshnessValueTruncLength"]))
            C.write("    %su, /* AuthPduHeaderLength */\n" % (AuthPduHeaderLength))
            C.write("    %s, /* UseAuthDataFreshness */\n" % (str(RxPdu.get("UseAuthDataFreshness", False)).upper()))
            C.write("  },\n")
        C.write("};\n\n")
    C.write("const SecOC_ConfigType SecOC_Config = {\n")
    C.write("  SecOC_FreshnessValues,\n")
    if len(cfg.get("TxProc", [])) > 0:
        C.write("  SecOC_TxPduProcs,\n")
    else:
        C.write("  NULL,\n")
    if len(cfg.get("RxProc", [])) > 0:
        C.write("  SecOC_RxPduProcs,\n")
    else:
        C.write("  NULL,\n")
    if len(cfg.get("TxProc", [])) > 0:
        C.write("  ARRAY_SIZE(SecOC_TxPduProcs),\n")
    else:
        C.write("  0,\n")
    if len(cfg.get("RxProc", [])) > 0:
        C.write("  ARRAY_SIZE(SecOC_RxPduProcs),\n")
    else:
        C.write("  0,\n")
    C.write("  ARRAY_SIZE(SecOC_FreshnessValues),\n")
    C.write("};\n\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    for fv in cfg["FreshnessValueManager"]:
        if fv.get("GenDummy", False):
            C.write(
                "Std_ReturnType __weak SecOC_GetFreshness_%s(uint8_t *FreshnessValue,uint32_t *FreshnessValueLength) {\n"
                % (fv["name"])
            )
            C.write("  Std_ReturnType ret = E_OK;\n")
            C.write("  static uint16_t fv%s = 0;\n" % (fv["name"]))
            C.write("  uint16_t fv = fv%s / 2;\n" % (fv["name"]))
            C.write("  (void)memset(FreshnessValue, 0, *FreshnessValueLength / 8);\n")
            C.write("  FreshnessValue[0] = (fv >> 8)&0xFFu;\n")
            C.write("  FreshnessValue[1] = fv&0xFFu;\n")
            C.write("  fv%s++;\n" % (fv["name"]))
            C.write("  return ret;\n")
            C.write("}\n\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_Csm(cfg, dir)
    return ["%s/SecOC_Cfg.c" % (dir)]
