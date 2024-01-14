/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref:
 * https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CRCLibrary.pdf
 */
#ifndef CRC_H
#define CRC_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */

/* ================================ [ TYPES     ] ============================================== */

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Crc_00031 */
uint8_t Crc_CalculateCRC8(const uint8_t *Crc_DataPtr, uint32_t Crc_Length, uint8_t Crc_StartValue8,
                          boolean Crc_IsFirstCall);

/* @SWS_Crc_00043 */
uint8_t Crc_CalculateCRC8H2F(const uint8_t *Crc_DataPtr, uint32_t Crc_Length,
                             uint8_t Crc_StartValue8H2F, boolean Crc_IsFirstCall);

/* @SWS_Crc_00002 */
uint16_t Crc_CalculateCRC16(const uint8_t *Crc_DataPtr, uint32_t Crc_Length,
                            uint16_t Crc_StartValue16, boolean Crc_IsFirstCall);

/* @SWS_Crc_00020 */
uint32_t Crc_CalculateCRC32(const uint8_t *Crc_DataPtr, uint32_t Crc_Length,
                            uint32_t Crc_StartValue32, boolean Crc_IsFirstCall);

#endif /* CRC_H */