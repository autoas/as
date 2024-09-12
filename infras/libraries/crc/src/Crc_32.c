/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CRCLibrary.pdf
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Crc.h"
#ifndef DISABLE_CRC32
/* ================================ [ MACROS    ] ============================================== */
#define crc_update crc32_update
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#include "crc32/crc32.h"
#include "crc32/crc32.c"
/* ================================ [ FUNCTIONS ] ============================================== */
uint32_t Crc_CalculateCRC32(const uint8_t *Crc_DataPtr, uint32_t Crc_Length,
                            uint32_t Crc_StartValue32, boolean Crc_IsFirstCall) {
  uint32_t u32Crc = Crc_StartValue32;

  if (Crc_IsFirstCall) {
    Crc_StartValue32 = crc_init();
  }

  u32Crc = crc_update(Crc_StartValue32, Crc_DataPtr, Crc_Length);

  return u32Crc;
}
#endif /* DISABLE_CRC32 */
