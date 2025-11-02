# SSAS - Simple Smart Automotive Software
# Copyright (C) 2025 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_Mirror(cfg, dir):
    H = open("%s/Mirror_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef MIRROR_CFG_H\n")
    H.write("#define MIRROR_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef MIRROR_MAIN_FUNCTION_PERIOD\n")
    H.write("#define MIRROR_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 1)))
    H.write("#endif\n")
    H.write("#define MIRROR_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  (((x) + MIRROR_MAIN_FUNCTION_PERIOD - 1u) / MIRROR_MAIN_FUNCTION_PERIOD)\n\n")
    H.write("/* Source Network CAN IDs */\n")
    idx = 0
    SourceNetworkCans = cfg.get("SourceNetworkCan", [])
    if len(SourceNetworkCans) > 0:
        H.write("#define MIRROR_USE_SOURCE_CAN\n")
    for network in SourceNetworkCans:
        H.write("#define MIRROR_SRC_NT_CAN_%s %su\n" % (toMacro(network["name"]), idx))
        idx += 1
    SourceNetworkLins = cfg.get("SourceNetworkLin", [])
    H.write("\n/* Source Network LIN IDs */\n")
    if len(SourceNetworkLins) > 0:
        H.write("#define MIRROR_USE_SOURCE_LIN\n")
    for network in SourceNetworkLins:
        H.write("#define MIRROR_SRC_NT_LIN_%s %su\n" % (toMacro(network["name"]), idx))
        idx += 1
    DestNetworkCans = cfg.get("DestNetworkCan", [])
    H.write("\n/* Dest Network CAN IDs */\n")
    if len(DestNetworkCans) > 0:
        H.write("#define MIRROR_USE_DEST_CAN\n")
    for network in DestNetworkCans:
        H.write("#define MIRROR_DST_NT_CAN_%s %su\n" % (toMacro(network["name"]), idx))
        H.write("#define MIRROR_%s %su\n" % (toMacro(network["TxPduId"]), idx))
        idx += 1
    DestNetworkIps = cfg.get("DestNetworkIp", [])
    H.write("\n/* Dest Network IP IDs */\n")
    if len(DestNetworkIps) > 0:
        H.write("#define MIRROR_USE_DEST_IP\n")
    for network in DestNetworkIps:
        H.write("#define MIRROR_DST_NT_IP_%s %su\n" % (toMacro(network["name"]), idx))
        idx += 1
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* MIRROR_CFG_H */\n")
    H.close()

    C = open("%s/Mirror_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Mirror.h"\n')
    C.write('#include "Mirror_Cfg.h"\n')
    C.write('#include "Mirror_Priv.h"\n')

    if len(DestNetworkIps) > 0:
        C.write('#include "SoAd_Cfg.h"\n')
    if len(DestNetworkCans) > 0:
        C.write("#ifdef USE_PDUR\n")
        C.write('#include "PduR_Cfg.h"\n')
        C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for network in SourceNetworkCans:
        staticFilters = network.get("StaticFilters", [])
        MaxDynamicFilters = network.get("MaxDynamicFilters", 128)
        if len(staticFilters) > 0:
            C.write(
                "static const Mirror_SourceCanFilterType Mirror_SourceCanStaticFilters_%s[]={\n" % (network["name"])
            )
            for ft in staticFilters:
                C.write("  {\n")
                if ft["type"] == "range":
                    C.write("    { { %s, %s } }, MIRROR_SOURCE_CAN_FILTER_RANGE\n" % (ft["lower"], ft["upper"]))
                elif ft["type"] == "mask":
                    C.write("    { { %s, %s } }, MIRROR_SOURCE_CAN_FILTER_MASK\n" % (ft["code"], ft["mask"]))
                else:
                    raise
                C.write("  },\n")
            C.write("};\n\n")
        if MaxDynamicFilters > 0:
            C.write(
                "static Mirror_SourceCanFilterType Mirror_SourceCanDynamicFilters_%s[%s];\n\n"
                % (network["name"], MaxDynamicFilters)
            )
        C.write(
            "static boolean Mirror_SourceNetworkCanFilterStatus_%s[%s];\n\n"
            % (network["name"], MaxDynamicFilters + len(staticFilters))
        )
        SingleIdMappings = network.get("SingleIdMappings", [])
        if len(SingleIdMappings) > 0:
            C.write(
                "static const Mirror_SourceCanSingleIdMappingType Mirror_SourceCanSingleIdMappings_%s[] = {\n"
                % (network["name"])
            )
            for sim in SingleIdMappings:
                C.write("  {\n")
                C.write("    %su, /* SourceCanId */\n" % (sim["SourceCanId"]))
                C.write("    %su, /* SourceCanId */\n" % (sim["DestCanId"]))
                C.write("  },\n")
            C.write("};\n\n")
        MaskBasedIdMappings = network.get("MaskBasedIdMappings", [])
        if len(MaskBasedIdMappings) > 0:
            C.write(
                "static const Mirror_SourceCanMaskBasedIdMappingType Mirror_SourceCanMaskBasedIdMappings_%s[] = {\n"
                % (network["name"])
            )
            for mim in MaskBasedIdMappings:
                C.write("  {\n")
                C.write("    %su, /* DestBaseId */\n" % (mim["DestBaseId"]))
                C.write("    %su, /* SourceCanIdCode */\n" % (mim["SourceCanIdCode"]))
                C.write("    %su, /* SourceCanIdMask */\n" % (mim["SourceCanIdMask"]))
                C.write("  },\n")
            C.write("};\n\n")
    if len(SourceNetworkCans) > 0:
        C.write(
            "static Mirror_SourceNetworkCanContextType Mirror_SourceNetworkCanContexts[%s];\n\n"
            % (len(SourceNetworkCans))
        )
        C.write("static const Mirror_SourceNetworkCanType Mirror_SourceNetworkCans[] = {\n")
        for idx, network in enumerate(SourceNetworkCans):
            staticFilters = network.get("StaticFilters", [])
            MaxDynamicFilters = network.get("MaxDynamicFilters", 128)
            SingleIdMappings = network.get("SingleIdMappings", [])
            MaskBasedIdMappings = network.get("MaskBasedIdMappings", [])
            C.write("  {\n")
            if len(staticFilters) > 0:
                C.write("    Mirror_SourceCanStaticFilters_%s,\n" % (network["name"]))
            else:
                C.write("    NULL, /* StaticFilters */\n")
            if MaxDynamicFilters > 0:
                C.write("    Mirror_SourceCanDynamicFilters_%s,\n" % (network["name"]))
            else:
                C.write("    NULL, /* DynamicFilters */\n")
            C.write("    &Mirror_SourceNetworkCanContexts[%s],\n" % (idx))
            if len(MaskBasedIdMappings) > 0:
                C.write("    Mirror_SourceCanMaskBasedIdMappings_%s,\n" % (network["name"]))
            else:
                C.write("    NULL, /* MaskBasedIdMappings */\n")
            if len(SingleIdMappings) > 0:
                C.write("    Mirror_SourceCanSingleIdMappings_%s,\n" % (network["name"]))
            else:
                C.write("    NULL, /* SingleIdMappings */\n")
            C.write("    Mirror_SourceNetworkCanFilterStatus_%s,\n" % (network["name"]))
            C.write("    %su, /* NumMaskBasedIdMappings */\n" % (len(MaskBasedIdMappings)))
            C.write("    %su, /* NumSingleIdMappings */\n" % (len(SingleIdMappings)))
            C.write("    %su, /* NumStaticFilters */\n" % (len(staticFilters)))
            C.write("    %su, /* MaxDynamicFilters */\n" % (MaxDynamicFilters))
            C.write("    %s, /* ControllerId */\n" % (network.get("ControllerId", idx)))
            C.write("    %s, /* NetworkId */\n" % (network.get("NetworkId", idx)))
            C.write("  },\n")
        C.write("};\n\n")
    for network in SourceNetworkLins:
        staticFilters = network.get("StaticFilters", [])
        MaxDynamicFilters = network.get("MaxDynamicFilters", 32)
        if len(staticFilters) > 0:
            C.write(
                "static const Mirror_SourceLinFilterType Mirror_SourceLinStaticFilters_%s[]={\n" % (network["name"])
            )
            for ft in staticFilters:
                C.write("  {\n")
                if ft["type"] == "range":
                    C.write("    { { %s, %s } }, MIRROR_SOURCE_LIN_FILTER_RANGE\n" % (ft["lower"], ft["upper"]))
                elif ft["type"] == "mask":
                    C.write("    { { %s, %s } }, MIRROR_SOURCE_LIN_FILTER_MASK\n" % (ft["code"], ft["mask"]))
                else:
                    raise
                C.write("  },\n")
            C.write("};\n\n")
        if MaxDynamicFilters > 0:
            C.write(
                "static Mirror_SourceLinFilterType Mirror_SourceLinDynamicFilters_%s[%s];\n\n"
                % (network["name"], MaxDynamicFilters)
            )
        C.write(
            "static boolean Mirror_SourceNetworkLinFilterStatus_%s[%s];\n\n"
            % (network["name"], MaxDynamicFilters + len(staticFilters))
        )
        SourceLinToCanIdMappings = network.get("SourceLinToCanIdMappings", [])
        if len(SourceLinToCanIdMappings) > 0:
            C.write(
                "static const Mirror_SourceLinToCanIdMappingType Mirror_SourceLinToCanIdMapping_%s[] = {\n"
                % (network["name"])
            )
            for sim in SourceLinToCanIdMappings:
                C.write("  {\n")
                C.write("    %su, /* CanId */\n" % (sim["CanId"]))
                C.write("    %su, /* LinId */\n" % (sim["LinId"]))
                C.write("  },\n")
            C.write("};\n\n")
    if len(SourceNetworkLins) > 0:
        C.write(
            "static Mirror_SourceNetworkLinContextType Mirror_SourceNetworkLinContexts[%s];\n\n"
            % (len(SourceNetworkLins))
        )
        C.write("static const Mirror_SourceNetworkLinType Mirror_SourceNetworkLins[] = {\n")
        for idx, network in enumerate(SourceNetworkLins):
            staticFilters = network.get("StaticFilters", [])
            MaxDynamicFilters = network.get("MaxDynamicFilters", 128)
            SourceLinToCanIdMappings = network.get("SourceLinToCanIdMappings", [])
            C.write("  {\n")
            if len(staticFilters) > 0:
                C.write("    Mirror_SourceLinStaticFilters_%s,\n" % (network["name"]))
            else:
                C.write("    NULL, /* StaticFilters */\n")
            if MaxDynamicFilters > 0:
                C.write("    Mirror_SourceLinDynamicFilters_%s,\n" % (network["name"]))
            else:
                C.write("    NULL, /* DynamicFilters */\n")
            C.write("    &Mirror_SourceNetworkLinContexts[%s],\n" % (idx))
            if len(SourceLinToCanIdMappings) > 0:
                C.write("    Mirror_SourceLinToCanIdMapping_%s,\n" % (network["name"]))
            else:
                C.write("    NULL, /* SourceLinToCanIdMappings */\n")
            C.write("    Mirror_SourceNetworkLinFilterStatus_%s,\n" % (network["name"]))
            C.write("    %su, /* SourceLinToCanBaseId */\n" % (network.get("SourceLinToCanBaseId", 0)))
            C.write("    %su, /* NumSourceLinToCanIdMappings */\n" % (len(SourceLinToCanIdMappings)))
            C.write("    %su, /* NumStaticFilters */\n" % (len(staticFilters)))
            C.write("    %su, /* MaxDynamicFilters */\n" % (MaxDynamicFilters))
            C.write("    %s, /* ControllerId */\n" % (network.get("ControllerId", idx)))
            C.write("    %s, /* NetworkId */\n" % (network.get("NetworkId", idx)))
            C.write("  },\n")
        C.write("};\n\n")
    if len(DestNetworkCans) > 0:
        C.write(
            "static Mirror_DestNetworkCanContextType Mirror_DestNetworkCanContexts[%s];\n\n" % (len(DestNetworkCans))
        )
        C.write(
            "static Mirror_RingContextType Mirror_DestNetworkCanRingBufferContexts[%s];\n\n" % (len(DestNetworkCans))
        )
    for idx, network in enumerate(DestNetworkCans):
        DestQueueSize = network.get("DestQueueSize", 64) * 2
        # must be power of 2
        assert (DestQueueSize > 0) and ((DestQueueSize & (DestQueueSize - 1)) == 0)
        C.write(
            "static Mirror_DataElementType NtCanRingBufferDataElements%s[%s];\n\n" % (network["name"], DestQueueSize)
        )
        C.write("static const Mirror_RingBufferType NtCanRingBuffer%s = {\n" % (network["name"]))
        C.write("  &Mirror_DestNetworkCanRingBufferContexts[%s],\n" % (idx))
        C.write("  NtCanRingBufferDataElements%s,\n" % (network["name"]))
        C.write("  ARRAY_SIZE(NtCanRingBufferDataElements%s),\n" % (network["name"]))
        C.write("};\n\n")
    if len(DestNetworkCans) > 0:
        C.write("static const Mirror_DestNetworkCanType Mirror_DestNetworkCans[] = {")
        for idx, network in enumerate(DestNetworkCans):
            C.write("  {\n")
            C.write("    &Mirror_DestNetworkCanContexts[%s],\n" % (idx))
            C.write("    &NtCanRingBuffer%s,\n" % (network["name"]))
            C.write("    %s, /* MirrorStatusCanId */\n" % (network.get("MirrorStatusCanId", "0xFFFFFFFFul")))
            C.write("    #ifdef USE_PDUR\n")
            C.write("    PDUR_%s, /* TxPduId */\n" % (network["TxPduId"]))
            C.write("    #else\n")
            C.write("    0u, /* TxPduId */\n")
            C.write("    #endif\n")
            C.write("  }\n")
        C.write("};\n\n")
    if len(DestNetworkIps) > 0:
        C.write("static Mirror_DestNetworkIpContextType Mirror_DestNetworkIpContexts[%s];\n\n" % (len(DestNetworkIps)))
    for idx, network in enumerate(DestNetworkIps):
        DestQueueSize = network.get("DestQueueSize", 2)
        DestBufferSize = network.get("DestBufferSize", 1400)
        # must be power of 2
        assert (DestQueueSize > 0) and ((DestQueueSize & (DestQueueSize - 1)) == 0)
        C.write(
            "static uint8_t NtIpRingDestBufferData%s[%s][%s];\n\n" % (network["name"], DestQueueSize, DestBufferSize)
        )
        C.write("static uint16_t NtIpRingDestBufferOffset%s[%s];\n\n" % (network["name"], DestQueueSize))
        C.write("static const Mirror_DestBufferType Mirror_DestBuffers%s[] = {\n" % (network["name"]))
        for i in range(DestQueueSize):
            C.write("  {\n")
            C.write("    NtIpRingDestBufferData%s[%s],\n" % (network["name"], i))
            C.write("    &NtIpRingDestBufferOffset%s[%s],\n" % (network["name"], i))
            C.write("    %s,\n" % (DestBufferSize))
            C.write("  },\n")
        C.write("};\n\n")
    if len(DestNetworkIps) > 0:
        C.write("static const Mirror_DestNetworkIpType Mirror_DestNetworkIps[] = {")
        for idx, network in enumerate(DestNetworkIps):
            C.write("  {\n")
            C.write("    &Mirror_DestNetworkIpContexts[%s],\n" % (idx))
            C.write("    Mirror_DestBuffers%s,\n" % (network["name"]))
            C.write(
                "    MIRROR_CONVERT_MS_TO_MAIN_CYCLES(%su), /* MirrorDestTransmissionDeadline */\n"
                % (network.get("MirrorDestTransmissionDeadline", 655))
            )
            C.write("    %s, /* TxPduId */\n" % (network["SoAd"]))
            C.write("    %su, /* NumDestBuffers */\n" % (network.get("DestQueueSize", 2)))
            C.write("  }\n")
        C.write("};\n\n")
    if len(SourceNetworkCans) > 0:
        MaxControllerId = 0
        CanCtrlIdToNetworkMaps = {}
        for idx, network in enumerate(SourceNetworkCans):
            ControllerId = network.get("ControllerId", idx)
            if ControllerId > MaxControllerId:
                MaxControllerId = ControllerId
            CanCtrlIdToNetworkMaps[ControllerId] = network, idx
        C.write("static const NetworkHandleType CanCtrlIdToNetworkMaps[] = {\n")
        for ControllerId in range(MaxControllerId + 1):
            if ControllerId in CanCtrlIdToNetworkMaps:
                network, idx = CanCtrlIdToNetworkMaps[ControllerId]
                C.write(
                    "  %su, /* ControllerId = %s -> MIRROR_SRC_NT_CAN_%s */\n"
                    % (idx, ControllerId, toMacro(network["name"]))
                )
            else:
                C.write("  MIRROR_INVALID_NETWORK,\n")
        C.write("};\n\n")
    if len(SourceNetworkLins) > 0:
        MaxControllerId = 0
        LinCtrlIdToNetworkMaps = {}
        for idx, network in enumerate(SourceNetworkLins):
            ControllerId = network.get("ControllerId", idx)
            if ControllerId > MaxControllerId:
                MaxControllerId = ControllerId
            LinCtrlIdToNetworkMaps[ControllerId] = network, idx
        C.write("static const NetworkHandleType LinCtrlIdToNetworkMaps[] = {\n")
        for ControllerId in range(MaxControllerId + 1):
            if ControllerId in LinCtrlIdToNetworkMaps:
                network, idx = LinCtrlIdToNetworkMaps[ControllerId]
                C.write(
                    "  %su, /* ControllerId = %s -> MIRROR_SRC_NT_LIN_%s */\n"
                    % (idx, ControllerId, toMacro(network["name"]))
                )
            else:
                C.write("  MIRROR_INVALID_NETWORK,\n")
        C.write("};\n\n")
    C.write("const Mirror_ComMChannelMapType Mirror_ComChannelMaps[] = {\n")
    for idx, network in enumerate(SourceNetworkCans):
        C.write("  {\n")
        C.write("    /* MIRROR_SRC_NT_CAN_%s */ %su,\n" % (toMacro(network["name"]), idx))
        C.write("    MIRROR_NT_CAN,\n")
        C.write("  },\n")
    for idx, network in enumerate(SourceNetworkLins):
        C.write("  {\n")
        C.write("    /* MIRROR_SRC_NT_LIN_%s */ %su,\n" % (toMacro(network["name"]), idx))
        C.write("    MIRROR_NT_LIN,\n")
        C.write("  },\n")
    for idx, network in enumerate(DestNetworkCans):
        C.write("  {\n")
        C.write("    /* MIRROR_DST_NT_CAN_%s */ %su,\n" % (toMacro(network["name"]), idx))
        C.write("    MIRROR_NT_CAN|MIRROR_NT_DEST,\n")
        C.write("  },\n")
    for idx, network in enumerate(DestNetworkIps):
        C.write("  {\n")
        C.write("    /* MIRROR_DST_NT_IP_%s */ %su,\n" % (toMacro(network["name"]), idx))
        C.write("    MIRROR_NT_ETHERNET|MIRROR_NT_DEST,\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("const Mirror_ConfigType Mirror_Config = {\n")
    if len(SourceNetworkCans) > 0:
        C.write("  Mirror_SourceNetworkCans,\n")
    else:
        C.write("  NULL, /* SourceNetworkCans */\n")
    if len(SourceNetworkLins) > 0:
        C.write("  Mirror_SourceNetworkLins,\n")
    else:
        C.write("  NULL, /* SourceNetworkLins */\n")
    if len(DestNetworkCans) > 0:
        C.write("  Mirror_DestNetworkCans,\n")
    else:
        C.write("  NULL, /* DestNetworkCans */\n")
    if len(DestNetworkIps) > 0:
        C.write("  Mirror_DestNetworkIps,\n")
    else:
        C.write("  NULL, /* DestNetworkIps */\n")
    if len(SourceNetworkCans) > 0:
        C.write("  CanCtrlIdToNetworkMaps,\n")
    else:
        C.write("  NULL, /* CanCtrlIdToNetworkMaps */\n")
    if len(SourceNetworkLins) > 0:
        C.write("  LinCtrlIdToNetworkMaps,\n")
    else:
        C.write("  NULL, /* LinCtrlIdToNetworkMaps */\n")
    C.write("  Mirror_ComChannelMaps,\n")
    C.write("  %su, /* NumOfSourceNetworkCans */\n" % (len(SourceNetworkCans)))
    C.write("  %su, /* NumOfSourceNetworkLins */\n" % (len(SourceNetworkLins)))
    C.write("  %su, /* NumOfDestNetworkCans */\n" % (len(DestNetworkCans)))
    C.write("  %su, /* NumOfDestNetworkIps */\n" % (len(DestNetworkIps)))
    if len(SourceNetworkCans) > 0:
        C.write("  ARRAY_SIZE(CanCtrlIdToNetworkMaps),\n")
    else:
        C.write("  0, /* SizeOfCanCtrlIdToNetworkMaps */\n")
    if len(SourceNetworkLins) > 0:
        C.write("  ARRAY_SIZE(LinCtrlIdToNetworkMaps),\n")
    else:
        C.write("  0, /* SizeOfLinCtrlIdToNetworkMaps */\n")
    C.write("  ARRAY_SIZE(Mirror_ComChannelMaps),\n")
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_Mirror(cfg, dir)
    return ["%s/Mirror_Cfg.c" % (dir)]
