/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef BL_H
#define BL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include "Std_Critical.h"
#include "Flash.h"
#include "Crc.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_BL -1
#define AS_LOG_BLI 1
#define AS_LOG_BLE 2

#define BL_FLASH_IDENTIFIER 0xFF
#define BL_EEPROM_IDENTIFIER 0xEE
#define BL_FLSDRV_IDENTIFIER 0xFD
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t addressLow;
  uint32_t addressHigh;
  uint8_t identifier;
} BL_MemoryInfoType;
/* ================================ [ DECLARES  ] ============================================== */
extern boolean BL_IsUpdateRequested(void);
extern void BL_JumpToApp(void);
extern Std_ReturnType BL_GetProgramCondition(Dcm_ProgConditionsType **cond);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void BL_Init(void);
void BL_SessionReset(void);
Std_ReturnType BL_CheckAppIntegrity(void);
void BL_CheckAndJump(void);
#endif /* BL_H */
