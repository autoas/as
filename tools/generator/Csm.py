# SSAS - Simple Smart Automotive Software
# Copyright (C) 2025 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def GenMacGenPrimitive(C, Family, Mode, BitLen):
    ApiName = "_".join([Family, Mode, str(BitLen)])
    C.write(
        "static Std_ReturnType Csm_MacGenerateInit_%s("
        "void *AlgorithmContext, const uint8_t *AlgorithmKey, uint32_t AlgorithmKeyLength) {\n" % (ApiName)
    )
    C.write("  Std_ReturnType ret = E_OK;\n")
    C.write("  int result;\n")
    C.write("  mbedtls_cipher_context_t* pCtx = (mbedtls_cipher_context_t*)AlgorithmContext;\n")
    C.write(
        "  const mbedtls_cipher_info_t *pCipherInfo = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_%s_%s_%s);\n"
        % (Family, BitLen, Mode)
    )
    C.write("  mbedtls_cipher_init( pCtx );\n")
    C.write("  result = mbedtls_cipher_setup( pCtx, pCipherInfo );\n")
    C.write("  if (0 != result) {\n")
    C.write("    ret = E_NOT_OK;\n")
    C.write("  } else {\n")
    C.write("    result = mbedtls_cipher_cmac_starts( pCtx, AlgorithmKey, AlgorithmKeyLength * 8 );\n")
    C.write("    if (0 != result) {\n")
    C.write("      ret = E_NOT_OK;\n")
    C.write("    }\n")
    C.write("  }\n")
    C.write("  return ret;\n")
    C.write("}\n\n")

    C.write("static Std_ReturnType Csm_MacGenerateStart_%s(void *AlgorithmContext) {\n" % (ApiName))
    C.write("  Std_ReturnType ret = E_OK;\n")
    C.write("  int result;\n")
    C.write("  mbedtls_cipher_context_t* pCtx = (mbedtls_cipher_context_t*)AlgorithmContext;\n")
    C.write("  result = mbedtls_cipher_cmac_reset( pCtx );\n")
    C.write("  if (0 != result) {\n")
    C.write("    ret = E_NOT_OK;\n")
    C.write("  }\n")
    C.write("  return ret;\n")
    C.write("}\n\n")

    C.write(
        "static Std_ReturnType Csm_MacGenerateUpdate_%s("
        "void *AlgorithmContext, const uint8_t *data, uint32_t len) {\n" % (ApiName)
    )
    C.write("  Std_ReturnType ret = E_OK;\n")
    C.write("  int result;\n")
    C.write("  mbedtls_cipher_context_t* pCtx = (mbedtls_cipher_context_t*)AlgorithmContext;\n")
    C.write("  result = mbedtls_cipher_cmac_update( pCtx, data, len );\n")
    C.write("  if (0 != result) {\n")
    C.write("    ret = E_NOT_OK;\n")
    C.write("  }\n")
    C.write("  return ret;\n")
    C.write("}\n\n")

    C.write(
        "static Std_ReturnType Csm_MacGenerateFinish_%s("
        "void *AlgorithmContext, uint8_t *mac, uint32_t *len) {\n" % (ApiName)
    )
    C.write("  Std_ReturnType ret = E_OK;\n")
    C.write("  int result;\n")
    C.write("  mbedtls_cipher_context_t* pCtx = (mbedtls_cipher_context_t*)AlgorithmContext;\n")
    C.write("  result = mbedtls_cipher_cmac_finish( pCtx, mac );\n")
    C.write("  DET_VALIDATE(*len >= %su, 0x60, CSM_E_PARAM_HANDLE, return E_NOT_OK);\n" % (BitLen // 8))
    C.write("  if (0 != result) {\n")
    C.write("    ret = E_NOT_OK;\n")
    C.write("  } else {\n")
    C.write("    *len = %su;\n" % (BitLen // 8))
    C.write("  }\n")
    C.write("  return ret;\n")
    C.write("}\n\n")

    C.write("static void Csm_MacGenerateDeinit_%s(void *AlgorithmContext) {\n" % (ApiName))
    C.write("  mbedtls_cipher_context_t* pCtx = (mbedtls_cipher_context_t*)AlgorithmContext;\n")
    C.write("  mbedtls_cipher_free( pCtx );\n")
    C.write("}\n\n")

    C.write("static const Csm_MacGenPrimitiveType Csm_MacGenPrimitive_%s = {\n" % (ApiName))
    C.write("  Csm_MacGenerateInit_%s,\n" % (ApiName))
    C.write("  Csm_MacGenerateStart_%s,\n" % (ApiName))
    C.write("  Csm_MacGenerateUpdate_%s,\n" % (ApiName))
    C.write("  Csm_MacGenerateFinish_%s,\n" % (ApiName))
    C.write("  Csm_MacGenerateDeinit_%s,\n" % (ApiName))
    C.write("};\n\n")


def Gen_Csm(cfg, dir):
    H = open("%s/Csm_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef CSM_CFG_H\n")
    H.write("#define CSM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    for idx, job in enumerate(cfg["Jobs"]):
        H.write("#define CSM_JOB_%s %su\n" % (toMacro(job["name"]), idx))
    H.write("\n#ifndef CSM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define CSM_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define CSM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + CSM_MAIN_FUNCTION_PERIOD - 1u) / CSM_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("%s#define CSM_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* CSM_CFG_H */\n")
    H.close()

    C = open("%s/Csm_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Csm.h"\n')
    C.write('#include "Csm_Cfg.h"\n')
    C.write('#include "Csm_Priv.h"\n')
    C.write('#include "Det.h"\n')
    C.write('#include "mbedtls/cmac.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    for idx, mg in enumerate(cfg.get("MacGen", [])):
        C.write("#define CSM_MAC_GENERATE_%s %su\n" % (toMacro(mg["name"]), idx))
        C.write("#define CSM_MAC_VERIFY_%s %su\n" % (toMacro(mg["name"]), idx))
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    MGL = []
    for mg in cfg.get("MacGen", []):
        Family = mg["Family"]
        Mode = mg["Mode"]
        BitLen = len(mg["Key"]) * 8
        ks = "_".join([Family, Mode, str(BitLen)])
        if ks not in MGL:
            GenMacGenPrimitive(C, Family, Mode, BitLen)
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for mg in cfg.get("MacGen", []):
        C.write("static mbedtls_cipher_context_t Csm_MacGen_%s_Context;\n" % (mg["name"]))
        C.write("static const uint8_t Csm_MacGen_%s_Key[] = {%s};\n" % (mg["name"], ",".join(mg["Key"])))
        if mg.get("Verify", False):
            C.write("static uint8_t Csm_MacGen_%s_MacBuffer[%s];\n" % (mg["name"], BitLen // 8))
    C.write("static const Csm_MacGenerateConfigType Csm_MacGenerateConfigs[] = {\n")
    for mg in cfg.get("MacGen", []):
        Family = mg["Family"]
        Mode = mg["Mode"]
        BitLen = len(mg["Key"]) * 8
        ks = "_".join([Family, Mode, str(BitLen)])
        C.write("  { /* %s */\n" % (mg["name"]))
        C.write("    &Csm_MacGen_%s_Context,\n" % (mg["name"]))
        C.write("    Csm_MacGen_%s_Key,\n" % (mg["name"]))
        C.write("    &Csm_MacGenPrimitive_%s,\n" % (ks))
        if mg.get("Verify", False):
            C.write("    Csm_MacGen_%s_MacBuffer,\n" % (mg["name"]))
        else:
            C.write("    NULL,\n")
        C.write("    sizeof(Csm_MacGen_%s_Key),\n" % (mg["name"]))
        C.write("    CRYPTO_ALGOFAM_%s,\n" % (Family))
        C.write("    CRYPTO_ALGOMODE_%s,\n" % (Mode))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static const Csm_JobConfigType Csm_JobConfigs[] = {\n")
    for job in cfg["Jobs"]:
        C.write("  { /* %s */\n" % (job["name"]))
        C.write("    CSM_%s_%s,\n" % (job["ServiceType"], toMacro(job["AlgoRef"])))
        C.write("    CRYPTO_%s,\n" % (job["ServiceType"]))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("const Csm_ConfigType Csm_Config = {\n")
    C.write("  Csm_JobConfigs,\n")
    C.write("  Csm_MacGenerateConfigs,\n")
    C.write("  ARRAY_SIZE(Csm_JobConfigs),\n")
    C.write("  ARRAY_SIZE(Csm_MacGenerateConfigs),\n")
    C.write("};\n\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")

    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_Csm(cfg, dir)
    return ["%s/Csm_Cfg.c" % (dir)]
