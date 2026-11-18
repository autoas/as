# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_Macros(cfg, H):
    mps = cfg["Clusters"]
    mps.sort(key=lambda x: eval(str(x["Size"])))
    MaxSize = 0
    for mp in mps:
        tsz = eval(str(mp["Size"]))
        H.write("#define MEMPOOL_%s_%s_SIZE %s\n" % (cfg["Name"].upper(), mp["Name"].upper(), tsz))
        if tsz > MaxSize:
            MaxSize = tsz
    H.write("#define MEMPOOL_%s_MAX_SIZE %s\n\n" % (cfg["Name"].upper(), MaxSize))


def Gen_Defs(cfg, C):
    mps = cfg["Clusters"]
    mps.sort(key=lambda x: eval(str(x["Size"])))
    for mp in mps:
        tsz = eval(str(mp["Size"])) * eval(str(mp["Number"]))
        C.write(
            "static uint32_t MC_%s_%s_Buffer[(%s+sizeof(uint32_t) - 1)/sizeof(uint32_t)];\n"
            % (cfg["Name"], mp["Name"], tsz)
        )
    C.write("static const mem_cluster_cfg_t MC_%sCfgs[] = {\n" % (cfg["Name"]))
    for mp in mps:
        C.write("  {\n")
        C.write("    (uint8_t*)MC_%s_%s_Buffer,\n" % (cfg["Name"], mp["Name"]))
        C.write("    %s,\n" % (mp["Size"]))
        C.write("    %s,\n" % (mp["Number"]))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static mempool_t MC_%sPools[%s];\n" % (cfg["Name"], len(mps)))
    C.write("static const mem_cluster_t MC_%s = {\n" % (cfg["Name"]))
    C.write("  MC_%sPools,\n" % (cfg["Name"]))
    C.write("  MC_%sCfgs,\n" % (cfg["Name"]))
    C.write("  %s,\n" % (len(mps)))
    C.write("};\n\n")


def Gen_MC(cfg, dir):
    H = open("%s/%sMem.h" % (dir, cfg["Name"]), "w")
    GenHeader(H)
    H.write("#ifndef %s_MEM_H\n" % (cfg["Name"].upper()))
    H.write("#define %s_MEM_H\n" % (cfg["Name"].upper()))
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "mempool.h"\n')
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    Gen_Macros(cfg, H)
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("void %s_MemInit(void);\n" % (cfg["Name"]))
    H.write("void* %s_MemAlloc(uint32_t size);\n" % (cfg["Name"]))
    H.write("void* %s_MemGet(uint32_t* size);\n" % (cfg["Name"]))
    H.write("void %s_MemFree(void* buffer);\n" % (cfg["Name"]))
    H.write("#endif /* %s_MEM_H */\n" % (cfg["Name"].upper()))
    H.close()

    C = open("%s/%sMem.c" % (dir, cfg["Name"]), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "%sMem.h"\n' % (cfg["Name"]))
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    Gen_Defs(cfg, C)
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write("void %s_MemInit(void) {\n" % (cfg["Name"]))
    C.write("  mc_init(&MC_%s);\n" % (cfg["Name"]))
    C.write("}\n\n")
    C.write("void* %s_MemAlloc(uint32_t size) {\n" % (cfg["Name"]))
    C.write("  return (void*)mc_alloc(&MC_%s, size);\n" % (cfg["Name"]))
    C.write("}\n\n")
    C.write("void* %s_MemGet(uint32_t *size) {\n" % (cfg["Name"]))
    C.write("  return (void*)mc_get(&MC_%s, size);\n" % (cfg["Name"]))
    C.write("}\n\n")
    C.write("void %s_MemFree(void* buffer) {\n" % (cfg["Name"]))
    C.write("  mc_free(&MC_%s, (uint8_t*)buffer);\n" % (cfg["Name"]))
    C.write("}\n\n")

    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_MC(cfg, dir)
    return ["%s/%sMem.c" % (dir, cfg["Name"])]
