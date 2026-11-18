/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "RoD.h"
#include "Crc.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_ROD 0
#define AS_LOG_RODE 3
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Rod_ReadData(const RoD_ConfigType *config, uint16_t dataId, const void **pData,
                            uint16_t *size) {
  Std_ReturnType ret = E_NOT_OK;
  const RoD_DataType *dinfo;
  uint16_t crc;

  if ((ROD_MAGIC_NUMBER == config->Magic) && (config->Magic == ((uint32_t)~config->InvMagic)) &&
      (config->NumOfData == ((uint16_t)~config->InvNumOfData)) && (dataId < config->NumOfData)) {
    dinfo = &config->Datas[dataId];
    if (dinfo->Crc == ((uint16_t)~dinfo->InvCrc)) {
      crc = Crc_CalculateCRC16(dinfo->Data, dinfo->Size, 0xFFFF, TRUE);
      if (crc == dinfo->Crc) {
        *pData = dinfo->Data;
        *size = dinfo->Size;
        ret = E_OK;
        ASLOG(ROD, ("[%" PRIu16 "] data %p size %" PRIu16 "\n", dataId, dinfo->Data, dinfo->Size));
      } else {
        ASLOG(RODE, ("[%" PRIu16 "] CRC %" PRIx16 " != %" PRIx16 "\n", dataId, crc, dinfo->Crc));
      }
    }
  } else {
    ASLOG(RODE, ("[%" PRIu16 "] Addr=%p Magic = %08" PRIx8 " %08" PRIx8 " Number %04" PRIx16
                 " %04" PRIx16 "\n",
                 dataId, config, config->Magic, config->InvMagic, config->NumOfData,
                 config->InvNumOfData));
  }

  return ret;
}
