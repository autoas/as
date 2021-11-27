/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
#ifndef _SOMEIP_H
#define _SOMEIP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "SoAd.h"
/* ================================ [ MACROS    ] ============================================== */
#define SOMEIP_E_PENDING 100

/* @SWS_SomeIpXf_00168 @SWS_SomeIpXf_00115 */
#define SOMEIPXF_E_UNKNOWN_SERVICE 0x02
#define SOMEIPXF_E_UNKNOWN_METHOD 0x03
#define SOMEIPXF_E_NOT_READY 0x04
#define SOMEIPXF_E_NOT_REACHABLE 0x05
#define SOMEIPXF_E_TIMEOUT 0x06
#define SOMEIPXF_E_WRONG_PROTOCOL_VERSION 0x07
#define SOMEIPXF_E_WRONG_INTERFACE_VERSION 0x08
#define SOMEIPXF_E_MALFORMED_MESSAGE 0x09
#define SOMEIPXF_E_WRONG_MESSAGE_TYPE 0x0a

/* ================================ [ TYPES     ] ============================================== */
typedef struct SomeIp_Config_s SomeIp_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void SomeIp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void SomeIp_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode);

Std_ReturnType SomeIp_Request(uint16_t TxMethodId, uint8_t *data, uint32_t length);
Std_ReturnType SomeIp_Notification(uint16_t TxEventId, uint8_t *data, uint32_t length);

void SomeIp_Init(const SomeIp_ConfigType *ConfigPtr);
#endif /* _SOMEIP_H */
