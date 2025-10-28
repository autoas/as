/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2025 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Bus Mirroring AUTOSAR CP R23-11
 */
#ifndef MIRROR_H
#define MIRROR_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "Lin.h"
/* ================================ [ MACROS    ] ============================================== */
#define MIRROR_NT_INVALID ((Mirror_NetworkType)0x00)
#define MIRROR_NT_CAN ((Mirror_NetworkType)0x01)
#define MIRROR_NT_LIN ((Mirror_NetworkType)0x02)
#define MIRROR_NT_FLEXRAY ((Mirror_NetworkType)0x03)
#define MIRROR_NT_ETHERNET ((Mirror_NetworkType)0x04)
#define MIRROR_NT_PROPRIETARY ((Mirror_NetworkType)0x05)
#define MIRROR_NT_CAN_XL ((Mirror_NetworkType)0x06)

/* @SWS_Mirror_00165 */
#define MIRROR_INVALID_NETWORK ((NetworkHandleType)0xFF)

#define MIRROR_E_UNINIT 0x01u
#define MIRROR_E_REINIT 0x02u
#define MIRROR_E_INIT_FAILED 0x03u
#define MIRROR_E_PARAM_POINTER 0x10u
#define MIRROR_E_INVALID_PDU_SDU_ID 0x11u
#define MIRROR_E_INVALID_NETWORK_ID 0x12u

#define MIRROR_CAN_NS_BUS_ONLINE ((Mirror_CanNetworkStateType)0x40)
#define MIRROR_CAN_NS_ERROR_PASSIVE ((Mirror_CanNetworkStateType)0x20)
#define MIRROR_CAN_NS_BUS_OFF ((Mirror_CanNetworkStateType)0x10)

#define MIRROR_NS_FRAME_LOST ((uint8_t)0x80)

/* Tx error counter, divided by 8 */
#define MIRROR_CAN_NS_TX_ERROR_COUNTER_MASK ((Mirror_CanNetworkStateType)0x0F)
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Mirror_01000 */
typedef uint8_t Mirror_NetworkType;

typedef struct Mirror_Config_s Mirror_ConfigType;

typedef uint8_t Mirror_CanNetworkStateType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Mirror_01003 */
void Mirror_Init(const Mirror_ConfigType *configPtr);

/* @SWS_Mirror_01006 */
Std_ReturnType Mirror_GetStaticFilterState(NetworkHandleType network, uint8_t filterId,
                                           boolean *isActive);

/* @SWS_Mirror_01007 */
Std_ReturnType Mirror_SetStaticFilterState(NetworkHandleType network, uint8_t filterId,
                                           boolean isActive);

/* @SWS_Mirror_01008 */
Std_ReturnType Mirror_AddCanRangeFilter(NetworkHandleType network, uint8_t *filterId,
                                        Can_IdType lowerId, Can_IdType upperId);

/* @SWS_Mirror_01009 */
Std_ReturnType Mirror_AddCanMaskFilter(NetworkHandleType network, uint8_t *filterId, Can_IdType id,
                                       Can_IdType mask);

/* @SWS_Mirror_01010 */
Std_ReturnType Mirror_AddLinRangeFilter(NetworkHandleType network, uint8_t *filterId,
                                        uint8_t lowerId, uint8_t upperId);

/* @SWS_Mirror_01011 */
Std_ReturnType Mirror_AddLinMaskFilter(NetworkHandleType network, uint8_t *filterId, uint8_t id,
                                       uint8_t mask);

/* @SWS_Mirror_01013 */
Std_ReturnType Mirror_RemoveFilter(NetworkHandleType network, uint8_t filterId);

/* @SWS_Mirror_01014 */
boolean Mirror_IsMirrorActive(void);

/* @SWS_Mirror_01016 */
NetworkHandleType Mirror_GetDestNetwork(void);

/* @SWS_Mirror_01017 */
Std_ReturnType Mirror_SwitchDestNetwork(NetworkHandleType network);

/* @SWS_Mirror_01018 */
boolean Mirror_IsSourceNetworkStarted(NetworkHandleType network);

/* @SWS_Mirror_01019 */
Std_ReturnType Mirror_StartSourceNetwork(NetworkHandleType network);

/* @SWS_Mirror_01020 */
Std_ReturnType Mirror_StopSourceNetwork(NetworkHandleType network);

/* @SWS_Mirror_01021 */
Mirror_NetworkType Mirror_GetNetworkType(NetworkHandleType network);

/* @SWS_Mirror_01022 */
uint8_t Mirror_GetNetworkId(NetworkHandleType network);

/* @SWS_Mirror_01023 */
NetworkHandleType Mirror_GetNetworkHandle(Mirror_NetworkType networkType, uint8_t networkId);

/* @SWS_Mirror_01015 */
void Mirror_Offline(void);

/* @SWS_Mirror_01004 */
void Mirror_DeInit(void);

/* @SWS_Mirror_01024 */
void Mirror_ReportCanFrame(uint8_t controllerId, Can_IdType canId, uint8_t length,
                           const uint8_t *payload);

/** This is a AS specific extension API, not defined in the spec.
 * @brief Mirror_ReportCanState
 * @param[in] controllerId The CAN controller ID
 * @param[in] NetworkState The CAN network state
 * @note This function shall be called by CanIf/Can module when the CAN controller
 * state changes. The NetworkState parameter is a bitmask composed by the following bits:
 *       - Bit 6: BUS_ONLINE
 *       - Bit 5: ERROR_PASSIVE
 *       - Bit 4: BUS_OFF
 *       - Bit 3-0: Tx error counter divided by 8
 */
void Mirror_ReportCanState(uint8_t controllerId, Mirror_CanNetworkStateType NetworkState);

/* @SWS_Mirror_01027 */
void Mirror_ReportLinFrame(NetworkHandleType network, Lin_FramePidType pid, const PduInfoType *pdu,
                           Lin_StatusType status);

/* @SWS_Mirror_01028 */
void Mirror_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_Mirror_01029 */
Std_ReturnType Mirror_TriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);

/* @SWS_Mirror_01030 */
void Mirror_MainFunction(void);
#endif /* MIRROR_H */
