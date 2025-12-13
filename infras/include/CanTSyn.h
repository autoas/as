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

/* @SWS_CanTSyn_00089 */
#define CANTSYN_E_INVALID_PDUID 0x01
#define CANTSYN_E_UNINIT 0x02
#define CANTSYN_E_NULL_POINTER 0x03
#define CANTSYN_E_INIT_FAILED 0x04
#define CANTSYN_E_PARAM 0x05
#define CANTSYN_E_INV_CTRL_IDX 0x06
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

/* @SWS_CanTSyn_00094 */
void CanTSyn_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* __CAN_TSYNC_H__ */
