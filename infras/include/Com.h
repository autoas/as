/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Communication AUTOSAR CP Release 4.4.0
 */
#ifndef _COM_H
#define _COM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_Com_00865 */
#define COM_SERVICE_NOT_AVAILABLE ((Std_ReturnType)0x80)
#define COM_BUSY ((Std_ReturnType)0x81)

/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Com_00819 */
typedef enum
{
  COM_INIT,
  COM_UNINIT
} Com_StatusType;

/* @SWS_Com_00820 */
typedef uint16_t Com_SignalIdType;

/* @SWS_Com_00821 */
typedef uint16_t Com_SignalGroupIdType;

/* @SWS_Com_00822 */
typedef uint16_t Com_IpduGroupIdType;

/* @SWS_Com_00825 */
typedef struct Com_Config_s Com_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Com_00432 */
void Com_Init(const Com_ConfigType *config);

/* @SWS_Com_00130 */
void Com_DeInit(void);

/* @SWS_Com_91001 */
void Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean initialize);

/* @SWS_Com_91002 */
void Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId);

/* @SWS_Com_91004 */
void Com_EnableReceptionDM(Com_IpduGroupIdType IpduGroupId);

/* @SWS_Com_91003 */
void Com_DisableReceptionDM(Com_IpduGroupIdType IpduGroupId);

/* @SWS_Com_00194 */
Com_StatusType Com_GetStatus(void);

/* @SWS_Com_00197 */
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr);

/* @SWS_Com_00627 */
Std_ReturnType Com_SendDynSignal(Com_SignalIdType SignalId, const void *SignalDataPtr,
                                 uint16_t Length);

/* @SWS_Com_00198 */
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalDataPtr);

/* @SWS_Com_00690 */
Std_ReturnType Com_ReceiveDynSignal(Com_SignalIdType SignalId, void *SignalDataPtr,
                                    uint16_t *Length);

/* @SWS_Com_00200 */
Std_ReturnType Com_SendSignalGroup(Com_SignalGroupIdType SignalGroupId);

/* @SWS_Com_00201 */
Std_ReturnType Com_ReceiveSignalGroup(Com_SignalGroupIdType SignalGroupId);

/* @SWS_Com_00851 */
Std_ReturnType Com_SendSignalGroupArray(Com_SignalGroupIdType SignalGroupId,
                                        const uint8_t *SignalGroupArrayPtr);

/* @SWS_Com_00855 */
Std_ReturnType Com_ReceiveSignalGroupArray(Com_SignalGroupIdType SignalGroupId,
                                           uint8_t *SignalGroupArrayPtr);

/* @SWS_Com_00203 */
Std_ReturnType Com_InvalidateSignal(Com_SignalIdType SignalId);

/* @SWS_Com_00557 */
Std_ReturnType Com_InvalidateSignalGroup(Com_SignalGroupIdType SignalGroupId);

/* @SWS_Com_00348 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId);

/* @SWS_Com_00858 */
Std_ReturnType Com_TriggerIPDUSendWithMetaData(PduIdType PduId, const uint8_t *MetaData);

/* @SWS_Com_00784 */
void Com_SwitchIpduTxMode(PduIdType PduId, boolean Mode);

/* @SWS_Com_00001 */
Std_ReturnType Com_TriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);

/* @SWS_Com_00123 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_Com_00650 */
void Com_TpRxIndication(PduIdType id, Std_ReturnType result);

/* @SWS_Com_00124 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_Com_00725 */
void Com_TpTxConfirmation(PduIdType id, Std_ReturnType result);

/* @SWS_Com_00691 */
BufReq_ReturnType Com_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType TpSduLength, PduLengthType *bufferSizePtr);

/* @SWS_Com_00692 */
BufReq_ReturnType Com_CopyRxData(PduIdType id, const PduInfoType *info,
                                 PduLengthType *bufferSizePtr);

/* @SWS_Com_00693 */
BufReq_ReturnType Com_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr);

/* @SWS_Com_00398 */
void Com_MainFunctionRx(void);

/* @SWS_Com_00399 */
void Com_MainFunctionTx(void);

/* @SWS_Com_00400 */
void Com_MainFunctionRouteSignals(void);

void Com_MainFunction(void);
#endif /* _COM_H */