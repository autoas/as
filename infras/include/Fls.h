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

/* @SWS_Fls_00004 */
#define FLS_E_PARAM_CONFIG 0x01
#define FLS_E_PARAM_ADDRESS 0x02
#define FLS_E_PARAM_LENGTH 0x03
#define FLS_E_PARAM_DATA 0x04
#define FLS_E_UNINIT 0x05
#define FLS_E_PARAM_POINTER 0x0a
#define FLS_E_ALREADY_INITIALIZED 0x0b
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
/* @SWS_Fls_00097: Initialize the Flash Access Control module */
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

 /* @ECUC_Fls_00270: Erase a flash memory area
 * @param address: Start address of the flash area to erase
 * @param length: Length of the flash area to erase in bytes
 * @return E_OK: Erase operation completed successfully
 *         E_NOT_OK: Erase operation failed
 *         E_FLS_PENDING: Erase operation is in progress (for asynchronous implementations)
 */
Std_ReturnType Fls_AcErase(Fls_AddressType address, Fls_LengthType length);

/* @ECUC_Fls_00305: Write data to flash memory with read verification
 * The underlying Fls_AcWrite will perform read verification to ensure the data is written
 * to the flash successfully, providing an additional layer of data integrity checking.
 * @param address: Start address in flash memory where data should be written
 * @param data: Pointer to the data buffer to be written
 * @param length: Length of the data to write in bytes
 * @return E_OK: Write operation completed successfully
 *         E_NOT_OK: Write operation failed
 *         E_FLS_PENDING: Write operation is in progress (for asynchronous implementations)
 */
Std_ReturnType Fls_AcWrite(Fls_AddressType address, const uint8_t *data, Fls_LengthType length);

/* @SWS_Fls_00078: Read data from flash memory
 * @param address: Start address in flash memory from where data should be read
 * @param data: Pointer to the buffer where read data will be stored
 * @param length: Length of the data to read in bytes
 * @return E_OK: Read operation completed successfully
 *         E_NOT_OK: Read operation failed
 */
Std_ReturnType Fls_AcRead(Fls_AddressType address, uint8_t *data, Fls_LengthType length);

/* @SWS_Fls_00107: Compare flash memory content with provided data
 * @param address: Start address in flash memory to compare
 * @param data: Pointer to the data buffer to compare with
 * @param length: Length of the data to compare in bytes
 * @return E_OK: Flash content matches the provided data
 *         E_NOT_OK: Flash content does not match the provided data
 */
Std_ReturnType Fls_AcCompare(Fls_AddressType address, uint8_t *data, Fls_LengthType length);
/* @SWS_Fls_00108: Check if a flash memory area is blank (erased)
 * @param address: Start address of the flash area to check
 * @param length: Length of the flash area to check in bytes
 * @return E_OK: Flash area is blank
 *         E_NOT_OK: Flash area is not blank
 */
Std_ReturnType Fls_AcBlankCheck(Fls_AddressType address, Fls_LengthType length);
/* @SWS_Fls_00080: Check if the flash access control module is idle
 * @return TRUE: Flash access control module is idle
 *         FALSE: Flash access control module is busy
 */
boolean Fls_AcIsIdle(void);
/* ================================ [ DATAS     ] ============================================== */
#if defined(linux) || defined(_WIN32)
extern uint8_t g_FlsAcMirror[];
#endif
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

/* @SWS_Fls_00259 */
void Fls_GetVersionInfo(Std_VersionInfoType *versionInfo);
#ifdef __cplusplus
}
#endif
#endif /* FLS_H */
