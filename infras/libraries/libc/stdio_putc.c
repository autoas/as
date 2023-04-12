/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021-2023 Parai Wang <parai@foxmail.com>
 */

/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdint.h>
#ifdef USE_STDIO_CAN
#include "ringbuffer.h"
#include "Std_Critical.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef STDIO_CAN_DLC
#define STDIO_CAN_DLC 8
#endif
#ifndef STDIO_CAN_IBUFFER_SIZE
#define STDIO_CAN_IBUFFER_SIZE 4096
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void __putchar(char chr);
extern int stdio_can_put(uint8_t *data, uint8_t dlc);
/* ================================ [ DATAS     ] ============================================== */
#ifdef USE_STDIO_CAN
RB_DECLARE(stdio_can, char, STDIO_CAN_IBUFFER_SIZE);
static uint32_t wmissing = 0;
#endif
/* ================================ [ LOCALS    ] ============================================== */
#ifdef USE_STDIO_CAN
static void can_putc(char chr) {
  rb_size_t r;
  EnterCritical();
  r = RB_PUSH(stdio_can, &chr, 1);
  ExitCritical();

  if (1 != r) {
    /* do noting as full */
    wmissing++;
  }
}

static void stdio_can_main(void) {
  uint8_t data[STDIO_CAN_DLC];
  rb_size_t sz;
  int r;

  EnterCritical();
  sz = RB_POLL(stdio_can, data, STDIO_CAN_DLC);
  ExitCritical();
  if (sz > 0) {
    r = stdio_can_put(data, (uint8_t)sz);
    if (0 == r) {
      EnterCritical();
      (void)RB_DROP(stdio_can, sz); /* consume it */
      ExitCritical();
    }
  }
}
#endif

void __attribute__((weak)) __putchar(char chr) {
#if defined(_WIN32) || defined(linux)
  fputc(chr, stdout);
#endif
}
/* ================================ [ FUNCTIONS ] ============================================== */
void stdio_putc(char chr) {
  __putchar(chr);
#ifdef USE_STDIO_CAN
  can_putc(chr);
#endif
}

void stdio_main_function(void) {
#ifdef USE_STDIO_CAN
  stdio_can_main();
#endif
}