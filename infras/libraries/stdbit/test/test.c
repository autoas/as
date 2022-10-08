/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Bit.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
/* ================================ [ MACROS    ] ============================================== */
#ifndef CAN_PDU_SIZE
#define CAN_PDU_SIZE 8
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static FILE *flog = NULL;
static uint8_t u8G[CAN_PDU_SIZE];
static uint8_t u8T[CAN_PDU_SIZE];
/* ================================ [ LOCALS    ] ============================================== */
static void logD(const char *prefix, const uint8_t *data) {
  int i;
  fprintf(flog, "  %s:", prefix);
  for (i = 0; i < CAN_PDU_SIZE; i++) {
    fprintf(flog, " %02X", data[i]);
  }
  fprintf(flog, "\n");
}

static void TestB_Write(int loop, uint32_t u32V, uint16_t bitPos, uint8_t bitSize) {
  int i, bPass;
  uint32_t u32R;
  printf("TestB %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:", loop, u32V, bitPos, bitSize);
  fprintf(flog, "TestB %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:\n", loop, u32V, bitPos, bitSize);
  Std_BitSetBEG(u8G, u32V, bitPos, bitSize);
  Std_BitSetBigEndian(u8T, u32V, bitPos, bitSize);
  u32R = Std_BitGetBigEndian(u8T, bitPos, bitSize);
  logD("testB G:", u8G);
  logD("testB T:", u8T);

  bPass = true;
  for (i = 0; i < sizeof(u8G); i++) {
    if (u8G[i] != u8T[i]) {
      bPass = false;
    }
  }
  if (u32R != u32V) {
    printf("\n read FAIL, read=0x%X\n", u32R);
    fprintf(flog, "\n read FAIL, read=0x%X\n", u32R);
    bPass = false;
  }
  printf(" %s\n", bPass ? "PASS" : "FAIL");
  fprintf(flog, " %s\n", bPass ? "PASS" : "FAIL");
  if (false == bPass) {
    fclose(flog);
    exit(-1);
  }
}

static void TestL_Write(int loop, uint32_t u32V, uint16_t bitPos, uint8_t bitSize) {
  int i, bPass;
  uint32_t u32R;
  printf("TestL %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:", loop, u32V, bitPos, bitSize);
  fprintf(flog, "TestL %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:\n", loop, u32V, bitPos, bitSize);
  Std_BitSetLEG(u8G, u32V, bitPos, bitSize);
  Std_BitSetLittleEndian(u8T, u32V, bitPos, bitSize);
  u32R = Std_BitGetLittleEndian(u8T, bitPos, bitSize);
  logD("testL G:", u8G);
  logD("testL T:", u8T);

  bPass = true;
  for (i = 0; i < sizeof(u8G); i++) {
    if (u8G[i] != u8T[i]) {
      bPass = false;
    }
  }
  if (u32R != u32V) {
    printf("\n read FAIL, read=0x%X\n", u32R);
    fprintf(flog, "\n read FAIL, read=0x%X\n", u32R);
    bPass = false;
  }
  printf(" %s\n", bPass ? "PASS" : "FAIL");
  fprintf(flog, " %s\n", bPass ? "PASS" : "FAIL");
  if (false == bPass) {
    fclose(flog);
    exit(-1);
  }
}

static void Test_WriteAll(void) {
  int i = 0;
  uint16_t bitPos;
  uint8_t bitSize;
  uint32_t mask, u32V;
  for (i = 0; i < 10000; i++) {
    bitPos = (uint16_t)rand() % ((CAN_PDU_SIZE - 4) * 8);
    bitSize = (uint8_t)rand() % 33;
    if (0 == bitSize) {
      bitSize = 7;
    }
    mask = 0xFFFFFFFFu >> (32 - bitSize);
    u32V = (uint32_t)rand() & mask;
    TestB_Write(i, u32V, bitPos, bitSize);
    TestL_Write(i, u32V, bitPos, bitSize);
  }
}

/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {

  flog = fopen(".testbit.log", "w");
  memset(u8G, 0, sizeof(u8G));
  memset(u8T, 0, sizeof(u8T));
  Test_WriteAll();
  fclose(flog);
  return 0;
}