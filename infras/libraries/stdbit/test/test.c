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
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 64
#endif

// #define STDBIT_DEBUG
#ifdef STDBIT_DEBUG
#define LOG_D(prefix, data) logD(prefix, data)
#define LOG_B(prefix, data) logB(prefix, data)
#define FPRINT(...) fprintf(flog, __VA_ARGS__)
#else
#define LOG_D(prefix, data)
#define LOG_B(prefix, data)
#define FPRINT(...)
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#ifdef STDBIT_DEBUG
static FILE *flog = NULL;
#endif
static uint8_t u8G[BUFFER_SIZE];
static uint8_t u8T[BUFFER_SIZE];

static uint8_t u8BitSrc[BUFFER_SIZE];
static uint8_t u8BitDstT[BUFFER_SIZE];
static uint8_t u8BitDstG[BUFFER_SIZE];
/* ================================ [ LOCALS    ] ============================================== */
#ifdef STDBIT_DEBUG
static void logD(const char *prefix, const uint8_t *data) {
  int i;
  FPRINT("  %s:", prefix);
  for (i = 0; i < BUFFER_SIZE; i++) {
    FPRINT(" %02X", data[i]);
  }
  FPRINT("\n");
}

static void logB(const char *prefix, const uint8_t *data) {
  int i, j;
  FPRINT("  %s:", prefix);
  for (i = 0; i < BUFFER_SIZE; i++) {
    for (j = 7; j >= 0; j--) {
      if (0 != (data[i] & (1u << j))) {
        FPRINT("1");
      } else {
        FPRINT("0");
      }
    }
  }
  FPRINT("\n");
}
#endif

static void TestB_Write(int loop, uint32_t u32V, uint16_t bitPos, uint8_t bitSize) {
  int i, bPass;
  uint32_t u32R;
  printf("TestB %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:", loop, u32V, bitPos, bitSize);
  FPRINT("TestB %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:\n", loop, u32V, bitPos, bitSize);
  Std_BitSetBEG(u8G, u32V, bitPos, bitSize);
  Std_BitSetBigEndian(u8T, u32V, bitPos, bitSize);
  u32R = Std_BitGetBigEndian(u8T, bitPos, bitSize);
  LOG_D("testB G:", u8G);
  LOG_D("testB T:", u8T);

  bPass = true;
  for (i = 0; i < BUFFER_SIZE; i++) {
    if (u8G[i] != u8T[i]) {
      bPass = false;
    }
  }
  if (u32R != u32V) {
    printf("\n read FAIL, read=0x%X\n", u32R);
    FPRINT("\n read FAIL, read=0x%X\n", u32R);
    bPass = false;
  }
  printf(" %s\n", bPass ? "PASS" : "FAIL");
  FPRINT(" %s\n", bPass ? "PASS" : "FAIL");
  if (false == bPass) {
#ifdef STDBIT_DEBUG
    fclose(flog);
#endif
    exit(-1);
  }
}

static void TestL_Write(int loop, uint32_t u32V, uint16_t bitPos, uint8_t bitSize) {
  int i, bPass;
  uint32_t u32R;
  printf("TestL %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:", loop, u32V, bitPos, bitSize);
  FPRINT("TestL %d loop: u32V=0x%X, bitPos=%d, bitSize=%d:\n", loop, u32V, bitPos, bitSize);
  Std_BitSetLEG(u8G, u32V, bitPos, bitSize);
  Std_BitSetLittleEndian(u8T, u32V, bitPos, bitSize);
  u32R = Std_BitGetLittleEndian(u8T, bitPos, bitSize);
  LOG_D("testL G:", u8G);
  LOG_D("testL T:", u8T);

  bPass = true;
  for (i = 0; i < BUFFER_SIZE; i++) {
    if (u8G[i] != u8T[i]) {
      bPass = false;
    }
  }
  if (u32R != u32V) {
    printf("\n read FAIL, read=0x%X\n", u32R);
    FPRINT("\n read FAIL, read=0x%X\n", u32R);
    bPass = false;
  }
  printf(" %s\n", bPass ? "PASS" : "FAIL");
  FPRINT(" %s\n", bPass ? "PASS" : "FAIL");
  if (false == bPass) {
#ifdef STDBIT_DEBUG
    fclose(flog);
#endif
    exit(-1);
  }
}

static void Test_WriteAll(void) {
  int i = 0;
  uint16_t bitPos;
  uint8_t bitSize;
  uint32_t mask, u32V;
  for (i = 0; i < 10000; i++) {
    bitPos = (uint16_t)rand() % ((BUFFER_SIZE - 4) * 8);
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

static void TestB64_Write(int loop, uint64_t u64V, uint16_t bitPos, uint8_t bitSize) {
  int i, bPass;
  uint64_t u64R;
  printf("TestB64 %d loop: u64V=0x%" PRIx64 ", bitPos=%d, bitSize=%d:", loop, u64V, bitPos,
         bitSize);
  FPRINT("TestB %d loop: u64V=0x%" PRIx64 ", bitPos=%d, bitSize=%d:\n", loop, u64V, bitPos,
         bitSize);
  Std_Bit64SetBEG(u8G, u64V, bitPos, bitSize);
  Std_Bit64SetBigEndian(u8T, u64V, bitPos, bitSize);
  u64R = Std_Bit64GetBigEndian(u8T, bitPos, bitSize);
  LOG_D("testB64 G:", u8G);
  LOG_D("testB64 T:", u8T);

  bPass = true;
  for (i = 0; i < BUFFER_SIZE; i++) {
    if (u8G[i] != u8T[i]) {
      bPass = false;
    }
  }
  if (u64R != u64V) {
    printf("\n read FAIL, read=0x%" PRIx64 "\n", u64R);
    FPRINT("\n read FAIL, read=0x%" PRIx64 "\n", u64R);
    bPass = false;
  }
  printf(" %s\n", bPass ? "PASS" : "FAIL");
  FPRINT(" %s\n", bPass ? "PASS" : "FAIL");
  if (false == bPass) {
#ifdef STDBIT_DEBUG
    fclose(flog);
#endif
    exit(-1);
  }
}

static void TestL64_Write(int loop, uint64_t u64V, uint16_t bitPos, uint8_t bitSize) {
  int i, bPass;
  uint64_t u64R;
  printf("TestL64 %d loop: u64V=0x%" PRIx64 ", bitPos=%d, bitSize=%d:", loop, u64V, bitPos,
         bitSize);
  FPRINT("TestL %d loop: u64V=0x%" PRIx64 ", bitPos=%d, bitSize=%d:\n", loop, u64V, bitPos,
         bitSize);
  Std_Bit64SetLEG(u8G, u64V, bitPos, bitSize);
  Std_Bit64SetLittleEndian(u8T, u64V, bitPos, bitSize);
  u64R = Std_Bit64GetLittleEndian(u8T, bitPos, bitSize);
  LOG_D("testL G:", u8G);
  LOG_D("testL T:", u8T);

  bPass = true;
  for (i = 0; i < BUFFER_SIZE; i++) {
    if (u8G[i] != u8T[i]) {
      bPass = false;
    }
  }
  if (u64R != u64V) {
    printf("\n read FAIL, read=0x%" PRIx64 "\n", u64R);
    FPRINT("\n read FAIL, read=0x%" PRIx64 "\n", u64R);
    bPass = false;
  }
  printf(" %s\n", bPass ? "PASS" : "FAIL");
  FPRINT(" %s\n", bPass ? "PASS" : "FAIL");
  if (false == bPass) {
#ifdef STDBIT_DEBUG
    fclose(flog);
#endif
    exit(-1);
  }
}

static void Test64_WriteAll(void) {
  int i = 0;
  uint16_t bitPos;
  uint8_t bitSize;
  uint64_t mask, u64V;
  for (i = 0; i < 10000; i++) {
    bitPos = (uint16_t)rand() % ((BUFFER_SIZE - 8) * 8);
    bitSize = (uint8_t)rand() % 65;
    if (0 == bitSize) {
      bitSize = 7;
    }
    mask = 0xFFFFFFFFFFFFFFFFul >> (64 - bitSize);
    u64V = (((uint64_t)rand() << 32) + (uint64_t)rand()) & mask;
    TestB64_Write(i, u64V, bitPos, bitSize);
    TestL64_Write(i, u64V, bitPos, bitSize);
  }
}

static void Test_BitCopyOne(int loop, uint16_t dstBitPos, uint16_t srcBitPos, uint16_t bitSize) {
  int i, bPass;
  printf("Test BitCopy %d loop: dstBitPos=%d, srcBitPos=%d, bitSize=%d:", loop, dstBitPos,
         srcBitPos, bitSize);
  FPRINT("Test BitCopy %d loop: dstBitPos=%d, srcBitPos=%d, bitSize=%d:\n", loop, dstBitPos,
         srcBitPos, bitSize);
  Std_BitCopyG(u8BitDstG, dstBitPos, u8BitSrc, srcBitPos, bitSize);
  Std_BitCopy(u8BitDstT, dstBitPos, u8BitSrc, srcBitPos, bitSize);
  FPRINT("                ::");
  for (i = 0; i < BUFFER_SIZE * 8; i++) {
    if (i == srcBitPos) {
      FPRINT("<");
    } else if (i == (srcBitPos + bitSize - 1u)) {
      FPRINT(">");
    } else if (i == dstBitPos) {
      FPRINT("[");
    } else if (i == (dstBitPos + bitSize - 1u)) {
      FPRINT("]");
    } else {
      FPRINT(" ");
    }
  }
  FPRINT("\n");
  LOG_B("Test BitCopy S:", u8BitSrc);
  LOG_B("Test BitCopy G:", u8BitDstG);
  LOG_B("Test BitCopy T:", u8BitDstT);

  bPass = true;
  for (i = 0; i < BUFFER_SIZE; i++) {
    if (u8BitDstG[i] != u8BitDstT[i]) {
      bPass = false;
    }
  }
  printf(" %s\n", bPass ? "PASS" : "FAIL");
  FPRINT(" %s\n", bPass ? "PASS" : "FAIL");
  if (false == bPass) {
#ifdef STDBIT_DEBUG
    fclose(flog);
#endif
    exit(-1);
  }
}

static void Test_BitCopy(void) {
  int i = 0;
  uint16_t bitPos;
  uint16_t srcBitPos;
  uint16_t dstBitPos;
  uint16_t bitSize;
  uint64_t u64V;
  memset(u8BitSrc, 0, BUFFER_SIZE);
  memset(u8BitDstT, 0, BUFFER_SIZE);
  memset(u8BitDstG, 0, BUFFER_SIZE);
  for (i = 0; i < BUFFER_SIZE; i++) {
    u8BitSrc[i] = (uint8_t)rand();
  }
  for (i = 0; i < 10000; i++) {
    bitPos = (uint16_t)rand() % ((BUFFER_SIZE - 8) * 8);
    srcBitPos = (uint16_t)rand() % ((BUFFER_SIZE - 8) * 8);
    dstBitPos = (uint16_t)rand() % ((BUFFER_SIZE - 8) * 8);
    bitSize = (uint8_t)rand() % 65;
    if (0 == bitSize) {
      bitSize = 7;
    }
    u64V = ((uint64_t)rand() << 32) + (uint64_t)rand();
    Std_Bit64SetBEG(u8BitSrc, u64V, bitPos, bitSize);
    Test_BitCopyOne(i, dstBitPos, srcBitPos, bitSize);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char *argv[]) {
#ifdef STDBIT_DEBUG
  flog = fopen(".testbit.log", "w");
#endif
  memset(u8G, 0, BUFFER_SIZE);
  memset(u8T, 0, BUFFER_SIZE);
  Test_WriteAll();
  Test64_WriteAll();
  Test_BitCopy();
#ifdef STDBIT_DEBUG
  fclose(flog);
#endif
  return 0;
}
