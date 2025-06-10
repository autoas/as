/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef _ODX_H_
#define _ODX_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  char *name;
  uint8_t *data;
  size_t size;
} odx_block_t;

typedef struct {
  char *name;
  odx_block_t **blocks;
  size_t numOfBlocks;
} odx_mem_t;

typedef struct {
  odx_mem_t **mems;
  size_t numOfMems;
} odx_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
odx_t *odx_open(const char *path);
void odx_close(odx_t *odx);
#ifdef __cplusplus
}
#endif
#endif /* _ODX_H_ */
