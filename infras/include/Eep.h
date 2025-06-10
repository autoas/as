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
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define E_EEP_PENDING ((Std_ReturnType)100)
#define E_EEP_INCONSISTENT ((Std_ReturnType)101)
/* ================================ [ TYPES     ] ============================================== */
#ifdef EEP_ADDRESS_TYPE_U32
typedef uint32_t Eep_AddressType;
typedef uint32_t Eep_LengthType;
#else
typedef uint16_t Eep_AddressType;
typedef uint16_t Eep_LengthType;
#endif

typedef struct Eep_Config_s Eep_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
void Eep_AcInit(void);
Std_ReturnType Eep_AcErase(Eep_AddressType address, Eep_LengthType length);
Std_ReturnType Eep_AcWrite(Eep_AddressType address, const uint8_t *data, Eep_LengthType length);
Std_ReturnType Eep_AcRead(Eep_AddressType address, uint8_t *data, Eep_LengthType length);
Std_ReturnType Eep_AcCompare(Eep_AddressType address, uint8_t *data, Eep_LengthType length);
Std_ReturnType Eep_AcBlankCheck(Eep_AddressType address, Eep_LengthType length);
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
#ifdef __cplusplus
}
#endif
#endif /* EEP_H */
