/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef ODX_H
#define ODX_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  char *Name;
  uint8_t *Data;
  size_t Size;
} Odx_BlockType;

typedef struct {
  char *Name;
  Odx_BlockType **Blocks;
  size_t NumOfBlocks;
} Odx_MemType;

typedef struct {
  Odx_MemType **Mems;
  size_t NumOfMems;
} Odx_Type;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Odx_Type *Odx_Open(const char *path);
void Odx_Close(Odx_Type *odx);
#ifdef __cplusplus
}
#endif
#endif /* ODX_H */
