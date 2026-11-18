/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <unistd.h>
#include "odx.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
  const char *path = argv[1];
  Odx_Type *odx;

  odx = Odx_Open(path);
  if (odx != NULL) {
    for (size_t i = 0; i < odx->NumOfMems; i++) {
      Odx_MemType *mem = odx->Mems[i];
      printf("mem[%lld]: %s\n", i, mem->Name);
      for (size_t j = 0; j < mem->NumOfBlocks; j++) {
        Odx_BlockType *block = mem->Blocks[j];
        printf("  block[%lld]: %s size=%llu data = ", j, block->Name, block->Size);
        for (size_t k = 0; (k < 8) && (k < block->Size); k++) {
          printf("%02X", block->Data[k]);
        }
        printf("\n");
      }
    }
    Odx_Close(odx);
  }

  return 0;
}
