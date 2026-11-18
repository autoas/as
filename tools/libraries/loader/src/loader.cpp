/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "loader.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include "Std_Timer.h"
#include <mutex>
#include <thread>
#include <chrono>
#include "PAL.h"
#include "loader/common.h"

using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
#define LOADER_LOG_SIZE (1024 * 32)

#define LOADER_STS_CREATED 0
#define LOADER_STS_STARTED 1
#define LOADER_STS_EXITED 2

#ifndef LOADER_KEEP_TESTER_ONLINE
#define LOADER_KEEP_TESTER_ONLINE 2000000
#endif

#ifdef _WIN32
#define DLL ".dll"
#else
#define DLL ".so"
#endif

#ifndef LOADER_BUILT_IN_APP_MAX
#define LOADER_BUILT_IN_APP_MAX 32
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct Loader_s {
  Srec_FileType *AppSRec;
  Srec_FileType *FlsSRec;
  size_t TotalSize;
  size_t LogSize;
  Srec_SignType SignType;
  uint32_t FuncAddr; /* functional address for CAN/FD only*/
  const Loader_AppType *App;
  int Progress;  /* resolution in 0.01% */
  int ProgRatio; /* for data transfer */
  std::thread Thread;
  std::mutex LogMutex;
  std::mutex TpMutex;
  int Status;
  Std_TimerType TesterTimer;
  uint8_t Request[LOADER_MSG_SIZE];
  uint8_t Response[LOADER_MSG_SIZE];
  char Logs[LOADER_LOG_SIZE];
  IsoTp_Type *Isotp;
  boolean Stop;
  int LogLevel;
  void *Dll;
  int Result;
} Loader_Type;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const Loader_AppType *loaderApps[LOADER_BUILT_IN_APP_MAX];
static uint32_t loaderAppsNum = 0;
/* ================================ [ LOCALS    ] ============================================== */
static void Loader_KeepTesterOnline(Loader_Type *loader) {
  static const uint8_t testerKeep[2] = {0x3E, 0x00 | SUPPRESS_POS_RESP_BIT};
  uint32_t funcAddr = loader->FuncAddr;

  if (Std_GetTimerElapsedTime(&loader->TesterTimer) > LOADER_KEEP_TESTER_ONLINE) {
    if (funcAddr != 0) {
      IsoTp_IoCtl(loader->Isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
    }

    IsoTp_Transmit(loader->Isotp, testerKeep, 2, NULL, 0);

    if (funcAddr != 0) {
      IsoTp_IoCtl(loader->Isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
    }

    Std_TimerStart(&loader->TesterTimer);
    std::this_thread::sleep_for(10ms);
  }
}

static int Loader_RequestServiceImpl(Loader_Type *loader, const uint8_t *data, size_t length,
                                     const int *expected, size_t eLen, int functional) {
  int r = L_R_OK;
  int rlen = 0;
  size_t i;
  uint32_t funcAddr = loader->FuncAddr;
  static const uint8_t servicesWithSubFunc[] = {0x10, 0x28, 0x85, 0x3E};
  char ts[64];
  std::string prefix;

  LDLOG(DEBUG, "\n request service %02X:\n", data[0]);
  Std_GetDateTime(ts, sizeof(ts));
  prefix = std::string("  ") + ts + std::string(" TX:");
  LDHEX(DEBUG, prefix.c_str(), data, length);

  std::lock_guard<std::mutex> lg(loader->TpMutex);
  if ((TRUE == functional) && (funcAddr != 0)) {
    /* switch to functional addressing mode */
    IsoTp_IoCtl(loader->Isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
  }

  Loader_KeepTesterOnline(loader);

  r = IsoTp_Transmit(loader->Isotp, data, length, NULL, 0);
  if (0 != r) {
    LDLOG(ERROR, "  isotp transmit error: %d\n", r);
  } else {
    for (i = 0; i < sizeof(servicesWithSubFunc); i++) {
      if ((length >= 2) && (servicesWithSubFunc[i] == data[0]) &&
          (0 != (data[1] & SUPPRESS_POS_RESP_BIT))) {
        r = L_R_OK_FOR_NOW;
        eLen = 0; /* as no reponse, no need to check exptected */
        std::this_thread::sleep_for(50ms);
        /* sleep 50 ms to wait the request done by all the addressed ECUs. */
        break;
      }
    }
  }
  while (L_R_OK == r) {
    rlen = IsoTp_Receive(loader->Isotp, loader->Response, sizeof(loader->Response));
    if (rlen > 0) {
      Std_GetDateTime(ts, sizeof(ts));
      prefix = std::string("  ") + ts + std::string(" RX:");
      LDHEX(DEBUG, prefix.c_str(), loader->Response, (size_t)rlen);
    }
    if ((3 == rlen)) {
      if ((0x7F == loader->Response[0]) && (loader->Response[1] == data[0])) {
        if (0x78 == loader->Response[2]) {
          /* pending response */
          Loader_KeepTesterOnline(loader);
        } else {
          /* negative response */
          LDLOG(INFO, "  negative response %02X\n", loader->Response[2]);
          r = L_R_NOK;
        }
      } else if ((data[0] | 0x40) == loader->Response[0]) {
        r = L_R_OK_FOR_NOW;
      } else {
        r = L_R_NOK;
      }
    } else if (rlen > 0) {
      r = L_R_OK_FOR_NOW;
    } else {
      LDLOG(ERROR, "  isotp receive error: %d\n", rlen);
      r = L_R_ERROR;
    }
  }

  if (L_R_OK_FOR_NOW == r) {
    if ((size_t)rlen < eLen) {
      LDLOG(INFO, "  response too short\n");
      r = L_R_NOK;
    }
  }

  if (L_R_OK_FOR_NOW == r) {
    r = L_R_OK;
    for (i = 0; i < eLen; i++) {
      if ((-1 != expected[i]) && (loader->Response[i] != (uint8_t)expected[i])) {
        LDLOG(INFO, "  response not as expected at %d R %02X != E %02X\n", (int)i,
              loader->Response[i], (uint8_t)expected[i]);
        r = L_R_NOK;
      }
    }
  }

  if (L_R_OK == r) {
    loader->Progress += 1; /* step 0.01% */
    LDLOG(DEBUG, "  PASS\n");
  } else {
    LDLOG(DEBUG, "  FAIL: %d\n", r);
  }

  if ((TRUE == functional) && (funcAddr != 0)) {
    /* switch back to physical addressing mode */
    IsoTp_IoCtl(loader->Isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
  }

  return r;
}

static void Loader_Main(void *args) {
  int r = L_R_OK;
  size_t i;
  float cost = 0;
  float speed = 0;
  Loader_Type *loader = (Loader_Type *)args;
  const Loader_AppType *app = loader->App;

  loader->Status = LOADER_STS_STARTED;
  LDLOG(INFO, "loader %s started:\n", app->Name);
  LDLOG(INFO, "Total data size %llu bytes\n", loader->TotalSize);
  auto begin = std::chrono::high_resolution_clock::now();
  Std_TimerStart(&loader->TesterTimer);

  loader->ProgRatio = 9900;

  for (i = 0; i < app->NumOfServices; i++) {
    loader->ProgRatio -= app->Services[i].Progress;
  }

  for (i = 0; (L_R_OK == r) && (i < app->NumOfServices && (FALSE == loader->Stop)); i++) {
    if (NULL != app->Services[i].PreLog) {
      LDLOG(INFO, app->Services[i].PreLog);
    }

    r = app->Services[i].Handle(loader);
    if (0 == r) {
      if (NULL != app->Services[i].PostLog) {
        LDLOG(INFO, app->Services[i].PostLog);
      }
      loader->Progress += app->Services[i].Progress;
    }
  }

  if (L_R_OK == r) {
    cost = std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::high_resolution_clock::now() - begin)
             .count() /
           1000.f;
    speed = loader->TotalSize / 1024.0 / cost;
    LDLOG(INFO, "loader average speed %.2f kbps cost %.2f seconds\n", speed, cost);
  } else {
    LDLOG(INFO, "loader failed\n");
  }

  loader->Status = LOADER_STS_EXITED;
  loader->Result = r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
Loader_Type *Loader_Create(Loader_ArgsType *args) {
  int r = 0;
  Loader_Type *loader;
  loader = new Loader_Type;
  uint32_t i;
  char *path;
  Loader_GetAppFuncType get_app_func;

  if (NULL != loader) {
    loader->Isotp = args->Isotp;
    loader->AppSRec = args->AppSRec;
    loader->FlsSRec = args->FlsSRec;
    loader->TotalSize = args->AppSRec->TotalSize;
    if (loader->FlsSRec != NULL) {
      loader->TotalSize += args->FlsSRec->TotalSize;
    }
    loader->SignType = args->SignType;
    loader->FuncAddr = args->FuncAddr;
    loader->LogLevel = L_LOG_INFO;
    loader->Status = LOADER_STS_CREATED;
    loader->Stop = FALSE;
    loader->App = NULL;
    loader->Dll = NULL;

    for (i = 0; i < loaderAppsNum; i++) {
      if (0 == strcmp(args->Choice, loaderApps[i]->Name)) {
        loader->App = loaderApps[i];
        break;
      }
    }

    if (NULL == loader->App) {
      path = (char *)loader->Request;
      snprintf(path, sizeof(loader->Request), "Loader%s" DLL, args->Choice);
      loader->Dll = PAL_DlOpen(path);
      if (NULL != loader->Dll) {
        std::string interface = std::string(args->Choice) + "Get";
        get_app_func = (Loader_GetAppFuncType)PAL_DlSym(loader->Dll, interface.c_str());
        if (NULL != get_app_func) {
          loader->App = get_app_func();
        }
        if (NULL == loader->App) {
          PAL_DlClose(loader->Dll);
          printf("loader application %s is invalid\n", path);
          r = -EINVAL;
        }
      } else {
        printf("loader application %s doesn't exist\n", path);
        r = -EEXIST;
      }
    }

    if (NULL == loader->App) {
      printf("invalid app %s, choose from:", args->Choice);
      for (i = 0; i < loaderAppsNum; i++) {
        printf(" %s", loaderApps[i]->Name);
      }
      printf("\n");
      r = -EEXIST;
    }

    if (0 == r) {
      loader->Thread = std::thread(Loader_Main, (void *)loader);
    }

    if (0 != r) {
      delete loader;
      loader = NULL;
      printf("loader start failed with error = %d\n", r);
    }
  }

  return loader;
}

void Loader_SetLogLevel(Loader_Type *loader, int level) {
  loader->LogLevel = level;
}

void Loader_Log(Loader_Type *loader, int level, const char *fmt, ...) {
  va_list args;
  if ((level) >= loader->LogLevel) {
    std::lock_guard<std::mutex> lg(loader->LogMutex);
    va_start(args, fmt);
    loader->LogSize +=
      vsnprintf(&loader->Logs[loader->LogSize], sizeof(loader->Logs) - loader->LogSize, fmt, args);
    va_end(args);
  }
}

void Loader_LogHex(Loader_Type *loader, int level, const char *prefix, const uint8_t *data,
                   size_t len) {
  size_t i;
  if (level >= loader->LogLevel) {
    std::lock_guard<std::mutex> lg(loader->LogMutex);
    loader->LogSize +=
      snprintf(&loader->Logs[loader->LogSize], sizeof(loader->Logs) - loader->LogSize, prefix);
    for (i = 0; (i < len) && (i < (size_t)16); i++) {
      loader->LogSize += snprintf(&loader->Logs[loader->LogSize],
                                  sizeof(loader->Logs) - loader->LogSize, " %02X", data[i]);
    }
    loader->LogSize +=
      snprintf(&loader->Logs[loader->LogSize], sizeof(loader->Logs) - loader->LogSize, "\n");
  }
}

int Loader_Poll(Loader_Type *loader, int *progress, char **msg) {
  int r = 0;

  *msg = NULL;
  *progress = loader->Progress;
  if (*progress >= 10000) {
    *progress = 9900;
  }

  if (LOADER_STS_EXITED == loader->Status) {
    r = loader->Result;
    if (0 == r) {
      *progress = 10000;
    }
  }

  std::lock_guard<std::mutex> lg(loader->LogMutex);
  if (loader->LogSize > 0) {
    *msg = strdup(loader->Logs);
    loader->LogSize = 0;
  }

  return r;
}

void Loader_Destroy(Loader_Type *loader) {
  loader->Stop = TRUE;
  if (loader->Thread.joinable()) {
    loader->Thread.join();
  }
  if (nullptr != loader->Dll) {
    PAL_DlClose(loader->Dll);
  }

  delete loader;
}

int Loader_RequestService(Loader_Type *loader, const uint8_t *data, size_t length,
                          const int *expected, size_t eLen) {
  return Loader_RequestServiceImpl(loader, data, length, expected, eLen, FALSE);
}

int Loader_BroadcastService(Loader_Type *loader, const uint8_t *data, size_t length,
                            const int *expected, size_t eLen) {
  return Loader_RequestServiceImpl(loader, data, length, expected, eLen, TRUE);
}

uint8_t *Loader_GetRequest(Loader_Type *loader) {
  return loader->Request;
}

uint8_t *Loader_GetResponse(Loader_Type *loader) {
  return loader->Response;
}

Loader_CrcType Loader_CrcInit(Loader_Type *loader) {
  Loader_CrcType crc = 0;
  switch (loader->SignType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC16_V2:
    crc = 0xFFFF;
    break;
  case SREC_SIGN_CRC32:
  case SREC_SIGN_CRC32_V2:
    crc = 0xFFFFFFFF;
    break;
  default:
    break;
  }
  return crc;
}

Loader_CrcType Loader_CalculateCrc(Loader_Type *loader, const uint8_t *DataPtr, uint32_t Length,
                                   Loader_CrcType StartValue, boolean IsFirstCall) {
  Loader_CrcType crc;
  switch (loader->SignType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC16_V2:
    crc = Crc_CalculateCRC16(DataPtr, Length, (uint16_t)StartValue, IsFirstCall);
    break;
  case SREC_SIGN_CRC32:
  case SREC_SIGN_CRC32_V2:
    crc = Crc_CalculateCRC32(DataPtr, Length, (uint32_t)StartValue, IsFirstCall);
    break;
  default:
    break;
  }
  return crc;
}

uint32_t Loader_SetCrc(Loader_Type *loader, uint8_t *DataPtr, Loader_CrcType Crc) {
  uint32_t len = 0;
  switch (loader->SignType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC16_V2:
    DataPtr[0] = (Crc >> 8) & 0xFF;
    DataPtr[1] = Crc & 0xFF;
    len = 2;
    break;
  case SREC_SIGN_CRC32:
  case SREC_SIGN_CRC32_V2:
    DataPtr[0] = (Crc >> 24) & 0xFF;
    DataPtr[1] = (Crc >> 16) & 0xFF;
    DataPtr[2] = (Crc >> 8) & 0xFF;
    DataPtr[3] = Crc & 0xFF;
    len = 4;
    break;
  default:
    break;
  }
  return len;
}

boolean Loader_IsStopped(Loader_Type *loader) {
  return loader->Stop;
}

void Loader_AddProgress(Loader_Type *loader, uint32_t doSize) {
  loader->Progress += (uint32_t)(((uint64_t)doSize * loader->ProgRatio) / loader->TotalSize);
}

Srec_FileType *Loader_GetAppSrec(Loader_Type *loader) {
  return loader->AppSRec;
}

Srec_FileType *Loader_GetFlsDrvSrec(Loader_Type *loader) {
  return loader->FlsSRec;
}

void Loader_RegisterApp(const Loader_AppType *app) {
  if (loaderAppsNum < ARRAY_SIZE(loaderApps)) {
    loaderApps[loaderAppsNum] = app;
    loaderAppsNum++;
  }
}
