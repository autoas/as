/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of EEPROM Abstraction AUTOSAR CP Release 4.4.0
 */
#ifndef _EA_PRIV_H
#define _EA_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "MemIf.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {        /* @ECUC_Ea_00040 */
  uint16_t BlockNumber; /* @ECUC_Ea_00130: The block numbers 0x0000 and 0xFFFF shall not be
                         * configurable for a logical block */
  uint16_t BlockAddress;
  uint16_t BlockSize;   /* with CRC */
  /* boolean ImmediateData; */
  uint32_t NumberOfWriteCycles;
  const void *Rom;
} Ea_BlockConfigType;

struct Ea_Config_s {
  void (*JobEndNotification)(void);
  void (*JobErrorNotification)(void);
  const Ea_BlockConfigType *Blocks;
  uint16_t numOfBlocks;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */

#endif /* _EA_PRIV_H */
