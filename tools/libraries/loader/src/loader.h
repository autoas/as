/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
#ifndef LOADER_H
#define LOADER_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "srec.h"
#include "isotp.h"
#include "stdlib.h"
#include "Crc.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define L_LOG_DEBUG 0
#define L_LOG_INFO 1
#define L_LOG_WARNING 2
#define L_LOG_ERROR 3

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#define L_R_OK 0
#define L_R_NOK 1
#define L_R_ERROR 2 /* timeout or LVDS bus error */

#define L_R_OK_FOR_NOW 3

#define LOADER_MSG_SIZE 4096

#define LDLOG(level, fmt, ...) Loader_Log(loader, L_LOG_##level, fmt, ##__VA_ARGS__)
#define LDHEX(level, prefix, data, len) Loader_LogHex(loader, L_LOG_##level, prefix, data, len)

#define LOADER_APP_REGISTER(services, name)                                                        \
  static const Loader_AppType s_LoaderApp##name = {                                                \
    #name,                                                                                         \
    services,                                                                                      \
    ARRAY_SIZE(services),                                                                          \
  };                                                                                               \
  INITIALIZER(__loader_register_##name) {                                                          \
    Loader_RegisterApp(&s_LoaderApp##name);                                                        \
  }                                                                                                \
  const Loader_AppType *name##Get(void) {                                                          \
    return &s_LoaderApp##name;                                                                     \
  }

#define SUPPRESS_POS_RESP_BIT (uint8_t)0x80
/* ================================ [ TYPES     ] ============================================== */
typedef struct Loader_s Loader_Type;

typedef struct {
  IsoTp_Type *Isotp;
  Srec_FileType *AppSRec;
  Srec_FileType *FlsSRec;
  const char *Choice;
  Srec_SignType SignType;
  uint32_t FuncAddr;
} Loader_ArgsType;

typedef struct {
  char *PreLog;
  char *PostLog;
  int (*Handle)(Loader_Type *loader);
  int Progress;
} Loader_ServiceType;

typedef struct {
  const char *Name;
  const Loader_ServiceType *Services;
  size_t NumOfServices;
} Loader_AppType;

typedef Loader_AppType *(*Loader_GetAppFuncType)(void);

typedef uint32_t Loader_CrcType;
/* ================================ [ DECLARES  ] ============================================== */
/* common APIs for loader application */
Loader_CrcType Loader_CrcInit(Loader_Type *loader);
Loader_CrcType Loader_CalculateCrc(Loader_Type *loader, const uint8_t *DataPtr, uint32_t Length,
                                   Loader_CrcType StartValue, boolean IsFirstCall);
uint32_t Loader_SetCrc(Loader_Type *loader, uint8_t *DataPtr, Loader_CrcType Crc);

int Loader_RequestService(Loader_Type *loader, const uint8_t *data, size_t length,
                          const int *expected, size_t eLen);
int Loader_BroadcastService(Loader_Type *loader, const uint8_t *data, size_t length,
                            const int *expected, size_t eLen);
uint8_t *Loader_GetRequest(Loader_Type *loader);
uint8_t *Loader_GetResponse(Loader_Type *loader);
void Loader_Log(Loader_Type *loader, int level, const char *fmt, ...);
void Loader_LogHex(Loader_Type *loader, int level, const char *prefix, const uint8_t *data,
                   size_t size);

boolean Loader_IsStopped(Loader_Type *loader);
void Loader_AddProgress(Loader_Type *loader, uint32_t doSize);

Srec_FileType *Loader_GetAppSrec(Loader_Type *loader);
Srec_FileType *Loader_GetFlsDrvSrec(Loader_Type *loader);

void Loader_RegisterApp(const Loader_AppType *app);
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Loader_Type *Loader_Create(Loader_ArgsType *args);
void Loader_SetLogLevel(Loader_Type *loader, int level);
int Loader_Poll(Loader_Type *loader, int *progress, char **msg);
void Loader_Destroy(Loader_Type *loader);
#ifdef __cplusplus
}
#endif
#endif /* LOADER_H */
