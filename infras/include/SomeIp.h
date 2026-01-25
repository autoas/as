/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * Ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
#ifndef SOMEIP_H
#define SOMEIP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "SoAd.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define SOMEIP_E_PENDING 100
#define SOMEIP_E_OK_SILENT SOMEIP_E_PENDING
#define SOMEIP_E_NOMEM 101
#define SOMEIP_E_MSG_TOO_SHORT 102
#define SOMEIP_E_MSG_TOO_LARGE 103
#define SOMEIP_E_BUSY 104

/* @SWS_SomeIpXf_00168 @SWS_SomeIpXf_00115 @PRS_SOMEIP_00191 */
#define SOMEIP_E_UNKNOWN_SERVICE 0x02
#define SOMEIP_E_UNKNOWN_METHOD 0x03
#define SOMEIP_E_NOT_READY 0x04
#define SOMEIP_E_NOT_REACHABLE 0x05
#define SOMEIP_E_TIMEOUT 0x06
#define SOMEIP_E_WRONG_PROTOCOL_VERSION 0x07
#define SOMEIP_E_WRONG_INTERFACE_VERSION 0x08
#define SOMEIP_E_MALFORMED_MESSAGE 0x09
#define SOMEIP_E_WRONG_MESSAGE_TYPE 0x0a

/* @SWS_SomeIpTp_00052 */
#define SOMEIP_E_UNINIT 0x01
#define SOMEIP_E_PARAM_POINTER 0x02
#define SOMEIP_E_PARAM 0x03
#define SOMEIP_E_INIT_FAILED 0x04
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t *data;
  uint32_t offset;
  uint32_t length;
  boolean moreSegmentsFlag;
} SomeIp_TpMessageType;

typedef struct {
  uint8_t *data;
  uint32_t length;
} SomeIp_MessageType;

typedef struct {
  uint8_t *data;
  uint32_t size;
  uint32_t length;
  uint32_t offset;
} SomeIp_CtrlRxBufferType;

typedef struct SomeIp_Config_s SomeIp_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void SomeIp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void SomeIp_SoConModeChg(SoAd_SoConIdType SoConId, SoAd_SoConModeType Mode);

BufReq_ReturnType SomeIp_SoAdTpStartOfReception(PduIdType RxPduId, const PduInfoType *PduInfoPtr,
                                                PduLengthType TpSduLength,
                                                PduLengthType *bufferSizePtr);

Std_ReturnType SomeIp_Request(uint32_t requestId, uint8_t *data, uint32_t length);
Std_ReturnType SomeIp_FireForgot(uint32_t requestId, uint8_t *data, uint32_t length);
Std_ReturnType SomeIp_Notification(uint32_t requestId, uint8_t *data, uint32_t length);

void SomeIp_Init(const SomeIp_ConfigType *ConfigPtr);
void SomeIp_MainFunction(void);

Std_ReturnType SomeIp_ConnectionTakeControl(uint16_t serviceId, uint16_t conId);
Std_ReturnType SomeIp_ConnectionRxControl(uint16_t serviceId, uint16_t conId,
                                          SomeIp_CtrlRxBufferType *buffer);

Std_ReturnType SomeIp_HeaderIndication(PduIdType RxPduId, const PduInfoType *info,
                                       uint32_t *payloadLength);
void SomeIp_RxIndication(PduIdType RxPduId, const PduInfoType *info);

void SomeIp_GetVersionInfo(Std_VersionInfoType *versionInfo);
#ifdef __cplusplus
}
#endif
#endif /* SOMEIP_H */
