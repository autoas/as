/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef _PCAP_H
#define _PCAP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "TcpIp.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void PCap_SD(uint8_t *data, uint32_t length, const TcpIp_SockAddrType *RemoteAddr, boolean isRx);
void PCap_SomeIp(uint16_t serviceId, uint16_t methodId, uint8_t interfaceVersion,
                 uint8_t messageType, uint8_t returnCode, uint8_t *payload, uint32_t payloadLength,
                 uint16_t clientId, uint16_t sessionId, const TcpIp_SockAddrType *RemoteAddr,
                 boolean isTp, uint32_t offset, boolean more, boolean isRx);
void PCap_Packet(const void *packet, uint32_t length);
#endif /* _PCAP_H */
