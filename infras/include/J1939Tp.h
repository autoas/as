/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of a Transport Layer for SAE J1939 AUTOSAR CP R23-11
 */
#ifndef J1939_TP_H
#define J1939_TP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_J1939Tp_00115 */
#define J1939TP_E_UNINIT 0x01u
#define J1939TP_E_REINIT 0x02u
#define J1939TP_E_INIT_FAILED 0x03u
#define J1939TP_E_PARAM_POINTER 0x10u
#define J1939TP_E_INVALID_PDU_SDU_ID 0x11u

/* @SWS_J1939Tp_00234 */

/* Timeout occurred on receiver side after reception of an intermediate TP.DT frame of a block. */
#define J1939TP_E_TIMEOUT_T1 ((Std_ReturnType)0x30)

/* Timeout occurred on receiver side after transmission of a TP.CM/CTS frame. */
#define J1939TP_E_TIMEOUT_T2 ((Std_ReturnType)0x31)

/* Timeout occurred on transmitter side after transmission of the last TP.DT frame of a block. */
#define J1939TP_E_TIMEOUT_T3 ((Std_ReturnType)0x32)

/* Timeout occurred on transmitter side after reception of a TP.CM/CTS(0) frame. */
#define J1939TP_E_TIMEOUT_T4 ((Std_ReturnType)0x33)

/* Timeout occurred on transmitter or receiver side while trying to send the next TP.DT or TP.CM
 * frame.*/
#define J1939TP_E_TIMEOUT_TR ((Std_ReturnType)0x34)

/* Timeout occurred on receiver side while trying to send the next TP.CM/CTS frame after a
 * TP.CM/CTS(0) frame. */
#define J1939TP_E_TIMEOUT_TH ((Std_ReturnType)0x35)

/* Invalid value for "total message size" in received TP.CM/RTS frame. */
#define J1939TP_E_INVALID_TMS ((Std_ReturnType)0x40)

/* Value for "total number of packets" in received TP.CM/RTS frame does not match the "total message
 * size". */
#define J1939TP_E_INVALID_TNOP ((Std_ReturnType)0x41)

/* Invalid value for "maximum number of packets" in received TP.CM/RTS frame. */
#define J1939TP_E_INVALID_MNOP ((Std_ReturnType)0x42)

/* Unexpected PGN in received TP.CM frame. */
#define J1939TP_E_INVALID_PGN ((Std_ReturnType)0x43)

/* Invalid value for "number of packets" in received TP.CM/CTS frame. */
#define J1939TP_E_INVALID_NOP ((Std_ReturnType)0x44)

/* Invalid value for "next packet number" in received TP.CM/CTS frame. */
#define J1939TP_E_INVALID_NPN ((Std_ReturnType)0x45)

/* Invalid value for "connection abort reason" in received TP.Conn_Abort frame. */
#define J1939TP_E_INVALID_CAR ((Std_ReturnType)0x46)

/* Unexpected serial number in received TP.DT frame. */
#define J1939TP_E_INVALID_SN ((Std_ReturnType)0x47)
/* ================================ [ TYPES     ] ============================================== */

typedef struct J1939Tp_Config_s J1939Tp_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_J1939Tp_00087 */
void J1939Tp_Init(const J1939Tp_ConfigType *ConfigPtr);

void J1939Tp_InitRxChannel(PduIdType Channel);
void J1939Tp_InitTxChannel(PduIdType Channel);

/* @SWS_J1939Tp_00093 */
void J1939Tp_Shutdown(void);

/* @SWS_J1939Tp_00089 */
void J1939Tp_GetVersionInfo(Std_VersionInfoType *VersionInfo);

/* @SWS_J1939Tp_00096*/
Std_ReturnType J1939Tp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_J1939Tp_00177 */
Std_ReturnType J1939Tp_CancelTransmit(PduIdType TxPduId);

/* @SWS_J1939Tp_00176 */
Std_ReturnType J1939Tp_CancelReceive(PduIdType RxPduId);

/* @SWS_J1939Tp_00180 */
Std_ReturnType J1939Tp_ChangeParameter(PduIdType id, TPParameterType parameter, uint16 value);

/* @SWS_J1939Tp_00108 */
void J1939Tp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_J1939Tp_00112 */
void J1939Tp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_J1939Tp_00104 */
void J1939Tp_MainFunction(void);

void J1939Tp_MainFunction_TxChannel(uint16_t Channel);
void J1939Tp_MainFunction_RxChannel(uint16_t Channel);

void J1939Tp_MainFunctionFast(void);

void J1939Tp_MainFunction_TxChannelFast(uint16_t Channel);
void J1939Tp_MainFunction_RxChannelFast(uint16_t Channel);

Std_ReturnType J1939Tp_GetRxPgPGN(PduIdType Channel, uint32_t *PgPGN);
Std_ReturnType J1939Tp_SetTxPgPGN(PduIdType Channel, uint32_t PgPGN);
#ifdef __cplusplus
}
#endif
#endif /* J1939_TP_H */
