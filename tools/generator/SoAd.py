# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
from .helper import *


__all__ = ["Gen_SoAd"]

__EphemeralPort = 49152


def GenEphemeralPort():
    global __EphemeralPort
    port = __EphemeralPort
    __EphemeralPort += 1
    return port


def Gen_Sock(C, RxPduId, SoConId, GID, SoConType):
    C.write("  {\n")
    C.write("    %s, /* RxPduId */\n" % (RxPduId))
    C.write("    %s, /* SoConId */\n" % (SoConId))
    C.write("    %s, /* GID */\n" % (GID))
    C.write("    SOAD_SOCON_%s, /* SoConType */\n" % (SoConType))
    C.write("  },\n")


def Gen_SoAd(cfg, dir):
    H = open("%s/SoAd_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef SOAD_CFG_H\n")
    H.write("#define SOAD_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    ID = 0
    for sock in cfg["Sockets"]:
        mn = toMacro(sock["Name"])
        if sock["Protocol"] == "UDP":
            H.write("#define SOAD_SOCKID_%s %s\n" % (mn, ID))
            ID += 1
        elif "Server" in sock:
            H.write("#define SOAD_SOCKID_%s_SERVER %s\n" % (mn, ID))
            ID += 1
            for i in range(sock["Listen"]):
                H.write("#define SOAD_SOCKID_%s_APT%s %s\n" % (mn, i, ID))
                ID += 1
        elif "Client" in sock:
            H.write("#define SOAD_SOCKID_%s %s\n" % (mn, ID))
            ID += 1
        else:
            raise Exception("wrong config for %s" % (sock))
    H.write("\n")
    ID = 0
    for sock in cfg["Sockets"]:
        mn = toMacro(sock["Name"])
        if sock["Protocol"] == "UDP":
            H.write("#define SOAD_TX_PID_%s %s\n" % (mn, ID))
            ID += 1
        elif "Server" in sock:
            for i in range(sock["Listen"]):
                H.write("#define SOAD_TX_PID_%s_APT%s %s\n" % (mn, i, ID))
                ID += 1
        elif "Client" in sock:
            H.write("#define SOAD_TX_PID_%s %s\n" % (mn, ID))
            ID += 1
    headerMaxLen = 1
    for GID, sock in enumerate(cfg["Sockets"]):
        headerLen = 0
        if sock["Up"] == "DoIP":
            headerLen = 8
        elif sock["Up"] == "SD":
            headerLen = 8
        elif sock["Up"] == "SOMEIP":
            headerLen = 8
        if headerLen > headerMaxLen:
            headerMaxLen = headerLen
    H.write("\n")
    H.write("#define SOAD_HEADER_MAX_LEN %su\n" % (headerMaxLen))
    H.write("#define SOAD_ERROR_COUNTER_LIMIT %su\n" % (cfg.get("ErrorCounterLimit", 3)))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* SOAD_CFG_H */\n")
    H.close()

    C = open("%s/SoAd_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "SoAd.h"\n')
    C.write('#include "SoAd_Cfg.h"\n')
    C.write('#include "SoAd_Priv.h"\n')
    if any(sock["Up"] == "DoIP" for sock in cfg["Sockets"]):
        C.write('#include "DoIP.h"\n')
        C.write('#include "DoIP_Cfg.h"\n')
    if any(sock["Up"] == "SD" for sock in cfg["Sockets"]):
        C.write('#include "Sd.h"\n')
        C.write('#include "Sd_Cfg.h"\n')
    if any(sock["Up"] == "SOMEIP" for sock in cfg["Sockets"]):
        C.write('#include "SomeIp.h"\n')
        C.write('#include "SomeIp_Cfg.h"\n')
    if any(sock["Up"] == "UdpNm" for sock in cfg["Sockets"]):
        C.write('#include "UdpNm.h"\n')
        C.write('#include "UdpNm_Cfg.h"\n')
    if any(sock["Up"] == "TLS" for sock in cfg["Sockets"]):
        C.write('#include "TLS.h"\n')
        C.write('#include "TLS_Cfg.h"\n')
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    if any(sock["Up"] == "DoIP" for sock in cfg["Sockets"]):
        C.write("static const SoAd_InterfaceType SoAd_DoIP_IF = {\n")
        C.write("  DoIP_HeaderIndication,\n")
        C.write("  DoIP_RxIndication,\n")
        C.write("  NULL,\n")
        C.write("};\n\n")

    if any(sock["Up"] == "SD" for sock in cfg["Sockets"]):
        C.write("static const SoAd_InterfaceType SoAd_SD_IF = {\n")
        C.write("  Sd_HeaderIndication,\n")
        C.write("  Sd_RxIndication,\n")
        C.write("  NULL,\n")
        C.write("};\n\n")

    if any(sock["Up"] == "UdpNm" for sock in cfg["Sockets"]):
        C.write("static const SoAd_InterfaceType SoAd_UdpNm_IF = {\n")
        C.write("  NULL,\n")
        C.write("  UdpNm_SoAdIfRxIndication,\n")
        C.write("  UdpNm_SoAdIfTxConfirmation,\n")
        C.write("};\n\n")

    if any(sock["Up"] == "SOMEIP" for sock in cfg["Sockets"]):
        C.write("static const SoAd_InterfaceType SoAd_SOMEIP_IF = {\n")
        C.write("  SomeIp_HeaderIndication,\n")
        C.write("  SomeIp_RxIndication,\n")
        C.write("  NULL,\n")
        C.write("};\n\n")

    if any(sock["Up"] == "TLS" for sock in cfg["Sockets"]):
        C.write("static const SoAd_InterfaceType SoAd_TLS_IF = {\n")
        C.write("  NULL,\n")
        C.write("  NULL,\n")
        C.write("  NULL,\n")
        C.write("};\n\n")

    if any(sock["Up"] == "Mirror" for sock in cfg["Sockets"]):
        C.write("static const SoAd_InterfaceType SoAd_Mirror_IF = {\n")
        C.write("  NULL,\n")
        C.write("  NULL,\n")
        C.write("  NULL,\n")
        C.write("};\n\n")

    C.write("static const SoAd_SocketConnectionType SoAd_SocketConnections[] = {\n")
    for GID, sock in enumerate(cfg["Sockets"]):
        RxPduId = sock["RxPduId"]
        mn = toMacro(sock["Name"])
        SoConType = "%s_%s" % (sock["Protocol"], "SERVER" if "Server" in sock else "CLIENT")
        if sock["Protocol"] == "UDP":
            SoConId = "SOAD_SOCKID_%s" % (mn)
        elif "Server" in sock:
            SoConId = "SOAD_SOCKID_%s_SERVER" % (mn)
            RxPduId = -1
        elif "Client" in sock:
            SoConId = "SOAD_SOCKID_%s" % (mn)
        Gen_Sock(C, RxPduId, SoConId, GID, SoConType)
        if ("Server" in sock) and (sock["Protocol"] == "TCP"):
            for i in range(sock["Listen"]):
                RxPduId = "%s%s" % (sock["RxPduId"], i)
                SoConId = "SOAD_SOCKID_%s_APT%s" % (mn, i)
                Gen_Sock(C, RxPduId, SoConId, GID, "TCP_ACCEPT")
    C.write("};\n\n")

    C.write("static SoAd_SocketContextType SoAd_SocketContexts[ARRAY_SIZE(SoAd_SocketConnections)];\n\n")

    C.write("static const SoAd_SocketConnectionGroupType SoAd_SocketConnectionGroups[] = {\n")
    for GID, sock in enumerate(cfg["Sockets"]):
        mn = toMacro(sock["Name"])
        IF = "TODO"
        SoConModeChgNotification = "TODO"
        SoConId = -1
        numOfConnections = 1
        if sock["Protocol"] == "UDP":
            pass
        elif "Server" in sock:
            numOfConnections = sock["Listen"]
            SoConId = "SOAD_SOCKID_%s_APT0" % (mn)
        elif "Client" in sock:
            pass
        headerLen = 0
        if sock["Up"] == "DoIP":
            SoConModeChgNotification = "DoIP_SoConModeChg"
            IF = "SoAd_DoIP_IF"
            headerLen = 8
        elif sock["Up"] == "SD":
            SoConModeChgNotification = "Sd_SoConModeChg"
            IF = "SoAd_SD_IF"
            headerLen = 8
        elif sock["Up"] == "SOMEIP":
            SoConModeChgNotification = "SomeIp_SoConModeChg"
            IF = "SoAd_SOMEIP_IF"
            headerLen = 8
        elif sock["Up"] == "UdpNm":
            SoConModeChgNotification = "NULL"
            IF = "SoAd_UdpNm_IF"
        elif sock["Up"] == "TLS":
            SoConModeChgNotification = "TLS_SoConModeChg"
            IF = "SoAd_TLS_IF"
        elif sock["Up"] == "Mirror":
            SoConModeChgNotification = "NULL"
            IF = "SoAd_Mirror_IF"
        else:
            raise
        if sock["Protocol"] == "UDP":
            headerLen = 0  # for UDP, I see on windows, must read the full packet
        if "ModeChg" in sock:
            SoConModeChgNotification = "%s_SoConModeChg" % (sock["ModeChg"])
        if "Server" in sock:
            IpAddress, Port = sock["Server"].split(":")
            LocalPort = 0
        else:
            IpAddress, Port = sock["Client"].split(":")
            LocalPort = GenEphemeralPort()
        if IpAddress.count(".") == 3:
            IpAddress = "TCPIP_IPV4_ADDR(%s)" % (",".join(IpAddress.split(".")))
        else:
            IpAddress = 0
        if sock.get("Multicast", False) and (sock["Protocol"] == "UDP"):
            multicast = "TRUE"
            LocalAddrId = 0  # TODO
        else:
            multicast = "FALSE"
            LocalAddrId = "TCPIP_LOCALADDRID_ANY"
        AutomaticSoConSetup = str(sock.get("AutomaticSoConSetup", False)).upper()
        C.write("  {\n")
        C.write("    /* %s: %s */\n" % (GID, sock["Name"]))
        C.write("    &%s, /* IF */\n" % (IF))
        C.write("    %s, /* SoConModeChgNotification */\n" % (SoConModeChgNotification))
        C.write("    TCPIP_IPPROTO_%s, /* ProtocolType */\n" % (sock["Protocol"]))
        C.write("    %s, /* Remote */\n" % (IpAddress))
        C.write("    %s, /* SoConId */\n" % (SoConId))
        C.write("    %s, /* Port */\n" % (Port))
        C.write("    %s, /* LocalPort */\n" % (LocalPort))
        C.write("    %s, /* HeaderLen */\n" % (headerLen))
        C.write("    %s, /* LocalAddrId */\n" % (LocalAddrId))
        C.write("    %s, /* NumOfConnections */\n" % (numOfConnections))
        C.write("    %s, /* AutomaticSoConSetup */\n" % (AutomaticSoConSetup))
        C.write("    %s, /* IsMulitcast */\n" % (multicast))
        C.write("  },\n")
    C.write("};\n\n")

    C.write("static const SoAd_SoConIdType TxPduIdToSoCondIdMap[] = {\n")
    for sock in cfg["Sockets"]:
        mn = toMacro(sock["Name"])
        if sock["Protocol"] == "UDP":
            C.write("  SOAD_SOCKID_%s, /* SOAD_TX_PID_%s */\n" % (mn, mn))
        elif "Server" in sock:
            for i in range(sock["Listen"]):
                C.write("  SOAD_SOCKID_%s_APT%s, /* SOAD_TX_PID_%s_APT%s */\n" % (mn, i, mn, i))
        else:
            C.write("  SOAD_SOCKID_%s, /* SOAD_TX_PID_%s */\n" % (mn, mn))
    C.write("};\n\n")
    C.write("const SoAd_ConfigType SoAd_Config = {\n")
    C.write("  SoAd_SocketConnections,\n")
    C.write("  SoAd_SocketContexts,\n")
    C.write("  ARRAY_SIZE(SoAd_SocketConnections),\n")
    C.write("  TxPduIdToSoCondIdMap,\n")
    C.write("  ARRAY_SIZE(TxPduIdToSoCondIdMap),\n")
    C.write("  SoAd_SocketConnectionGroups,\n")
    C.write("  ARRAY_SIZE(SoAd_SocketConnectionGroups),\n")
    C.write("};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.close()
    return {"SoAd": ["%s/SoAd_Cfg.c" % (dir)]}
