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

#define LDLOG(level, fmt, ...) loader_log(loader, L_LOG_##level, fmt, ##__VA_ARGS__)
#define LDHEX(level, prefix, data, len) loader_log_hex(loader, L_LOG_##level, prefix, data, len)

#ifdef LOADER_USE_APP_BUILT_IN
#define LOADER_APP_REGISTER(app)                                                                   \
  static void __attribute__((constructor)) __loader_register_##app(void) {                         \
    loader_register_app(&app);                                                                     \
  }
#else
#define LOADER_APP_REGISTER(app)                                                                   \
  const loader_app_t *get(void) {                                                                  \
    return &app;                                                                                   \
  }
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct loader_s loader_t;

typedef struct {
  isotp_t *isotp;
  srec_t *appSRec;
  srec_t *flsSRec;
  const char *choice;
  srec_sign_type_t signType;
  uint32_t funcAddr;
} loader_args_t;

typedef struct {
  char *preLog;
  char *postLog;
  int (*handle)(loader_t *loader);
} loader_service_t;

typedef struct {
  const char *name;
  const loader_service_t *services;
  size_t numOfServices;
} loader_app_t;

typedef loader_app_t *(*loader_get_app_func_t)(void);

typedef uint32_t loader_crc_t;
/* ================================ [ DECLARES  ] ============================================== */
/* common APIs for loader application */
loader_crc_t loader_crc_init(loader_t *loader);
loader_crc_t loader_calulate_crc(loader_t *loader, const uint8_t *DataPtr, uint32_t Length,
                                 loader_crc_t StartValue, boolean IsFirstCall);
uint32_t loader_set_crc(loader_t *loader, uint8_t *DataPtr, loader_crc_t Crc);

int uds_request_service(loader_t *loader, const uint8_t *data, size_t length, const int *expected,
                        size_t eLen);
int uds_broadcast_service(loader_t *loader, const uint8_t *data, size_t length, const int *expected,
                          size_t eLen);
uint8_t *loader_get_request(loader_t *loader);
uint8_t *loader_get_response(loader_t *loader);
void loader_log(loader_t *loader, int level, const char *fmt, ...);
void loader_log_hex(loader_t *loader, int level, const char *prefix, const uint8_t *data,
                    size_t size);

boolean loader_is_stopt(loader_t *loader);
void loader_add_progress(loader_t *loader, int progress, uint32_t doSize);

srec_t *loader_get_app_srec(loader_t *loader);
srec_t *loader_get_flsdrv_srec(loader_t *loader);

#ifdef LOADER_USE_APP_BUILT_IN
void loader_register_app(const loader_app_t *app);
#endif
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
loader_t *loader_create(loader_args_t *args);
void loader_set_log_level(loader_t *loader, int level);
int loader_poll(loader_t *loader, int *progress, char **msg);
void loader_destory(loader_t *loader);
#ifdef __cplusplus
}
#endif
#endif /* LOADER_H */