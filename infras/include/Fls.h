/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash Driver AUTOSAR CP Release 4.4.0
 */
#ifndef FLS_H
#define FLS_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "MemIf.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define E_FLS_PENDING ((Std_ReturnType)100)
#define E_FLS_INCONSISTENT ((Std_ReturnType)101)
/* ================================ [ TYPES     ] ============================================== */
#ifdef FLS_ADDRESS_TYPE_U16
typedef uint16_t Fls_AddressType;
typedef uint16_t Fls_LengthType;
#else
typedef uint32_t Fls_AddressType;
typedef uint32_t Fls_LengthType;
#endif

typedef struct Fls_Config_s Fls_ConfigType;
/* ================================ [ DECLARES  ] ============================================== */
void Fls_AcInit(void);
/*   For Flash AC Erase/Write, the HW engine generally require the driver located in RAM,
 * and during the programing phase, Flash is not accessable maybe for some MCUs. So
 * for those type of MCU flash, below API are generally synchronous.
 *
 *   But in the market, there is some kind of MCU flash that alow programing and execution from
 * flash can happen at the same time. For such kind of MCU flash, the Erase/Write API can be
 * asynchronous. The first call just start the flash engine and the API should return E_FLS_PENDING,
 * thus this Fls driver will know that job is accepted by the flash engine and this Fls driver will
 * call the API again on the next schedule with exactly the same parameters again and gain if the
 * fls engine is busy and the called API should still return E_FLS_PENDING, until fls engine
 * completes the job sucessfully and return E_OK or there is HW error and return E_NOT_OK, also
 * timeout strategy can be implemented by the Fls AC.
 */
/* @ECUC_Fls_00270 */
Std_ReturnType Fls_AcErase(Fls_AddressType address, Fls_LengthType length);
/* @ECUC_Fls_00305 */
Std_ReturnType Fls_AcWrite(Fls_AddressType address, const uint8_t *data, Fls_LengthType length);
Std_ReturnType Fls_AcRead(Fls_AddressType address, uint8_t *data, Fls_LengthType length);
Std_ReturnType Fls_AcCompare(Fls_AddressType address, uint8_t *data, Fls_LengthType length);
Std_ReturnType Fls_AcBlankCheck(Fls_AddressType address, Fls_LengthType length);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Fls_Init(const Fls_ConfigType *ConfigPtr);
Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length);
Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8_t *SourceAddressPtr,
                         Fls_LengthType Length);
void Fls_Cancel(void);
MemIf_StatusType Fls_GetStatus(void);
MemIf_JobResultType Fls_GetJobResult(void);
Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8_t *TargetAddressPtr,
                        Fls_LengthType Length);
Std_ReturnType Fls_Compare(Fls_AddressType SourceAddress, const uint8_t *TargetAddressPtr,
                           Fls_LengthType Length);
void Fls_SetMode(MemIf_ModeType Mode);
Std_ReturnType Fls_BlankCheck(Fls_AddressType TargetAddress, Fls_LengthType Length);

void Fls_MainFunction(void);
#ifdef __cplusplus
}
#endif
#endif /* FLS_H */
