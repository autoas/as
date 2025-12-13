/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Module XCP AUTOSAR CP R23-11
 */
#ifndef XCP_H
#define XCP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* XCP predefined channels */
#define XCP_ON_CAN_CHL ((uint8_t)0)
#define XCP_ON_FLEXRAY_CHL ((uint8_t)1)
#define XCP_ON_ETHERNET_CHL ((uint8_t)2)
#define XCP_INVALID_CHL ((uint8_t)0xFF)

#define XCP_INITIAL ((Xcp_OpStatusType)0x00)
#define XCP_PENDING ((Xcp_OpStatusType)0x01)
#define XCP_CANCEL ((Xcp_OpStatusType)0x02)

/* command process is on going, not finished yet */
#define XCP_E_PENDING ((Std_ReturnType)0x20)
#define XCP_E_OK_NO_RES ((Std_ReturnType)0xAC)

/* xcp supported resources */
/** ProGraMming resource. */
#define XCP_RES_PGM (0x10)
/*data STIMulation resource. */
#define XCP_RES_STIM (0x08)
/* Data AcQuisition resource. */
#define XCP_RES_DAQ (0x04)
/* CALibration and PAGing resource. */
#define XCP_RES_CALPAG (0x01)

#define XCP_RES_MASK (XCP_RES_PGM | XCP_RES_STIM | XCP_RES_DAQ | XCP_RES_CALPAG)

#define XCP_E_OK ((Xcp_NegativeResponseCodeType)0x00)

/* @[3] error codes */
/* Command processor synchronisation. */
#define XCP_E_CMD_SYNC ((Xcp_NegativeResponseCodeType)0x00)
/* Command was not executed. */
#define XCP_E_CMD_BUSY ((Xcp_NegativeResponseCodeType)0x10)
/* Command rejected because DAQ is running. */
#define XCP_E_DAQ_ACTIVE ((Xcp_NegativeResponseCodeType)0x11)
/* Command rejected because PGM is running. */
#define XCP_E_PGM_ACTIVE ((Xcp_NegativeResponseCodeType)0x12)
/* Unknown command or not implemented optional command. */
#define XCP_E_CMD_UNKNOWN ((Xcp_NegativeResponseCodeType)0x20)
/* Command syntax invalid. */
#define XCP_E_CMD_SYNTAX ((Xcp_NegativeResponseCodeType)0x21)
/* Command syntax valid but command parameter(s) out of range. */
#define XCP_E_OUT_OF_RANGE ((Xcp_NegativeResponseCodeType)0x22)
/* The memory location is write protected. */
#define XCP_E_WRITE_PROTECTED ((Xcp_NegativeResponseCodeType)0x23)
/* The memory location is not accessible. */
#define XCP_E_ACCESS_DENIED ((Xcp_NegativeResponseCodeType)0x24)
/* Access denied, Seed & Key is required. */
#define XCP_E_ACCESS_LOCKED ((Xcp_NegativeResponseCodeType)0x25)
/* Selected page not available. */
#define XCP_E_PAGE_NOT_VALID ((Xcp_NegativeResponseCodeType)0x26)
/* Selected page mode not available. */
#define XCP_E_MODE_NOT_VALID ((Xcp_NegativeResponseCodeType)0x27)
/* Selected segment not valid. */
#define XCP_E_SEGMENT_NOT_VALID ((Xcp_NegativeResponseCodeType)0x28)
/* Sequence error. */
#define XCP_E_SEQUENCE ((Xcp_NegativeResponseCodeType)0x29)
/* DAQ configuration not valid. */
#define XCP_E_DAQ_CONFIG ((Xcp_NegativeResponseCodeType)0x2A)
/* Memory overflow error. */
#define XCP_E_MEMORY_OVERFLOW ((Xcp_NegativeResponseCodeType)0x30)
/* Generic error. */
#define XCP_E_GENERIC ((Xcp_NegativeResponseCodeType)0x31)
/* The slave internal program verify routine detects an error. */
#define XCP_E_VERIFY ((Xcp_NegativeResponseCodeType)0x32)
/* Access to the requested resource is temporary not possible */
#define XCP_E_RESOURCE_TEMPORARY_NOT_ACCESSIBLE ((Xcp_NegativeResponseCodeType)0x33)

/* @SWS_Xcp_00857 */
#define XCP_E_UNINIT 0x02
#define XCP_E_INVALID_PDUID 0x03
#define XCP_E_INIT_FAILED 0x04
#define XCP_E_PARAM_POINTER 0x12
/* ================================ [ TYPES     ] ============================================== */
typedef struct Xcp_Config_s Xcp_ConfigType;

/* @SWS_Xcp_00846 */
typedef enum {
  XCP_TX_OFF,
  XCP_TX_ON,
} Xcp_TransmissionModeType;

typedef uint16_t Xcp_MsgLenType;

typedef uint8_t Xcp_OpStatusType;

typedef uint8_t Xcp_NegativeResponseCodeType;

typedef struct {
  uint8_t *reqData;
  uint8_t *resData;
  Xcp_MsgLenType reqDataLen;
  Xcp_MsgLenType resDataLen;
  Xcp_MsgLenType resMaxDataLen;
  Xcp_OpStatusType opStatus;
} Xcp_MsgContextType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Xcp_00803 */
void Xcp_Init(const Xcp_ConfigType *ConfigPtr);

/* @SWS_Xcp_00813 */
void Xcp_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/* @SWS_Xcp_00814 */
void Xcp_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/* @SWS_Xcp_00835 */
Std_ReturnType Xcp_CanIfTriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);

/* @SWS_Xcp_00844 */
void Xcp_SetTransmissionMode(NetworkHandleType Channel, Xcp_TransmissionModeType Mode);

/* @SWS_Xcp_00823 */
void Xcp_MainFunction(void);

void Xcp_MainFunction_Write(void);

void Xcp_PerformReset(void);

/* @SWS_Xcp_00807 */
void Xcp_GetVersionInfo(Std_VersionInfoType *versionInfo);
#endif /* XCP_H */
