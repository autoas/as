/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef CANTSYN_SLAVE_CFG_H
#define CANTSYN_SLAVE_CFG_H
/* ================================ [ INCLUDES  ] ============================================== */
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTSYN_MAIN_FUNCTION_PERIOD
#define CANTSYN_MAIN_FUNCTION_PERIOD 10
#endif
#define CANTSYN_CONVERT_MS_TO_MAIN_CYCLES(x)                                                       \
  ((x + CANTSYN_MAIN_FUNCTION_PERIOD - 1) / CANTSYN_MAIN_FUNCTION_PERIOD)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* CANTSYN_SLAVE_CFG_H */
