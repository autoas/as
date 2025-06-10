/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Communication Manager AUTOSAR CP Release 4.3.1
 */
#ifndef COMM_H
#define COMM_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Rte_ComM_Type.h"
/* ================================ [ MACROS    ] ============================================== */
#define COMM_NO_COM_NO_PENDING_REQUEST ((ComM_StateType)0)
#define COMM_NO_COM_REQUEST_PENDING ((ComM_StateType)1)
#define COMM_FULL_COM_NETWORK_REQUESTED ((ComM_StateType)2)
#define COMM_FULL_COM_READY_SLEEP ((ComM_StateType)3)
#define COMM_SILENT_COM ((ComM_StateType)4)

#define COMM_WAKEUP_INHIBITION_ACTIVE ((ComM_InhibitionStatusType)0x01)
#define COMM_LIMITED_TO_NO_COM ((ComM_InhibitionStatusType)0x02)

/* @SWS_ComM_00234 */
#define COMM_E_UNINIT 0x01
#define COMM_E_WRONG_PARAMETERS 0x02
#define COMM_E_PARAM_POINTER 0x03
#define COMM_E_INIT_FAILED 0x04
/* ================================ [ TYPES     ] ============================================== */
typedef struct ComM_Config_s ComM_ConfigType;

/* @SWS_ComM_00674 */
typedef uint8_t ComM_StateType;

/* @SWS_ComM_00669 */
typedef uint8_t ComM_InhibitionStatusType;

/* @SWS_ComM_00670 */
typedef uint8_t ComM_UserHandleType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_ComM_00146 */
void ComM_Init(const ComM_ConfigType *ConfigPtr);

/* @SWS_ComM_00147 */
void ComM_DeInit(void);

/* @SWS_ComM_00872 */
Std_ReturnType ComM_GetState(NetworkHandleType Channel, ComM_StateType *State);

/* @SWS_ComM_00619 */
Std_ReturnType ComM_GetInhibitionStatus(NetworkHandleType Channel,
                                        ComM_InhibitionStatusType *Status);

/* @SWS_ComM_00110 */
Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode);

/* @SWS_ComM_00079 */
Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType User, ComM_ModeType *ComMode);

/* @SWS_ComM_00083 */
Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType *ComMode);

/* @SWS_ComM_00156 */
Std_ReturnType ComM_PreventWakeUp(NetworkHandleType Channel, boolean Status);

/* @SWS_ComM_00163 */
Std_ReturnType ComM_LimitChannelToNoComMode(NetworkHandleType Channel, boolean Status);

/* @SWS_ComM_00124 */
Std_ReturnType ComM_LimitECUToNoComMode(boolean Status);

/* @SWS_ComM_00224 */
Std_ReturnType ComM_ReadInhibitCounter(uint16 *CounterValue);

/* @SWS_ComM_00108 */
Std_ReturnType ComM_ResetInhibitCounter(void);

/* @SWS_ComM_00552 */
Std_ReturnType ComM_SetECUGroupClassification(ComM_InhibitionStatusType Status);

/* @SWS_ComM_00383 */
void ComM_Nm_NetworkStartIndication(NetworkHandleType Channel);

/* @SWS_ComM_00390 */
void ComM_Nm_NetworkMode(NetworkHandleType Channel);

/* @SWS_ComM_00391 */
void ComM_Nm_PrepareBusSleepMode(NetworkHandleType Channel);

/* @SWS_ComM_00392 */
void ComM_Nm_BusSleepMode(NetworkHandleType Channel);

/* @SWS_ComM_00792 */
void ComM_Nm_RestartIndication(NetworkHandleType Channel);

/* @SWS_ComM_00873 */
void ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel);

/* @SWS_ComM_00874 */
void ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel);

/* @SWS_ComM_00275 */
void ComM_EcuM_WakeUpIndication(NetworkHandleType Channel);

/* @SWS_ComM_91001 */
void ComM_EcuM_PNCWakeUpIndication(PNCHandleType PNCid);

/* @SWS_ComM_00871 */
void ComM_CommunicationAllowed(NetworkHandleType Channel, boolean Allowed);

/* @SWS_ComM_00675 */
void ComM_BusSM_ModeIndication(NetworkHandleType Channel, ComM_ModeType ComMode);

/* @SWS_ComM_00429 */
void ComM_MainFunction(void);
#endif /* COMM_H */
