/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of EEPROM Driver AUTOSAR CP Release 4.4.0
 */
#ifndef EEP_H
#define EEP_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "MemIf.h"
/* ================================ [ MACROS    ] ============================================== */
#define E_EEP_PENDING ((Std_ReturnType)100)
#define E_EEP_INCONSISTENT ((Std_ReturnType)101)
/* ================================ [ TYPES     ] ============================================== */
typedef uint16_t Eep_AddressType;
typedef uint16_t Eep_LengthType;

typedef struct Eep_Config_s Eep_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Eep_Init(const Eep_ConfigType *ConfigPtr);
void Eep_SetMode(MemIf_ModeType Mode);
Std_ReturnType Eep_Read(Eep_AddressType EepromAddress, uint8_t *DataBufferPtr,
                        Eep_LengthType Length);
Std_ReturnType Eep_Write(Eep_AddressType EepromAddress, const uint8_t *DataBufferPtr,
                         Eep_LengthType Length);
Std_ReturnType Eep_Erase(Eep_AddressType EepromAddress, Eep_LengthType Length);
Std_ReturnType Eep_Compare(Eep_AddressType EepromAddress, const uint8_t *DataBufferPtr,
                           Eep_LengthType Length);
void Eep_Cancel(void);
MemIf_StatusType Eep_GetStatus(void);
MemIf_JobResultType Eep_GetJobResult(void);

void Eep_MainFunction(void);
#endif /* EEP_H */
