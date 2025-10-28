/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash EEPROM Emulation AUTOSAR CP Release 4.4.0
 */
#ifndef FEE_PRIV_H
#define FEE_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#include "MemIf.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_FEE

#define FEE_MAGIC_NUMBER                                                                           \
  (((uint32_t)'F' << 24) | ((uint32_t)'E' << 16) | ((uint32_t)'E' << 8) | ((uint32_t)'F'))

#ifndef FLS_ERASED_VALUE
#define FLS_ERASED_VALUE 0xFFu
#endif
/* For most of the Flash, after erase, the flash is all 0xFF.
 * But in fact, for some type of flash, the flash value maybe uncertaion after erase.
 * Then for this kind of flash, we need to use Fls_BankCheck.
 */
#define FEE_BANK_NOT_FULL_MAGIC                                                                    \
  (((uint32_t)FLS_ERASED_VALUE << 24) | ((uint32_t)FLS_ERASED_VALUE << 16) |                       \
   ((uint32_t)FLS_ERASED_VALUE << 8) | ((uint32_t)FLS_ERASED_VALUE))

#define FEE_BANK_FULL_MAGIC                                                                        \
  (((uint32_t)'D' << 24) | ((uint32_t)'E' << 16) | ((uint32_t)'A' << 8) | ((uint32_t)'D'))

#define FEE_INVALID_ADDRESS ((uint32_t)0xFFFFFFFFul)

#ifndef FEE_PAGE_SIZE
#define FEE_PAGE_SIZE 8u
#endif

#define ALIGNED(sz, alignsz) ((sz + alignsz - 1u) & (~(alignsz - 1u)))
#define FEE_ALIGNED(sz) ALIGNED(sz, FEE_PAGE_SIZE)

#define FEE_FAULTS_SIZE FEE_ALIGNED(sizeof(Fee_BlockType))

#ifndef FEE_CONST
#define FEE_CONST
#endif
/* ================================ [ TYPES     ] ============================================== */
/* Mapping of Fee Banks, paddings are stripped
 * High:
 *       | data ...     | Crc | ~Crc | <-+
 *   +-> | data ...     | Crc | ~Crc |   |
 *   |   | ...                       |   | ----------\
 *   |   | ...                       |   |           + Free Space
 *   |   | ...                       |   | ----------/
 *   |   | BN  | BS   |  Crc |  ~CRC |   | ----------\
 *   |   |   Address  |  NoOfWrite   |   |           + Admin Data of Fee Block {BN}
 *   |         + ------------------------+           |
 *   |   |   Address  |  NoOfWrite   | ------------/
 *   |   |  BN | BS   | Crc | ~CRC   | ----------\
 *   +--------+                      |            + Admin Data of Fee Block {BN}
 *       |  Address   |  NoOfWrite   | ----------/
 *       |       Full Magic          | <- Status -\
 *       | Number     | ~ Number     | <- Info     + <- Bank Admin
 * Low:  | FEE Magic  | ~ FEE Magic  | <- Header -/
 */
typedef struct {
  uint32_t Address; /* Data Address */
  uint32_t NumberOfWriteCycles;
  uint16_t BlockNumber;
  uint16_t BlockSize;
  /* CRC of the above admin data, this doesn't include the CRC of the data block,
   * the data block has its own CRC.
   * Somehow this is not perfect, it was much more better that the Crc include
   * both the admin block and the data block.
   */
  uint16_t Crc;
  uint16_t InvCrc;
} Fee_BlockType;

typedef struct {
  uint32_t MagicNumber;
  uint32_t InvMagicNumber;
} Fee_BankHeaderMagicType;

typedef struct {
  uint32_t Number; /* Total Erased Numbers of this Bank */
  uint32_t InvNumber;
} Fee_BankInfoType;

typedef struct {
  /* any other value of FEE_BANK_NOT_FULL_MAGIC means this block is full, the usefull data should be
   * copied to the other empty bank, after the copy is done, this bank should be erased */
  uint32_t FullMagic;
} Fee_BankStatusType;

typedef struct {
  Fee_BankHeaderMagicType HeaderMagic;
#if FEE_PAGE_SIZE > 8 /*sizeof(Fee_BankMagicType) */
  uint8_t _padding1[FEE_PAGE_SIZE - 8u];
#endif
  Fee_BankInfoType Info;
#if FEE_PAGE_SIZE > 8 /*sizeof(Fee_BankInfoType) */
  uint8_t _padding2[FEE_PAGE_SIZE - 8u];
#endif
  Fee_BankStatusType Status;
#if FEE_PAGE_SIZE > 4 /*sizeof(Fee_BankInfoType) */
  uint8_t _padding3[FEE_PAGE_SIZE - 4u];
#endif
#ifdef FEE_USE_FAULTS
  /* a space used to track FEE faults, support up to 255 fault: 0-254 */
  uint8_t faults[FEE_FAULTS_SIZE];
#endif
  uint8_t blocks[FEE_PAGE_SIZE];
} Fee_BankAdminType;

typedef struct {
  uint32_t LowAddress;
  uint32_t HighAddress;
} Fee_BankType;

typedef struct {        /* @ECUC_Fee_00040 */
  uint16_t BlockNumber; /* @SWS_Fee_00006: The block numbers 0x0000 and 0xFFFF shall not be
                         * configurable for a logical block */
  uint16_t BlockSize;   /* without CRC */
  /* boolean ImmediateData; */
  uint32_t NumberOfWriteCycles;
  const uint8_t *Rom;
} Fee_BlockConfigType;

typedef struct {
  uint32_t Address;
#ifndef FLS_DIRECT_ACCESS /* sacrifice RAM to cache the NumberOfWriteCycles */
  uint32_t NumberOfWriteCycles;
#endif
} Fee_BlockContextType;

struct Fee_Config_s {
  void (*JobEndNotification)(void);
  void (*JobErrorNotification)(void);
  Fee_BlockContextType *blockContexts;
  P2CONST(Fee_BlockConfigType, AUTOMATIC, FEE_CONST) Blocks;
  uint16_t numOfBlocks;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) Banks;
  uint16_t numOfBanks;
  uint8_t *workingArea;
  uint16_t sizeOfWorkingArea;
  uint8_t maxJobRetry;
  uint16_t maxDataSize;
  uint32_t NumberOfErasedCycles;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#ifdef __cplusplus
}
#endif
#endif /* FEE_PRIV_H */
