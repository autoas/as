/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CRCLibrary.pdf
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Crc.h"
/* ================================ [ MACROS    ] ============================================== */
#define crc_update crc8_update
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#include "crc8h2f/crc8h2f.h"
#include "crc8h2f/crc8h2f.c"
/* ================================ [ FUNCTIONS ] ============================================== */
uint8_t Crc_CalculateCRC8(const uint8_t *Crc_DataPtr, uint32_t Crc_Length, uint8_t Crc_StartValue8,
                          boolean Crc_IsFirstCall) {
  uint8_t u8Crc = Crc_StartValue8;

  if (Crc_IsFirstCall) {
    Crc_StartValue8 = crc_init();
  }

  u8Crc = crc_update(Crc_StartValue8, Crc_DataPtr, Crc_Length);

  return u8Crc;
}
