/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of EEPROM Driver AUTOSAR CP Release 4.4.0
 */
#ifndef _EEP_PRIV_H
#define _EEP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define FLS_CFG_H
#define FLS_ADDRESS_TYPE_U16

#define Fls_AddressType Eep_AddressType
#define Fls_LengthType Eep_LengthType
#define Fls_SectorType Eep_SectorType 
#define Fls_ConfigType Eep_ConfigType
#define Fls_Config_s Eep_Config_s

#define Fls_AcInit Eep_AcInit
#define Fls_AcErase Eep_AcErase
#define Fls_AcWrite Eep_AcWrite
#define Fls_AcRead Eep_AcRead
#define Fls_AcCompare Eep_AcCompare
#define Fls_AcBlankCheck Eep_AcBlankCheck

#define Fls_Init Eep_Init
#define Fls_Erase Eep_Erase
#define Fls_Write Eep_Write
#define Fls_Cancel Eep_Cancel
#define Fls_GetStatus Eep_GetStatus
#define Fls_GetJobResult Eep_GetJobResult
#define Fls_Read Eep_Read
#define Fls_Compare Eep_Compare
#define Fls_SetMode Eep_SetMode
#define Fls_BlankCheck Eep_BlankCheck
#define Fls_MainFunction Eep_MainFunction

#define Fls_Config Eep_Config
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ ALIAS     ] ============================================== */
#include "Fls.h"
#include "../Fls/Fls_Priv.h"
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _EEP_PRIV_H */
