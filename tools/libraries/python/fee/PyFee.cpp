/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <map>
#include <iostream>
#include <thread>

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "Fee.h"
#include "Fee_Priv.h"
#include "Fls.h"
#include "Fls_Priv.h"
#define WEAK_ALIAS_PRINTF
#include "Std_Debug.h"

namespace py = pybind11;
/* ================================ [ MACROS    ] ============================================== */
#ifndef FLS_TOTAL_SIZE
#define FLS_TOTAL_SIZE (1 * 1024 * 1024)
#endif

#define LOG_FILE_MAX_SIZE (10 * 1024 * 1024)
#define LOG_FILE_MAX_NUMBER 2
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern "C" {
Std_ReturnType Fls_AcErase(Fls_AddressType address, Fls_LengthType length);
Std_ReturnType Fls_AcWrite(Fls_AddressType address, const uint8_t *data, Fls_LengthType length);
boolean Fls_AcIsIdle(void);
}
/* ================================ [ DATAS     ] ============================================== */
FILE *_stddebug = NULL;
int lLogIndex = 0;
extern "C" {
Fls_ConfigType Fls_Config;
Fee_ConfigType Fee_Config;
extern uint8_t g_FlsAcMirror[];
}

static Fls_SectorType *Fls_Sectors = nullptr;
static Fee_BankType *Fee_Banks = nullptr;
static std::vector<Fee_BlockConfigType> Fee_BlockConfig;
static std::map<std::string, uint16_t> Fee_BlockMap;
static bool lConfigured = false;
static bool lPowerOn = false;
static bool lResult = false;
static uint8_t *lDataPtr = nullptr;
/* ================================ [ LOCALS    ] ============================================== */
static void AsPyExit(void) {
  if (_stddebug) {
    fclose(_stddebug);
  }
}

static void __attribute__((constructor)) AsPyInit(void) {
  _stddebug = fopen(".Fee0.log", "w");
  atexit(AsPyExit);
}

static void JobEndNotification(void) {
  lResult = true;
}

static void JobErrorNotification(void) {
  lResult = false;
}

static void precheck(void) {
  if (false == lConfigured) {
    throw std::runtime_error("PyFee has not been configured");
  }

  if (false == lPowerOn) {
    throw std::runtime_error("PyFee is not powerred on");
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
extern "C" int std_printf(const char *fmt, ...) {
  va_list args;
  int length;
  char logP[128];

  if (_stddebug) {
    va_start(args, fmt);
    length = vfprintf(_stddebug, fmt, args);
    // vprintf(fmt, args);
    va_end(args);

    if (ftell(_stddebug) > LOG_FILE_MAX_SIZE) {
      lLogIndex++;
      if (lLogIndex >= LOG_FILE_MAX_NUMBER) {
        lLogIndex = 0;
      }
      snprintf(logP, sizeof(logP), ".Fee%d.log", lLogIndex);
      fclose(_stddebug);
      _stddebug = fopen(logP, "w");
    }
  }

  return length;
}

template <typename To, typename Ti> To get(py::kwargs &kwargs, std::string key, To dft) {
  To r = dft;
  try {
    r = Ti(kwargs[key.c_str()]);
  } catch (const std::exception &e) {
  }
  return r;
}

template <typename To, typename Ti> To get(py::dict &kwargs, std::string key, To dft) {
  To r = dft;
  try {
    r = Ti(kwargs[key.c_str()]);
  } catch (const std::exception &e) {
  }
  return r;
}

bool PyFee_Erase(py::kwargs kwargs) {
  uint32_t address = get<uint32_t, py::int_>(kwargs, "address", 0);
  uint32_t size = get<uint32_t, py::int_>(kwargs, "size", 1024 * 1024);
  int nLoops = 10;
  Std_ReturnType r = E_FLS_PENDING;

  while ((E_FLS_PENDING == r) && (nLoops > 0)) {
    r = Fls_AcErase(address, size);
    nLoops--;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return (E_OK == r);
}

bool PyFee_Config(py::dict blocks, py::kwargs kwargs) {
  uint32_t pageSize = get<uint32_t, py::int_>(kwargs, "pageSize", 8);
  uint32_t sectorSize = get<uint32_t, py::int_>(kwargs, "sectorSize", 512);
  uint32_t blockSize = get<uint32_t, py::int_>(kwargs, "blockSize", 64 * 1024);
  uint32_t bankSize = get<uint32_t, py::int_>(kwargs, "bankSize", 2);
  uint32_t scratchSize = get<uint32_t, py::int_>(kwargs, "scratchSize", 512);
  uint32_t NumberOfWriteCycles = get<uint32_t, py::int_>(kwargs, "NumberOfWriteCycles", 10000000);
  uint32_t NumberOfErasedCycles = get<uint32_t, py::int_>(kwargs, "NumberOfErasedCycles", 10000000);
  if (Fls_Sectors)
    delete[] Fls_Sectors;
  Fls_Sectors = new Fls_SectorType[bankSize];
  for (uint32_t i = 0; i < bankSize; i++) {
    Fls_Sectors[i].NumberOfSectors = blockSize / sectorSize;
    Fls_Sectors[i].PageSize = pageSize;
    Fls_Sectors[i].SectorSize = sectorSize;
    Fls_Sectors[i].SectorStartAddress = i * blockSize;
    Fls_Sectors[i].SectorEndAddress = (i + 1) * blockSize;
  }

  Fls_Config.JobEndNotification = Fee_JobEndNotification;
  Fls_Config.JobErrorNotification = Fee_JobErrorNotification;
  Fls_Config.defaultMode = MEMIF_MODE_FAST;
  Fls_Config.MaxReadFastMode = 4096;
  Fls_Config.MaxReadNormalMode = 512;
  Fls_Config.MaxWriteFastMode = 4096;
  Fls_Config.MaxWriteNormalMode = 512;
  Fls_Config.MaxEraseFastMode = 4096;
  Fls_Config.MaxEraseNormalMode = 512;
  Fls_Config.SectorList = Fls_Sectors;
  Fls_Config.numOfSectors = bankSize;

  uint16_t BlockNumber = 1;
  uint32_t maxDataSize = 0;
  Fee_BlockConfig.clear();
  for (auto item : blocks) {
    std::string name = py::str(item.first);
    uint16_t blockSize = get<uint16_t, py::int_>(blocks, name, 0);
    Fee_BlockConfigType blockConfig;
    blockConfig.BlockNumber = BlockNumber;
    blockConfig.BlockSize = blockSize;
    blockConfig.NumberOfWriteCycles = NumberOfWriteCycles;
    uint8_t *rom = new uint8_t[blockSize];
    memset(rom, 0, blockSize);
    if (blockSize > maxDataSize) {
      maxDataSize = blockSize;
    }
    blockConfig.Rom = rom;
    Fee_BlockConfig.push_back(blockConfig);
    Fee_BlockMap[name] = BlockNumber;
    BlockNumber++;
  };

  uint32_t *Fee_BlockDataAddress = new uint32_t[BlockNumber - 1];

  if (Fee_Banks)
    delete[] Fee_Banks;
  Fee_Banks = new Fee_BankType[bankSize];
  for (uint32_t i = 0; i < bankSize; i++) {
    Fee_Banks[i].LowAddress = blockSize * i;
    Fee_Banks[i].HighAddress = blockSize * (i + 1);
  }

  if (lDataPtr)
    delete[] lDataPtr;
  lDataPtr = new uint8_t[maxDataSize + 32];

  uint32_t workingAreaSize = maxDataSize + 32;
  if (workingAreaSize < (bankSize * sizeof(Fee_BankAdminType))) {
    workingAreaSize = bankSize * sizeof(Fee_BankAdminType);
  }
  if (workingAreaSize < scratchSize) {
    workingAreaSize = scratchSize;
  }
  workingAreaSize = ((workingAreaSize + 31) / 32) * 32;
  uint32_t *Fee_WorkingArea = new uint32_t[workingAreaSize / sizeof(uint32_t)];

  Fee_Config.JobEndNotification = JobEndNotification;
  Fee_Config.JobErrorNotification = JobErrorNotification;
  if (Fee_Config.blockAddress)
    delete[] Fee_Config.blockAddress;
  Fee_Config.blockAddress = Fee_BlockDataAddress;
  Fee_Config.Blocks = Fee_BlockConfig.data();
  Fee_Config.numOfBlocks = Fee_BlockConfig.size();
  Fee_Config.Banks = Fee_Banks;
  Fee_Config.numOfBanks = bankSize;
  if (Fee_Config.workingArea)
    delete[] Fee_Config.workingArea;
  Fee_Config.workingArea = (uint8_t *)Fee_WorkingArea;
  Fee_Config.sizeOfWorkingArea = workingAreaSize;
  Fee_Config.maxJobRetry = 0;
  Fee_Config.maxDataSize = maxDataSize;
  Fee_Config.NumberOfErasedCycles = NumberOfErasedCycles;

  lConfigured = true;
  return true;
}

int PyFee_Schedule(int nLoops = 0) {
  int r = 0;
  int i;

  precheck();

  if (nLoops > 0) {
    for (i = 0; i < nLoops; i++) {
      Fee_MainFunction();
      Fls_MainFunction();
      auto status = Fee_GetStatus();
      if (MEMIF_IDLE == status) {
        break;
      }
    }
    r = i;
  } else {
    auto status = Fee_GetStatus();
    while (MEMIF_IDLE != status) {
      Fee_MainFunction();
      Fls_MainFunction();
      status = Fee_GetStatus();
      r++;
    }
  }

  return r;
}

void PyFee_PowerOn() {
  if (false == lConfigured) {
    throw std::runtime_error("PyFee has not been configured");
  }

  if (true == lPowerOn) {
    throw std::runtime_error("PyFee is already powerred on");
  }

  Fls_Init(NULL);
  Fee_Init(NULL);
  lPowerOn = true;

  PyFee_Schedule();
}

void PyFee_PowerOff() {
  int timeouts = 100;
  if (false == lConfigured) {
    throw std::runtime_error("PyFee has not been configured");
  }

  if (false == lPowerOn) {
    throw std::runtime_error("PyFee is already powerred off");
  }

  while ((!Fls_AcIsIdle()) && (timeouts-- > 0)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (timeouts <= 0) {
    throw std::runtime_error("powerred off failed");
  }

  lPowerOn = false;
}

bool PyFee_Write(std::string name, py::bytes data, bool blocking = true) {
  bool r = false;
  std::string str = data;

  precheck();

  lResult = false;
  auto it = Fee_BlockMap.find(name);
  if (it != Fee_BlockMap.end()) {
    auto BlockNumber = it->second;
    Fee_BlockConfigType &config = Fee_BlockConfig[BlockNumber - 1];
    memcpy(lDataPtr, str.c_str(), config.BlockSize);
    auto ret = Fee_Write(BlockNumber, lDataPtr);
    if (E_OK == ret) {
      if (blocking) {
        PyFee_Schedule();
      }
      r = lResult;
    }
  }

  return r;
}

bool PyFee_Result(void) {
  return lResult;
}

py::object PyFee_Image(py::bytes raw) {
  std::string str = raw;
  if (str.empty()) {
    return py::bytes((char *)g_FlsAcMirror, FLS_TOTAL_SIZE);
  } else {
    int nLoops = 10;
    Std_ReturnType r = E_FLS_PENDING;

    while ((E_FLS_PENDING == r) && (nLoops > 0)) {
      r = Fls_AcWrite(0, (const uint8_t *)str.c_str(), str.size());
      nLoops--;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return py::bool_(E_OK == r);
  }
}

py::object PyFee_Read(std::string name, bool blocking = true) {
  py::object r = py::none();

  precheck();

  lResult = false;
  auto it = Fee_BlockMap.find(name);
  if (it != Fee_BlockMap.end()) {
    auto BlockNumber = it->second;
    Fee_BlockConfigType &config = Fee_BlockConfig[BlockNumber - 1];
    uint8_t *data = new uint8_t[config.BlockSize];
    auto ret = Fee_Read(BlockNumber, 0, data, config.BlockSize);
    if (E_OK == ret) {
      if (blocking) {
        PyFee_Schedule();
      }
      if (lResult) {
        r = py::bytes((char *)data, config.BlockSize);
      }
    }
    delete[] data;
  }

  return r;
}

PYBIND11_MODULE(PyFee, m) {
  m.doc() = "pybind11 PyFee library";
  m.def("erase", &PyFee_Erase, "\tErase Flash\n");
  m.def("config", &PyFee_Config,
        "\tblocks: dict, {'BlockName': 'BlockSize'}\n"
        "\tpageSize: int, the page size of flash simulated, default 8\n"
        "\tsectorSize: int, the sector size of flash simulated, default 512\n"
        "\tblockSize: int, the block size of each bach of Fee Flash bank, default 64KB\n");
  m.def("power_on", &PyFee_PowerOn, "\tpower on PyFee\n");
  m.def("power_off", &PyFee_PowerOff, "\tpower off PyFee\n");
  m.def("schedule", &PyFee_Schedule,
        "\tdo schedule until idel or do schedule nLoops if nLoops > 0\n"
        "\treturn True if Fee state idle else False\n",
        py::arg("nLoops") = 0);
  m.def("write", &PyFee_Write, "\twrite data of block <name> to fee\n", py::arg("name"),
        py::arg("data"), py::arg("blocking") = true);
  m.def("read", &PyFee_Read, "\tread data of block <name> from fee\n", py::arg("name"),
        py::arg("blocking") = true);
  m.def("result", &PyFee_Result, "get job result\n");
  m.def("img", &PyFee_Image, "read/write image raw\n", py::arg("raw") = py::bytes());
}