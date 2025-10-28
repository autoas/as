/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Flash EEPROM Emulation AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Fee.h"
#include "Fee_Cfg.h"
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

#include "Det.h"
#ifdef USE_TRACE_APP
#include "TraceApp_Cfg.h"
#else
#define STD_TRACE_APP(ev)
#endif
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_FEE 0
#define AS_LOG_FEEI 0
#define AS_LOG_FEEE 2

#define FEE_CONFIG (&Fee_Config)

#define FEE_E_EXIST 0x10u

#ifndef FEE_MAX_ADMIN_READ_PER_CYCLE
#define FEE_MAX_ADMIN_READ_PER_CYCLE 100u
#endif

/* additional 4 for u16Crc and u16InvCrc */
#define FEE_DATA_ALIGNED(sz) FEE_ALIGNED(ALIGNED(sz, 2u) + 4u)

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

#define FEE_FAULT_TOO_MANY_FULL_BANK 0
#define FEE_FAULT_NO_BANK_IS_FULL 1
#define FEE_FAULT_TOO_MANY_BANK_WITH_DATA 2
#define FEE_FAULT_NO_SPACE_DURING_BACKUP 3
#define FEE_FAULT_BANK_WITH_INVALID_INFO 4
#define FEE_FAULT_BANK_WITH_INVALID_MAGIC 5
#define FEE_FAULT_CONTEXT_CORRUPT 6

#ifdef FEE_USE_CONTEXT_CRC
#define Fee_RefreshContextCrc() Fee_RefreshOrCheckContextCrc(TRUE)
#define Fee_CheckContextCrc() Fee_RefreshOrCheckContextCrc(FALSE)
#else
#define Fee_RefreshContextCrc()
#define Fee_CheckContextCrc()
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  /* For backup, DataBufferPtr = adminFreeAddr of full bank  */
  uint8_t *jobDataBufferPtr;
  uint32_t erasedNumber;  /* The maximum erased Number of all the bank */
  uint32_t adminFreeAddr; /* bank low address */
  uint32_t dataFreeAddr;  /* bank high address */
  uint32_t jobNextAddr;
  uint32_t jobLength;
  uint16_t jobBlockId;
  uint16_t jobBlockOffset;
  uint8_t curWrokingBank;
  uint8_t retryCounter;
} Fee_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
extern CONSTANT(Fee_ConfigType, FEE_CONST) Fee_Config;

#if defined(_WIN32) || defined(linux)
#define FEE_ADDRESS(v) ((void *)(&g_FlsAcMirror[(v)]))
#else
#define FEE_ADDRESS(v) ((void *)(v))
#endif

Std_ReturnType Fls_AcBlankCheck(Fls_AddressType address, Fls_LengthType length);
/* ================================ [ DATAS     ] ============================================== */
static Fee_ContextType Fee_Context;
#ifdef FEE_USE_CONTEXT_CRC
static uint16_t Fee_ContextCrc;
#endif
/* ================================ [ LOCALS    ] ============================================== */
static void Fee_Panic(uint8_t fault) {
#ifdef FEE_USE_FAULTS
  uint8_t faults[FEE_FAULTS_SIZE];
  uint16_t i;
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t curBank = context->curWrokingBank;
#endif
  ASLOG(
    FEEE,
    ("panic %u during %s:%s\n", fault, Fee_Factory.machines[Fee_Factory.context->machineId].name,
     Fee_Factory.machines[Fee_Factory.context->machineId].nodes[Fee_Factory.context->nodeId].name));
#if defined(_WIN32) || defined(linux)
  assert(0);
#else
  /* Note: panic doesn't yield */
#endif
#ifdef FEE_USE_FAULTS
  if (curBank > config->numOfBanks) {
    curBank = 0;
  }
  Fls_AcInit();
  do {
    ret = Fls_AcRead(config->Banks[curBank].LowAddress + offsetof(Fee_BankAdminType, faults),
                     faults, FEE_FAULTS_SIZE);
  } while (E_OK != ret);
  for (i = 0; i < FEE_FAULTS_SIZE; i++) {
    if (faults[i] == 0xFFu) {
      faults[i] = fault;
      break;
    }
  }
  do {
    ret = Fls_AcWrite(config->Banks[curBank].LowAddress + offsetof(Fee_BankAdminType, faults),
                      faults, FEE_FAULTS_SIZE);
  } while (E_OK != ret);
#endif

  Fee_PanicUserAction(fault);
}

#ifdef FEE_USE_CONTEXT_CRC
static void Fee_RefreshOrCheckContextCrc(boolean bRefresh) {
  uint16_t u16Crc = 0xFFFF;
  u16Crc = Crc_CalculateCRC16((uint8_t *)&Fee_Context, sizeof(Fee_Context), u16Crc, FALSE);
  u16Crc =
    Crc_CalculateCRC16((uint8_t *)Fee_Factory.context, sizeof(factory_context_t), u16Crc, FALSE);
  if (TRUE == bRefresh) {
    Fee_ContextCrc = u16Crc;
  } else {
    if (u16Crc != Fee_ContextCrc) {
      Fee_Panic(FEE_FAULT_CONTEXT_CORRUPT);
    }
  }
}
#endif

static Std_ReturnType Fee_FlsWrite(uint32_t address, uint8_t *data, uint16_t length) {
  Std_ReturnType r;
  uint16_t alignedLen = FEE_ALIGNED(length);

  ASLOG(FEE, ("Fee write @%X, len=%d, data: %02X %02X %02X %02X %02X %02X %02X %02X\n", address,
              length, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

  if (alignedLen > length) {
    (void)memset(&data[length], FLS_ERASED_VALUE, alignedLen - length);
  }
  r = Fls_Write(address, data, alignedLen);

  return r;
}

static boolean Fee_IsAllErased(uint8_t *data, uint16_t size) {
  boolean r = TRUE;
  uint16_t i;

  for (i = 0; i < size; i++) {
    if (FLS_ERASED_VALUE != data[i]) {
      r = FALSE;
      break;
    }
  }

  return r;
}

static Std_ReturnType Fee_DoWrite_Admin(boolean fromBackup) {
  Std_ReturnType r = E_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BlockType *admin;
  uint32_t address;
  uint16_t size;
  uint32_t NumberOfWriteCycles = 1u;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block;
#endif

#ifdef FLS_DIRECT_ACCESS
  if (FEE_INVALID_ADDRESS != config->blockContexts[context->jobBlockId].Address) {
    block = (const Fee_BlockType *)FEE_ADDRESS(config->blockContexts[context->jobBlockId].Address);
    if (TRUE == fromBackup) {
      NumberOfWriteCycles = block->NumberOfWriteCycles;
    } else {
      NumberOfWriteCycles = block->NumberOfWriteCycles + 1u;
      if (NumberOfWriteCycles > config->Blocks[context->jobBlockId].NumberOfWriteCycles) {
        ASLOG(FEEE, ("trigger dead block %d write\n", context->jobBlockId));
        r = E_NOT_OK;
      }
    }
  }
#else
  if (FEE_INVALID_ADDRESS != config->blockContexts[context->jobBlockId].Address) {
    if (TRUE == fromBackup) {
      NumberOfWriteCycles = config->blockContexts[context->jobBlockId].NumberOfWriteCycles;
    } else {
      NumberOfWriteCycles = config->blockContexts[context->jobBlockId].NumberOfWriteCycles + 1u;
      if (NumberOfWriteCycles > config->Blocks[context->jobBlockId].NumberOfWriteCycles) {
        ASLOG(FEEE, ("trigger dead block %d write\n", context->jobBlockId));
        r = E_NOT_OK;
      }
    }
  }
#endif

  if (E_OK == r) {
    size = config->Blocks[context->jobBlockId].BlockSize;
    address = context->dataFreeAddr - FEE_DATA_ALIGNED(size);
    admin = (Fee_BlockType *)config->workingArea;
    admin->BlockNumber = context->jobBlockId + 1u;
    admin->Address = address;
    admin->NumberOfWriteCycles = NumberOfWriteCycles;
    admin->BlockSize = size;
    admin->Crc = Crc_CalculateCRC16((uint8_t *)admin, offsetof(Fee_BlockType, Crc), 0, TRUE);
    admin->InvCrc = ~(admin->Crc);
    r = Fee_FlsWrite(context->adminFreeAddr, (uint8_t *)admin, sizeof(Fee_BlockType));
    if (E_OK == r) {
      context->adminFreeAddr += FEE_ALIGNED(sizeof(Fee_BlockType));
    }
  }

  return r;
}

static void Fee_DoWrite_UpdateDataFreeAddr(void) {
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  uint16_t size;
  size = config->Blocks[context->jobBlockId].BlockSize;
  context->dataFreeAddr -= FEE_DATA_ALIGNED(size);
}

static Std_ReturnType Fee_DoWrite_Data(uint8_t *jobDataBufferPtr) {
  Std_ReturnType r;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address;
  uint16_t size;
  uint8_t *data;
  uint16_t *crc;
  uint16_t *invCrc;
  uint16_t i;
  size = config->Blocks[context->jobBlockId].BlockSize;
  address = context->dataFreeAddr;
  data = config->workingArea;
  if (data != jobDataBufferPtr) {
    (void)memcpy(data, jobDataBufferPtr, size);
    for (i = size; i < FEE_DATA_ALIGNED(size) - 4u; i++) {
      data[i] = FLS_ERASED_VALUE;
    }
    crc = (uint16_t *)&data[FEE_DATA_ALIGNED(size) - 4u];
    invCrc = &crc[1];
    *crc = Crc_CalculateCRC16(data, FEE_DATA_ALIGNED(size) - 4u, 0, TRUE);
    *invCrc = ~(*crc);
  }
  r = Fee_FlsWrite(address, data, FEE_DATA_ALIGNED(size));
  return r;
}

#if defined(FLS_DIRECT_ACCESS) || defined(FLS_DATA_DIRECT_ACCESS)
static Std_ReturnType Fee_IsDataCrcOK(uint32_t address, uint16_t size) {
  Std_ReturnType r = E_NOT_OK;
  const uint8_t *pData = (uint8_t *)FEE_ADDRESS(address);
  const uint16_t *pCrc = (uint16_t *)FEE_ADDRESS(address + FEE_DATA_ALIGNED(size) - 4u);
  const uint16_t *pInvCrc = &pCrc[1];
  uint16_t Crc;

  if (((uint16_t)(~(*pCrc)) == (*pInvCrc))) {
    Crc = Crc_CalculateCRC16(pData, FEE_DATA_ALIGNED(size) - 4u, 0, TRUE);

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

static boolean Fee_SearchFreeSpace_Analyze(const Fee_BlockType *blocks, uint16_t numOfBlocks) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  const Fee_BlockType *block;
  boolean ending = FALSE;
  uint16_t Crc;
  uint16_t i;

  for (i = 0; i < numOfBlocks; i++) {
    block = (Fee_BlockType *)(((size_t)blocks) + FEE_ALIGNED(sizeof(Fee_BlockType)) * i);
    if (block->Crc == ((uint16_t)~block->InvCrc)) {
      Crc = Crc_CalculateCRC16((uint8_t *)block, offsetof(Fee_BlockType, Crc), 0, TRUE);
      if (Crc == block->Crc) {
        context->dataFreeAddr -= FEE_DATA_ALIGNED(block->BlockSize);
        if ((block->BlockNumber <= config->numOfBlocks) && (block->BlockNumber > 0u)) {
          if (block->BlockSize == config->Blocks[block->BlockNumber - 1u].BlockSize) {
            if (block->Address <= context->dataFreeAddr) {
#if defined(FLS_DIRECT_ACCESS) || defined(FLS_DATA_DIRECT_ACCESS)
              Std_ReturnType r = Fee_IsDataCrcOK(block->Address, block->BlockSize);
              if (E_OK == r) {
#if defined(FLS_DIRECT_ACCESS)
                config->blockContexts[block->BlockNumber - 1u].Address = context->adminFreeAddr;
#else
                config->blockContexts[block->BlockNumber - 1u].Address = context->dataFreeAddr;
#endif
#else
              config->blockContexts[block->BlockNumber - 1u].Address = context->dataFreeAddr;
              config->blockContexts[block->BlockNumber - 1u].NumberOfWriteCycles =
                block->NumberOfWriteCycles;
#endif
                ASLOG(FEEI, ("Got block number %d @ %X, %X\n", block->BlockNumber,
                             context->adminFreeAddr, context->dataFreeAddr));
#if defined(FLS_DIRECT_ACCESS) || defined(FLS_DATA_DIRECT_ACCESS)
              } else {
                ASLOG(FEEE, ("block number %d has invalid data\n", block->BlockNumber));
              }
#endif
              context->dataFreeAddr = block->Address; /* for robustness purpose */
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
                       config->Blocks[blocks->BlockNumber - 1u].BlockSize),
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
    }
#ifdef FEE_USE_BLANK_CHECK
    else if (E_OK == Fls_AcBlankCheck(context->adminFreeAddr, FEE_ALIGNED(sizeof(Fee_BlockType)))) {
      ending = TRUE; /* searching Free Space End */
    }
#else
    else if (TRUE == Fee_IsAllErased((uint8_t *)block, FEE_ALIGNED(sizeof(Fee_BlockType)))) {
      ending = TRUE; /* searching Free Space End */
    }
#endif
    else {
      /* also invalid block */
      context->adminFreeAddr += FEE_ALIGNED(sizeof(Fee_BlockType));
    }

    if ((context->dataFreeAddr - context->adminFreeAddr) < FEE_MIN_FREE_SPACE) {
      ending = TRUE;
    }

    if (TRUE == ending) {
      break;
    }
  }

  return ending;
}

static Std_ReturnType Fee_DoSearchNextValidDataStart(uint8_t bankId) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address;
  uint32_t blockAdminLow;
  Std_ReturnType r = FEE_E_EXIST;
#ifdef FLS_DIRECT_ACCESS
  uint16_t numOfBlocks = FEE_MAX_ADMIN_READ_PER_CYCLE;
#else
  uint16_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
#endif
  blockAdminLow = config->Banks[bankId].LowAddress + offsetof(Fee_BankAdminType, blocks);
  if (context->jobNextAddr > (numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)))) {
    address = context->jobNextAddr - (numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)));
  } else {
    address = 0;
  }

  if (address < blockAdminLow) {
    address = blockAdminLow;
    numOfBlocks = (context->jobNextAddr - blockAdminLow) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  if (numOfBlocks > 0u) {
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
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address;
  uint32_t blockAdminLow;
  uint16_t Crc;
  int32_t i; /* intend int32_t */
#ifdef FLS_DIRECT_ACCESS
  uint16_t numOfBlocks = FEE_MAX_ADMIN_READ_PER_CYCLE;
#else
  uint16_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
#endif
  Fee_BlockType *block;
  blockAdminLow = config->Banks[bankId].LowAddress + offsetof(Fee_BankAdminType, blocks);
  if (context->jobNextAddr > (numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)))) {
    address = context->jobNextAddr - (numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)));
  } else {
    address = 0;
  }

  if (address < blockAdminLow) {
    address = blockAdminLow;
    numOfBlocks = (context->jobNextAddr - blockAdminLow) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  for (i = (int32_t)numOfBlocks - 1; i >= 0; i--) {
    context->jobNextAddr -= FEE_ALIGNED(sizeof(Fee_BlockType));
#ifdef FLS_DIRECT_ACCESS
    block = (Fee_BlockType *)FEE_ADDRESS(context->jobNextAddr);
#else
    block = (Fee_BlockType *)(&config->workingArea[i * FEE_ALIGNED(sizeof(Fee_BlockType))]);
#endif
    if ((block->BlockNumber == (context->jobBlockId + 1u)) &&
        (block->Crc == ((uint16_t)~block->InvCrc))) {
      Crc = Crc_CalculateCRC16((uint8_t *)block, offsetof(Fee_BlockType, Crc), 0, TRUE);
      if (Crc == block->Crc) { /* bing go, valid block */
        if (block->BlockSize == config->Blocks[block->BlockNumber - 1u].BlockSize) {
#if defined(FLS_DIRECT_ACCESS) || defined(FLS_DATA_DIRECT_ACCESS)
          r = Fee_IsDataCrcOK(block->Address, block->BlockSize);
          if (E_OK == r) {
#if defined(FLS_DIRECT_ACCESS)
            config->blockContexts[context->jobBlockId].Address = context->jobNextAddr;
#else
            config->blockContexts[context->jobBlockId].Address = block->Address;
#endif
#else
          config->blockContexts[context->jobBlockId].Address = block->Address;
#endif
            r = E_OK;
#if defined(FLS_DIRECT_ACCESS) || defined(FLS_DATA_DIRECT_ACCESS)
          } else {
            ASLOG(FEEE, ("block %d has invalid data\n", context->jobBlockId));
          }
#endif
          break;
        } else {
          ASLOG(FEEE, ("block %d size mismatch, config updated\n", context->jobBlockId));
        }
      } else {
        ASHEXDUMP(FEEE,
                  ("block %d admin @ %X CRC NOK, E %X != R %X", context->jobBlockId,
                   context->jobNextAddr, Crc, block->Crc),
                  &block, sizeof(Fee_BlockType));
      }
    }
  }

  return r;
}

static Std_ReturnType Fee_DoBackup_Copy_Start(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  uint8_t nextBank = context->curWrokingBank + 1u;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ASLOG(FEE, ("do backup copy from bank %d to bank %d\n", context->curWrokingBank, nextBank));
  /* using DataBufferPtr to record current bank adminFreeAddr */
  context->jobDataBufferPtr = U32_TO_U8PTR(context->adminFreeAddr);
  context->curWrokingBank = nextBank;
  context->adminFreeAddr = bank->LowAddress + offsetof(Fee_BankAdminType, blocks);
  context->dataFreeAddr = bank->HighAddress;
  context->jobBlockId = 0;

  return factory_goto(FEE_NODE_BACKUP_COPY_ADMIN);
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Fee_FactoryStateNotification(uint8_t machineId, machine_state_t state) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  switch (machineId) {
  case FEE_MACHINE_READ:
  case FEE_MACHINE_WRITE:
    switch (state) {
    case MACHINE_STOP:
      Fee_RefreshContextCrc();
      config->JobEndNotification();
      break;
    case MACHINE_FAIL:
      Fee_RefreshContextCrc();
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
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
#ifndef FLS_DIRECT_ACCESS
  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(FEE_NODE_INIT_CHECK_BANK_INFO);
  } else {
    bank = &config->Banks[context->curWrokingBank];
    bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
    ret = Fls_Read(bank->LowAddress, (uint8_t *)bankAdmin, sizeof(*bankAdmin));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_RETRY;
    }
  }
#else
  while ((context->curWrokingBank < config->numOfBanks) && (E_NOT_OK == ret)) {
    bank = &config->Banks[context->curWrokingBank];
    bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
    (void)memcpy(bankAdmin, FEE_ADDRESS(bank->LowAddress), sizeof(*bankAdmin));
    if ((FEE_MAGIC_NUMBER != bankAdmin->HeaderMagic.MagicNumber) ||
        (((uint32_t)~FEE_MAGIC_NUMBER) != bankAdmin->HeaderMagic.InvMagicNumber)) {
      ASLOG(FEEE, ("FEE Bank %d is invalid, erase it\n", context->curWrokingBank));
      ret = factory_goto(FEE_NODE_INIT_ERASE_INVALID_BANK);
    } else {
#ifdef FEE_USE_BLANK_CHECK
      ret = factory_goto(FEE_NODE_INIT_BLANK_CHECK_INFO);
#else /* This Bank is valid, moving to read next bank admin data */
      context->curWrokingBank++;
#endif
    }
  }
  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(FEE_NODE_INIT_CHECK_BANK_INFO);
  }
#endif

  return ret;
}

Std_ReturnType Fee_Init_ReadBankAdmin_End(void) {
  Std_ReturnType ret = E_NOT_OK;
#ifndef FLS_DIRECT_ACCESS
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  if ((FEE_MAGIC_NUMBER != bankAdmin->HeaderMagic.MagicNumber) ||
      (((uint32_t)~FEE_MAGIC_NUMBER) != bankAdmin->HeaderMagic.InvMagicNumber)) {
    ASLOG(FEEE, ("FEE Bank %d is invalid, erase it\n", context->curWrokingBank));
    ret = factory_goto(FEE_NODE_INIT_ERASE_INVALID_BANK);
  } else {
#ifdef FEE_USE_BLANK_CHECK
    ret = factory_goto(FEE_NODE_INIT_BLANK_CHECK_INFO);
#else /* This Bank is valid, moving to read next bank admin data */
    context->curWrokingBank++;
    ret = factory_goto(FEE_NODE_INIT_READ_BANK_ADMIN);
#endif
  }
#endif
  return ret;
}

Std_ReturnType Fee_Init_ReadBankAdmin_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Init_BlankCheckInfo_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;

  bank = &config->Banks[context->curWrokingBank];
  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, Info),
                       FEE_ALIGNED(sizeof(Fee_BankInfoType)));
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Init_BlankCheckInfo_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  /* info is in erased state */
  (void)memset(&bankAdmin->Info, FLS_ERASED_VALUE, sizeof(bankAdmin->Info));
  return factory_goto(FEE_NODE_INIT_BLANK_CHECK_BLOCK);
}

Std_ReturnType Fee_Init_BlankCheckInfo_Error(void) {
  Fee_ContextType *context = &Fee_Context;
  /* info has data, trust it */
  context->retryCounter = 0;

  return factory_goto(FEE_NODE_INIT_BLANK_CHECK_BLOCK);
}

Std_ReturnType Fee_Init_BlankCheckBlock_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;

  bank = &config->Banks[context->curWrokingBank];
  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, blocks), FEE_PAGE_SIZE);
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Init_BlankCheckBlock_End(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  /* This bank has been erased, moving to read next bank admin data */
  (void)memset(bankAdmin->blocks, FLS_ERASED_VALUE, FEE_PAGE_SIZE);
  context->curWrokingBank++;
  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(FEE_NODE_INIT_CHECK_BANK_INFO);
  } else {
    ret = factory_goto(FEE_NODE_INIT_READ_BANK_ADMIN);
  }
  return ret;
}

Std_ReturnType Fee_Init_BlankCheckBlock_Error(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  /* block has data, trust it */
  context->retryCounter = 0;
  context->curWrokingBank++;

  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(FEE_NODE_INIT_CHECK_BANK_INFO);
  } else {
    ret = factory_goto(FEE_NODE_INIT_READ_BANK_ADMIN);
  }
  return ret;
}

Std_ReturnType Fee_Init_EraseInvalidBank_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;

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
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin =
    &(((Fee_BankAdminType *)config->workingArea)[context->curWrokingBank]);
  /* This bank has been erased, moving to read next bank admin data */
  (void)memset(bankAdmin, FLS_ERASED_VALUE, sizeof(*bankAdmin));
  context->curWrokingBank++;

  if (context->curWrokingBank >= config->numOfBanks) {
    ret = factory_goto(FEE_NODE_INIT_CHECK_BANK_INFO);
  } else {
    ret = factory_goto(FEE_NODE_INIT_READ_BANK_ADMIN);
  }

  return ret;
}

Std_ReturnType Fee_Init_EraseInvalidBank_Error(void) {
  return FACTORY_E_RETRY;
}

/* make sure that block has valid info, if found invalid info, it must be just after
 * erase, so that update the info with correct information as possible */
Std_ReturnType Fee_Init_CheckBankInfo_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  uint16_t i;
  uint32_t maxNumber = 0;
  int invalidBank = -1;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  Fee_ContextType *context = &Fee_Context;

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
      ret = FACTORY_E_RETRY;
    }
  } else {
    context->erasedNumber = maxNumber;
    ret = factory_goto(FEE_NODE_INIT_CHECK_BANK_MAGIC);
  }

  return ret;
}

Std_ReturnType Fee_Init_CheckBankInfo_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  uint16_t i;
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

  return factory_goto(FEE_NODE_INIT_CHECK_BANK_INFO);
}

Std_ReturnType Fee_Init_CheckBankInfo_Error(void) {
  return FACTORY_E_RETRY;
}

/* check each bank has a valid magic number, if magic number invalid, it must be just after
 * erase, so in this case, set the magic number to the correct one */
Std_ReturnType Fee_Init_CheckBankMagic_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  uint16_t i;
  int invalidBank = -1;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;

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
      ret = FACTORY_E_RETRY;
    }
  } else {
    ret = factory_goto(FEE_NODE_INIT_GET_WORKING_BANK);
  }

  return ret;
}

Std_ReturnType Fee_Init_CheckBankMagic_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[config->numOfBanks];
  uint16_t i;
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

  return factory_goto(FEE_NODE_INIT_CHECK_BANK_MAGIC);
}

Std_ReturnType Fee_Init_CheckBankMagic_Error(void) {
  return FACTORY_E_RETRY;
}

/* will firstly to get a bank that is being used by check the block admin area, and then go on
 * to search the free space */
Std_ReturnType Fee_Init_GetWorkingBank_Main(void) {
  Std_ReturnType ret = E_OK;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  uint16_t i;
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
          Fee_Panic(FEE_FAULT_TOO_MANY_FULL_BANK);
          /* Note, 2 or more bank are full, but will still going on and use the first full bank as
           * data soruce, and will erase the next bank if needed */
          ret = E_NOT_OK;
        }
      }
    }
  }

  if (0 == bankWithBlocks) {
    whichBank = 0;
  }
  if (E_OK != ret) {
    /* panic, do nothing */
  } else if ((0 == bankWithBlocks) || (1 == bankWithBlocks)) {
    /* no bank or only 1 bank has data, use the first bank */
    context->adminFreeAddr =
      config->Banks[whichBank].LowAddress + offsetof(Fee_BankAdminType, blocks);
    context->dataFreeAddr = config->Banks[whichBank].HighAddress;
    context->curWrokingBank = whichBank;
    ASLOG(FEE, ("FEE Init and Post Check Finished, activate bank is %d\n", whichBank));
    ret = factory_goto(FEE_NODE_INIT_SEARCH_FREE_SPACE);
  } else if (2 == bankWithBlocks) {
    if (fullBank != -1) {
      /* 2 banks has data, there is must be one in FULL state, and should ensure all data backed up
       * and erase it */
      context->adminFreeAddr =
        config->Banks[fullBank].LowAddress + offsetof(Fee_BankAdminType, blocks);
      context->dataFreeAddr = config->Banks[fullBank].HighAddress;
      context->curWrokingBank = fullBank;
      ASLOG(FEE, ("FEE Init and Post Check Finished, activate bank is %d, but which is full\n",
                  whichBank));
      ret = factory_goto(FEE_NODE_INIT_SEARCH_FREE_SPACE);
    } else {
      ASLOG(FEEE, ("impossible case, 2 banks has data, but no one is full\n"));
      Fee_Panic(FEE_FAULT_NO_BANK_IS_FULL);
      ret = E_NOT_OK;
    }
  } else {
    ASLOG(FEEE, ("impossible case, %d banks has data\n", bankWithBlocks));
    Fee_Panic(FEE_FAULT_TOO_MANY_BANK_WITH_DATA);
    ret = E_NOT_OK;
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
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
#ifndef FLS_DIRECT_ACCESS
  uint16_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
#else
  boolean ending = FALSE;
  uint16_t numOfBlocks = FEE_MAX_ADMIN_READ_PER_CYCLE;
#endif
  uint32_t endAddress = context->adminFreeAddr + (numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)));
  bank = &config->Banks[context->curWrokingBank];

  if (endAddress > bank->HighAddress) {
    endAddress = bank->HighAddress;
    numOfBlocks = (endAddress - context->adminFreeAddr) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  if (numOfBlocks > 0u) {
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
        ret = factory_switch(FEE_MACHINE_BACKUP);
        ASLOG(FEE, ("bank %d is full, do backup\n", context->curWrokingBank));
        STD_TRACE_APP(FEE_BACKUP_B);
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
      ret = FACTORY_E_RETRY;
    }
#endif
  } else {
    ASLOG(FEEE, ("Free space between 0x%X - 0x%X, abnormal, do backup\n", context->adminFreeAddr,
                 context->dataFreeAddr));
    ret = factory_switch(FEE_MACHINE_BACKUP);
    STD_TRACE_APP(FEE_BACKUP_B);
  }

  return ret;
}

Std_ReturnType Fee_Init_SearchFreeSpace_End(void) {
#ifndef FLS_DIRECT_ACCESS
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  uint16_t numOfBlocks = config->sizeOfWorkingArea / FEE_ALIGNED(sizeof(Fee_BlockType));
  uint32_t endAddress = context->adminFreeAddr + (numOfBlocks * FEE_ALIGNED(sizeof(Fee_BlockType)));
  Fee_BlockType *blocks = (Fee_BlockType *)config->workingArea;
  boolean ending;

  bank = &config->Banks[context->curWrokingBank];
  if (endAddress > bank->HighAddress) {
    endAddress = bank->HighAddress;
    numOfBlocks = (endAddress - context->adminFreeAddr) / FEE_ALIGNED(sizeof(Fee_BlockType));
  }

  ending = Fee_SearchFreeSpace_Analyze(blocks, numOfBlocks);

  if (FALSE == ending) {
    ret = factory_goto(FEE_NODE_INIT_SEARCH_FREE_SPACE);
  } else {
    ASLOG(FEE, ("Free space between 0x%X - 0x%X\n", context->adminFreeAddr, context->dataFreeAddr));
    if ((context->dataFreeAddr - context->adminFreeAddr) < FEE_MIN_FREE_SPACE) {
      ret = factory_switch(FEE_MACHINE_BACKUP);
      ASLOG(FEE, ("bank %d is full, do backup\n", context->curWrokingBank));
      STD_TRACE_APP(FEE_BACKUP_B);
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
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Read_ReadData_Main(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint32_t address = config->blockContexts[context->jobBlockId].Address;

  if (FEE_INVALID_ADDRESS != address) {
    ASLOG(FEEI, ("block %d goto read data @ %X\n", context->jobBlockId, address));
#if defined(FLS_DIRECT_ACCESS) || defined(FLS_DATA_DIRECT_ACCESS)
    ret = Fee_Read_ReadData_End();
#else
    ret = Fls_Read(address, config->workingArea,
                   FEE_DATA_ALIGNED(config->Blocks[context->jobBlockId].BlockSize));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ASLOG(FEE, ("Fls_Read Failed, try again\n"));
      ret = FACTORY_E_RETRY;
    }
#endif
  } else {
    ret = FACTORY_E_STOP;
    ASLOG(FEE, ("Read from ROM for block %d\n", context->jobBlockId));
    (void)memcpy(context->jobDataBufferPtr,
                 (uint8_t *)&(config->Blocks[context->jobBlockId].Rom[context->jobBlockOffset]),
                 context->jobLength);
  }

  return ret;
}

Std_ReturnType Fee_Read_ReadData_End(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint16_t *pCrc;
  uint16_t *pInvCrc;
  uint16_t Crc;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block =
    (const Fee_BlockType *)FEE_ADDRESS(config->blockContexts[context->jobBlockId].Address);
  const uint8_t *pData = (const uint8_t *)FEE_ADDRESS(block->Address);
#elif defined(FLS_DATA_DIRECT_ACCESS)
  const uint8_t *pData =
    (const uint8_t *)FEE_ADDRESS(config->blockContexts[context->jobBlockId].Address);
#else
  const uint8_t *pData = config->workingArea;
#endif
  pCrc = (uint16_t *)&pData[FEE_DATA_ALIGNED(config->Blocks[context->jobBlockId].BlockSize) - 4u];
  pInvCrc = &pCrc[1];

  if (((uint16_t) ~(*pCrc)) == (*pInvCrc)) {

    Crc = Crc_CalculateCRC16(
      pData, FEE_DATA_ALIGNED(config->Blocks[context->jobBlockId].BlockSize) - 4u, 0, TRUE);

    if (Crc == *pCrc) {
      ret = E_OK;
    }
  }

  if (E_OK == ret) {
    ASLOG(FEE, ("Read from addres %X for block %d\n",
                config->blockContexts[context->jobBlockId].Address, context->jobBlockId));
    (void)memcpy(context->jobDataBufferPtr, &pData[context->jobBlockOffset], context->jobLength);
    ret = FACTORY_E_STOP;
  } else {
    if (config->blockContexts[context->jobBlockId].Address != FEE_INVALID_ADDRESS) {
      ASLOG(
        FEE,
        ("Addres %X for block %d has invalid data (R %X ~ %X E %X ), searching next valid one\n",
         config->blockContexts[context->jobBlockId].Address, context->jobBlockId, *pCrc, *pInvCrc,
         Crc));
      config->blockContexts[context->jobBlockId].Address = FEE_INVALID_ADDRESS;
    }
    context->retryCounter = 0;
    ret = factory_goto(FEE_NODE_READ_SEARCH_NEXT);
  }

  return ret;
}

Std_ReturnType Fee_Read_ReadData_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Read_SearchNext_Main(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  ret = Fee_DoSearchNextValidDataStart(context->curWrokingBank);
  if (FEE_E_EXIST == ret) {
    config->blockContexts[context->jobBlockId].Address = FEE_INVALID_ADDRESS;
    ret = FACTORY_E_STOP;
    ASLOG(FEE, ("Read from ROM for block %d\n", context->jobBlockId));
    (void)memcpy(context->jobDataBufferPtr,
                 (uint8_t *)&(config->Blocks[context->jobBlockId].Rom[context->jobBlockOffset]),
                 context->jobLength);
  } else {
#ifndef FLS_DIRECT_ACCESS
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_RETRY;
    }
#else
    ret = Fee_DoSearchNextValidDataEnd(context->curWrokingBank);
    if (E_OK == ret) {
      ret = factory_goto(FEE_NODE_READ_READ_DATA);
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
    ret = factory_goto(FEE_NODE_READ_READ_DATA);
  } else {
    ret = factory_goto(FEE_NODE_READ_SEARCH_NEXT);
  }

#ifdef FLS_DIRECT_ACCESS
  context->retryCounter = 0;
#endif

  return ret;
}

Std_ReturnType Fee_Read_SearchNext_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Write_WriteCheckDataChanged_Main(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  uint32_t address = config->blockContexts[context->jobBlockId].Address;
  uint16_t size = config->Blocks[context->jobBlockId].BlockSize;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block;
  const uint8_t *pData;
#elif defined(FLS_DATA_DIRECT_ACCESS)
  const uint8_t *pData;
#endif
  if (FEE_INVALID_ADDRESS != address) {
#ifdef FLS_DIRECT_ACCESS
    block = (const Fee_BlockType *)FEE_ADDRESS(address);
    pData = (const uint8_t *)FEE_ADDRESS(block->Address);
#elif defined(FLS_DATA_DIRECT_ACCESS)
    pData = (const uint8_t *)FEE_ADDRESS(address);
#endif
#if defined(FLS_DIRECT_ACCESS) || defined(FLS_DATA_DIRECT_ACCESS)
    (void)memcpy(config->workingArea, pData, FEE_DATA_ALIGNED(size));
    ret = Fee_Write_WriteCheckDataChanged_End();
#else
    ASLOG(FEEI, ("block %d goto read data @ %X\n", context->jobBlockId, address));
    ret = Fls_Read(address, config->workingArea, FEE_DATA_ALIGNED(size));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ASLOG(FEE, ("Fls_Read Failed, try again\n"));
      ret = FACTORY_E_RETRY;
    }
#endif
  } else { /* no data in FEE */
    ret = factory_goto(FEE_NODE_WRITE_WRITE_ADMIN);
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteCheckDataChanged_End(void) {
  Std_ReturnType ret = E_OK;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  uint16_t size = config->Blocks[context->jobBlockId].BlockSize;
  const uint8_t *pData = config->workingArea;
#if !defined(FLS_DIRECT_ACCESS) && !defined(FLS_DATA_DIRECT_ACCESS)
  uint16_t *pCrc;
  uint16_t *pInvCrc;
  uint16_t Crc;
#endif
#if !defined(FLS_DIRECT_ACCESS) && !defined(FLS_DATA_DIRECT_ACCESS)
  pCrc = (uint16_t *)&pData[FEE_DATA_ALIGNED(size) - 4u];
  pInvCrc = &pCrc[1];
  if (((uint16_t) ~(*pCrc)) == (*pInvCrc)) {
    Crc = Crc_CalculateCRC16(pData, FEE_DATA_ALIGNED(size) - 4u, 0, TRUE);
    if (Crc != *pCrc) { /* CRC invalid */
      ret = factory_goto(FEE_NODE_WRITE_WRITE_ADMIN);
    }
  } else { /* data block corrupted */
    ret = factory_goto(FEE_NODE_WRITE_WRITE_ADMIN);
  }
#endif

  if (E_OK == ret) {
    if (0u != memcmp(context->jobDataBufferPtr, pData, size)) {
      ret = factory_goto(FEE_NODE_WRITE_WRITE_ADMIN);
    } else { /* data unchanged, ignore this write request */
      ret = FACTORY_E_STOP;
    }
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteCheckDataChanged_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Write_WriteAdmin_Main(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;

  if ((context->dataFreeAddr - context->adminFreeAddr) >=
      FEE_BLOCK_ADMIN_AND_DATA_SIZE(config->Blocks[context->jobBlockId].BlockSize)) {
    ret = Fee_DoWrite_Admin(FALSE);
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_RETRY;
    }
  } else {
    ASLOG(FEEE, ("No space left, flash is dead\n"));
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteAdmin_End(void) {
  Fee_DoWrite_UpdateDataFreeAddr();
  return factory_goto(FEE_NODE_WRITE_WRITE_DATA);
}

Std_ReturnType Fee_Write_WriteAdmin_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Write_WriteData_Main(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;

  ret = Fee_DoWrite_Data(context->jobDataBufferPtr);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteData_End(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;

  ASLOG(FEE, ("Write block %d done, free space %X - %X\n", context->jobBlockId,
              context->adminFreeAddr, context->dataFreeAddr));
#ifdef FLS_DIRECT_ACCESS
  config->blockContexts[context->jobBlockId].Address =
    context->adminFreeAddr - FEE_ALIGNED(sizeof(Fee_BlockType));
#else
  config->blockContexts[context->jobBlockId].Address = context->dataFreeAddr;
#endif

  if ((context->dataFreeAddr - context->adminFreeAddr) < FEE_MIN_FREE_SPACE) {
    ret = factory_switch(FEE_MACHINE_BACKUP);
    ASLOG(FEEI, ("bank %d is full, do backup\n", context->curWrokingBank));
    STD_TRACE_APP(FEE_BACKUP_B);
  } else {
    ret = FACTORY_E_STOP;
  }

  return ret;
}

Std_ReturnType Fee_Write_WriteData_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_ReadAdmin_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank = &config->Banks[context->curWrokingBank];
  Std_ReturnType ret;

  STD_TRACE_APP(FEE_BACKUP_B);

#ifndef FLS_DIRECT_ACCESS
  ret = Fls_Read(bank->LowAddress, (uint8_t *)bankAdmin, sizeof(*bankAdmin));
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }
#else
  (void)memcpy(bankAdmin, (uint8_t *)FEE_ADDRESS(bank->LowAddress), sizeof(*bankAdmin));
  ret = Fee_Backup_ReadAdmin_End();
#endif

  return ret;
}

Std_ReturnType Fee_Backup_ReadAdmin_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Std_ReturnType ret;

  if ((FEE_MAGIC_NUMBER != bankAdmin->HeaderMagic.MagicNumber) ||
      (((uint32_t)~FEE_MAGIC_NUMBER) != bankAdmin->HeaderMagic.InvMagicNumber)) {
    ASLOG(FEEE, ("FEE Bank %d is with invalid magic\n", context->curWrokingBank));
    Fee_Panic(FEE_FAULT_BANK_WITH_INVALID_MAGIC);
    ret = E_NOT_OK;
  } else if (bankAdmin->Info.Number == (uint32_t)(~bankAdmin->Info.InvNumber)) {
#ifdef FEE_USE_BLANK_CHECK
    ret = factory_goto(FEE_NODE_BACKUP_CHECK_BANK_STATUS);
#else
    ret = factory_goto(FEE_NODE_BACKUP_ENSURE_FULL);
#endif
  } else {
    ASLOG(FEEE, ("bank with invalid Number\n"));
    Fee_Panic(FEE_FAULT_BANK_WITH_INVALID_INFO);
    ret = E_NOT_OK;
  }

  return ret;
}

Std_ReturnType Fee_Backup_ReadAdmin_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_CheckBankStatus_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank = &config->Banks[context->curWrokingBank];
  Std_ReturnType ret;

  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, Status),
                       FEE_ALIGNED(sizeof(Fee_BankStatusType)));
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_CheckBankStatus_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  (void)memset(&bankAdmin->Status, FLS_ERASED_VALUE, sizeof(bankAdmin->Status));

  return factory_goto(FEE_NODE_BACKUP_ENSURE_FULL);
}

Std_ReturnType Fee_Backup_CheckBankStatus_Error(void) {
  Fee_ContextType *context = &Fee_Context;
  context->retryCounter = 0;
  /* trust the bank status*/
  return factory_goto(FEE_NODE_BACKUP_ENSURE_FULL);
}

Std_ReturnType Fee_Backup_EnsureFull_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[1];
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank = &config->Banks[context->curWrokingBank];

  if (FEE_BANK_NOT_FULL_MAGIC == bankAdmin->Status.FullMagic) {
    workAdmin->Status.FullMagic = FEE_BANK_FULL_MAGIC;
    ret = Fee_FlsWrite(bank->LowAddress + offsetof(Fee_BankAdminType, Status),
                       (uint8_t *)&workAdmin->Status, sizeof(workAdmin->Status));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_RETRY;
    }
  } else {
    ret = factory_goto(FEE_NODE_BACKUP_READ_NEXT_BANK_ADMIN);
  }

  return ret;
}

Std_ReturnType Fee_Backup_EnsureFull_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  bankAdmin->Status.FullMagic = FEE_BANK_FULL_MAGIC;
  return factory_goto(FEE_NODE_BACKUP_READ_NEXT_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_EnsureFull_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_ReadNextBankAdmin_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);
  uint8_t nextBank = context->curWrokingBank + 1u;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  Std_ReturnType ret;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ret = Fls_Read(bank->LowAddress, (uint8_t *)bankAdmin, sizeof(*bankAdmin));
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_ReadNextBankAdmin_End(void) {
#ifdef FEE_USE_BLANK_CHECK
  return factory_goto(FEE_NODE_BACKUP_BLANK_CHECK_NEXT_BANK_EMPTY);
#else
  return factory_goto(FEE_NODE_BACKUP_ENSURE_NEXT_BANK_EMPTY);
#endif
}

Std_ReturnType Fee_Backup_ReadNextBankAdmin_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_BlankCheckNextBankEmpty_Main(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t nextBank = context->curWrokingBank + 1u;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ret = Fls_BlankCheck(bank->LowAddress + offsetof(Fee_BankAdminType, blocks), FEE_PAGE_SIZE);
  if (E_OK == ret) {
    context->retryCounter = 0;
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Backup_BlankCheckNextBankEmpty_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);
  (void)memset(bankAdmin->blocks, FLS_ERASED_VALUE, FEE_PAGE_SIZE);
  return factory_goto(FEE_NODE_BACKUP_ENSURE_NEXT_BANK_EMPTY);
}

Std_ReturnType Fee_Backup_BlankCheckNextBankEmpty_Error(void) {
  Fee_ContextType *context = &Fee_Context;
  /* block has data, trust it */
  context->retryCounter = 0;

  return factory_goto(FEE_NODE_BACKUP_ENSURE_NEXT_BANK_EMPTY);
}

Std_ReturnType Fee_Backup_EnsureNextBankEmpty_Main(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);
  if ((FEE_MAGIC_NUMBER != bankAdmin->HeaderMagic.MagicNumber) ||
      (((uint32_t)~FEE_MAGIC_NUMBER) != bankAdmin->HeaderMagic.InvMagicNumber)) {
    ASLOG(FEEE, ("FEE Next Bank Magic %d is invalid, erase it\n",
                 (context->curWrokingBank + 1) % config->numOfBanks));
    if (context->erasedNumber < config->NumberOfErasedCycles) {
      ret = factory_goto(FEE_NODE_BACKUP_ERASE_NEXT_BANK);
    } else {
      ASLOG(FEEE, ("flash is dead but next bank with invalid magic\n"));
      ret = FACTORY_E_STOP;
    }
  } else if (bankAdmin->Info.Number != (~bankAdmin->Info.InvNumber)) {
    ASLOG(FEEE, ("FEE Next Bank Info %d is invalid, erase it\n",
                 (context->curWrokingBank + 1) % config->numOfBanks));
    if (context->erasedNumber < config->NumberOfErasedCycles) {
      ret = factory_goto(FEE_NODE_BACKUP_ERASE_NEXT_BANK);
    } else {
      ASLOG(FEEE, ("flash is dead but next bank with invalid info\n"));
      ret = FACTORY_E_STOP;
    }
  } else if (TRUE == Fee_IsAllErased(bankAdmin->blocks, sizeof(bankAdmin->blocks))) {
    if (bankAdmin->Info.Number < config->NumberOfErasedCycles) {
      ret = Fee_DoBackup_Copy_Start();
    } else {
      ASLOG(FEEE, ("flash is dead\n"));
      ret = FACTORY_E_STOP;
    }
  } else {
    if (context->erasedNumber < config->NumberOfErasedCycles) {
      ret = factory_goto(FEE_NODE_BACKUP_ERASE_NEXT_BANK);
    } else {
      ASLOG(FEEE, ("flash is dead but next bank with some data\n"));
      ret = FACTORY_E_STOP;
    }
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
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t nextBank = context->curWrokingBank + 1u;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  Std_ReturnType ret;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  bank = &config->Banks[nextBank];

  ret = Fls_Erase(bank->LowAddress, bank->HighAddress - bank->LowAddress);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_EraseNextBank_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = &(((Fee_BankAdminType *)config->workingArea)[1]);

  (void)memset(bankAdmin, FLS_ERASED_VALUE, sizeof(Fee_BankAdminType));
  return factory_goto(FEE_NODE_BACKUP_SET_NEXT_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_EraseNextBank_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_SetNextBankAdmin_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *actAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *bankAdmin = &actAdmin[1];
  Fee_BankAdminType *workAdmin = &actAdmin[2];
  uint8_t nextBank = context->curWrokingBank + 1u;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  Std_ReturnType ret;
  uint32_t Number = context->erasedNumber;

  if (nextBank >= config->numOfBanks) {
    nextBank = 0;
  }
  Number += 1u; /* as exception case, always increase the number by 1 */
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
      ret = FACTORY_E_RETRY;
    }
  } else if (bankAdmin->HeaderMagic.MagicNumber != (~bankAdmin->HeaderMagic.InvMagicNumber)) {
    workAdmin->HeaderMagic.MagicNumber = FEE_MAGIC_NUMBER;
    workAdmin->HeaderMagic.InvMagicNumber = ~FEE_MAGIC_NUMBER;
    ret = Fee_FlsWrite(bank->LowAddress, (uint8_t *)&workAdmin->HeaderMagic,
                       sizeof(workAdmin->HeaderMagic));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_RETRY;
    }
  } else {
    context->erasedNumber = Number;
    ret = Fee_DoBackup_Copy_Start();
  }

  return ret;
}

Std_ReturnType Fee_Backup_SetNextBankAdmin_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
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

  return factory_goto(FEE_NODE_BACKUP_SET_NEXT_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_SetNextBankAdmin_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_CopyAdmin_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint16_t blockId;
  Std_ReturnType ret;

  blockId = context->jobBlockId;
  if (blockId < config->numOfBlocks) {
    if (config->blockContexts[blockId].Address == FEE_INVALID_ADDRESS) {
      for (blockId = blockId + 1u; blockId < config->numOfBlocks; blockId++) {
        if (config->blockContexts[blockId].Address != FEE_INVALID_ADDRESS) {
          break;
        }
      }
    }
  }

  if (blockId < config->numOfBlocks) {
    context->jobBlockId = blockId;
    /* for valid data search from */
#ifdef FLS_DIRECT_ACCESS
    context->jobNextAddr =
      config->blockContexts[blockId].Address - FEE_ALIGNED(sizeof(Fee_BlockType));
#else
    context->jobNextAddr = PTR_TO_U32(context->jobDataBufferPtr);
#endif
    if ((context->dataFreeAddr - context->adminFreeAddr) >=
        FEE_BLOCK_ADMIN_AND_DATA_SIZE(config->Blocks[blockId].BlockSize)) {
      ret = Fee_DoWrite_Admin(TRUE);
      if (E_OK == ret) {
        ret = FACTORY_E_EVENT;
      } else {
        ret = FACTORY_E_RETRY;
      }
    } else {
      ASLOG(FEEE, ("No space left, flash is dead\n"));
      Fee_Panic(FEE_FAULT_NO_SPACE_DURING_BACKUP);
      ret = E_NOT_OK;
    }
  } else {
    ret = factory_goto(FEE_NODE_BACKUP_ERASE_BANK);
  }

  return ret;
}

Std_ReturnType Fee_Backup_CopyAdmin_End(void) {
  Fee_DoWrite_UpdateDataFreeAddr();
  return factory_goto(FEE_NODE_BACKUP_COPY_READ_DATA);
}

Std_ReturnType Fee_Backup_CopyAdmin_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_CopyReadData_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Std_ReturnType ret;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block =
    (const Fee_BlockType *)FEE_ADDRESS(config->blockContexts[context->jobBlockId].Address);
  (void)memcpy(config->workingArea, FEE_ADDRESS(block->Address),
               FEE_DATA_ALIGNED(config->Blocks[context->jobBlockId].BlockSize));
  ret = Fee_Backup_CopyReadData_End();
#else
  ret = Fls_Read(config->blockContexts[context->jobBlockId].Address, config->workingArea,
                 FEE_DATA_ALIGNED(config->Blocks[context->jobBlockId].BlockSize));
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }
#endif
  return ret;
}

Std_ReturnType Fee_Backup_CopyReadData_End(void) {
  Std_ReturnType ret = E_NOT_OK;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint16_t *pCrc;
  uint16_t *pInvCrc;
  uint16_t Crc = 0;
  uint16_t alignedSize = FEE_DATA_ALIGNED(config->Blocks[context->jobBlockId].BlockSize);

  pCrc = (uint16_t *)&(config->workingArea)[alignedSize - 4u];
  pInvCrc = &pCrc[1];
  if (((uint16_t) ~(*pCrc)) == (*pInvCrc)) {
    Crc = Crc_CalculateCRC16(config->workingArea, alignedSize - 4u, 0, TRUE);
    if (Crc == *pCrc) {
      ret = E_OK;
    }
  }

  if (ret != E_OK) {
    if (config->blockContexts[context->jobBlockId].Address != FEE_INVALID_ADDRESS) {
      ASLOG(
        FEEE,
        ("Addres %X for block %d has invalid data (R %X, ~ %X, E %X), searching next valid one\n",
         config->blockContexts[context->jobBlockId].Address, context->jobBlockId, *pCrc, *pInvCrc,
         Crc));
      config->blockContexts[context->jobBlockId].Address = FEE_INVALID_ADDRESS;
    }
    ret = factory_goto(FEE_NODE_BACKUP_SEARCH_NEXT_DATA);
  } else {
    ret = factory_goto(FEE_NODE_BACKUP_COPY_DATA);
  }

  return ret;
}

Std_ReturnType Fee_Backup_CopyReadData_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_SearchNextData_Main(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t bankId = context->curWrokingBank - 1u;
  if (bankId >= config->numOfBanks) {
    bankId = config->numOfBanks - 1u;
  }

  ret = Fee_DoSearchNextValidDataStart(bankId);
  if (FEE_E_EXIST == ret) {
    ASLOG(FEEE, ("Fatal, can't backup for block %d\n", context->jobBlockId));
    context->jobBlockId++;
    ret = factory_goto(FEE_NODE_BACKUP_COPY_ADMIN);
  } else {
#ifndef FLS_DIRECT_ACCESS
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_RETRY;
    }
#else
    ret = Fee_Backup_SearchNextData_End();
    if (ret == factory_goto(FEE_NODE_BACKUP_SEARCH_NEXT_DATA)) {
      ret = E_OK;
    }
#endif
  }

  return ret;
}

Std_ReturnType Fee_Backup_SearchNextData_End(void) {
  Std_ReturnType ret;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  uint8_t bankId = context->curWrokingBank - 1u;

  if (bankId >= config->numOfBanks) {
    bankId = config->numOfBanks - 1u;
  }
  ret = Fee_DoSearchNextValidDataEnd(bankId);

  if (E_OK == ret) {
    ret = factory_goto(FEE_NODE_BACKUP_COPY_READ_DATA);
  } else {
    ret = factory_goto(FEE_NODE_BACKUP_SEARCH_NEXT_DATA);
  }

#ifdef FLS_DIRECT_ACCESS
  context->retryCounter = 0;
#endif

  return ret;
}

Std_ReturnType Fee_Backup_SearchNextData_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_CopyData_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Std_ReturnType ret;

  ret = Fee_DoWrite_Data(config->workingArea);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }
  return ret;
}

Std_ReturnType Fee_Backup_CopyData_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
#ifdef FLS_DIRECT_ACCESS
  config->blockContexts[context->jobBlockId].Address =
    context->adminFreeAddr - FEE_ALIGNED(sizeof(Fee_BlockType));
#else
  config->blockContexts[context->jobBlockId].Address = context->dataFreeAddr;
#endif
  context->jobBlockId += 1u;
  return factory_goto(FEE_NODE_BACKUP_COPY_ADMIN);
}

Std_ReturnType Fee_Backup_CopyData_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_EraseBank_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  Std_ReturnType ret;
  uint8_t erasedBank = context->curWrokingBank - 1u;

  if (erasedBank >= config->numOfBanks) {
    erasedBank = config->numOfBanks - 1u;
  }

  ASLOG(FEE, ("backup erase bank %d\n", erasedBank));
  bank = &config->Banks[erasedBank];
  ret = Fls_Erase(bank->LowAddress, bank->HighAddress - bank->LowAddress);
  if (E_OK == ret) {
    ret = FACTORY_E_EVENT;
  } else {
    ret = FACTORY_E_RETRY;
  }

  return ret;
}

Std_ReturnType Fee_Backup_EraseBank_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;

  (void)memset(bankAdmin, FLS_ERASED_VALUE, sizeof(Fee_BankAdminType));
  return factory_goto(FEE_NODE_BACKUP_SET_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_EraseBank_Error(void) {
  return FACTORY_E_RETRY;
}

Std_ReturnType Fee_Backup_SetBankAdmin_Main(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[1];
  P2CONST(Fee_BankType, AUTOMATIC, FEE_CONST) bank;
  Std_ReturnType ret;
  uint32_t Number = context->erasedNumber;
  uint8_t erasedBank = context->curWrokingBank - 1u;

  if (erasedBank >= config->numOfBanks) {
    erasedBank = config->numOfBanks - 1u;
  }
  if (0 == erasedBank) {
    Number += 1u; /* only increate Number for the first bank */
  }
  bank = &config->Banks[erasedBank];

  if (bankAdmin->Info.Number != ((uint32_t)~bankAdmin->Info.InvNumber)) {
    ASLOG(FEE, ("set bank %d Number = %d\n", erasedBank, Number));
    workAdmin->Info.Number = Number;
    workAdmin->Info.InvNumber = ~Number;
    ret = Fee_FlsWrite(bank->LowAddress + offsetof(Fee_BankAdminType, Info),
                       (uint8_t *)&workAdmin->Info, sizeof(workAdmin->Info));
    if (E_OK == ret) {
      ret = FACTORY_E_EVENT;
    } else {
      ret = FACTORY_E_RETRY;
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
      ret = FACTORY_E_RETRY;
    }
  } else {
    context->erasedNumber = Number;
    ret = FACTORY_E_STOP;
    ASLOG(FEE, ("backup done\n"));
    STD_TRACE_APP(FEE_BACKUP_E);
  }

  return ret;
}

Std_ReturnType Fee_Backup_SetBankAdmin_End(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_BankAdminType *bankAdmin = (Fee_BankAdminType *)config->workingArea;
  Fee_BankAdminType *workAdmin = &bankAdmin[1];

  if (bankAdmin->Info.Number != ((uint32_t)~bankAdmin->Info.InvNumber)) {
    bankAdmin->Info = workAdmin->Info;
  } else if (bankAdmin->HeaderMagic.MagicNumber != (~bankAdmin->HeaderMagic.InvMagicNumber)) {
    bankAdmin->HeaderMagic = workAdmin->HeaderMagic;
  } else {
  }

  return factory_goto(FEE_NODE_BACKUP_SET_BANK_ADMIN);
}

Std_ReturnType Fee_Backup_SetBankAdmin_Error(void) {
  return FACTORY_E_RETRY;
}

void Fee_JobEndNotification(void) {
  Fee_ContextType *context = &Fee_Context;
  Fee_CheckContextCrc();
  context->retryCounter = 0;
  (void)factory_on_event(&Fee_Factory, FEE_EVENT_END);
  Fee_RefreshContextCrc();
}

void Fee_JobErrorNotification(void) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;

  Fee_CheckContextCrc();
  if ((FEE_MACHINE_INIT == Fee_Factory.context->machineId) &&
      ((FEE_NODE_INIT_BLANK_CHECK_INFO == Fee_Factory.context->nodeId) ||
       (FEE_NODE_INIT_BLANK_CHECK_BLOCK == Fee_Factory.context->nodeId))) {
    /* OK, blank check error callback is not error. */
  } else if ((FEE_MACHINE_BACKUP == Fee_Factory.context->machineId) &&
             (FEE_NODE_BACKUP_CHECK_BANK_STATUS == Fee_Factory.context->nodeId)) {
    /* OK, blank check error callback is not error. */
  } else {
    ASLOG(FEEE, ("Error when %s:%s\n", Fee_Factory.machines[Fee_Factory.context->machineId].name,
                 Fee_Factory.machines[Fee_Factory.context->machineId]
                   .nodes[Fee_Factory.context->nodeId]
                   .name));
  }

  if (context->retryCounter < config->maxJobRetry) {
    context->retryCounter++;
    (void)factory_on_event(&Fee_Factory, FEE_EVENT_ERROR);
  } else {
    ASLOG(FEEE, ("reach max attempts during error:%s:%s\n",
                 Fee_Factory.machines[Fee_Factory.context->machineId].name,
                 Fee_Factory.machines[Fee_Factory.context->machineId]
                   .nodes[Fee_Factory.context->nodeId]
                   .name));
    factory_cancel(&Fee_Factory);
  }
  Fee_RefreshContextCrc();
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

void Fee_Init(P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) ConfigPtr) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint16_t i;
  (void)ConfigPtr;

  asAssert(config->sizeOfWorkingArea >= ((config->numOfBanks + 1) * sizeof(Fee_BankAdminType)));

  for (i = 0; i < config->numOfBlocks; i++) {
    config->blockContexts[i].Address = FEE_INVALID_ADDRESS;
#ifndef FLS_DIRECT_ACCESS
    config->blockContexts[i].NumberOfWriteCycles = 0;
#endif
  }
  (void)memset(context, 0, sizeof(Fee_ContextType));
  factory_init(&Fee_Factory);
  (void)factory_start_machine(&Fee_Factory, FEE_MACHINE_INIT);
  Fee_RefreshContextCrc();
}

Std_ReturnType Fee_Read(uint16_t BlockNumber, uint16_t BlockOffset, uint8_t *DataBufferPtr,
                        uint16_t Length) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Std_ReturnType r;

  (void)config;

  DET_VALIDATE((BlockNumber > 0) && ((BlockNumber - 1) < config->numOfBlocks), 0x02,
               FEE_E_INVALID_BLOCK_NO, return E_NOT_OK);
  DET_VALIDATE(config->Blocks[BlockNumber - 1].BlockSize >= Length, 0x02, FEE_E_INVALID_BLOCK_LEN,
               return E_NOT_OK);
  DET_VALIDATE(config->Blocks[BlockNumber - 1].BlockSize >= ((uint32_t)BlockOffset + Length), 0x02,
               FEE_E_INVALID_BLOCK_OFS, return E_NOT_OK);

  Fee_CheckContextCrc();
  r = factory_start_machine(&Fee_Factory, FEE_MACHINE_READ);
  if (E_OK == r) {
    context->retryCounter = 0;
    context->jobBlockId = BlockNumber - 1u;
    context->jobBlockOffset = BlockOffset;
    context->jobDataBufferPtr = DataBufferPtr;
    context->jobLength = Length;
#ifdef FLS_DIRECT_ACCESS
    if (FEE_INVALID_ADDRESS != config->blockContexts[context->jobBlockId].Address) {
      /* valid history data before current address */
      context->jobNextAddr =
        config->blockContexts[context->jobBlockId].Address - FEE_ALIGNED(sizeof(Fee_BlockType));
    } else {
      context->jobNextAddr = context->adminFreeAddr;
    }
#else
    context->jobNextAddr = context->adminFreeAddr;
#endif
    Fee_RefreshContextCrc();
  }

  return r;
}

Std_ReturnType Fee_Write(uint16_t BlockNumber, const uint8_t *DataBufferPtr) {
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  Std_ReturnType r = E_OK;
#ifdef FLS_DIRECT_ACCESS
  const Fee_BlockType *block;
#endif

  (void)config;

  DET_VALIDATE((BlockNumber > 0) && ((BlockNumber - 1) < config->numOfBlocks), 0x03,
               FEE_E_INVALID_BLOCK_NO, return E_NOT_OK);

#ifdef FLS_DIRECT_ACCESS
  if (FEE_INVALID_ADDRESS != config->blockContexts[BlockNumber - 1u].Address) {
    block = (const Fee_BlockType *)FEE_ADDRESS(config->blockContexts[BlockNumber - 1u].Address);
    if (block->NumberOfWriteCycles >= config->Blocks[BlockNumber - 1u].NumberOfWriteCycles) {
      ASLOG(FEEE, ("block %d reach end of life %u\n", BlockNumber, block->NumberOfWriteCycles));
      r = E_NOT_OK;
    }
  }
#endif

  if (E_OK == r) {
    Fee_CheckContextCrc();
    r = factory_start_machine(&Fee_Factory, FEE_MACHINE_WRITE);
  }

  if (E_OK == r) {
    context->retryCounter = 0;
    context->jobBlockId = BlockNumber - 1u;
    context->jobDataBufferPtr = (uint8_t *)DataBufferPtr;
    Fee_RefreshContextCrc();
  }

  return r;
}

void Fee_SetMode(MemIf_ModeType Mode) {
  Fls_SetMode(Mode);
}

void Fee_MainFunction(void) {
  Std_ReturnType ret;
  P2CONST(Fee_ConfigType, AUTOMATIC, FEE_CONST) config = FEE_CONFIG;
  Fee_ContextType *context = &Fee_Context;
  uint8_t state = factory_get_state(&Fee_Factory);

  if (FACTORY_RUNNING == state) {
    Fee_CheckContextCrc();
    if (context->retryCounter < config->maxJobRetry) {
      context->retryCounter++;
      ret = factory_main(&Fee_Factory);
      if ((FACTORY_E_SWITCH_TO == ret) || (FACTORY_E_GOTO == ret)) {
        context->retryCounter = 0;
        (void)factory_main(&Fee_Factory);
      }
    } else {
      ASLOG(FEEE, ("reach max attempts during main:%s:%s\n",
                   Fee_Factory.machines[Fee_Factory.context->machineId].name,
                   Fee_Factory.machines[Fee_Factory.context->machineId]
                     .nodes[Fee_Factory.context->nodeId]
                     .name));
      factory_cancel(&Fee_Factory);
    }
    Fee_RefreshContextCrc();
  }
}

void Fee_GetAdminInfo(Fee_AdminInfoType *pAdminInfo) {
  Fee_ContextType *context = &Fee_Context;
  pAdminInfo->adminFreeAddr = context->adminFreeAddr;
  pAdminInfo->dataFreeAddr = context->dataFreeAddr;
  pAdminInfo->curWrokingBank = context->curWrokingBank;
  pAdminInfo->erasedNumber = context->erasedNumber;
}
