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
  odx_t *odx;

  odx = odx_open(path);
  if (odx != NULL) {
    for (size_t i = 0; i < odx->numOfMems; i++) {
      odx_mem_t *mem = odx->mems[i];
      printf("mem[%lld]: %s\n", i, mem->name);
      for (size_t j = 0; j < mem->numOfBlocks; j++) {
        odx_block_t *block = mem->blocks[j];
        printf("  block[%lld]: %s size=%llu data = ", j, block->name, block->size);
        for (size_t k = 0; (k < 8) && (k < block->size); k++) {
          printf("%02X", block->data[k]);
        }
        printf("\n");
      }
    }
    odx_close(odx);
  }

  return 0;
}
