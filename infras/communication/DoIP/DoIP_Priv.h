/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic over IP AUTOSAR CP Release 4.4.0
 */
#ifndef _DOIP_PRIV_H
#define _DOIP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "DoIP.h"
#include "SoAd.h"
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_DoIP_00008, @SWS_DoIP_00009 */
#define DOIP_GENERAL_HEADER_NEGATIVE_ACK 0x0000u                /* UDP/TCP*/
#define DOIP_VID_REQUEST 0x0001u                                /* UDP */
#define DOIP_VID_REQUEST_WITH_EID 0x0002u                       /* UDP */
#define DOIP_VID_REQUEST_WITH_VIN 0x0003u                       /* UDP */
#define DOIP_VAN_MSG_OR_VIN_RESPONCE 0x0004u                    /* UDP */
#define DOIP_ROUTING_ACTIVATION_REQUEST 0x0005u                 /* TCP */
#define DOIP_ROUTING_ACTIVATION_RESPONSE 0x0006u                /* TCP */
#define DOIP_ALIVE_CHECK_REQUEST 0x0007u                        /* TCP */
#define DOIP_ALIVE_CHECK_RESPONSE 0x0008u                       /* TCP */
#define DOIP_ENTITY_STATUS_REQUEST 0x4001u                      /* UDP */
#define DOIP_ENTITY_STATUS_RESPONSE 0x4002u                     /* UDP */
#define DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_REQUEST 0x4003u  /* UDP */
#define DOIP_DIAGNOSTIC_POWER_MODE_INFORMATION_RESPONSE 0x4004u /* UDP */
#define DOIP_DIAGNOSTIC_MESSAGE 0x8001                          /* TCP */
#define DOIP_DIAGNOSTIC_MESSAGE_POSITIVE_ACK 0x8002             /* TCP */
#define DOIP_DIAGNOSTIC_MESSAGE_NEGATIVE_ACK 0x8003             /* TCP */

#define DOIP_INVALID_PROTOCOL_NACK 0x00u       /* SWS_DoIP_00014 */
#define DOIP_UNKNOW_PAYLOAD_TYPE_NACK 0x01u    /* SWS_DoIP_00016 */
#define DOIP_TOO_MUCH_PAYLOAD_NACK 0x02u       /* @SWS_DoIP_00017 */
#define DOIP_NO_BUF_AVAIL_NACK 0x03u           /* @SWS_DoIP_00018 */
#define DOIP_INVALID_PAYLOAD_LENGTH_NACK 0x04u /* @SWS_DoIP_00019 */

#define DOIP_INVALID_PAYLOAD_TYPE 0xFFFFu

#define DOIP_INVALID_INDEX ((uint8_t)-1)

#ifndef DOIP_MAX_ROUTINE_ACTIVATIONS
#define DOIP_MAX_ROUTINE_ACTIVATIONS 32
#endif
/* ================================ [ TYPES     ] ============================================== */
/* @ECUC_DoIP_00035, @SWS_DoIP_00048 */
typedef Std_ReturnType (*DoIP_RoutingActivationAuthenticationCallbackType)(
  boolean *Authentified, const uint8_t *AuthenticationReqData, uint8_t *AuthenticationResData);

/* @ECUC_DoIP_00061, @SWS_DoIP_00048 */
typedef Std_ReturnType (*DoIP_RoutingActivationConfirmationCallbackType)(
  boolean *Confirmed, const uint8_t *ConfirmationReqData, uint8_t *ConfirmationResData);

typedef Std_ReturnType (*DoIP_GetVinFncType)(uint8_t *Data);
typedef Std_ReturnType (*DoIP_GetEIDFncType)(uint8_t *Data);
typedef Std_ReturnType (*DoIP_GetGIDFncType)(uint8_t *Data);

typedef enum
{
  DOIP_CON_CLOSED,
  DOIP_CON_OPEN,
} DoIP_ConnectionStateType;

typedef enum
{
  DOIP_MSG_IDLE,
  DOIP_MSG_RX,
  DOIP_MSG_TX,
} DoIP_MessageStateType;

/* @ECUC_DoIP_00053 */
typedef struct {
  uint16_t TargetAddress;
  PduIdType RxPduId;
  PduIdType TxPduId;
} DoIP_TargetAddressType;

typedef struct {
  DoIP_MessageStateType state;
  PduLengthType TpSduLength;
  PduLengthType index;
  const DoIP_TargetAddressType *TargetAddressRef;
} DoIP_MessageContextType;

typedef struct DoIP_Tester_s DoIP_TesterType;

/* @ECUC_DoIP_00030 */
typedef struct {
  uint8_t Number;
  uint8_t OEMReqLen;
  uint8_t OEMResLen;
  const DoIP_TargetAddressType *TargetAddressRefs;
  uint8_t numOfTargetAddressRefs;
  DoIP_RoutingActivationAuthenticationCallbackType AuthenticationCallback;
  DoIP_RoutingActivationConfirmationCallbackType ConfirmationCallback;
  const DoIP_TesterType *tester;
} DoIP_RoutingActivationType;

typedef enum
{
  DOIP_RA_IDLE,
  DOIP_RA_SOCKET_HANDLER,
  DOIP_RA_SOCKET_HANDLER_2,
  DOIP_RA_CHECK_AUTHENTICATION,
  DOIP_RA_CHECK_CONFIRMATION,
} DoIP_RoutineActivationStateType;

typedef struct {
  DoIP_RoutineActivationStateType state;
  const DoIP_TesterType *tester;
  uint8_t raid;
  uint8_t OEM[4];
} DoIP_RoutineActivationManagerType;

typedef struct {
  DoIP_ConnectionStateType state;
  DoIP_MessageContextType msg;
  DoIP_RoutineActivationManagerType ramgr;
  uint32_t RAMask; /* maximum 32 Routine can be activated per tester */
  const DoIP_TesterType *TesterRef;
  uint16_t InactivityTimer;
  uint16_t AliveCheckResponseTimer;
  boolean isAlive;
} DoIP_TesterConnectionContextType;

/* @ECUC_DoIP_00032 */
typedef struct {
  DoIP_TesterConnectionContextType *context;
  SoAd_SoConIdType SoConId;
  PduIdType SoAdTxPdu;
  uint16_t InitialInactivityTime;
  uint16_t GeneralInactivityTime;
  uint16_t AliveCheckResponseTimeout;
} DoIP_TesterConnectionType;

/* @ECUC_DoIP_00031 */
struct DoIP_Tester_s {
  uint16_t TesterSA;
  const DoIP_RoutingActivationType *const *RoutingActivationRefs;
  uint8_t numOfRoutingActivations;
};

/* @ECUC_DoIP_00045 */
typedef struct {
  SoAd_SoConIdType SoConId;
  boolean RequestAddressAssignment;
} DoIP_TcpConnectionType;

/* @ECUC_DoIP_00052 */
typedef struct {
  SoAd_SoConIdType SoConId;
  boolean RequestAddressAssignment;
} DoIP_UdpConnectionType;

typedef struct {
  DoIP_ConnectionStateType state;
  uint16_t AnnouncementTimer;
  uint8_t AnnouncementCounter;
} DoIP_UdpVehicleAnnouncementConnectionContextType;

/* @ECUC_DoIP_00076 */
typedef struct {
  DoIP_UdpVehicleAnnouncementConnectionContextType *context;
  SoAd_SoConIdType SoConId;
  PduIdType SoAdTxPdu;
  uint16_t InitialVehicleAnnouncementTime;
  uint16_t VehicleAnnouncementInterval;
  boolean RequestAddressAssignment;
  uint8_t VehicleAnnouncementCount;
} DoIP_UdpVehicleAnnouncementConnectionType;

/* @SWS_DoIP_00271 */
typedef enum
{
  DOIP_ACTIVATION_LINE_INACTIVE,
  DOIP_ACTIVATION_LINE_ACTIVE,
} DoIP_ActivationLineType;

typedef struct {
  DoIP_ActivationLineType ActivationLineState;
  PduLengthType txLen;
} DoIP_ChannelContextType;

/* @ECUC_DoIP_00069 */
typedef struct {
  DoIP_ChannelContextType *context;
  uint8_t *txBuf;
  PduLengthType txBufLen;
  const DoIP_TesterType *testers;
  uint8_t numOfTesters;
  const DoIP_TesterConnectionType *testerConnections;
  uint8_t numOfTesterConnections;
  const DoIP_TcpConnectionType *TcpConnections;
  uint8_t numOfTcpConnections;
  const DoIP_UdpConnectionType *UdpConnections;
  uint8_t numOfUdpConnections;
  const DoIP_UdpVehicleAnnouncementConnectionType *UdpVehicleAnnouncementConnections;
  uint8_t numOfUdpVehicleAnnouncementConnections;
  DoIP_GetVinFncType GetVin;
  DoIP_GetEIDFncType GetEID;
  DoIP_GetGIDFncType GetGID;
  uint16_t LogicalAddress;
  uint8_t VinInvalidityPattern;
} DoIP_ChannelConfigType;

struct DoIP_Config_s {
  const DoIP_ChannelConfigType *ChannelConfigs;
  const uint8_t *RxPduIdToChannelMap;
  const uint8_t *RxPduIdToConnectionMap;
  const uint8_t *TxPduIdToChannelMap;
  uint8_t numOfChannels;
  uint8_t numOfRxPduIds;
  uint8_t numOfTxPduIds;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _DOIP_PRIV_H */
