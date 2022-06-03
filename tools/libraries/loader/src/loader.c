/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "loader.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "Std_Types.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define LOADER_LOG_SIZE (1024 * 32)
#define LOADER_MSG_SIZE 4096

#define FLASH_SECTOR_SIZE 512
#define FLASH_WRITE_SIZE 1

#define FL_ALIGN(length, sz) (((length + sz - 1) / sz) * sz)

#define FL_MIN_ABILITY 128

#define LOADER_STS_CREATED 0
#define LOADER_STS_STARTED 1
#define LOADER_STS_EXITED 2

#define L_LOG_DEBUG 0
#define L_LOG_INFO 1
#define L_LOG_WARNING 2
#define L_LOG_ERROR 3

#define L_LOG_LEVEL L_LOG_INFO

#ifdef USE_STD_PRINTF
#define PRINTF std_printf
#else
#define PRINTF printf
#endif

#define LDLOG(level, fmt, ...)                                                                     \
  do {                                                                                             \
    fprintf(loader->flog, fmt, ##__VA_ARGS__);                                                     \
    PRINTF(fmt, ##__VA_ARGS__);                                                                    \
    if ((L_LOG_##level) >= L_LOG_LEVEL) {                                                          \
      pthread_mutex_lock(&loader->mutex);                                                          \
      loader->lsz += snprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz, fmt, \
                              ##__VA_ARGS__);                                                      \
      pthread_mutex_unlock(&loader->mutex);                                                        \
    }                                                                                              \
  } while (0)

#define FDLOG(level, prefix, data, len)                                                            \
  do {                                                                                             \
    int i;                                                                                         \
    fprintf(loader->flog, "%s len=%d", prefix, (int)len);                                          \
    PRINTF("%s len=%d", prefix, (int)len);                                                         \
    for (i = 0; (i < len) && (i < 32); i++) {                                                      \
      fprintf(loader->flog, " %02X", data[i]);                                                     \
      PRINTF(" %02X", data[i]);                                                                    \
    }                                                                                              \
    fprintf(loader->flog, "\n");                                                                   \
    PRINTF("\n");                                                                                  \
    if ((L_LOG_##level) >= L_LOG_LEVEL) {                                                          \
      pthread_mutex_lock(&loader->mutex);                                                          \
      loader->lsz +=                                                                               \
        snprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz, prefix);          \
      for (i = 0; (i < len) && (i < 32); i++) {                                                    \
        loader->lsz += snprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz,    \
                                " %02X", data[i]);                                                 \
      }                                                                                            \
      loader->lsz +=                                                                               \
        snprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz, "\n");            \
      pthread_mutex_unlock(&loader->mutex);                                                        \
    }                                                                                              \
  } while (0)

#define L_R_OK 0
#define L_R_NOK 1
#define L_R_ERROR 2 /* timeout or LVDS bus error */

#define L_R_OK_FOR_NOW 3
/* ================================ [ TYPES     ] ==============================================
 */
typedef struct loader_s {
  srec_t *appSRec;
  srec_t *flsSRec;
  size_t totalSize;
  size_t lsz;   /* log size */
  int progress; /* resolution in 0.01% */
  pthread_t pthread;
  pthread_mutex_t mutex;
  int status;
  uint16_t crc;
  uint8_t request[LOADER_MSG_SIZE];
  uint8_t response[LOADER_MSG_SIZE];
  char logs[LOADER_LOG_SIZE];
  FILE *flog;
  isotp_t *isotp;
} loader_t;

typedef struct {
  char *preLog;
  char *postLog;
  int (*handle)(loader_t *loader);
} step_t;
/* ================================ [ DECLARES  ] ============================================== */
static int lLoaderId = 0;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static int uds_request_service(loader_t *loader, const uint8_t *data, size_t length,
                               const int *expected, size_t eLen) {
  int r = L_R_OK;
  int rlen = 0;
  int i;

  LDLOG(DEBUG, "\n request service %02X:\n", data[0]);
  FDLOG(DEBUG, "  TX:", data, length);

  r = isotp_transmit(loader->isotp, data, length, NULL, 0);
  usleep(50000); /* delay 50ms to give the server sometime to handle the request */

  while (L_R_OK == r) {
    rlen = isotp_receive(loader->isotp, loader->response, sizeof(loader->response));
    if (rlen > 0) {
      FDLOG(DEBUG, "  RX:", loader->response, rlen);
    }
    if ((3 == rlen)) {
      if ((0x7F == loader->response[0]) && (loader->response[1] == data[0])) {
        if (0x78 == loader->response[2]) {
          /* pending response */
        } else {
          /* negative response */
          LDLOG(DEBUG, "  negative response %02X\n", loader->response[2]);
          r = L_R_NOK;
        }
      } else if ((data[0] | 0x40) == loader->response[0]) {
        r = L_R_OK_FOR_NOW;
      } else {
        r = L_R_NOK;
      }
    } else if (rlen > 0) {
      r = L_R_OK_FOR_NOW;
    } else {
      r = L_R_ERROR;
    }
  }

  if (L_R_OK_FOR_NOW == r) {
    r = L_R_OK;
    for (i = 0; i < eLen; i++) {
      if ((-1 != expected[i]) && (loader->response[i] != (uint8_t)expected[i])) {
        r = L_R_NOK;
      }
    }
  }

  if (L_R_OK == r) {
    loader->progress += 10; /* step 0.1% */
    LDLOG(DEBUG, "  PASS\n");
  } else {
    LDLOG(DEBUG, "  FAIL\n");
  }

  return r;
}

static int enter_extend_session(loader_t *loader) {
  static const uint8_t data[] = {0x10, 0x03};
  static const int expected[] = {0x50, 0x03, -1, -1, -1, -1};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static int security_extds_access(loader_t *loader) {
  static const uint8_t data[] = {0x27, 0x01};
  static const int expected1[] = {0x67, 0x01, -1, -1, -1, -1};
  static const int expected2[] = {0x67, 0x02};
  uint32_t seed, key;
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected1, ARRAY_SIZE(expected1));
  if (L_R_OK == r) {
    seed = ((uint32_t)loader->response[2] << 24) + ((uint32_t)loader->response[3] << 16) +
           ((uint32_t)loader->response[4] << 8) + loader->response[5];
    /* TODO: TBD */
    key = seed ^ 0x78934673;
    loader->request[0] = 0x27;
    loader->request[1] = 0x02;
    loader->request[2] = (key >> 24) & 0xFF;
    loader->request[3] = (key >> 16) & 0xFF;
    loader->request[4] = (key >> 8) & 0xFF;
    loader->request[5] = key & 0xFF;
    r = uds_request_service(loader, loader->request, 6, expected2, ARRAY_SIZE(expected2));
  }

  return r;
}

static int enter_program_session(loader_t *loader) {
  static const uint8_t data[] = {0x10, 0x02};
  static const int expected[] = {0x50, 0x02, -1, -1, -1, -1};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static int security_prgs_access(loader_t *loader) {
  static const uint8_t data[] = {0x27, 0x03};
  static const int expected1[] = {0x67, 0x03, -1, -1, -1, -1};
  static const int expected2[] = {0x67, 0x04};
  uint32_t seed, key;
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected1, ARRAY_SIZE(expected1));
  if (L_R_OK == r) {
    seed = ((uint32_t)loader->response[2] << 24) + ((uint32_t)loader->response[3] << 16) +
           ((uint32_t)loader->response[4] << 8) + loader->response[5];
    /* TODO: TBD */
    key = seed ^ 0x94586792;
    loader->request[0] = 0x27;
    loader->request[1] = 0x04;
    loader->request[2] = (key >> 24) & 0xFF;
    loader->request[3] = (key >> 16) & 0xFF;
    loader->request[4] = (key >> 8) & 0xFF;
    loader->request[5] = key & 0xFF;
    r = uds_request_service(loader, loader->request, 6, expected2, ARRAY_SIZE(expected2));
  }

  return r;
}

static int routine_erase_flash(loader_t *loader) {
  static const int expected[] = {0x71, 0x01, 0xFF, 0x01};
  int r;

  loader->request[0] = 0x31;
  loader->request[1] = 0x01;
  loader->request[2] = 0xFF;
  loader->request[3] = 0x01;

  r = uds_request_service(loader, loader->request, 4, expected, ARRAY_SIZE(expected));

  if (L_R_OK == r) {
    loader->progress += 500; /* step 5% */
  }

  return r;
}

static int request_download(loader_t *loader, uint32_t address, size_t length) {
  int r;
  static const int expected[] = {0x74, 0x20};
  loader->request[0] = 0x34;
  loader->request[1] = 0x00;
  loader->request[2] = 0x44;

  loader->request[3] = (address >> 24) & 0xFF;
  loader->request[4] = (address >> 16) & 0xFF;
  loader->request[5] = (address >> 8) & 0xFF;
  loader->request[6] = address & 0xFF;

  loader->request[7] = (length >> 24) & 0xFF;
  loader->request[8] = (length >> 16) & 0xFF;
  loader->request[9] = (length >> 8) & 0xFF;
  loader->request[10] = length & 0xFF;

  r = uds_request_service(loader, loader->request, 11, expected, ARRAY_SIZE(expected));

  return r;
}

static int transfer_data(loader_t *loader, uint32_t ability, uint8_t *data, size_t length) {
  int r = L_R_OK;
  uint8_t blockSequenceCounter = 1;
  size_t leftSize = length;
  size_t doSz;
  size_t curPos = 0;
  size_t i;
  int expected[] = {0x76, 0};

  while ((leftSize > 0) && (L_R_OK == r)) {
    loader->request[0] = 0x36;
    loader->request[1] = blockSequenceCounter;
    expected[1] = blockSequenceCounter;

    if (leftSize > ability) {
      doSz = ability;
      leftSize = leftSize - ability;
    } else {
      doSz = FL_ALIGN(leftSize, FLASH_WRITE_SIZE);
      leftSize = 0;
    }

    for (i = 0; i < doSz; i++) {
      if ((curPos + i) < length) {
        loader->request[2 + i] = data[curPos + i];
      } else {
        loader->request[2 + i] = 0xFF;
      }
    }

    r = uds_request_service(loader, loader->request, 2 + doSz, expected, ARRAY_SIZE(expected));

    if (L_R_OK == r) {
      loader->progress += (doSz * 8000) / loader->totalSize;
    }

    blockSequenceCounter = (blockSequenceCounter + 1) & 0xFF;
    curPos += doSz;
  }

  return r;
}

static int transfer_exit(loader_t *loader) {
  static const uint8_t data[] = {0x37};
  static const int expected[] = {0x77};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static int download_one_section(loader_t *loader, sblk_t *blk) {
  int r;
  uint32_t ability = sizeof(loader->request);

  r = request_download(loader, blk->address, blk->length);

  if (L_R_OK == r) {
    ability = ((uint32_t)loader->response[2] << 8) + loader->response[3];
    if ((ability >= FL_MIN_ABILITY) && (ability < sizeof(loader->request))) {
      r = transfer_data(loader, ability - 2, blk->data, blk->length);
    } else {
      LDLOG(DEBUG, "server ability error %d", ability);
      r = L_R_NOK;
    }
  }

  if (L_R_OK == r) {
    r = transfer_exit(loader);
  }

  return r;
}

static int download_application(loader_t *loader) {
  int r = L_R_OK;
  int i;

  for (i = 0; (L_R_OK == r) && (i < loader->appSRec->numOfBlks); i++) {
    r = download_one_section(loader, &loader->appSRec->blks[i]);
  }

  return r;
}

static int download_flash_driver(loader_t *loader) {
  int r = L_R_OK;
  int i;

  if (NULL != loader->flsSRec) {
    LDLOG(INFO, "download flash driver");
    for (i = 0; (L_R_OK == r) && (i < loader->flsSRec->numOfBlks); i++) {
      r = download_one_section(loader, &loader->flsSRec->blks[i]);
    }
    if (L_R_OK == r) {
      LDLOG(INFO, " okay\n");
    }
  }

  return r;
}

static int routine_check_integrity(loader_t *loader) {
  static const int expected[] = {0x71, 0x01, 0xFF, 0x02};
  int r;

  loader->request[0] = 0x31;
  loader->request[1] = 0x01;
  loader->request[2] = 0xFF;
  loader->request[3] = 0x02;

  r = uds_request_service(loader, loader->request, 4, expected, ARRAY_SIZE(expected));

  if (L_R_OK == r) {
    loader->progress += 500; /* step 5% */
  }

  return r;
}

static int ecu_reset(loader_t *loader) {
  static const uint8_t data[] = {0x11, 0x03};
  static const int expected[] = {0x51, 0x03};
  int r;

  r = uds_request_service(loader, data, sizeof(data), expected, ARRAY_SIZE(expected));

  return r;
}

static const step_t lSteps[] = {
  {"enter extended session", " okay\n", enter_extend_session},
  {"level 1 security access", " okay\n", security_extds_access},
  {"enter program session", " okay\n", enter_program_session},
  {"level 2 security access", " okay\n", security_prgs_access},
  {NULL, NULL, download_flash_driver},
  {"erase flash", " okay\n", routine_erase_flash},
  {"download application", " okay\n", download_application},
  {"check integrity", " okay\n", routine_check_integrity},
  {"ecu reset", " okay\n", ecu_reset},
};

static void *loader_main(void *args) {
  int r = L_R_OK;
  int i;
  loader_t *loader = (loader_t *)args;

  loader->status = LOADER_STS_STARTED;
  LDLOG(INFO, "loader started:\n");

  for (i = 0; (L_R_OK == r) && (i < ARRAY_SIZE(lSteps)); i++) {
    if (NULL != lSteps[i].preLog) {
      LDLOG(INFO, lSteps[i].preLog);
    }
    r = lSteps[i].handle(loader);
    if (0 == r) {
      if (NULL != lSteps[i].postLog) {
        LDLOG(INFO, lSteps[i].postLog);
      }
    }
  }

  loader->status = LOADER_STS_EXITED;

  if (L_R_OK == r) {
    LDLOG(INFO, "loader exited without error\n");
  } else {
    LDLOG(INFO, "loader failed\n");
  }
  return (void *)(unsigned long long)r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
loader_t *loader_create(isotp_t *isotp, srec_t *appSRec, srec_t *flsSRec) {
  int r = 0;
  loader_t *loader;
  char flogPath[] = "loader?.log";

  loader = malloc(sizeof(loader_t));

  if (NULL != loader) {
    memset(loader, 0, sizeof(loader_t));
    loader->isotp = isotp;
    loader->appSRec = appSRec;
    loader->flsSRec = flsSRec;
    loader->totalSize = appSRec->totalSize;
    if (loader->flsSRec != NULL) {
      loader->totalSize += flsSRec->totalSize;
    }
    loader->status = LOADER_STS_CREATED;
    flogPath[6] = '0' + (lLoaderId++ % 10);
    loader->flog = fopen(flogPath, "w");
    if (NULL == loader->flog) {
      r = -1;
    }

    if (0 == r) {
      r = pthread_mutex_init(&loader->mutex, NULL);
      r |= pthread_create(&loader->pthread, NULL, loader_main, loader);
    }

    if (0 != r) {
      if (NULL != loader->flog) {
        fclose(loader->flog);
      }
      free(loader);
      loader = NULL;
      printf("loader start failed with error = %d\n", r);
    }
  }

  return loader;
}

int loader_poll(loader_t *loader, int *progress, char **msg) {
  int r = 0;
  void *pResult;

  *msg = NULL;
  *progress = loader->progress;
  if (*progress > 10000) {
    *progress = 9900;
  }

  if (LOADER_STS_EXITED == loader->status) {
    r = pthread_join(loader->pthread, &pResult);
    if (0 == r) {
      r = (int)(unsigned long long)pResult;
      if (0 == r) {
        *progress = 10000;
      }
    }
  }

  if (loader->lsz > 0) {
    pthread_mutex_lock(&loader->mutex);
    *msg = strdup(loader->logs);
    loader->lsz = 0;
    pthread_mutex_unlock(&loader->mutex);
  }

  return r;
}

void loader_destory(loader_t *loader) {
  fclose(loader->flog);
  free(loader);
}
