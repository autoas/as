/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash EEPROM Emulation AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Fee.h"
#include "Fee_Priv.h"
#include "Fls.h"
#include "Std_Debug.h"
#include "Std_Critical.h"
#include "Crc.h"
#include <string.h>
#include <stddef.h>
#if defined(_WIN32) || defined(linux)
#include <assert.h>
#endif
#include "factory.h"
#include "GEN/Fee_Factory.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_FEE 0
#define AS_LOG_FEEI 0
#define AS_LOG_FEEE 2

#define FEE_CONFIG (&Fee_Config)

#define FACTORY_E_FEE_RETRY E_OK

#define FEE_E_EXIST 0x10

#ifndef FEE_MAX_ADMIN_READ_PER_CYCLE
#define FEE_MAX_ADMIN_READ_PER_CYCLE 100
#endif

#define ALIGNED(sz, alignsz) ((sz + alignsz - 1) & (~(alignsz - 1)))
#define FEE_ALIGNED(sz) ALIGNED(sz, FEE_PAGE_SIZE)
/* additional 4 for u16Crc and u16InvCrc */
#define FEE_DATA_ALIGNED(sz) FEE_ALIGNED(ALIGNED(sz, 2) + 4)

#define FEE_MIN_FREE_SPACE                                                                         \
  (FEE_DATA_ALIGNED(FEE_CONFIG->maxDataSize) + FEE_ALIGNED(sizeof(Fee_BlockType)))

#define FEE_BLOCK_ADMIN_AND_DATA_SIZE(dataSize)                                                    \
  (FEE_DATA_ALIGNED(dataSize) + FEE_ALIGNED(sizeof(Fee_BlockType)))

#if defined(_WIN32) || defined(linux)
#define PTR_TO_U32(ptr) ((uint32_t)(uint64_t)(ptr))
#define U32_TO_U8PTR(u32) ((uint8_t *)(uint64_t)(u32))
#else
#define PTR_TO_U32(ptr) ((uint32_t)(ptr))
#define U32_TO_U8PTR(u32) ((uint8_t *)(u32))
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint32_t adminFreeAddr; /* bank low address */
  uint32_t dataFreeAddr;  /* bank high address */
  uint8_t curWrokingBank;
  uint8_t retryCounter;
  struct {
    uint16_t BlockId;
    uint16_t BlockOffset;
    /* For backup, DataBufferPtr = adminFreeAddr of full bank  */
    uint8_t *DataBufferPtr;
    /* For backup, Length = Number of full bank  */
    uint32_t Length;
    uint32_t NextAddr;
  } job;
} Fee_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern const Fee_ConfigType Fee_Config;

#if defined(_WIN32) || defined(linux)
extern uint8_t g_FlsAcMirror[];
#define FEE_ADDRESS(v) ((void *)(&g_FlsAcMirror[(v)]))
#else
#define FEE_ADDRESS(v) ((void *)(v))
#endif
/* ================================ [ DATAS     ] ============================================== */
static Fee_ContextType Fee_Context;
/* ================================ [ LOCALS    ] ============================================== */
static void Fee_Panic(void) {
  ASLOG(
    FEEE,
    ("panic during %s:%s\n", Fee_Factory.machines[Fee_Factory.context->machineId].name,
     Fee_Factory.machines[Fee_Factory.context->machineId].nodes[Fee_Factory.context->nodeId].name));
#if defined(_WIN32) || defined(linux)
  assert(0);
#endif
}

static Std_ReturnType Fee_FlsWrite(uint32_t address, uint8_t *data, uint16_t length) {
  Std_ReturnType r;
  uint16_t alignedLen = FEE_ALIGNED(length);

  ASLOG(FEE, ("Fee write @%X, len=%d, data: %02X %02X %02X %02X %02X %02X %02X %02X\n", address,
              length, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

  if (alignedLen > length) {
    memset(&data[length], FLS_ERASED_VALUE, alignedLen - length);
  }
  r = Fls_Write(address, data, alignedLen);

  return r;
}

static boolean Fee_IsAllErased(uint8_t *data, uint16_t size) {
  boolean r = TRUE;
  int i;

  for (i = 0; i < size; i++) {
    if (FLS_ERASED_VALUE != data[i]) {
      r = FALSE;
      break;
    }
  }

  return r;
}

static Std_ReturnType Fee_DoWrite_Admin(boolean fromBackup) {
  Std_ReturnType r;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BlockType *admin;
  uint32_t address;
  uint16_t size;
  uint32_t NumberOfWriteCycles = 1;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block;
#endif

#ifdef FLS_DIRECT_ACCESS
  if (FEE_INVALID_ADDRESS != config->blockAddress[context->job.BlockId]) {
    block = (const Fee_BlockType *)FEE_ADDRESS(config->blockAddress[context->job.BlockId]);
    if (fromBackup) {
      NumberOfWriteCycles = block->NumberOfWriteCycles;
    } else {
      NumberOfWriteCycles = block->NumberOfWriteCycles + 1;
      if (NumberOfWriteCycles > config->Blocks[context->job.BlockId].NumberOfWriteCycles) {
        NumberOfWriteCycles = config->Blocks[context->job.BlockId].NumberOfWriteCycles;
        ASLOG(FEEE, ("trigger dead block %d write\n", context->job.BlockId));
        Fee_Panic();
      }
    }
  }
#endif

  size = config->Blocks[context->job.BlockId].BlockSize;
  address = context->dataFreeAddr - FEE_DATA_ALIGNED(size);
  admin = (Fee_BlockType *)config->workingArea;
  admin->BlockNumber = context->job.BlockId + 1;
  admin->InvBlockNumber = ~(context->job.BlockId + 1);
  admin->Address = address;
  admin->NumberOfWriteCycles = NumberOfWriteCycles;
  admin->BlockSize = size;
  admin->Crc = Crc_CalculateCRC16((uint8_t *)admin, offsetof(Fee_BlockType, Crc), 0, TRUE);
  r = Fee_FlsWrite(context->adminFreeAddr, (uint8_t *)admin, sizeof(Fee_BlockType));
  if (E_OK == r) {
    context->adminFreeAddr += FEE_ALIGNED(sizeof(Fee_BlockType));
    context->dataFreeAddr -= FEE_DATA_ALIGNED(size);
  }

  return r;
}

static Std_ReturnType Fee_DoWrite_Data(uint8_t *jobDataBufferPtr) {
  Std_ReturnType r;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address;
  uint16_t size;
  uint8_t *data;
  uint16_t *crc;
  uint16_t *invCrc;
  int i;
  size = config->Blocks[context->job.BlockId].BlockSize;
  address = context->dataFreeAddr;
  data = config->workingArea;
  if (data != jobDataBufferPtr) {
    memcpy(data, jobDataBufferPtr, size);
    for (i = size; i < FEE_DATA_ALIGNED(size) - 4; i++) {
      data[i] = FLS_ERASED_VALUE;
    }
    crc = (uint16_t *)(data + FEE_DATA_ALIGNED(size) - 4);
    invCrc = crc + 1;
    *crc = Crc_CalculateCRC16(data, FEE_DATA_ALIGNED(size) - 4, 0, TRUE);
    *invCrc = ~(*crc);
  }
  r = Fee_FlsWrite(address, data, FEE_DATA_ALIGNED(size));
  return r;
}

#ifdef FLS_DIRECT_ACCESS
static Std_ReturnType Fee_IsDataCrcOK(uint32_t address, uint16_t size) {
  Std_ReturnType r = E_NOT_OK;
  const uint8_t *pData = (uint8_t *)FEE_ADDRESS(address);
  const uint16_t *pCrc = (uint16_t *)FEE_ADDRESS(address + FEE_DATA_ALIGNED(size) - 4);
  const uint16_t *pInvCrc = pCrc + 1;
  uint16_t Crc;

  if (((uint16_t)(~(*pCrc)) == (*pInvCrc))) {
    Crc = Crc_CalculateCRC16(pData, FEE_DATA_ALIGNED(size) - 4, 0, TRUE);

    if ((*pCrc) == Crc) {
      r = E_OK;
    } else {
      ASLOG(FEEE, ("Data @ %X, CRC E %X != R %X\n", address, Crc, *pCrc));
    }
  } else {
    ASLOG(FEEE, ("Data @ %X, CRC %X != ~ %X\n", address, *pCrc, *pInvCrc));
  }

  return r;
}
#endif

static boolean Fee_SearchFreeSpace_Analyze(const Fee_BlockType *blocks, uint32_t numOfBlocks) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BlockType *block;
  boolean ending = FALSE;
  uint16_t Crc;
  int i;

  for (i = 0; i < numOfBlocks; i++) {
    block = (Fee_BlockType *)(((size_t)blocks) + FEE_ALIGNED(sizeof(Fee_BlockType)) * i);
    if (block->BlockNumber == ((uint16_t)~block->InvBlockNumber)) {
      Crc = Crc_CalculateCRC16((uint8_t *)block, offsetof(Fee_BlockType, Crc), 0, TRUE);
      if (Crc == block->Crc) {
        /* During sudden power off test, find that sometimes CRC is correct
         * for the admin block, but in fact, it is not correct, so it can't panic */
        if ((block->BlockNumber <= config->numOfBlocks) && (block->BlockNumber > 0)) {
          if (block->BlockSize == config->Blocks[block->BlockNumber - 1].BlockSize) {
            context->dataFreeAddr -= FEE_DATA_ALIGNED(block->BlockSize);
            if (block->Address == context->dataFreeAddr) {
#ifdef FLS_DIRECT_ACCESS
              Std_ReturnType r = Fee_IsDataCrcOK(block->Address, block->BlockSize);
              if (E_OK == r) {
                config->blockAddress[block->BlockNumber - 1] = context->adminFreeAddr;
#else
              config->blockAddress[block->BlockNumber - 1] = context->dataFreeAddr;
#endif
                ASLOG(FEEI, ("Got block number %d @ %X, %X\n", block->BlockNumber,
                             context->adminFreeAddr, context->dataFreeAddr));
#ifdef FLS_DIRECT_ACCESS
              } else {
                ASLOG(FEEE, ("block number %d has invalid data\n", block->BlockNumber));
              }
#endif
            } else {
              ASLOG(
                FEEE,
                ("Got block number %d with invalid address %X (!=%X), maybe FEE config update\n",
                 block->BlockNumber, block->Address, context->dataFreeAddr));
            }
          } else {
            ASHEXDUMP(FEEE,
                      ("Got block number %d with invalid size %d (!=%d), maybe FEE config update",
                       block->BlockNumber, block->BlockSize,
                       config->Blocks[blocks->BlockNumber - 1].BlockSize),
                      blocks, sizeof(Fee_BlockType));
          }
        } else {
          ASLOG(FEEE,
                ("Got an invalid block number %d, maybe FEE config update\n", block->BlockNumber));
        }
      } else {
        ASLOG(FEEE, ("Got an invalid block number %d with wrong crc (R %X != E %X)\n",
                     block->BlockNumber, block->Crc, Crc));
        /* Invalid Block: note, it's always write block admin firstly and then write data */
      }
      context->adminFreeAddr += FEE_ALIGNED(sizeof(Fee_BlockType));
    } else if (Fee_IsAllErased((uint8_t *)block, FEE_ALIGNED(sizeof(Fee_BlockType)))) {
      /* TODO: For some type of flash, the erased value maybe uncertaion, such as tc387,
       * the right way is using Fls_BlankCheck to determine that the block is erased or not */
      ending = TRUE;
      break; /* searching Free Space End */
    } else {
      /* also invalid block */
      context->adminFreeAddr += FEE_ALIGNED(sizeof(Fee_BlockType));
    }

    if ((context->dataFreeAddr - context->adminFreeAddr) < FEE_MIN_FREE_SPACE) {
      ending = TRUE;
      break;
    }
  }

  return ending;
}

static Std_ReturnType Fee_DoSearchNextValidDataStart(uint8_t bankId) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address;
  uint32_t blockAdminLow;
  Std_ReturnType r = FEE_E_EXIST;
#ifdef FLS_DIRECT_ACCESS
  uint32_t numOfBlocks = FEE_MAX_ADMIN_READ_PER_CYCLE;
#else
  uint32_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
#endif
  blockAdminLow = config->Banks[bankId].LowAddress + offsetof(Fee_BankAdminType, blocks);
  if (context->job.NextAddr > numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType))) {
    address = context->job.NextAddr - numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType));
  } else {
    address = 0;
  }

  if (address < blockAdminLow) {
    address = blockAdminLow;
    numOfBlocks = (context->job.NextAddr - blockAdminLow) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  if (numOfBlocks > 0) {
#ifndef FLS_DIRECT_ACCESS
    ASLOG(FEE, ("  seraching from 0x%X, %d blocks\n", address, numOfBlocks));
    r = Fls_Read(address, config->workingArea, numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)));
#else
    r = E_OK;
#endif
  }

  return r;
}

static Std_ReturnType Fee_DoSearchNextValidDataEnd(uint8_t bankId) {
  Std_ReturnType r = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address;
  uint32_t blockAdminLow;
  uint16_t Crc;
  int i;
#ifdef FLS_DIRECT_ACCESS
  uint32_t numOfBlocks = FEE_MAX_ADMIN_READ_PER_CYCLE;
#else
  uint32_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
#endif
  Fee_BlockType *block;
  blockAdminLow = config->Banks[bankId].LowAddress + offsetof(Fee_BankAdminType, blocks);
  if (context->job.NextAddr > numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType))) {
    address = context->job.NextAddr - numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType));
  } else {
    address = 0;
  }

  if (address < blockAdminLow) {
    address = blockAdminLow;
    numOfBlocks = (context->job.NextAddr - blockAdminLow) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  for (i = numOfBlocks - 1; i >= 0; i--) {
    context->job.NextAddr -= FEE_ALIGNED(sizeof(Fee_BlockType));
#ifdef FLS_DIRECT_ACCESS
    block = (Fee_BlockType *)FEE_ADDRESS(context->job.NextAddr);
#else
    block = (Fee_BlockType *)(config->workingArea + i * FEE_ALIGNED(sizeof(Fee_BlockType)));
#endif
    if ((block->BlockNumber == (context->job.BlockId + 1)) &&
        (block->BlockNumber == ((uint16_t)~block->InvBlockNumber))) {
      Crc = Crc_CalculateCRC16((uint8_t *)block, offsetof(Fee_BlockType, Crc), 0, TRUE);
      if (Crc == block->Crc) { /* bing go, valid block */
        if (block->BlockSize == config->Blocks[block->BlockNumber - 1].BlockSize) {
#ifdef FLS_DIRECT_ACCESS
          r = Fee_IsDataCrcOK(block->Address, block->BlockSize);
          if (E_OK == r) {
            config->blockAddress[context->job.BlockId] = context->job.NextAddr;
#else
          config->blockAddress[context->job.BlockId] = block->Address;
#endif
            r = E_OK;
#ifdef FLS_DIRECT_ACCESS
          } else {
            ASLOG(FEEE, ("block %d has invalid data\n", context->job.BlockId));
          }
#endif
          break;
        } else {
          ASLOG(FEEE, ("block %d size mismatch, config updated\n", context->job.BlockId));
        }
      } else {
        ASHEXDUMP(FEEE,
                  ("block %d admin @ %X CRC NOK, E %X != R %X", context->job.BlockId,
                   context->job.NextAddr, Crc, block->Crc),
                  &block, sizeof(Fee_BlockType));
      }
    }
  }

  return r;
}

static Std_ReturnType Fee_DoBackup_Copy_Start(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank;
  uint8_t nextBank = context->curWrokingBank + 1;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ASLOG(FEE, ("do backup copy from bank %d to bank %d\n", context->curWrokingBank, nextBank));
  /* using DataBufferPtr to record current bank adminFreeAddr */
  context->job.DataBufferPtr = U32_TO_U8PTR(context->adminFreeAddr);
  context->curWrokingBank = nextBank;
  context->adminFreeAddr = bank->LowAddress + offsetof(Fee_BankAdminType, blocks);
  context->dataFreeAddr = bank->HighAddress;
  context->job.BlockId = 0;

  return factory_goto(BACKUP_COPY_ADMIN);
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Fee_FactoryStateNotification(uint8_t machineId, machine_state_t state) {
  const Fee_ConfigType *config = FEE_CONFIG;
  switch (machineId) {
  case FEE_MACHINE_READ:
  case FEE_MACHINE_WRITE:
    switch (state) {
    case MACHINE_STOP:
      config->JobEndNotification();
      break;
    case MACHINE_FAIL:
      config->JobErrorNotification();
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

/* Firstly, read and check the admin data of each bank, if the magic number was invalid,
 * erase it, else goto read next admin data. All the admin data will be stored into
 * the workingArea.
 * Secondly, when all bank admin data has been read, goto check each part of admin data:
 * 1) check info record was correct, if it was not correct, set it according to the info
 * from other banks', generally, the the max erase number of all the valid banks'.
 * 2) check the magic number was correct, if not correct, set it to the right one.
 * Then the last step, decide current working bank and search free space. But there is case
 * that maybe current working bank is full and back up is not finished yet, then backup
 * will be done again.
 * Please notice that backup always start from ensure the next bank erased and then do copy data
 * from begin.
 */
Std_ReturnType Fee_Init_ReadBankAdmin_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin;
  const Fee_BankType *bank;
#ifndef FLS_DIRECT_ACCESS
  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(INIT_CHECK_BANK_INFO);
  } else {
    bank = &config->Banks[context->curWrokingBank];
    bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
    ret = Fls_Read(bank->LowAddress, (uint8_t *)bankAdmin, sizeof(*bankAdmin));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  }
#else
  while ((context->curWrokingBank < config->numOfBanks) && (E_NOT_OK == ret)) {
    bank = &config->Banks[context->curWrokingBank];
    bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
    memcpy(bankAdmin, FEE_ADDRESS(bank->LowAddress), sizeof(*bankAdmin));
    if ((FEE_MAGIC_NUMBER != bankAdmin->HeaderMagic.MagicNumber) ||
        (((uint32_t)~FEE_MAGIC_NUMBER) != bankAdmin->HeaderMagic.InvMagicNumber)) {
      ASLOG(FEEE, ("FEE Bank %d is invalid, erase it\n", context->curWrokingBank));
      ret = factory_goto(INIT_ERASE_INVALID_BANK);
    } else {
#ifdef FEE_USE_BLANK_CHECK
      ret = factory_goto(INIT_BLANK_CHECK_INFO);
#else /* This Bank is valid, moving to read next bank admin data */
      context->curWrokingBank++;
#endif
    }
  }
  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(INIT_CHECK_BANK_INFO);
  }
#endif

  return ret;
}

Std_ReturnType Fee_Init_ReadBankAdmin_End(void) {
  Std_ReturnType ret = E_NOT_OK;
#ifndef FLS_DIRECT_ACCESS
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  if ((FEE_MAGIC_NUMBER != bankAdmin->HeaderMagic.MagicNumber) ||
      (((uint32_t)~FEE_MAGIC_NUMBER) != bankAdmin->HeaderMagic.InvMagicNumber)) {
    ASLOG(FEEE, ("FEE Bank %d is invalid, erase it\n", context->curWrokingBank));
    ret = factory_goto(INIT_ERASE_INVALID_BANK);
  } else {
#ifdef FEE_USE_BLANK_CHECK
    ret = factory_goto(INIT_BLANK_CHECK_INFO);
#else /* This Bank is valid, moving to read next bank admin data */
    context->curWrokingBank++;
    ret = factory_goto(INIT_READ_BANK_ADMIN);
#endif
  }
#endif
  return ret;
}

Std_ReturnType Fee_Init_ReadBankAdmin_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Init_BlankCheckInfo_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank;

  bank = &config->Banks[context->curWrokingBank];
  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, Info),
                       FEE_ALIGNED(sizeof(Fee_BankInfoType)));
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Init_BlankCheckInfo_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  /* info is in erased state */
  memset(&bankAdmin->Info, FLS_ERASED_VALUE, sizeof(bankAdmin->Info));
  return factory_goto(INIT_BLANK_CHECK_BLOCK);
}

Std_ReturnType Fee_Init_BlankCheckInfo_Error(void) {
  Fee_ContextType *context = &Fee_Context;
  /* info has data, trust it */
  context->retryCounter = 0;

  return factory_goto(INIT_BLANK_CHECK_BLOCK);
}

Std_ReturnType Fee_Init_BlankCheckBlock_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank;

  bank = &config->Banks[context->curWrokingBank];
  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, blocks), FEE_PAGE_SIZE);
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Init_BlankCheckBlock_End(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  /* This bank has been erased, moving to read next bank admin data */
  memset(bankAdmin->blocks, FLS_ERASED_VALUE, FEE_PAGE_SIZE);
  context->curWrokingBank++;
  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(INIT_CHECK_BANK_INFO);
  } else {
    ret = factory_goto(INIT_READ_BANK_ADMIN);
  }
  return ret;
}

Std_ReturnType Fee_Init_BlankCheckBlock_Error(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  /* block has data, trust it */
  context->retryCounter = 0;
  context->curWrokingBank++;

  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(INIT_CHECK_BANK_INFO);
  } else {
    ret = factory_goto(INIT_READ_BANK_ADMIN);
  }
  return ret;
}

Std_ReturnType Fee_Init_EraseInvalidBank_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank;

  bank = &config->Banks[context->curWrokingBank];
  ret = Fls_Erase(bank->LowAddress, bank->HighAddress - bank->LowAddress);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = E_OK;
  }
  return ret;
}

Std_ReturnType Fee_Init_EraseInvalidBank_End(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  /* This bank has been erased, moving to read next bank admin data */
  memset(bankAdmin, FLS_ERASED_VALUE, sizeof(*bankAdmin));
  context->curWrokingBank++;

  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(INIT_CHECK_BANK_INFO);
  } else {
    ret = factory_goto(INIT_READ_BANK_ADMIN);
  }

  return ret;
}

Std_ReturnType Fee_Init_EraseInvalidBank_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

/* make sure that block has valid info, if found invalid info, it must be just after
 * erase, so that update the info with correct information as possible */
Std_ReturnType Fee_Init_CheckBankInfo_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  int i;
  int maxNumber = 0;
  int invalidBank = -1;
  const Fee_BankType *bank;

  for (i = 0; i < config->numOfBanks; i++) {
    if (bankAdmin[i].Info.Number == (~bankAdmin[i].Info.InvNumber)) {
      if (maxNumber < bankAdmin->Info.Number) {
        maxNumber = bankAdmin->Info.Number;
      }
    } else if (-1 == invalidBank) {
      invalidBank = i;
    } else {
      /* do nothing as already has one bank with invalid Number */
    }
  }

  if (-1 != invalidBank) {
    ASLOG(FEE, ("Found bank %d without info record, set Number=%d\n", invalidBank, maxNumber));
    bankAdmin = &bankAdmin[invalidBank];
    *workAdmin = *bankAdmin;
    workAdmin->Info.Number = maxNumber;
    workAdmin->Info.InvNumber = ~maxNumber;
    bank = &config->Banks[invalidBank];
    ret = Fee_FlsWrite(bank->LowAddress + offsetof(Fee_BankAdminType, Info),
                       (uint8_t *)&workAdmin->Info, sizeof(workAdmin->Info));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else {
    ret = factory_goto(INIT_CHECK_BANK_MAGIC);
  }

  return ret;
}

Std_ReturnType Fee_Init_CheckBankInfo_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  int i;
  int invalidBank = -1;

  for (i = 0; i < config->numOfBanks; i++) {
    if (bankAdmin[i].Info.Number != (~bankAdmin[i].Info.InvNumber)) {
      invalidBank = i;
      break;
    }
  }

  if (-1 != invalidBank) {
    bankAdmin = &bankAdmin[invalidBank];
    *bankAdmin = *workAdmin;
  }

  return factory_goto(INIT_CHECK_BANK_INFO);
}

Std_ReturnType Fee_Init_CheckBankInfo_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

/* check each bank has a valid magic number, if magic number invalid, it must be just after
 * erase, so in this case, set the magic number to the correct one */
Std_ReturnType Fee_Init_CheckBankMagic_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  int i;
  int invalidBank = -1;
  const Fee_BankType *bank;

  for (i = 0; i < config->numOfBanks; i++) {
    if ((bankAdmin[i].HeaderMagic.MagicNumber != (~bankAdmin[i].HeaderMagic.InvMagicNumber) ||
         (FEE_MAGIC_NUMBER != bankAdmin[i].HeaderMagic.MagicNumber))) {
      if (-1 == invalidBank) {
        invalidBank = i;
        break;
      }
    }
  }

  if (-1 != invalidBank) {
    ASLOG(FEE, ("Found bank %d without magic record, set magic\n", invalidBank));
    bankAdmin = &bankAdmin[invalidBank];
    *workAdmin = *bankAdmin;
    workAdmin->HeaderMagic.MagicNumber = FEE_MAGIC_NUMBER;
    workAdmin->HeaderMagic.InvMagicNumber = ~FEE_MAGIC_NUMBER;
    bank = &config->Banks[invalidBank];
    ret = Fee_FlsWrite(bank->LowAddress, (uint8_t *)&workAdmin->HeaderMagic,
                       sizeof(workAdmin->HeaderMagic));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else {
    ret = factory_goto(INIT_GET_WORKING_BANK);
  }

  return ret;
}

Std_ReturnType Fee_Init_CheckBankMagic_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  int i;
  int invalidBank = -1;

  for (i = 0; i < config->numOfBanks; i++) {
    if ((bankAdmin[i].HeaderMagic.MagicNumber != (~bankAdmin[i].HeaderMagic.InvMagicNumber) ||
         (FEE_MAGIC_NUMBER != bankAdmin[i].HeaderMagic.MagicNumber))) {
      invalidBank = i;
      break;
    }
  }

  if (-1 != invalidBank) {
    bankAdmin = &bankAdmin[invalidBank];
    *bankAdmin = *workAdmin;
  }

  return factory_goto(INIT_CHECK_BANK_MAGIC);
}

Std_ReturnType Fee_Init_CheckBankMagic_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

/* will firstly to get a bank that is being used by check the block admin area, and then go on
 * to search the free space */
Std_ReturnType Fee_Init_GetWorkingBank_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  Fee_ContextType *context = &Fee_Context;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  int i;
  boolean r;
  int bankWithBlocks = 0;
  int whichBank = -1; /* whichBank is the first bank has data blocks */
  int fullBank = -1;  /* fullBank is the first bank that is full */

  for (i = 0; i < config->numOfBanks; i++) {
    r = Fee_IsAllErased(bankAdmin[i].blocks, (uint16_t)sizeof(bankAdmin[i].blocks));
    if (TRUE != r) {
      bankWithBlocks++;
      if (-1 == whichBank) {
        whichBank = i;
      }
      if (FEE_BANK_NOT_FULL_MAGIC != bankAdmin[i].Status.FullMagic) {
        if (-1 == fullBank) {
          fullBank = i;
        } else {
          ASLOG(FEEE, ("impossible case, bank %d and bank %d are both full\n", fullBank, i));
          Fee_Panic();
        }
      }
    }
  }

  if (0 == bankWithBlocks) {
    whichBank = 0;
  }

  if ((0 == bankWithBlocks) || (1 == bankWithBlocks)) {
    /* no bank or only 1 bank has data, use the first bank */
    context->adminFreeAddr =
      config->Banks[whichBank].LowAddress + offsetof(Fee_BankAdminType, blocks);
    context->dataFreeAddr = config->Banks[whichBank].HighAddress;
    context->curWrokingBank = whichBank;
    ASLOG(FEE, ("FEE Init and Post Check Finished, activate bank is %d\n", whichBank));
    ret = factory_goto(INIT_SEARCH_FREE_SPACE);
  } else if (2 == bankWithBlocks) {
    if (fullBank != -1) {
      /* 2 banks has data, there is must be one in FULL state, and should ensure all data backedup
       * and erase it */
      context->adminFreeAddr =
        config->Banks[fullBank].LowAddress + offsetof(Fee_BankAdminType, blocks);
      context->dataFreeAddr = config->Banks[fullBank].HighAddress;
      context->curWrokingBank = fullBank;
      ASLOG(FEE, ("FEE Init and Post Check Finished, activate bank is %d, but which is full\n",
                  whichBank));
      ret = factory_goto(INIT_SEARCH_FREE_SPACE);
    } else {
      ASLOG(FEEE, ("impossible case, 2 banks has data, but no one is full\n"));
      Fee_Panic();
    }
  } else {
    ASLOG(FEEE, ("impossible case, %d banks has data\n", bankWithBlocks));
    Fee_Panic();
  }

  return ret;
}

Std_ReturnType Fee_Init_GetWorkingBank_End(void) {
  return E_NOT_OK;
}

Std_ReturnType Fee_Init_GetWorkingBank_Error(void) {
  return E_NOT_OK;
}

Std_ReturnType Fee_Init_SearchFreeSpace_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank;
#ifndef FLS_DIRECT_ACCESS
  uint32_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
#else
  boolean ending = FALSE;
  uint32_t numOfBlocks = FEE_MAX_ADMIN_READ_PER_CYCLE;
#endif
  uint32_t endAddress = context->adminFreeAddr + numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType));
  bank = &config->Banks[context->curWrokingBank];

  if (endAddress > bank->HighAddress) {
    endAddress = bank->HighAddress;
    numOfBlocks = (endAddress - context->adminFreeAddr) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  if (numOfBlocks > 0) {
#ifdef FLS_DIRECT_ACCESS
    ending = Fee_SearchFreeSpace_Analyze((const Fee_BlockType *)FEE_ADDRESS(context->adminFreeAddr),
                                         numOfBlocks);
    if (FALSE == ending) {
      context->retryCounter = 0;
      ret = E_OK;
    } else {
      ASLOG(FEE,
            ("Free space between 0x%X - 0x%X\n", context->adminFreeAddr, context->dataFreeAddr));
      if ((context->dataFreeAddr - context->adminFreeAddr) < FEE_MIN_FREE_SPACE) {
        context->retryCounter = 0;
        ret = factory_switch(BACKUP);
        ASLOG(FEE, ("bank %d is full, do backup\n", context->curWrokingBank));
      } else {
        ret = FACTORY_E_STOP;
      }
    }
#else
    ret = Fls_Read(context->adminFreeAddr, config->workingArea,
                   numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)));

    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
#endif
  } else {
    ASLOG(FEEE, ("Free space between 0x%X - 0x%X, abnormal, do backup\n", context->adminFreeAddr,
                 context->dataFreeAddr));
    ret = factory_switch(BACKUP);
  }

  return ret;
}

Std_ReturnType Fee_Init_SearchFreeSpace_End(void) {
#ifndef FLS_DIRECT_ACCESS
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank;
  uint32_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
  uint32_t endAddress = context->adminFreeAddr + numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType));
  Fee_BlockType *blocks = (Fee_BlockType *)config->workingArea;
  boolean ending;

  bank = &config->Banks[context->curWrokingBank];
  if (endAddress > bank->HighAddress) {
    endAddress = bank->HighAddress;
    numOfBlocks = (endAddress - context->adminFreeAddr) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  ending = Fee_SearchFreeSpace_Analyze(blocks, numOfBlocks);

  if (FALSE == ending) {
    ret = factory_goto(INIT_SEARCH_FREE_SPACE);
  } else {
    ASLOG(FEE, ("Free space between 0x%X - 0x%X\n", context->adminFreeAddr, context->dataFreeAddr));
    if ((context->dataFreeAddr - context->adminFreeAddr) < FEE_MIN_FREE_SPACE) {
      ret = factory_switch(BACKUP);
      ASLOG(FEE, ("bank %d is full, do backup\n", context->curWrokingBank));
    } else {
      ret = FACTORY_E_STOP;
    }
  }

  return ret;
#else
  return E_NOT_OK;
#endif
}

Std_ReturnType Fee_Init_SearchFreeSpace_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Read_ReadData_Main(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address = config->blockAddress[context->job.BlockId];

  if (FEE_INVALID_ADDRESS != address) {
    ASLOG(FEEI, ("block %d goto read data @ %X\n", context->job.BlockId, address));
#ifdef FLS_DIRECT_ACCESS
    ret = Fee_Read_ReadData_End();
#else
    ret = Fls_Read(address, config->workingArea,
                   FEE_DATA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ASLOG(FEE, ("Fls_Read Failed, try again\n"));
      ret = FACTORY_E_FEE_RETRY;
    }
#endif
  } else {
    ret = FACTORY_E_STOP;
    ASLOG(FEE, ("Read from ROM for block %d\n", context->job.BlockId));
    memcpy(context->job.DataBufferPtr,
           config->Blocks[context->job.BlockId].Rom + context->job.BlockOffset,
           context->job.Length);
  }

  return ret;
}

Std_ReturnType Fee_Read_ReadData_End(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint16_t *pCrc;
  uint16_t *pInvCrc;
  uint16_t Crc;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block =
    (const Fee_BlockType *)FEE_ADDRESS(config->blockAddress[context->job.BlockId]);
  const uint8_t *pData = (const uint8_t *)FEE_ADDRESS(block->Address);
#else
  const uint8_t *pData = config->workingArea;
#endif
  pCrc = (uint16_t *)(pData + FEE_DATA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize) - 4);
  pInvCrc = pCrc + 1;

  if (((uint16_t) ~(*pCrc)) == (*pInvCrc)) {

    Crc = Crc_CalculateCRC16(
      pData, FEE_DATA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize) - 4, 0, TRUE);

    if (Crc == *pCrc) {
      ret = E_OK;
    }
  }

  if (E_OK == ret) {
    ASLOG(FEE, ("Read from addres %X for block %d\n", config->blockAddress[context->job.BlockId],
                context->job.BlockId));
    memcpy(context->job.DataBufferPtr, &pData[context->job.BlockOffset], context->job.Length);
    ret = FACTORY_E_STOP;
  } else {
    if (config->blockAddress[context->job.BlockId] != FEE_INVALID_ADDRESS) {
      ASLOG(
        FEE,
        ("Addres %X for block %d has invalid data (R %X ~ %X E %X ), searching next valid one\n",
         config->blockAddress[context->job.BlockId], context->job.BlockId, *pCrc, *pInvCrc, Crc));
      config->blockAddress[context->job.BlockId] = FEE_INVALID_ADDRESS;
    }
    context->retryCounter = 0;
    ret = factory_goto(READ_SEARCH_NEXT);
  }

  return ret;
}

Std_ReturnType Fee_Read_ReadData_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Read_SearchNext_Main(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  ret = Fee_DoSearchNextValidDataStart(context->curWrokingBank);
  if (FEE_E_EXIST == ret) {
    config->blockAddress[context->job.BlockId] = FEE_INVALID_ADDRESS;
    ret = FACTORY_E_STOP;
    ASLOG(FEE, ("Read from ROM for block %d\n", context->job.BlockId));
    memcpy(context->job.DataBufferPtr,
           config->Blocks[context->job.BlockId].Rom + context->job.BlockOffset,
           context->job.Length);
  } else {
#ifndef FLS_DIRECT_ACCESS
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
#else
    ret = Fee_DoSearchNextValidDataEnd(context->curWrokingBank);
    if (E_OK == ret) {
      ret = factory_goto(READ_READ_DATA);
    } else {
      ret = E_OK;
    }
#endif
  }
  return ret;
}

Std_ReturnType Fee_Read_SearchNext_End(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;

  ret = Fee_DoSearchNextValidDataEnd(context->curWrokingBank);

  if (E_OK == ret) {
    ret = factory_goto(READ_READ_DATA);
  } else {
    ret = factory_goto(READ_SEARCH_NEXT);
  }

#ifdef FLS_DIRECT_ACCESS
  context->retryCounter = 0;
#endif

  return ret;
}

Std_ReturnType Fee_Read_SearchNext_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Write_WriteAdmin_Main(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;
  const Fee_ConfigType *config = FEE_CONFIG;

  if ((context->dataFreeAddr - context->adminFreeAddr) >=
      FEE_BLOCK_ADMIN_AND_DATA_SIZE(config->Blocks[context->job.BlockId].BlockSize)) {
    ret = Fee_DoWrite_Admin(FALSE);

    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else {
    ASLOG(FEEE, ("No space left, flash is dead\n"));
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteAdmin_End(void) {
  return factory_goto(WRITE_WRITE_DATA);
}

Std_ReturnType Fee_Write_WriteAdmin_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Write_WriteData_Main(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;

  ret = Fee_DoWrite_Data(context->job.DataBufferPtr);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteData_End(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;

  ASLOG(FEE, ("Write block %d done, free space %X - %X\n", context->job.BlockId,
              context->adminFreeAddr, context->dataFreeAddr));
#ifdef FLS_DIRECT_ACCESS
  config->blockAddress[context->job.BlockId] =
    context->adminFreeAddr - FEE_ALIGNED(sizeof(Fee_BlockType));
#else
  config->blockAddress[context->job.BlockId] = context->dataFreeAddr;
#endif

  if ((context->dataFreeAddr - context->adminFreeAddr) < FEE_MIN_FREE_SPACE) {
    ret = factory_switch(BACKUP);
    ASLOG(FEEI, ("bank %d is full, do backup\n", context->curWrokingBank));
  } else {
    ret = FACTORY_E_STOP;
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteData_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_ReadAdmin_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  const Fee_BankType *bank = &config->Banks[context->curWrokingBank];
  Std_ReturnType ret;

#ifndef FLS_DIRECT_ACCESS
  ret = Fls_Read(bank->LowAddress, (uint8_t *)bankAdmin, sizeof(*bankAdmin));
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }
#else
  memcpy(bankAdmin, (uint8_t *)FEE_ADDRESS(bank->LowAddress), sizeof(*bankAdmin));
  ret = Fee_Backup_ReadAdmin_End();
#endif

  return ret;
}

Std_ReturnType Fee_Backup_ReadAdmin_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Std_ReturnType ret;

  if (bankAdmin->Info.Number == (uint32_t)(~bankAdmin->Info.InvNumber)) {
    if (bankAdmin->Info.Number < config->NumberOfErasedCycles) {
#ifdef FEE_USE_BLANK_CHECK
      ret = factory_goto(BACKUP_CHECK_BANK_STATUS);
#else
      ret = factory_goto(BACKUP_ENSURE_FULL);
#endif
    } else {
      ASLOG(FEEE, ("flash is dead\n"));
      ret = FACTORY_E_STOP;
    }
  } else {
    ASLOG(FEEE, ("bank with invalid Number\n"));
    Fee_Panic();
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType Fee_Backup_ReadAdmin_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_CheckBankStatus_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank = &config->Banks[context->curWrokingBank];
  Std_ReturnType ret;

  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, Status),
                       FEE_ALIGNED(sizeof(Fee_BankStatusType)));
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_CheckBankStatus_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  memset(&bankAdmin->Status, FLS_ERASED_VALUE, sizeof(bankAdmin->Status));

  return factory_goto(BACKUP_ENSURE_FULL);
}

Std_ReturnType Fee_Backup_CheckBankStatus_Error(void) {
  Fee_ContextType *context = &Fee_Context;
  context->retryCounter = 0;
  /* trust the bank status*/
  return factory_goto(BACKUP_ENSURE_FULL);
}

Std_ReturnType Fee_Backup_EnsureFull_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[1];
  const Fee_BankType *bank = &config->Banks[context->curWrokingBank];

  if (FEE_BANK_NOT_FULL_MAGIC == bankAdmin->Status.FullMagic) {
    workAdmin->Status.FullMagic = FEE_BANK_FULL_MAGIC;
    ret = Fee_FlsWrite(bank->LowAddress + offsetof(Fee_BankAdminType, Status),
                       (uint8_t *)&workAdmin->Status, sizeof(workAdmin->Status));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else {
    context->job.Length = bankAdmin->Info.Number;
    ret = factory_goto(BACKUP_READ_NEXT_BANK_ADMIN);
  }

  return ret;
}

Std_ReturnType Fee_Backup_EnsureFull_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  bankAdmin->Status.FullMagic = FEE_BANK_FULL_MAGIC;

  context->job.Length = bankAdmin->Info.Number;
  return factory_goto(BACKUP_READ_NEXT_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_EnsureFull_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_ReadNextBankAdmin_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);
  uint8_t nextBank = context->curWrokingBank + 1;
  const Fee_BankType *bank;
  Std_ReturnType ret;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ret = Fls_Read(bank->LowAddress, (uint8_t *)bankAdmin, sizeof(*bankAdmin));
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_ReadNextBankAdmin_End(void) {
#ifdef FEE_USE_BLANK_CHECK
  return factory_goto(BACKUP_BLANK_CHECK_NEXT_BANK_EMPTY);
#else
  return factory_goto(BACKUP_ENSURE_NEXT_BANK_EMPTY);
#endif
}

Std_ReturnType Fee_Backup_ReadNextBankAdmin_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_BlankCheckNextBankEmpty_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t nextBank = context->curWrokingBank + 1;
  const Fee_BankType *bank;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, blocks), FEE_PAGE_SIZE);
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Backup_BlankCheckNextBankEmpty_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);
  memset(bankAdmin->blocks, FLS_ERASED_VALUE, FEE_PAGE_SIZE);
  return factory_goto(BACKUP_ENSURE_NEXT_BANK_EMPTY);
}

Std_ReturnType Fee_Backup_BlankCheckNextBankEmpty_Error(void) {
  Fee_ContextType *context = &Fee_Context;
  /* block has data, trust it */
  context->retryCounter = 0;

  return factory_goto(BACKUP_ENSURE_NEXT_BANK_EMPTY);
}

Std_ReturnType Fee_Backup_EnsureNextBankEmpty_Main(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);

  if (Fee_IsAllErased(bankAdmin->blocks, sizeof(bankAdmin->blocks))) {
    ret = Fee_DoBackup_Copy_Start();
  } else {
    ret = factory_goto(BACKUP_ERASE_NEXT_BANK);
  }

  return ret;
}

Std_ReturnType Fee_Backup_EnsureNextBankEmpty_End(void) {
  return E_NOT_OK;
}

Std_ReturnType Fee_Backup_EnsureNextBankEmpty_Error(void) {
  return E_NOT_OK;
}

Std_ReturnType Fee_Backup_EraseNextBank_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t nextBank = context->curWrokingBank + 1;
  const Fee_BankType *bank;
  Std_ReturnType ret;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ret = Fls_Erase(bank->LowAddress, bank->HighAddress - bank->LowAddress);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_EraseNextBank_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);

  memset(bankAdmin, FLS_ERASED_VALUE, sizeof(Fee_BankAdminType));
  return factory_goto(BACKUP_SET_NEXT_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_EraseNextBank_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_SetNextBankAdmin_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *actAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *bankAdmin = &actAdmin[1];
  Fee_BankAdminType *workAdmin = &actAdmin[2];
  uint8_t nextBank = context->curWrokingBank + 1;
  const Fee_BankType *bank;
  Std_ReturnType ret;
  uint32_t Number = context->job.Length;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  Number += 1;
  bank = &config->Banks[nextBank];

  if (bankAdmin->Info.Number != ((uint16_t)~bankAdmin->Info.InvNumber)) {
    ASLOG(FEE, ("set next bank %d Number = %d\n", nextBank, Number));
    workAdmin->Info.Number = Number;
    workAdmin->Info.InvNumber = ~Number;
    ret = Fee_FlsWrite(bank->LowAddress + offsetof(Fee_BankAdminType, Info),
                       (uint8_t *)&workAdmin->Info, sizeof(workAdmin->Info));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else if (bankAdmin->HeaderMagic.MagicNumber != (~bankAdmin->HeaderMagic.InvMagicNumber)) {
    workAdmin->HeaderMagic.MagicNumber = FEE_MAGIC_NUMBER;
    workAdmin->HeaderMagic.InvMagicNumber = ~FEE_MAGIC_NUMBER;
    ret = Fee_FlsWrite(bank->LowAddress, (uint8_t *)&workAdmin->HeaderMagic,
                       sizeof(workAdmin->HeaderMagic));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else {
    ret = Fee_DoBackup_Copy_Start();
  }

  return ret;
}

Std_ReturnType Fee_Backup_SetNextBankAdmin_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *actAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *bankAdmin = &actAdmin[1];
  Fee_BankAdminType *workAdmin = &actAdmin[2];

  if (bankAdmin->Info.Number != ((uint16_t)~bankAdmin->Info.InvNumber)) {
    bankAdmin->Info = workAdmin->Info;
  } else if (bankAdmin->HeaderMagic.MagicNumber != (~bankAdmin->HeaderMagic.InvMagicNumber)) {
    bankAdmin->HeaderMagic = workAdmin->HeaderMagic;
  } else {
    /* do nothing */
  }
  return factory_goto(BACKUP_SET_NEXT_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_SetNextBankAdmin_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_CopyAdmin_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint16_t blockId;
  Std_ReturnType ret;

  blockId = context->job.BlockId;
  if (blockId < config->numOfBlocks) {
    if (config->blockAddress[blockId] == FEE_INVALID_ADDRESS) {
      for (blockId = blockId + 1; blockId < config->numOfBlocks; blockId++) {
        if (config->blockAddress[blockId] != FEE_INVALID_ADDRESS) {
          break;
        }
      }
    }
  }

  if (blockId < config->numOfBlocks) {
    context->job.BlockId = blockId;
    /* for valid data search from */
#ifdef FLS_DIRECT_ACCESS
    context->job.NextAddr = config->blockAddress[blockId] - FEE_ALIGNED(sizeof(Fee_BlockType));
#else
    context->job.NextAddr = PTR_TO_U32(context->job.DataBufferPtr);
#endif
    if ((context->dataFreeAddr - context->adminFreeAddr) >=
        FEE_BLOCK_ADMIN_AND_DATA_SIZE(config->Blocks[blockId].BlockSize)) {
      ret = Fee_DoWrite_Admin(TRUE);
      if (E_OK == ret) {
        ret = FACTORY_E_EVENT;
      } else {
        ret = FACTORY_E_FEE_RETRY;
      }
    } else {
      ASLOG(FEEE, ("No space left, flash is dead\n"));
      Fee_Panic();
      ret = E_NOT_OK;
    }
  } else {
    ret = factory_goto(BACKUP_ERASE_BANK);
  }

  return ret;
}

Std_ReturnType Fee_Backup_CopyAdmin_End(void) {
  return factory_goto(BACKUP_COPY_READ_DATA);
}

Std_ReturnType Fee_Backup_CopyAdmin_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_CopyReadData_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Std_ReturnType ret;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block =
    (const Fee_BlockType *)FEE_ADDRESS(config->blockAddress[context->job.BlockId]);
  memcpy(config->workingArea, FEE_ADDRESS(block->Address),
         FEE_DATA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize));
  ret = Fee_Backup_CopyReadData_End();
#else
  ret = Fls_Read(config->blockAddress[context->job.BlockId], config->workingArea,
                 FEE_DATA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize));
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }
#endif
  return ret;
}

Std_ReturnType Fee_Backup_CopyReadData_End(void) {
  Std_ReturnType ret = E_NOT_OK;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint16_t *pCrc;
  uint16_t *pInvCrc;
  uint16_t Crc;

  pCrc = (uint16_t *)(config->workingArea +
                      FEE_DATA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize) - 4);
  pInvCrc = pCrc + 1;
  if (((uint16_t) ~(*pCrc)) == (*pInvCrc)) {
    Crc = Crc_CalculateCRC16(config->workingArea,
                             FEE_DATA_ALIGNED(config->Blocks[context->job.BlockId].BlockSize) - 4,
                             0, TRUE);

    if (Crc == *pCrc) {
      ret = E_OK;
    }
  }

  if (ret != E_OK) {
    if (config->blockAddress[context->job.BlockId] != FEE_INVALID_ADDRESS) {
      ASLOG(
        FEEE,
        ("Addres %X for block %d has invalid data (R %X, ~ %X, E %X), searching next valid one\n",
         config->blockAddress[context->job.BlockId], context->job.BlockId, *pCrc, *pInvCrc, Crc));
      config->blockAddress[context->job.BlockId] = FEE_INVALID_ADDRESS;
    }
    ret = factory_goto(BACKUP_SEARCH_NEXT_DATA);
  } else {
    ret = factory_goto(BACKUP_COPY_DATA);
  }

  return ret;
}

Std_ReturnType Fee_Backup_CopyReadData_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_SearchNextData_Main(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t bankId = context->curWrokingBank - 1;
  if (bankId >= config->numOfBanks) {
    bankId = config->numOfBanks - 1;
  }

  ret = Fee_DoSearchNextValidDataStart(bankId);
  if (FEE_E_EXIST == ret) {
    ASLOG(FEEE, ("Fatal, can't backup for block %d\n", context->job.BlockId));
    context->job.BlockId++;
    ret = factory_goto(BACKUP_COPY_ADMIN);
  } else {
#ifndef FLS_DIRECT_ACCESS
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
#else
    ret = Fee_Backup_SearchNextData_End();
    if (ret == factory_goto(BACKUP_SEARCH_NEXT_DATA)) {
      ret = E_OK;
    }
#endif
  }

  return ret;
}

Std_ReturnType Fee_Backup_SearchNextData_End(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;
  const Fee_ConfigType *config = FEE_CONFIG;
  uint8_t bankId = context->curWrokingBank - 1;

  if (bankId >= config->numOfBanks) {
    bankId = config->numOfBanks - 1;
  }
  ret = Fee_DoSearchNextValidDataEnd(bankId);

  if (E_OK == ret) {
    ret = factory_goto(BACKUP_COPY_READ_DATA);
  } else {
    ret = factory_goto(BACKUP_SEARCH_NEXT_DATA);
  }

#ifdef FLS_DIRECT_ACCESS
  context->retryCounter = 0;
#endif

  return ret;
}

Std_ReturnType Fee_Backup_SearchNextData_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_CopyData_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Std_ReturnType ret;

  ret = Fee_DoWrite_Data(config->workingArea);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Backup_CopyData_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
#ifdef FLS_DIRECT_ACCESS
  config->blockAddress[context->job.BlockId] =
    context->adminFreeAddr - FEE_ALIGNED(sizeof(Fee_BlockType));
#else
  config->blockAddress[context->job.BlockId] = context->dataFreeAddr;
#endif
  context->job.BlockId += 1;
  return factory_goto(BACKUP_COPY_ADMIN);
}

Std_ReturnType Fee_Backup_CopyData_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_EraseBank_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BankType *bank;
  Std_ReturnType ret;
  uint8_t erasedBank = context->curWrokingBank - 1;

  if (erasedBank >= config->numOfBanks) {
    erasedBank = config->numOfBanks - 1;
  }

  ASLOG(FEE, ("backup erase bank %d\n", erasedBank));
  bank = &config->Banks[erasedBank];
  ret = Fls_Erase(bank->LowAddress, bank->HighAddress - bank->LowAddress);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_FEE_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_EraseBank_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;

  memset(bankAdmin, FLS_ERASED_VALUE, sizeof(Fee_BankAdminType));
  return factory_goto(BACKUP_SET_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_EraseBank_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

Std_ReturnType Fee_Backup_SetBankAdmin_Main(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[1];
  const Fee_BankType *bank;
  Std_ReturnType ret;
  uint32_t Number = context->job.Length;
  uint8_t erasedBank = context->curWrokingBank - 1;

  if (erasedBank >= config->numOfBanks) {
    erasedBank = config->numOfBanks - 1;
  }
  bank = &config->Banks[erasedBank];

  Number += 1;

  if (bankAdmin->Info.Number != ((uint32_t)~bankAdmin->Info.InvNumber)) {
    ASLOG(FEE, ("set bank %d Number = %d\n", erasedBank, Number));
    workAdmin->Info.Number = Number;
    workAdmin->Info.InvNumber = ~Number;
    ret = Fee_FlsWrite(bank->LowAddress + offsetof(Fee_BankAdminType, Info),
                       (uint8_t *)&workAdmin->Info, sizeof(workAdmin->Info));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else if (bankAdmin->HeaderMagic.MagicNumber != (~bankAdmin->HeaderMagic.InvMagicNumber)) {
    ASLOG(FEE, ("set bank %d magic\n", erasedBank));
    workAdmin->HeaderMagic.MagicNumber = FEE_MAGIC_NUMBER;
    workAdmin->HeaderMagic.InvMagicNumber = ~FEE_MAGIC_NUMBER;
    ret = Fee_FlsWrite(bank->LowAddress, (uint8_t *)&workAdmin->HeaderMagic,
                       sizeof(workAdmin->HeaderMagic));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_FEE_RETRY;
    }
  } else {
    ret = FACTORY_E_STOP;
    ASLOG(FEE, ("backup done\n"));
  }

  return ret;
}

Std_ReturnType Fee_Backup_SetBankAdmin_End(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[1];

  if (bankAdmin->Info.Number != ((uint32_t)~bankAdmin->Info.InvNumber)) {
    bankAdmin->Info = workAdmin->Info;
  } else if (bankAdmin->HeaderMagic.MagicNumber != (~bankAdmin->HeaderMagic.InvMagicNumber)) {
    bankAdmin->HeaderMagic = workAdmin->HeaderMagic;
  } else {
  }

  return factory_goto(BACKUP_SET_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_SetBankAdmin_Error(void) {
  return FACTORY_E_FEE_RETRY;
}

void Fee_JobEndNotification(void) {
  Fee_ContextType *context = &Fee_Context;
  context->retryCounter = 0;
  factory_on_event(&Fee_Factory, FEE_EVENT_END);
}

void Fee_JobErrorNotification(void) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;

  if (context->retryCounter <= config->maxJobRetry) {
    context->retryCounter++;
    factory_on_event(&Fee_Factory, FEE_EVENT_ERROR);
  } else {
    ASLOG(FEEE, ("reach max attempts during error:%s:%s\n",
                 Fee_Factory.machines[Fee_Factory.context->machineId].name,
                 Fee_Factory.machines[Fee_Factory.context->machineId]
                   .nodes[Fee_Factory.context->nodeId]
                   .name));
    factory_cancel(&Fee_Factory);
  }
}

MemIf_StatusType Fee_GetStatus(void) {
  MemIf_StatusType status = MEMIF_IDLE;
  uint8_t state = factory_get_state(&Fee_Factory);

  switch (state) {
  case FACTORY_RUNNING:
  case FACTORY_WAITING:
    status = MEMIF_BUSY;
    break;
  default:
    break;
  }

  return status;
}

void Fee_Init(const Fee_ConfigType *ConfigPtr) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  int i;
  (void)ConfigPtr;

  for (i = 0; i < config->numOfBlocks; i++) {
    config->blockAddress[i] = FEE_INVALID_ADDRESS;
  }
  memset(context, 0, sizeof(Fee_ContextType));
  factory_init(&Fee_Factory);
  (void)factory_start_machine(&Fee_Factory, FEE_MACHINE_INIT);
}

Std_ReturnType Fee_Read(uint16_t BlockNumber, uint16_t BlockOffset, uint8_t *DataBufferPtr,
                        uint16_t Length) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Std_ReturnType r = E_NOT_OK;

  if (BlockNumber > 0 && ((BlockNumber - 1) < config->numOfBlocks)) {
    if (config->Blocks[BlockNumber - 1].BlockSize >= ((uint32_t)BlockOffset + Length)) {
      r = E_OK;
    }
  }

  if (E_OK == r) {
    r = factory_start_machine(&Fee_Factory, FEE_MACHINE_READ);
  }

  if (E_OK == r) {
    context->retryCounter = 0;
    context->job.BlockId = BlockNumber - 1;
    context->job.BlockOffset = BlockOffset;
    context->job.DataBufferPtr = DataBufferPtr;
    context->job.Length = Length;
#ifdef FLS_DIRECT_ACCESS
    if (FEE_INVALID_ADDRESS != config->blockAddress[context->job.BlockId]) {
      /* valid history data before current address */
      context->job.NextAddr =
        config->blockAddress[context->job.BlockId] - FEE_ALIGNED(sizeof(Fee_BlockType));
    } else {
      context->job.NextAddr = context->adminFreeAddr;
    }
#else
    context->job.NextAddr = context->adminFreeAddr;
#endif
  }

  return r;
}

Std_ReturnType Fee_Write(uint16_t BlockNumber, const uint8_t *DataBufferPtr) {
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Std_ReturnType r = E_NOT_OK;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block;
#endif

  if (BlockNumber > 0 && ((BlockNumber - 1) < config->numOfBlocks)) {
    r = E_OK;
  }

#ifdef FLS_DIRECT_ACCESS
  if (FEE_INVALID_ADDRESS != config->blockAddress[BlockNumber - 1]) {
    block = (const Fee_BlockType *)FEE_ADDRESS(config->blockAddress[BlockNumber - 1]);
    if (block->NumberOfWriteCycles >= config->Blocks[BlockNumber - 1].NumberOfWriteCycles) {
      ASLOG(FEEE, ("block %d reach end of life %u\n", BlockNumber, block->NumberOfWriteCycles));
      r = E_NOT_OK;
    }
  }
#endif

  if (E_OK == r) {
    r = factory_start_machine(&Fee_Factory, FEE_MACHINE_WRITE);
  }

  if (E_OK == r) {
    context->retryCounter = 0;
    context->job.BlockId = BlockNumber - 1;
    context->job.DataBufferPtr = (uint8_t *)DataBufferPtr;
  }

  return r;
}

void Fee_SetMode(MemIf_ModeType Mode) {
  Fls_SetMode(Mode);
}

void Fee_MainFunction(void) {
  Std_ReturnType ret;
  const Fee_ConfigType *config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t state = factory_get_state(&Fee_Factory);

  if (FACTORY_RUNNING == state) {
    if (context->retryCounter <= config->maxJobRetry) {
      context->retryCounter++;
      ret = factory_main(&Fee_Factory);
      if ((FACTORY_E_SWITCH_TO == ret) || (FACTORY_E_GOTO == ret)) {
        context->retryCounter = 0;
        factory_main(&Fee_Factory);
      }
    } else {
      ASLOG(FEEE, ("reach max attempts during main:%s:%s\n",
                   Fee_Factory.machines[Fee_Factory.context->machineId].name,
                   Fee_Factory.machines[Fee_Factory.context->machineId]
                     .nodes[Fee_Factory.context->nodeId]
                     .name));
      factory_cancel(&Fee_Factory);
    }
  }
}
