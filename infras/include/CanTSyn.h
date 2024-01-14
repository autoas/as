/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: https://www.autosar.org/fileadmin/standards/R20-11/CP/AUTOSAR_SWS_TimeSyncOverCAN.pdf
 */
#ifndef __CAN_TSYNC_H__
#define __CAN_TSYNC_H__
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define CANTSYN_TX_OFF ((CanTSyn_TransmissionModeType)0x00)
#define CANTSYN_TX_ON ((CanTSyn_TransmissionModeType)0x01)
/* ================================ [ TYPES     ] ============================================== */
typedef struct CanTSyn_Config_s CanTSyn_ConfigType;

/* @SWS_CanTSyn_00092 */
typedef uint8_t CanTSyn_TransmissionModeType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_CanTSyn_00093 */
void CanTSyn_Init(const CanTSyn_ConfigType *ConfigPtr);

/* @SWS_CanTSyn_00095 */
void CanTSyn_SetTransmissionMode(uint8_t CtrlIdx, CanTSyn_TransmissionModeType Mode);

/* @SWS_CanTSyn_00096 */
void CanTSyn_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_CanTSyn_00099 */
void CanTSyn_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_CanTSyn_00102s */
void CanTSyn_MainFunction(void);
#endif /* __CAN_TSYNC_H__ */
