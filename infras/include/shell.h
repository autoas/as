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
#include <stdint.h>
#include "Std_Types.h"
#include "Std_Debug.h"
#include "Std_Compiler.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
#define SHELL_REGISTER(name, desc, func)                                                           \
  static const Shell_CmdType shCmd_##name = {#name, desc, func};                                   \
  INITIALIZER(_sh_cmd_##name##_ctor) {                                                             \
    Shell_Register(&shCmd_##name);                                                                 \
  }
#else
#if defined(__CSP__) || defined(__ghs__)
#define SHELL_CONST
#endif
#ifndef SHELL_CONST
#define SHELL_CONST __attribute__((section("ShellCmdTab")))
#endif
#define SHELL_REGISTER(name, desc, func)                                                           \
  CONSTANT(Shell_CmdType, SHELL_CONST) shCmd_##name = {#name, desc, func};
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef int (*Sh_CmdFuncType)(int argc, const char *argv[]);

typedef struct {
  const char *cmdName;
  const char *cmdDesc;
  Sh_CmdFuncType cmdFunc;
} Shell_CmdType;

typedef struct {
  const Shell_CmdType *const *cmds;
  uint32_t numOfCmds;
} Shell_ConfigType;
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
