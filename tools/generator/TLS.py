# SSAS - Simple Smart Automotive Software
# Copyright (C) 2025 Parai Wang <parai@foxmail.com>

import pprint
import os
import json
from .helper import *

__all__ = ["Gen"]


def LoadPem(dir, path):
    p = path
    if not os.path.exists(p):
        p = os.path.join(dir, path)
    with open(p) as f:
        text = "\\\n"
        for l in f.readlines():
            l = l.strip()
            text += '  "%s\\r\\n" \\\n' % (l)
    return text


def Gen_TLS(cfg, dir):
    H = open("%s/TLS_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef TLS_CFG_H\n")
    H.write("#define TLS_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    servers = cfg.get("servers", [])
    idx = 0
    srvidx = 0
    for server in servers:
        if "repeat" in server:
            for i in range(server["repeat"]):
                H.write("#define TLS_SERVER_%s%s %su\n" % (server["name"], i, srvidx))
                H.write("#define TLS_RX_PID_%s%s %su\n" % (server["name"], i, idx))
                H.write("#define TLS_TX_PID_%s%s %su\n" % (server["name"], i, idx))
                srvidx = srvidx + 1
                idx = idx + 1
        else:
            H.write("#define TLS_SERVER_%s %su\n" % (server["name"], srvidx))
            H.write("#define TLS_RX_PID_%s %su\n" % (server["name"], idx))
            H.write("#define TLS_TX_PID_%s %su\n" % (server["name"], idx))
            srvidx = srvidx + 1
            idx = idx + 1
    H.write("#define TLS_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + TLS_MAIN_FUNCTION_PERIOD - 1u) / TLS_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("%s#define TLS_USE_PB_CONFIG\n\n" % ("" if cfg.get("UsePostBuildConfig", False) else "// "))
    headerMaxLen = 1
    for srv in servers:
        up = srv["up"]
        headerLen = 0
        if up in ["DoIP", "SD", "SOMEIP"]:
            headerLen = 8
        if headerLen > headerMaxLen:
            headerMaxLen = headerLen
    H.write("\n")
    H.write("#define TLS_HEADER_MAX_LEN %su\n" % (headerMaxLen))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* TLS_CFG_H */\n")
    H.close()

    C = open("%s/TLS_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "TLS.h"\n')
    C.write('#include "TLS_Cfg.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write('#include "TLS_Priv.h"\n')
    if any(server["up"] == "DoIP" for server in servers):
        C.write('#include "DoIP_Cfg.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    ups = []
    for idx, srv in enumerate(servers):
        up = srv["up"]
        if up not in ups:
            ups.append(up)
            C.write("void %s_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode);\n" % (up))
            C.write(
                "Std_ReturnType %s_HeaderIndication(PduIdType id, "
                "const PduInfoType *info, uint32_t *payloadLength);\n" % (up)
            )
            C.write("void %s_RxIndication(PduIdType id, const PduInfoType *info);\n" % (up))
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    ups = []
    for idx, srv in enumerate(servers):
        up = srv["up"]
        if up not in ups:
            ups.append(up)
            C.write("static const SoAd_InterfaceType SoAd_%s_IF = {\n" % (up))
            C.write("  %s_HeaderIndication,\n" % (up))
            C.write("  %s_RxIndication,\n" % (up))
            C.write("  NULL,\n")
            C.write("};\n\n")
    if len(servers) > 0:
        C.write(
            "static TLS_ServerContextType TLS_ServerContexts[%s];\n\n"
            % (sum([srv.get("repeat", 1) for srv in servers]))
        )
        for srv in servers:
            text = LoadPem(os.path.abspath(dir + "/.."), srv["ServerCerts"])
            C.write("static const char TLS_%s_ServerCerts[] = %s;\n\n" % (srv["name"], text))
            text = LoadPem(os.path.abspath(dir + "/.."), srv["CasCerts"])
            C.write("static const char TLS_%s_CasCerts[] = %s;\n\n" % (srv["name"], text))
            text = LoadPem(os.path.abspath(dir + "/.."), srv["ServerKey"])
            C.write("static const char TLS_%s_ServerKey[] = %s;\n\n" % (srv["name"], text))
        C.write("static const TLS_ServerConfigType TLS_ServerConfigs[] = {\n")
        idx = 0
        for srv in servers:
            up = srv["up"]
            headerLen = 0
            if up in ["DoIP", "SD", "SOMEIP"]:
                headerLen = 8
            if "repeat" in srv:
                for i in range(srv["repeat"]):
                    C.write("  { /* %s %s */\n" % (srv["name"], i))
                    C.write("    &TLS_ServerContexts[%s],\n" % (idx))
                    C.write('    "%s%s",\n' % (srv["name"], i))
                    C.write("    &SoAd_%s_IF,\n" % (up))
                    C.write("    &%s_SoConModeChg,\n" % (up))
                    C.write("    (const uint8_t*)TLS_%s_ServerCerts,\n" % (srv["name"]))
                    C.write("    (const uint8_t*)TLS_%s_CasCerts,\n" % (srv["name"]))
                    C.write("    (const uint8_t*)TLS_%s_ServerKey,\n" % (srv["name"]))
                    C.write("    sizeof(TLS_%s_ServerCerts),\n" % (srv["name"]))
                    C.write("    sizeof(TLS_%s_CasCerts),\n" % (srv["name"]))
                    C.write("    sizeof(TLS_%s_ServerKey),\n" % (srv["name"]))
                    C.write("    SOAD_SOCKID_%s%s, /* SoConId */\n" % (srv["SoConId"], i))
                    C.write("    SOAD_TX_PID_%s%s, /* TxPduId */\n" % (srv["SoConId"], i))
                    C.write("    %s%s, /* RxPduId */\n" % (srv["RxPduId"], i))
                    C.write("    %s, /* headerLen */\n" % (headerLen))
                    C.write("  },\n")
                    idx += 1
            else:
                C.write("  { /* %s */\n" % (srv["name"]))
                C.write("    &TLS_ServerContexts[%s],\n" % (idx))
                C.write('    "%s",\n' % (srv["name"]))
                C.write("    &SoAd_%s_IF,\n" % (up))
                C.write("    &%s_SoConModeChg,\n" % (up))
                C.write("    (const uint8_t*)TLS_%s_ServerCerts,\n" % (srv["name"]))
                C.write("    (const uint8_t*)TLS_%s_CasCerts,\n" % (srv["name"]))
                C.write("    (const uint8_t*)TLS_%s_ServerKey,\n" % (srv["name"]))
                C.write("    sizeof(TLS_%s_ServerCerts),\n" % (srv["name"]))
                C.write("    sizeof(TLS_%s_CasCerts),\n" % (srv["name"]))
                C.write("    sizeof(TLS_%s_ServerKey),\n" % (srv["name"]))
                C.write("    %su, /* MaxSize */\n" % (srv.get("MaxSize", 512)))
                C.write("    SOAD_SOCKID_%s, /* SoConId */\n" % (srv["SoConId"]))
                C.write("    SOAD_TX_PID_%s, /* TxPduId */\n" % (srv["SoConId"]))
                C.write("    %s, /* RxPduId */\n" % (srv["RxPduId"]))
                C.write("    %s, /* headerLen */\n" % (headerLen))
                C.write("  },\n")
                idx += 1
        C.write("};\n\n")
    C.write("static TLS_TxPduIdType TLS_TxPduIds[] = {\n")
    for srv in servers:
        if "repeat" in srv:
            for idx in range(srv["repeat"]):
                C.write("  { /* %s %s */\n" % (srv["name"], idx))
                C.write("    TLS_SERVER_%s%s,\n" % (srv["name"], idx))
                C.write("    TRUE\n")
                C.write("  },\n")
        else:
            C.write("  { /* %s */\n" % (srv["name"]))
            C.write("    TLS_SERVER_%s,\n" % (srv["name"]))
            C.write("    TRUE\n")
            C.write("  },\n")
    C.write("};\n\n")
    C.write("const TLS_ConfigType TLS_Config = {\n")
    if len(servers) > 0:
        C.write("  TLS_ServerConfigs,\n")
    else:
        C.write("  NULL,\n")
    C.write("  TLS_TxPduIds,\n")
    if len(servers) > 0:
        C.write("  ARRAY_SIZE(TLS_ServerConfigs),\n")
    else:
        C.write("  0,\n")
    C.write("  ARRAY_SIZE(TLS_TxPduIds),\n")
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_TLS(cfg, dir)
    return ["%s/TLS_Cfg.c" % (dir)]
