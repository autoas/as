/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CRCLibrary.pdf
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Crc.h"
/* ================================ [ MACROS    ] ============================================== */
#define crc_update crc16_update
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#include "crc16/crc16.h"
#include "crc16/crc16.c"
/* ================================ [ FUNCTIONS ] ============================================== */
uint16_t Crc_CalculateCRC16(const uint8_t *Crc_DataPtr, uint32_t Crc_Length,
                            uint16_t Crc_StartValue16, boolean Crc_IsFirstCall) {
  uint16_t u16Crc = Crc_StartValue16;

  if (Crc_IsFirstCall) {
    Crc_StartValue16 = crc_init();
  }

  u16Crc = crc_update(Crc_StartValue16, Crc_DataPtr, Crc_Length);

  return u16Crc;
}
