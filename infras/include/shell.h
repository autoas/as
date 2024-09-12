/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
#ifndef _SHELL_H
#define _SHELL_H
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Std_Debug.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
#define SHELL_REGISTER(name, desc, func)                                                           \
  static const Shell_CmdType shCmd_##name = {#name, desc, func};                                   \
  static void __attribute__((constructor)) _sh_cmd_##name##_ctor(void) {                           \
    Shell_Register(&shCmd_##name);                                                                 \
  }
#else
#define SHELL_REGISTER(name, desc, func)                                                           \
  const Shell_CmdType __attribute__((section("ShellCmdTab"))) shCmd_##name = {#name, desc, func};
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef int (*Sh_CmdFuncType)(int argc, const char *argv[]);

typedef struct {
  const char *cmdName;
  const char *cmdDesc;
  Sh_CmdFuncType cmdFunc;
} Shell_CmdType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void Shell_Init(void);
void Shell_Input(char ch);
#if defined(_WIN32) || defined(linux)
void Shell_Register(const Shell_CmdType *cmd);
#endif
void Shell_MainFunction(void);
#ifdef __cplusplus
}
#endif
#endif /* _SHELL_H */
