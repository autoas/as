# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
from .helper import *
from .SomeIp_Proxy import *
from .SomeIp_Skeleton import *
from .SomeIp_ProxyC import *
from .SomeIp_SkeletonC import *
from .VSomeIp_Proxy import *
from .VSomeIp_Skeleton import *
from .SomeIpXf import *
from .E2E import Gen_E2E
import json

__all__ = ["Gen_SomeIp"]


def GetArgs(cfg):
    args = {}
    for arg in cfg.get("Args", []):
        args[arg["Name"]] = arg["Args"]
    return args


def Gen_SD(cfg, dir):
    H = open("%s/Sd_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef SD_CFG_H\n")
    H.write("#define SD_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    subscriberPoolSize = 8
    for service in cfg.get("Servers", []):
        listenNum = service.get("Listen", 1)
        subscriberPoolSize += listenNum * len(service.get("EventGroups", []))
    H.write(f"#define SD_EVENT_HANDLER_SUBSCRIBER_POOL_SIZE {subscriberPoolSize}\n\n")
    H.write("#define SD_RX_PID_MULTICAST 0\n")
    H.write("#define SD_RX_PID_UNICAST 0\n\n")
    for ID, service in enumerate(cfg.get("Servers", [])):
        mn = toMacro(service["Name"])
        H.write("#define SD_SERVER_SERVICE_HANDLE_ID_%s %s\n" % (mn, ID))
    for ID, service in enumerate(cfg.get("Clients", [])):
        mn = toMacro(service["Name"])
        H.write("#define SD_CLIENT_SERVICE_HANDLE_ID_%s %s\n" % (mn, ID))
    H.write("\n")
    ID = 0
    for service in cfg.get("Servers", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        mn = toMacro(service["Name"])
        for ge in service["EventGroups"]:
            H.write("#define SD_EVENT_HANDLER_%s_%s %s\n" % (mn, toMacro(ge["Name"]), ID))
            ID += 1
    ID = 0
    for service in cfg.get("Clients", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        mn = toMacro(service["Name"])
        for ge in service["EventGroups"]:
            H.write("#define SD_CONSUMED_EVENT_GROUP_%s_%s %s\n" % (mn, toMacro(ge["Name"]), ID))
            ID += 1
    H.write("#ifndef SD_MAIN_FUNCTION_PERIOD\n")
    H.write("#define SD_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define SD_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  (((x) + SD_MAIN_FUNCTION_PERIOD - 1u) / SD_MAIN_FUNCTION_PERIOD)\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* SD_CFG_H */\n")
    H.close()

    C = open("%s/Sd_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Sd.h"\n')
    C.write('#include "Sd_Cfg.h"\n')
    C.write('#include "Sd_Priv.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write('#include "SomeIp_Cfg.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    for ID, service in enumerate(cfg.get("Servers", [])):
        C.write(f"boolean Sd_ServerService{service['Name']}_CRMC(PduIdType pduID, uint8_t type, uint16_t serviceID,\n")
        C.write("                               uint16_t instanceID, uint8_t majorVersion, uint32_t minorVersion,\n")
        C.write("                               const Sd_ConfigOptionStringType *receivedConfigOptionPtrArray,\n")
        C.write("                               const Sd_ConfigOptionStringType *configuredConfigOptionPtrArray);\n")
    for service in cfg.get("Servers", []):
        for egroup in service["EventGroups"]:
            C.write(
                "void SomeIp_%s_%s_OnSubscribe(boolean isSubscribe, TcpIp_SockAddrType* RemoteAddr);\n"
                % (service["Name"], egroup["Name"])
            )
    for service in cfg.get("Clients", []):
        for ID, ge in enumerate(service["EventGroups"]):
            C.write("void SomeIp_%s_%s_OnSubscribeAck(boolean isSubscribe);\n" % (service["Name"], ge["Name"]))
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    if len(cfg.get("Servers", [])) > 0:
        C.write("static Sd_ServerTimerType Sd_ServerTimerDefault = {\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(100),  /* InitialOfferDelayMax */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(10),   /* InitialOfferDelayMin */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(200),  /* InitialOfferRepetitionBaseDelay */\n")
        C.write("  3,                                  /* InitialOfferRepetitionsMax */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(3000), /* OfferCyclicDelay */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(1500), /* RequestResponseMaxDelay */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(0),    /* RequestResponseMinDelay */\n")
        C.write("  5, /* TTL seconds */\n")
        C.write("};\n\n")

    if len(cfg.get("Clients", [])) > 0:
        C.write("static Sd_ClientTimerType Sd_ClientTimerDefault = {\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(100),  /* InitialFindDelayMax */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(10),   /* InitialFindDelayMin */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(200),  /* InitialFindRepetitionsBaseDelay */\n")
        C.write("  3,                                  /* InitialFindRepetitionsMax */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(1500), /* RequestResponseMaxDelay */\n")
        C.write("  SD_CONVERT_MS_TO_MAIN_CYCLES(0),    /* RequestResponseMinDelay */\n")
        C.write("  5, /* TTL seconds */\n")
        C.write("};\n\n")

    for service in cfg.get("Servers", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        mn = toMacro(service["Name"])
        C.write(
            "static Sd_EventHandlerContextType Sd_EventHandlerContext_%s[%d];\n"
            % (service["Name"], len(service["EventGroups"]))
        )
        C.write("static const Sd_EventHandlerType Sd_EventHandlers_%s[] = {\n" % (service["Name"]))
        for ID, ge in enumerate(service["EventGroups"]):
            C.write("  {\n")
            C.write("    SD_EVENT_HANDLER_%s_%s, /* HandleId */\n" % (mn, toMacro(ge["Name"])))
            C.write("    %s, /* EventGroupId */\n" % (ge["GroupId"]))
            if "Multicast" in ge:
                IpAddress, Port = ge["Multicast"].get("Addr", "0.0.0.0:0").split(":")
                a1, a2, a3, a4 = IpAddress.split(".")
                mcgn = toMacro("_".join([service["Name"], ge["Name"]]))
                C.write("    SOAD_SOCKID_SOMEIP_%s, /* MulticastEventSoConRef */\n" % (mcgn))
                C.write("    {%s, {%s, %s, %s, %s}}, /* MulticastEventAddr */\n" % (Port, a1, a2, a3, a4))
                C.write("    SOAD_TX_PID_SOMEIP_%s, /* MulticastTxPduId */\n" % (mcgn))
                C.write("    %s, /* MulticastThreshold */\n" % (ge.get("Threshold", 1)))
            else:
                C.write("    0, /* MulticastEventSoConRef */\n")
                C.write("    {0, {0, 0, 0, 0}}, /* MulticastEventAddr */\n")
                C.write("    -1, /* MulticastTxPduId */\n")
                C.write("    0, /* MulticastThreshold */\n")
            C.write("    &Sd_EventHandlerContext_%s[%d],\n" % (service["Name"], ID))
            C.write("   SomeIp_%s_%s_OnSubscribe,\n" % (service["Name"], ge["Name"]))
            C.write("  },\n")
        C.write("};\n\n")
    for service in cfg.get("Clients", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        mn = toMacro(service["Name"])
        C.write(
            "static Sd_ConsumedEventGroupContextType Sd_ConsumedEventGroupContext_%s[%d];\n"
            % (service["Name"], len(service["EventGroups"]))
        )
        C.write("static const Sd_ConsumedEventGroupType Sd_ConsumedEventGroups_%s[] = {\n" % (service["Name"]))
        for ID, ge in enumerate(service["EventGroups"]):
            C.write("  {\n")
            C.write("    FALSE, /* AutoRequire */\n")
            C.write("    SD_CONSUMED_EVENT_GROUP_%s_%s, /* HandleId */\n" % (mn, toMacro(ge["Name"])))
            C.write("    %s, /* EventGroupId */\n" % (ge["GroupId"]))
            if "Multicast" in ge:
                mcgn = toMacro("_".join([service["Name"], ge["Name"]]))
                C.write("    SOAD_SOCKID_SOMEIP_%s, /* MulticastEventSoConRef */\n" % (mcgn))
                C.write("    %s, /* MulticastThreshold */\n" % (ge.get("Threshold", 1)))
            else:
                C.write("    0, /* MulticastEventSoConRef */\n")
                C.write("    0, /* MulticastThreshold */\n")
            C.write("    &Sd_ConsumedEventGroupContext_%s[%d],\n" % (service["Name"], ID))
            C.write("    SomeIp_%s_%s_OnSubscribeAck,\n" % (service["Name"], ge["Name"]))
            C.write("  },\n")
        C.write("};\n\n")
    if len(cfg.get("Servers", [])) > 0:
        C.write("static Sd_ServerServiceContextType Sd_ServerService_Contexts[%s];\n\n" % (len(cfg.get("Servers", []))))
        C.write("static const Sd_ServerServiceType Sd_ServerServices[] = {\n")
    for ID, service in enumerate(cfg.get("Servers", [])):
        mn = toMacro(service["Name"])
        C.write("  {\n")
        C.write("    FALSE,                           /* AutoAvailable */\n")
        C.write("    SD_SERVER_SERVICE_HANDLE_ID_%s,  /* HandleId */\n" % (mn))
        C.write("    %s,                         /* ServiceId */\n" % (service["Service"]))
        C.write("    %s,                         /* InstanceId */\n" % (service["Instance"]))
        C.write("    0,                              /* MajorVersion */\n")
        C.write("    0,                              /* MinorVersion */\n")
        if "Unreliable" in service or service.get("Protocol", None) == "UDP":
            C.write("    SOAD_SOCKID_SOMEIP_%s,     /* SoConId */\n" % (mn))
            C.write("    TCPIP_IPPROTO_UDP,              /* ProtocolType */\n")
        else:
            C.write("    SOAD_SOCKID_SOMEIP_%s_SERVER,     /* SoConId */\n" % (mn))
            C.write("    TCPIP_IPPROTO_TCP,              /* ProtocolType */\n")
        C.write(f"    Sd_ServerService{service['Name']}_CRMC,         /* CapabilityRecordMatchCalloutRef */\n")
        C.write("    &Sd_ServerTimerDefault,\n")
        C.write("    &Sd_ServerService_Contexts[%s],\n" % (ID))
        C.write("    0, /* InstanceIndex */\n")
        if "EventGroups" not in service:
            C.write("    NULL,\n    0,\n")
        elif 0 == len(service["EventGroups"]):
            C.write("    NULL,\n    0,\n")
        else:
            C.write("    Sd_EventHandlers_%s,\n" % (service["Name"]))
            C.write("    ARRAY_SIZE(Sd_EventHandlers_%s),\n" % (service["Name"]))
        C.write("    SOMEIP_SSID_%s, /* SomeIpServiceId */\n" % (mn))
        C.write("  },\n")
    if len(cfg.get("Servers", [])) > 0:
        C.write("};\n\n")
    if len(cfg.get("Clients", [])) > 0:
        C.write("static Sd_ClientServiceContextType Sd_ClientService_Contexts[%s];\n\n" % (len(cfg.get("Clients", []))))
        C.write("static const Sd_ClientServiceType Sd_ClientServices[] = {\n")
    for ID, service in enumerate(cfg.get("Clients", [])):
        mn = toMacro(service["Name"])
        C.write("  {\n")
        C.write("    FALSE,                           /* AutoRequire */\n")
        C.write("    SD_CLIENT_SERVICE_HANDLE_ID_%s,  /* HandleId */\n" % (mn))
        C.write("    %s,                         /* ServiceId */\n" % (service["Service"]))
        C.write("    %s,                         /* InstanceId */\n" % (service["Instance"]))
        C.write("    0,                              /* MajorVersion */\n")
        C.write("    0,                              /* MinorVersion */\n")
        C.write("    SOAD_SOCKID_SOMEIP_%s, /* SoConId */\n" % (mn))
        if "Unreliable" in service or service.get("Protocol", None) == "UDP":
            C.write("    TCPIP_IPPROTO_UDP,              /* ProtocolType */\n")
        else:
            C.write("    TCPIP_IPPROTO_TCP,              /* ProtocolType */\n")
        C.write("    NULL,                           /* CapabilityRecordMatchCalloutRef */\n")
        C.write("    &Sd_ClientTimerDefault,\n")
        C.write("    &Sd_ClientService_Contexts[%s],\n" % (ID))
        C.write("    0, /* InstanceIndex */\n")
        if "EventGroups" not in service:
            C.write("    NULL,\n    0,\n")
        elif 0 == len(service["EventGroups"]):
            C.write("    NULL,\n    0,\n")
        else:
            C.write("    Sd_ConsumedEventGroups_%s,\n" % (service["Name"]))
            C.write("    ARRAY_SIZE(Sd_ConsumedEventGroups_%s),\n" % (service["Name"]))
        C.write("  },\n")
    if len(cfg.get("Clients", [])) > 0:
        C.write("};\n\n")
    C.write("static uint8_t sd_buffer[1400];\n")
    C.write("static Sd_InstanceContextType sd_context;\n")
    C.write("static const Sd_InstanceType Sd_Instances[] = {\n")
    C.write("  {\n")
    C.write('    "%s",                             /* Hostname */\n' % (cfg["SD"]["Hostname"]))
    C.write("    SD_CONVERT_MS_TO_MAIN_CYCLES(1000), /* SubscribeEventgroupRetryDelay */\n")
    C.write("    3,                                  /* SubscribeEventgroupRetryMax */\n")
    C.write("    {\n")
    C.write("      SD_RX_PID_MULTICAST,      /* RxPduId */\n")
    C.write("      SOAD_SOCKID_SD_MULTICAST, /* SoConId */\n")
    C.write("    },                          /* MulticastRxPdu */\n")
    C.write("    {\n")
    C.write("      SD_RX_PID_UNICAST,      /* RxPduId */\n")
    C.write("      SOAD_SOCKID_SD_UNICAST, /* SoConId */\n")
    C.write("    },                        /* UnicastRxPdu */\n")
    C.write("    {\n")
    C.write("      SOAD_TX_PID_SD_MULTICAST,    /* MulticastTxPduId */\n")
    C.write("      SOAD_TX_PID_SD_UNICAST,      /* UnicastTxPduId */\n")
    C.write("    },                             /* TxPdu */\n")
    if len(cfg.get("Servers", [])) > 0:
        C.write("    Sd_ServerServices,             /* ServerServices */\n")
        C.write("    ARRAY_SIZE(Sd_ServerServices), /* NumOfServerServices */\n")
    else:
        C.write("    NULL,                          /* ServerServices */\n")
        C.write("    0,                             /* NumOfServerServices */\n")
    if len(cfg.get("Clients", [])) > 0:
        C.write("    Sd_ClientServices,             /* ClientServices */\n")
        C.write("    ARRAY_SIZE(Sd_ClientServices), /* NumOfClientServices */\n")
    else:
        C.write("    NULL,                          /* ClientServices */\n")
        C.write("    0,                             /* NumOfClientServices */\n")
    C.write("    sd_buffer,                     /* Buffer */\n")
    C.write("    sizeof(sd_buffer),\n")
    C.write("    &sd_context,\n")
    C.write("  },\n")
    C.write("};\n\n")
    if len(cfg.get("Servers", [])) > 0:
        C.write("static const Sd_ServerServiceType* Sd_ServerServicesMap[] = {\n")
        for ID, service in enumerate(cfg.get("Servers", [])):
            C.write("  &Sd_ServerServices[%s],\n" % (ID))
        C.write("};\n\n")
    if len(cfg.get("Clients", [])) > 0:
        C.write("static const Sd_ClientServiceType* Sd_ClientServicesMap[] = {\n")
        for ID, service in enumerate(cfg.get("Clients", [])):
            C.write("  &Sd_ClientServices[%s],\n" % (ID))
        C.write("};\n\n")
    C.write("static const uint16_t Sd_EventHandlersMap[] = {\n")
    for service in cfg.get("Servers", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        mn = toMacro(service["Name"])
        for ge in service["EventGroups"]:
            C.write("  SD_SERVER_SERVICE_HANDLE_ID_%s,\n" % (mn))
    C.write("  -1,\n};\n\n")
    C.write("static const uint16_t Sd_PerServiceEventHandlerMap[] = {\n")
    for service in cfg.get("Servers", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        for id, ge in enumerate(service["EventGroups"]):
            C.write("  %s,\n" % (id))
    C.write("  -1,\n};\n\n")
    C.write("static const uint16_t Sd_ConsumedEventGroupsMap[] = {\n")
    for service in cfg.get("Clients", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        mn = toMacro(service["Name"])
        for ge in service["EventGroups"]:
            C.write("  SD_CLIENT_SERVICE_HANDLE_ID_%s,\n" % (mn))
    C.write("  -1,\n};\n\n")
    C.write("static const uint16_t Sd_PerServiceConsumedEventGroupsMap[] = {\n")
    for service in cfg.get("Clients", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        for id, ge in enumerate(service["EventGroups"]):
            C.write("  %s,\n" % (id))
    C.write("  -1,\n};\n\n")
    C.write("const Sd_ConfigType Sd_Config = {\n")
    C.write("  Sd_Instances,\n")
    C.write("  ARRAY_SIZE(Sd_Instances),\n")
    if len(cfg.get("Servers", [])) > 0:
        C.write("  Sd_ServerServicesMap,\n")
        C.write("  ARRAY_SIZE(Sd_ServerServicesMap),\n")
    else:
        C.write("  NULL,\n")
        C.write("  0,\n")
    if len(cfg.get("Clients", [])) > 0:
        C.write("  Sd_ClientServicesMap,\n")
        C.write("  ARRAY_SIZE(Sd_ClientServicesMap),\n")
    else:
        C.write("  NULL,\n")
        C.write("  0,\n")
    C.write("  Sd_EventHandlersMap,\n")
    C.write("  Sd_PerServiceEventHandlerMap,\n")
    C.write("  ARRAY_SIZE(Sd_EventHandlersMap)-1,\n")
    C.write("  Sd_ConsumedEventGroupsMap,\n")
    C.write("  Sd_PerServiceConsumedEventGroupsMap,\n")
    C.write("  ARRAY_SIZE(Sd_ConsumedEventGroupsMap)-1,\n")
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen_SOMEIPXF(cfg, dir):
    H = open("%s/SomeIpXf_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef SOMEIPXF_CFG_H\n")
    H.write("#define SOMEIPXF_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write('#include "SomeIpXf.h"\n')
    H.write("#ifdef __cplusplus\n")
    H.write('extern "C" {\n')
    H.write("#endif\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    for name, struct in GetStructs(cfg).items():
        H.write("typedef struct %s_s %s_Type;\n\n" % (name, name))
    for name, struct in GetStructs(cfg).items():
        H.write("struct %s_s {\n" % (name))
        for data in struct["Data"]:
            dinfo = GetTypeInfo(data, GetStructs(cfg))
            cstr = "%s %s" % (dinfo["ctype"], data["Name"])
            if dinfo["IsArray"]:
                cstr += "[%s]" % (data["Size"])
            H.write("  %s;\n" % (cstr))
            if dinfo["IsArray"] and (data.get("VariableArray", False) or dinfo.get("VariableArray", False)):
                if data["Size"] < 256:
                    dtype = "uint8_t"
                elif data["Size"] < 65536:
                    dtype = "uint16_t"
                else:
                    dtype = "uint32_t"
                H.write("  %s %sLen;\n" % (dtype, data["Name"]))
                # mark data and its container struct both has length field
                data["WithLength"] = True
                struct["WithLength"] = True
            if data.get("Optional", False):
                H.write("  boolean has_%s;\n" % (data["Name"]))
                struct["WithTag"] = True
                struct["WithLength"] = True
        H.write("};\n\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    for name, struct in GetStructs(cfg).items():
        H.write("extern const SomeIpXf_StructDefinitionType SomeIpXf_Struct%sDef;\n" % (name))
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#ifdef __cplusplus\n")
    H.write("}\n")
    H.write("#endif\n")
    H.write("#endif /* SOMEIPXF_CFG_H */\n")
    H.close()
    C = open("%s/SomeIpXf_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "SomeIpXf_Priv.h"\n')
    C.write('#include "SomeIpXf_Cfg.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for name, struct in GetStructs(cfg).items():
        C.write("static const SomeIpXf_DataElementType Struct%sDataElements[] = {\n" % (name))
        for idx, data in enumerate(struct["Data"]):
            dinfo = GetTypeInfo(data, GetStructs(cfg))
            if dinfo["ctype"] in ["boolean", "uint8_t", "int8_t"]:
                dtype = "Byte"
            elif dinfo["ctype"] in ["uint16_t", "int16_t"]:
                dtype = "Short"
            elif dinfo["ctype"] in ["uint32_t", "int32_t", "float"]:
                dtype = "Long"
            elif dinfo["ctype"] in ["uint64_t", "int64_t", "double"]:
                dtype = "LongLong"
            else:
                dtype = "Struct"
            if dinfo["IsArray"]:
                dtype += "Array"
            C.write("  {\n")
            C.write('    "%s",\n' % (data["Name"]))
            if "Struct" in dtype:
                C.write("    &SomeIpXf_Struct%sDef,\n" % (data["Type"]))
            else:
                C.write("    NULL,\n")
            C.write("    sizeof(((%s_Type*)0)->%s),\n" % (name, data["Name"]))
            C.write("    __offsetof(%s_Type, %s),\n" % (name, data["Name"]))
            if dinfo["IsArray"] and (data.get("VariableArray", False) or dinfo.get("VariableArray", False)):
                C.write("    __offsetof(%s_Type, %sLen),\n" % (name, data["Name"]))
            else:
                C.write("    0,\n")
            if data.get("Optional", False):
                C.write("    __offsetof(%s_Type, has_%s),\n" % (name, data["Name"]))
            else:
                C.write("    0,\n")
            if struct.get("WithTag", False):
                C.write("    %s, /* Tag */\n" % (idx))
            else:
                C.write("    SOMEIPXF_TAG_NOT_USED, /* Tag */\n")
            C.write("    SOMEIPXF_DATA_ELEMENT_TYPE_%s,\n" % (toMacro(dtype)))
            sz = GetStructDataSize(data, GetStructs(cfg))
            if data.get("WithLength", False):
                if sz < 256:
                    sizeOfDataLengthField = 1
                elif sz < 65536:
                    sizeOfDataLengthField = 2
                else:
                    sizeOfDataLengthField = 4
            else:
                sizeOfDataLengthField = 0
            C.write("    %s, /* sizeOfDataLengthField for %s */\n" % (sizeOfDataLengthField, sz))
            C.write("  },\n")
        C.write("};\n\n")
    for name, struct in GetStructs(cfg).items():
        C.write("const SomeIpXf_StructDefinitionType SomeIpXf_Struct%sDef = {\n" % (name))
        C.write('  "%s",\n' % (name))
        C.write("  Struct%sDataElements,\n" % (name))
        C.write("  sizeof(%s_Type),\n" % (name))
        C.write("  ARRAY_SIZE(Struct%sDataElements),\n" % (name))
        sz = GetStructSize(struct, GetStructs(cfg))
        if struct.get("WithLength", False) or struct.get("WithTag", False):
            if sz < 256:
                sizeOfStructLengthField = 1
            elif sz < 65536:
                sizeOfStructLengthField = 2
            else:
                sizeOfStructLengthField = 4
        else:
            sizeOfStructLengthField = 0
        C.write("  %s /* sizeOfStructLengthField for %s */,\n" % (sizeOfStructLengthField, sz))
        C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def Gen_SOMEIP(cfg, dir, source):
    H = open("%s/SomeIp_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef SOMEIP_CFG_H\n")
    H.write("#define SOMEIP_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    WaitResposeTpMessagePoolSize = 8
    AsyncRequestMessagePoolSize = 8
    RxTpMessagePoolSize = 8
    TxTpMessagePoolSize = 8
    TxTpEventMessagePoolSize = 8
    for service in cfg.get("Servers", []):
        listenNum = service.get("Listen", 1)
        AsyncRequestMessagePoolSize += len(service.get("Methods", [])) * listenNum
        for method in service.get("Methods", []):
            if method.get("Tp", False):
                WaitResposeTpMessagePoolSize += listenNum
                RxTpMessagePoolSize += listenNum
                TxTpMessagePoolSize += listenNum
        for eg in service.get("EventGroups", []):
            for event in eg.get("Events", []):
                if event.get("Tp", False):
                    TxTpEventMessagePoolSize += listenNum
    for service in cfg.get("Clients", []):
        for method in service.get("Methods", []):
            if method.get("Tp", False):
                WaitResposeTpMessagePoolSize += 1
                RxTpMessagePoolSize += 1
                TxTpMessagePoolSize += 1
        for eg in service.get("EventGroups", []):
            for event in eg.get("Events", []):
                if event.get("Tp", False):
                    RxTpMessagePoolSize += 1
    H.write(
        "#define SOMEIP_ASYNC_REQUEST_MESSAGE_POOL_SIZE %s\n"
        % (cfg.get("AsyncRequestMessagePoolSize", AsyncRequestMessagePoolSize))
    )
    H.write(
        "#define SOMEIP_WAIT_RESPOSE_MESSAGE_POOL_SIZE %s\n\n"
        % (cfg.get("WaitResposeTpMessagePoolSize", WaitResposeTpMessagePoolSize))
    )
    H.write("#define SOMEIP_RX_TP_MESSAGE_POOL_SIZE %s\n" % (cfg.get("RxTpMessagePoolSize", RxTpMessagePoolSize)))
    H.write("#define SOMEIP_TX_TP_MESSAGE_POOL_SIZE %s\n" % (cfg.get("TxTpMessagePoolSize", TxTpMessagePoolSize)))
    H.write(
        "#define SOMEIP_TX_TP_EVENT_MESSAGE_POOL_SIZE %s\n"
        % (cfg.get("TxTpEventMessagePoolSize", TxTpEventMessagePoolSize))
    )
    ID = 0
    for service in cfg.get("Servers", []):
        mn = toMacro(service["Name"])
        H.write("#define SOMEIP_SSID_%s %s\n" % (mn, ID))
        ID += 1
    for service in cfg.get("Clients", []):
        mn = toMacro(service["Name"])
        H.write("#define SOMEIP_CSID_%s %s\n" % (mn, ID))
        ID += 1
    H.write("\n")
    ID = 0
    for service in cfg.get("Servers", []):
        mn = toMacro(service["Name"])
        if "Reliable" in service:
            listen = service["Listen"] if "Listen" in service else 3
            for i in range(listen):
                H.write("#define SOMEIP_RX_PID_SOMEIP_%s%s %s\n" % (mn, i, ID))
                H.write("#define SOMEIP_TX_PID_SOMEIP_%s%s %s\n" % (mn, i, ID))
                ID += 1
        else:
            H.write("#define SOMEIP_RX_PID_SOMEIP_%s %s\n" % (mn, ID))
            H.write("#define SOMEIP_TX_PID_SOMEIP_%s %s\n" % (mn, ID))
            ID += 1
    for service in cfg.get("Clients", []):
        mn = toMacro(service["Name"])
        H.write("#define SOMEIP_RX_PID_SOMEIP_%s %s\n" % (mn, ID))
        H.write("#define SOMEIP_TX_PID_SOMEIP_%s %s\n" % (mn, ID))
        ID += 1
    H.write("\n")
    ID = 0
    for service in cfg.get("Servers", []):
        if "Methods" not in service:
            continue
        for method in service.get("Methods", []):
            bName = "%s_%s" % (service["Name"], method["Name"])
            H.write("#define SOMEIP_RX_METHOD_%s %s\n" % (toMacro(bName), ID))
            ID += 1
    H.write("\n")
    ID = 0
    for service in cfg.get("Clients", []):
        if "Methods" not in service:
            continue
        for method in service.get("Methods", []):
            bName = "%s_%s" % (service["Name"], method["Name"])
            H.write("#define SOMEIP_TX_METHOD_%s %s\n" % (toMacro(bName), ID))
            ID += 1
    H.write("\n")
    ID = 0
    for service in cfg.get("Servers", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        for egroup in service["EventGroups"]:
            for event in egroup["Events"]:
                beName = "%s_%s_%s" % (service["Name"], egroup["Name"], event["Name"])
                H.write("#define SOMEIP_TX_EVT_%s %s\n" % (toMacro(beName), ID))
                ID += 1
    H.write("\n")
    ID = 0
    for service in cfg.get("Clients", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        for egroup in service["EventGroups"]:
            for event in egroup["Events"]:
                beName = "%s_%s_%s" % (service["Name"], egroup["Name"], event["Name"])
                H.write("#define SOMEIP_RX_EVT_%s %s\n" % (toMacro(beName), ID))
                ID += 1
    H.write("\n")
    H.write("#ifndef SOMEIP_MAIN_FUNCTION_PERIOD\n")
    H.write("#define SOMEIP_MAIN_FUNCTION_PERIOD %su\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + SOMEIP_MAIN_FUNCTION_PERIOD - 1u) / SOMEIP_MAIN_FUNCTION_PERIOD)\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* SOMEIP_CFG_H */\n")
    H.close()

    C = open("%s/SomeIp_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "SomeIp.h"\n')
    C.write('#include "SomeIp_Cfg.h"\n')
    C.write('#include "SomeIp_Priv.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write('#include "Sd_Cfg.h"\n')
    for service in cfg.get("Servers", []):
        C.write('#include "%sSkeleton.h"\n' % (service["Name"]))
    for service in cfg.get("Clients", []):
        C.write('#include "%sProxy.h"\n' % (service["Name"]))
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    for service in cfg.get("Servers", []):
        if "Methods" not in service:
            continue
        C.write("static const SomeIp_ServerMethodType someIpServerMethods_%s[] = {\n" % (service["Name"]))
        for method in service.get("Methods", []):
            bName = "%s_%s" % (service["Name"], method["Name"])
            C.write("  {\n")
            C.write("    %s, /* Method ID */\n" % (method["MethodId"]))
            C.write("    %s, /* interface version */\n" % (method["Version"]))
            C.write("    SomeIp_%s_OnRequest,\n" % (bName))
            C.write("    SomeIp_%s_OnFireForgot,\n" % (bName))
            C.write("    SomeIp_%s_OnAsyncRequest,\n" % (bName))
            if method.get("Tp", False):
                C.write("    SomeIp_%s_OnTpCopyRxData,\n" % (bName))
                C.write("    SomeIp_%s_OnTpCopyTxData,\n" % (bName))
                service["Tp"] = True
            else:
                C.write("    NULL,\n")
                C.write("    NULL,\n")
            resMaxLen = method.get("ResMaxLen", 512)
            if method.get("Tp", False):
                resMaxLen = 1396
            C.write("    %s /* ResMaxLen */\n" % (resMaxLen))
            C.write("  },\n")
        C.write("};\n\n")
    for service in cfg.get("Servers", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        C.write("static const SomeIp_ServerEventType someIpServerEvents_%s[] = {\n" % (service["Name"]))
        for egroup in service["EventGroups"]:
            for event in egroup["Events"]:
                bName = "%s_%s" % (service["Name"], egroup["Name"])
                beName = "%s_%s" % (bName, event["Name"])
                C.write("  {\n")
                C.write("    SD_EVENT_HANDLER_%s, /* SD EventGroup Handle ID */\n" % (toMacro(bName)))
                C.write("    %s, /* Event ID */\n" % (event["EventId"]))
                C.write("    %s, /* interface version */\n" % (event["Version"]))
                if event.get("Tp", False):
                    C.write("    SomeIp_%s_OnTpCopyTxData,\n" % (beName))
                    service["Tp"] = True
                else:
                    C.write("    NULL,\n")
                C.write("  },\n")
        C.write("};\n\n")
    for service in cfg.get("Clients", []):
        if "Methods" not in service:
            continue
        C.write("static const SomeIp_ClientMethodType someIpClientMethods_%s[] = {\n" % (service["Name"]))
        for method in service.get("Methods", []):
            bName = "%s_%s" % (service["Name"], method["Name"])
            C.write("  {\n")
            C.write("    %s, /* Method ID */\n" % (method["MethodId"]))
            C.write("    %s, /* interface version */\n" % (method["Version"]))
            C.write("    SomeIp_%s_OnResponse,\n" % (bName))
            C.write("    SomeIp_%s_OnError,\n" % (bName))
            if method.get("Tp", False):
                C.write("    SomeIp_%s_OnTpCopyRxData,\n" % (bName))
                C.write("    SomeIp_%s_OnTpCopyTxData,\n" % (bName))
                service["Tp"] = True
            else:
                C.write("    NULL,\n")
                C.write("    NULL,\n")
            C.write("  },\n")
        C.write("};\n\n")
    for service in cfg.get("Clients", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        C.write("static const SomeIp_ClientEventType someIpClientEvents_%s[] = {\n" % (service["Name"]))
        for egroup in service["EventGroups"]:
            for event in egroup["Events"]:
                beName = "%s_%s_%s" % (service["Name"], egroup["Name"], event["Name"])
                C.write("  {\n")
                C.write("    %s, /* Event ID */\n" % (event["EventId"]))
                C.write("    %s, /* interface version */\n" % (event["Version"]))
                C.write("    SomeIp_%s_OnNotification,\n" % (beName))
                if event.get("Tp", False):
                    C.write("    SomeIp_%s_OnTpCopyRxData,\n" % (beName))
                    service["Tp"] = True
                else:
                    C.write("  NULL,\n")
                C.write("  },\n")
        C.write("};\n\n")
    for service in cfg.get("Servers", []):
        mn = toMacro(service["Name"])
        if "Reliable" in service:
            numOfConnections = service["Listen"] if "Listen" in service else 1
        else:
            numOfConnections = 1
        C.write("static SomeIp_ServerContextType someIpServerContext_%s;\n\n" % (service["Name"]))
        C.write(
            "static SomeIp_ServerConnectionContextType someIpServerConnectionContext_%s[%s];\n\n"
            % (service["Name"], numOfConnections)
        )
        C.write(
            "static const SomeIp_ServerConnectionType someIpServerServiceConnections_%s[%s] = {\n"
            % (service["Name"], numOfConnections)
        )
        for i in range(numOfConnections):
            C.write("  {\n")
            C.write("    &someIpServerConnectionContext_%s[%s],\n" % (service["Name"], i))
            if "Reliable" in service:
                C.write("    SOMEIP_RX_PID_SOMEIP_%s%s,\n" % (mn, i))
                C.write("    SOAD_TX_PID_SOMEIP_%s_APT%s,\n" % (mn, i))
                C.write("    SOAD_SOCKID_SOMEIP_%s_APT%s,\n" % (mn, i))
            else:
                C.write("    SOMEIP_RX_PID_SOMEIP_%s,\n" % (mn))
                C.write("    SOAD_TX_PID_SOMEIP_%s,\n" % (mn))
                C.write("    SOAD_SOCKID_SOMEIP_%s,\n" % (mn))
            C.write("  },\n")
        C.write("};\n\n")
        C.write("static const SomeIp_ServerServiceType someIpServerService_%s = {\n" % (service["Name"]))
        C.write("  %s, /* ServiceId */\n" % (service["Service"]))
        C.write("  %s, /* ClientId */\n" % (service["ClientId"]))
        C.write("  SD_SERVER_SERVICE_HANDLE_ID_%s, /* SdHandleID */\n" % (toMacro(service["Name"])))
        if "Methods" not in service:
            C.write("  NULL,\n  0,\n")
        else:
            C.write("  someIpServerMethods_%s,\n" % (service["Name"]))
            C.write("  ARRAY_SIZE(someIpServerMethods_%s),\n" % (service["Name"]))
        if "EventGroups" not in service:
            C.write("  NULL,\n  0,\n")
        elif 0 == len(service["EventGroups"]):
            C.write("  NULL,\n  0,\n")
        else:
            C.write("  someIpServerEvents_%s,\n" % (service["Name"]))
            C.write("  ARRAY_SIZE(someIpServerEvents_%s),\n" % (service["Name"]))
        C.write("  someIpServerServiceConnections_%s,\n" % (service["Name"]))
        C.write("  ARRAY_SIZE(someIpServerServiceConnections_%s),\n" % (service["Name"]))
        if "Reliable" in service:
            C.write("  TCPIP_IPPROTO_TCP,\n")
        else:
            C.write("  TCPIP_IPPROTO_UDP,\n")
        C.write("  &someIpServerContext_%s,\n" % (service["Name"]))
        C.write("  SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (service.get("SeparationTime", 10)))
        C.write("  SomeIp_%s_OnConnect,\n" % (service["Name"]))
        C.write("};\n\n")
    for service in cfg.get("Clients", []):
        mn = toMacro(service["Name"])
        C.write("static SomeIp_ClientServiceContextType someIpClientServiceContext_%s;\n" % (service["Name"]))
        C.write("static const SomeIp_ClientServiceType someIpClientService_%s = {\n" % (service["Name"]))
        C.write("  %s, /* ServiceId */\n" % (service["Service"]))
        C.write("  %s, /* ClientId */\n" % (service["ClientId"]))
        C.write("  SD_CLIENT_SERVICE_HANDLE_ID_%s, /* SdHandleID */\n" % (mn))
        if "Methods" not in service:
            C.write("  NULL,\n  0,\n")
        else:
            C.write("  someIpClientMethods_%s,\n" % (service["Name"]))
            C.write("  ARRAY_SIZE(someIpClientMethods_%s),\n" % (service["Name"]))
        if "EventGroups" not in service:
            C.write("  NULL,\n  0,\n")
        elif 0 == len(service["EventGroups"]):
            C.write("  NULL,\n  0,\n")
        else:
            C.write("  someIpClientEvents_%s,\n" % (service["Name"]))
            C.write("  ARRAY_SIZE(someIpClientEvents_%s),\n" % (service["Name"]))
        C.write("  &someIpClientServiceContext_%s,\n" % (service["Name"]))
        C.write("  SOAD_TX_PID_SOMEIP_%s,\n" % (mn))
        C.write("  SomeIp_%s_OnAvailability,\n" % (service["Name"]))
        C.write("  SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (service.get("SeparationTime", 10)))
        C.write("  SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (service.get("ResponseTimeout", 1000)))
        C.write("};\n\n")
    C.write("static const SomeIp_ServiceType SomeIp_Services[] = {\n")
    for service in cfg.get("Servers", []):
        mn = toMacro(service["Name"])
        C.write("  {\n")
        C.write("    TRUE,\n")
        if "Reliable" in service:
            C.write("    SOAD_SOCKID_SOMEIP_%s_SERVER,\n" % (mn))
        else:
            C.write("    SOAD_SOCKID_SOMEIP_%s,\n" % (mn))
        C.write("    &someIpServerService_%s,\n" % (service["Name"]))
        C.write("  },\n")
    for service in cfg.get("Clients", []):
        mn = toMacro(service["Name"])
        C.write("  {\n")
        C.write("    FALSE,\n")
        C.write("    SOAD_SOCKID_SOMEIP_%s,\n" % (mn))
        C.write("    &someIpClientService_%s,\n" % (service["Name"]))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static const uint16_t Sd_PID2ServiceMap[] = {\n")
    for service in cfg.get("Servers", []):
        mn = toMacro(service["Name"])
        if "Reliable" in service:
            numOfConnections = service["Listen"] if "Listen" in service else 3
        else:
            numOfConnections = 1
        for i in range(numOfConnections):
            C.write("  SOMEIP_SSID_%s,\n" % (mn))
    for service in cfg.get("Clients", []):
        mn = toMacro(service["Name"])
        C.write("  SOMEIP_CSID_%s,\n" % (mn))
    C.write("};\n\n")
    C.write("static const uint16_t Sd_PID2ServiceConnectionMap[] = {\n")
    for service in cfg.get("Servers", []):
        if "Reliable" in service:
            numOfConnections = service["Listen"] if "Listen" in service else 3
        else:
            numOfConnections = 1
        for i in range(numOfConnections):
            C.write("  %s,\n" % (i))
    for service in cfg.get("Clients", []):
        C.write("  0,\n")
    C.write("};\n\n")
    C.write("static const uint16_t Sd_TxMethod2ServiceMap[] = {\n")
    for service in cfg.get("Clients", []):
        mn = toMacro(service["Name"])
        if "Methods" not in service:
            continue
        for method in service.get("Methods", []):
            C.write("  SOMEIP_CSID_%s,/* %s */\n" % (mn, method["Name"]))
    C.write("  -1\n};\n\n")
    C.write("static const uint16_t Sd_TxMethod2PerServiceMap[] = {\n")
    for service in cfg.get("Clients", []):
        if "Methods" not in service:
            continue
        for i, method in enumerate(service.get("Methods", [])):
            C.write("  %s, /* %s */\n" % (i, method["Name"]))
    C.write("  -1\n};\n\n")
    C.write("static const uint16_t Sd_TxEvent2ServiceMap[] = {\n")
    for service in cfg.get("Servers", []):
        mn = toMacro(service["Name"])
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        for egroup in service["EventGroups"]:
            for event in egroup["Events"]:
                C.write("  SOMEIP_SSID_%s, /* %s %s */\n" % (mn, egroup["Name"], event["Name"]))
    C.write("  -1\n};\n\n")
    C.write("static const uint16_t Sd_TxEvent2PerServiceMap[] = {\n")
    for service in cfg.get("Servers", []):
        if "EventGroups" not in service:
            continue
        if 0 == len(service["EventGroups"]):
            continue
        ID = 0
        for egroup in service["EventGroups"]:
            for event in egroup["Events"]:
                C.write("  %s, /* %s %s */\n" % (ID, egroup["Name"], event["Name"]))
                ID += 1
    C.write("  -1\n};\n\n")
    C.write("const SomeIp_ConfigType SomeIp_Config = {\n")
    C.write("  SOMEIP_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (cfg.get("TpRxTimeoutTime", 200)))
    C.write("  SomeIp_Services,\n")
    C.write("  ARRAY_SIZE(SomeIp_Services),\n")
    C.write("  Sd_PID2ServiceMap,\n")
    C.write("  Sd_PID2ServiceConnectionMap,\n")
    C.write("  ARRAY_SIZE(Sd_PID2ServiceMap),\n")
    C.write("  Sd_TxMethod2ServiceMap,\n")
    C.write("  Sd_TxMethod2PerServiceMap,\n")
    C.write("  ARRAY_SIZE(Sd_TxMethod2ServiceMap)-1,\n")
    C.write("  Sd_TxEvent2ServiceMap,\n")
    C.write("  Sd_TxEvent2PerServiceMap,\n")
    C.write("  ARRAY_SIZE(Sd_TxEvent2ServiceMap)-1,\n")
    C.write("};\n\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()


def ProcFieldGet(cfg, service, field):
    if "Get" in field:
        if "Methods" not in service:
            service["Methods"] = []
        service["Methods"].append(
            {
                "Name": f"Get{field['Name']}",
                "MethodId": field["Get"]["MethodId"],
                "Return": field["Type"],
                "Tp": field.get("Tp", False),
                "Version": field.get("Version", 0),
                "for": "field",
            }
        )


def ProcFieldSet(cfg, service, field):
    if "Set" in field:
        if "Methods" not in service:
            service["Methods"] = []
        service["Methods"].append(
            {
                "Name": f"Set{field['Name']}",
                "MethodId": field["Set"]["MethodId"],
                "Args": f"get-{field['Name']}-args",
                "Return": field["Type"],
                "Tp": field.get("Tp", False),
                "Version": field.get("Version", 0),
                "for": "field",
            }
        )
        if "Args" not in cfg:
            cfg["Args"] = []
        cfg["Args"].append({"Name": f"get-{field['Name']}-args", "Args": [{"Name": "Value", "Type": field["Type"]}]})


def ProcFieldEvent(cfg, service, field):
    if "Event" in field:
        if "EventGroups" not in service:
            service["EventGroups"] = []
        groupName = field["Event"]["GroupName"]
        event_group = None
        for eg in service["EventGroups"]:
            if eg["Name"] == groupName:
                event_group = eg
                break
        if event_group is None:
            event_group = {
                "Name": groupName,
                "GroupId": field["Event"]["GroupId"],
                "Events": [],
            }
            if field["Event"].get("EnableMulticast", False):
                event_group["Multicast"] = field["Event"].get("Multicast", {})
            service["EventGroups"].append(event_group)
        if "Events" not in event_group:
            event_group["Events"] = []
        event_group["Events"].append(
            {
                "Name": field["Name"],
                "Type": field["Type"],
                "EventId": field["Event"]["EventId"],
                "Tp": field.get("Tp", False),
                "Version": "0",
                "for": "field",
            }
        )


def ProcFields(cfg):
    for service in cfg.get("Clients", []):
        for field in service.get("Fields", []):
            ProcFieldGet(cfg, service, field)
            ProcFieldSet(cfg, service, field)
            ProcFieldEvent(cfg, service, field)
    for service in cfg.get("Servers", []):
        for field in service.get("Fields", []):
            ProcFieldGet(cfg, service, field)
            ProcFieldSet(cfg, service, field)
            ProcFieldEvent(cfg, service, field)

    return cfg


def GenSomeIpE2E(cfg, dir, source):
    e2e_cfg = {
        "Class": "E2E",
        "ProtectP11": [],
        "ProtectP22": [],
        "ProtectP44": [],
        "ProtectP05": [],
        "CheckP11": [],
        "CheckP22": [],
        "CheckP44": [],
        "CheckP05": [],
    }
    bHasE2E = False

    def add_e2e_config(name, profile, e2e_data, is_protect):
        nonlocal bHasE2E
        bHasE2E = True
        config = {"Name": name}
        if "DataID" in e2e_data:
            config["DataID"] = "0x%x" % e2e_data["DataID"]
        if "DataIDMode" in e2e_data:
            config["DataIDMode"] = e2e_data["DataIDMode"]
        if "CRCOffset" in e2e_data:
            config["CRCOffset"] = e2e_data["CRCOffset"]
        if "CounterOffset" in e2e_data:
            config["CounterOffset"] = e2e_data["CounterOffset"]
        if "DataIDNibbleOffset" in e2e_data:
            config["DataIDNibbleOffset"] = e2e_data["DataIDNibbleOffset"]
        if "Offset" in e2e_data:
            config["Offset"] = e2e_data["Offset"]
        if "DataIDList" in e2e_data:
            config["DataIDList"] = e2e_data["DataIDList"]
        if "MaxDeltaCounter" in e2e_data:
            config["MaxDeltaCounter"] = e2e_data["MaxDeltaCounter"]

        if is_protect:
            e2e_cfg["Protect" + profile].append(config)
        else:
            e2e_cfg["Check" + profile].append(config)

    def process_service(service, is_server):
        service_name = service["Name"]

        # Process methods
        for method in service.get("Methods", []):
            if method.get("UseE2E", False):
                # Both server and client methods have both E2E-RX and E2E-TX
                if "E2E-RX" in method:
                    name = f"{service_name}_{method['Name']}_RX"
                    add_e2e_config(name, method["E2E-RX"]["Profile"], method["E2E-RX"], False)
                if "E2E-TX" in method:
                    name = f"{service_name}_{method['Name']}_TX"
                    add_e2e_config(name, method["E2E-TX"]["Profile"], method["E2E-TX"], True)

        # Process events
        for egroup in service.get("EventGroups", []):
            for event in egroup.get("Events", []):
                if event.get("UseE2E", False):
                    if is_server:
                        # Server events send notifications (TX)
                        if "E2E-TX" in event:
                            name = f"{service_name}_{egroup['Name']}_{event['Name']}_TX"
                            add_e2e_config(name, event["E2E-TX"]["Profile"], event["E2E-TX"], True)
                    else:
                        # Client events receive notifications (RX)
                        if "E2E-RX" in event:
                            name = f"{service_name}_{egroup['Name']}_{event['Name']}_RX"
                            add_e2e_config(name, event["E2E-RX"]["Profile"], event["E2E-RX"], False)

        # Process fields
        for field in service.get("Fields", []):
            # Process get operation
            if "Get" in field and field["Get"].get("UseE2E", False):
                if "E2E-RX" in field["Get"]:
                    name = f"{service_name}_{field['Name']}_GET_RX"
                    add_e2e_config(name, field["Get"]["E2E-RX"]["Profile"], field["Get"]["E2E-RX"], False)
                if "E2E-TX" in field["Get"]:
                    name = f"{service_name}_{field['Name']}_GET_TX"
                    add_e2e_config(name, field["Get"]["E2E-TX"]["Profile"], field["Get"]["E2E-TX"], True)
            # Process set operation
            if "Set" in field and field["Set"].get("UseE2E", False):
                if "E2E-RX" in field["Set"]:
                    name = f"{service_name}_{field['Name']}_SET_RX"
                    add_e2e_config(name, field["Set"]["E2E-RX"]["Profile"], field["Set"]["E2E-RX"], False)
                if "E2E-TX" in field["Set"]:
                    name = f"{service_name}_{field['Name']}_SET_TX"
                    add_e2e_config(name, field["Set"]["E2E-TX"]["Profile"], field["Set"]["E2E-TX"], True)
            # Process event
            if "Event" in field and field["Event"].get("UseE2E", False):
                if is_server:
                    # Server field events send notifications (TX)
                    if "E2E-TX" in field["Event"]:
                        name = f"{service_name}_{field['Name']}_EVENT_TX"
                        add_e2e_config(name, field["Event"]["E2E-TX"]["Profile"], field["Event"]["E2E-TX"], True)
                else:
                    # Client field events receive notifications (RX)
                    if "E2E-RX" in field["Event"]:
                        name = f"{service_name}_{field['Name']}_EVENT_RX"
                        add_e2e_config(name, field["Event"]["E2E-RX"]["Profile"], field["Event"]["E2E-RX"], False)

    for service in cfg.get("Servers", []):
        process_service(service, True)

    for service in cfg.get("Clients", []):
        process_service(service, False)

    if bHasE2E:
        with open("%s/E2E.json" % (dir), "w") as f:
            json.dump(e2e_cfg, f, indent=2)
        Gen_E2E(e2e_cfg, dir)
        source["E2E"] = ["%s/E2E_Cfg.c" % (dir)]


def Gen_SomeIp(cfg, dir):
    source = {
        "Sd": ["%s/Sd_Cfg.c" % (dir)],
        "SomeIpXf": ["%s/SomeIpXf_Cfg.c" % (dir)],
        "SomeIp": ["%s/SomeIp_Cfg.c" % (dir)],
    }
    cfg = ProcFields(cfg)
    Gen_SD(cfg, dir)
    Gen_SOMEIPXF(cfg, dir)
    Gen_SOMEIP(cfg, dir, source)
    for service in cfg.get("Servers", []):
        Gen_SomeIpSkeleton(cfg, service, dir, source)
        Gen_VSomeIpSkeleton(cfg, service, dir, source)
        Gen_SomeIpSkeletonC(cfg, service, dir, source)
    for service in cfg.get("Clients", []):
        Gen_SomeIpProxy(cfg, service, dir, source)
        Gen_VSomeIpProxy(cfg, service, dir, source)
        Gen_SomeIpProxyC(cfg, service, dir, source)
    with open("%s/SomeIp.json" % (dir), "w") as f:
        json.dump(cfg, f, indent=2)
    GenSomeIpE2E(cfg, dir, source)
    return source
