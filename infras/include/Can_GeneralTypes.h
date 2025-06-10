/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Driver AUTOSAR CP Release 4.4.0
 */
#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* @SWS_Can_00039 */
#define CAN_BUSY 0x02

#ifdef CAN_VERSION_422
/* define this for the purpose to use RH850 v422 CAN */
#define CAN_CS_UNINIT ((Can_ControllerStateType)0xFF)
#define CAN_CS_STARTED ((Can_ControllerStateType)0)
#define CAN_CS_STOPPED ((Can_ControllerStateType)1)
#define CAN_CS_SLEEP ((Can_ControllerStateType)2)
#else
#define CAN_CS_UNINIT ((Can_ControllerStateType)0)
#define CAN_CS_STARTED ((Can_ControllerStateType)1)
#define CAN_CS_STOPPED ((Can_ControllerStateType)2)
#define CAN_CS_SLEEP ((Can_ControllerStateType)3)
#endif

#define CANTRCV_TRCVMODE_SLEEP ((CanTrcv_TrcvModeType)0)
#define CANTRCV_TRCVMODE_STANDBY ((CanTrcv_TrcvModeType)1)
#define CANTRCV_TRCVMODE_NORMAL ((CanTrcv_TrcvModeType)2)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t Can_ControllerStateType;

/* @SWS_Can_00429 */
typedef uint16_t Can_HwHandleType;

/* @SWS_Can_00416 */
typedef uint32_t Can_IdType;

/* @SWS_Can_00415 */
typedef struct {
  uint8_t *sdu;
  Can_IdType id;
  PduIdType swPduHandle;
  uint8_t length;
} Can_PduType;

/* @SWS_CAN_00496 */
typedef struct {
  Can_IdType CanId;
  Can_HwHandleType Hoh;
  uint8_t ControllerId;
} Can_HwType;

/* @SWS_Can_91021 */
typedef enum {
  CAN_ERROR_BIT_MONITORING1 = 0x01, /* A 0 was transmitted and a 1 was read back */
  CAN_ERROR_BIT_MONITORING0,        /* A 1 was transmitted and a 0 was read back */

  /* The HW reports a CAN bit error but can't report distinguish between CAN_ERROR_BIT_MONITORING1
   * and CAN_ERROR_BIT_MONITORING0 */
  CAN_ERROR_BIT,

  CAN_ERROR_CHECK_ACK_FAILED,       /* Acknowledgement check failed */
  CAN_ERROR_CHECK_ACK_DELIMITER,    /* Acknowledgement delimiter check failed */
  CAN_ERROR_CHECK_ARBITRATION_LOST, /* The sender lost in arbitration. */

  /* CAN overload detected via an overload frame. Indicates that the receive buffers of a receiver
   * are full. */
  CAN_ERROR_CHECK_OVERLOAD,

  CAN_ERROR_CHECK_FORM_FAILED,     /* Violations of the fixed frame format */
  CAN_ERROR_CHECK_STUFFING_FAILED, /* Stuffing bits not as expected */
  CAN_ERROR_CHECK_CRC_FAILED,      /* CRC failed */
  CAN_ERROR_BUS_LOCK,              /* Bus lock (Bus is stuck to dominant level) */
} Can_ErrorType;

/* @SWS_CanTrcv_00163 */
typedef uint8_t CanTrcv_TrcvModeType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* CAN_GENERAL_TYPES_H */
