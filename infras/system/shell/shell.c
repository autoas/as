/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "shell.h"
#include "ringbuffer.h"
#include "Std_Types.h"
#include "Std_Critical.h"
#include "Std_Debug.h"
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(linux)
#include "osal.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
#ifndef SHELL_MAX_ARGS
#define SHELL_MAX_ARGS 32
#endif

#ifndef SHELL_CMDLINE_MAX
#define SHELL_CMDLINE_MAX 1024
#endif

#if defined(_WIN32) || defined(linux)
#define SHELL_DISABLE_ECHO_BACK
#endif
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void stdio_can_putc(char chr);
/* ================================ [ DATAS     ] ============================================== */
#if defined(_WIN32) || defined(linux)
Shell_CmdType __ssymtab_start[1024];
Shell_CmdType *__ssymtab_end = &__ssymtab_start[0];
static OSAL_ThreadType lStdinThread = NULL;
#else
#ifdef __CC_ARM
extern const Shell_CmdType ShellCmdTab$$Base;
extern const Shell_CmdType ShellCmdTab$$Limit;
#define __ssymtab_start &ShellCmdTab$$Base
#define __ssymtab_end &ShellCmdTab$$Limit
#else
extern const Shell_CmdType __ssymtab_start[];
extern const Shell_CmdType __ssymtab_end[];
#endif
#endif

static char lCmdLine[SHELL_CMDLINE_MAX];
static const char *lCmdArgv[SHELL_MAX_ARGS];
static uint32_t lCmdPos = 0;
RB_DECLARE(shin, char, SHELL_CMDLINE_MAX);
/* ================================ [ LOCALS    ] ============================================== */
#if defined(_WIN32) || defined(linux)
static void ProcessStdio(void *arg) {
  char ch;
  (void)arg;
  while (1) {
    ch = getchar();
    Shell_Input(ch);
  }
}
#endif

static int Shell_Help(int argc, const char *argv[]) {
  int r = 0;
  const Shell_CmdType *cmdIt;
  const Shell_CmdType *cmd;
  int i;

  if (1 == argc) {
    for (cmdIt = __ssymtab_start; cmdIt < __ssymtab_end; cmdIt++) {
      PRINTF("%s - %s\n", cmdIt->cmdName, cmdIt->cmdDesc);
    }
  } else {
    for (i = 1; i < argc; i++) {
      cmd = NULL;
      for (cmdIt = __ssymtab_start; (cmdIt < __ssymtab_end) && (NULL == cmd); cmdIt++) {
        if (0 == strcmp(cmdIt->cmdName, argv[i])) {
          cmd = cmdIt;
        }
      }
      if (cmd) {
        PRINTF("%s - %s\n", cmd->cmdName, cmd->cmdDesc);
      } else {
        ASLOG(ERROR, ("command %s is not found!\n", argv[i]));
      }
    }
  }
  return r;
}
SHELL_REGISTER(help, "help [cmd]\n", Shell_Help)

static void Shell_PutC(char ch) {
#ifdef SHELL_DISABLE_ECHO_BACK
#ifdef USE_STDIO_CAN
  stdio_can_putc(ch);
#endif
#else
  PRINTF("%c", ch);
#endif
}

static void Shell_Prompt(void) {
  PRINTF("as # ");
}

static void Shell_RunCmd(void) {
  int argc = 0;
  int i = 0;
  char ch;
  boolean isArg;
  boolean isEol = FALSE;
  const Shell_CmdType *cmdIt;
  const Shell_CmdType *cmd = NULL;

  while ((argc < SHELL_MAX_ARGS) && (FALSE == isEol)) {
    isArg = FALSE;
    /* skip leading space */
    while ((i < SHELL_CMDLINE_MAX) && (FALSE == isArg) && (FALSE == isEol)) {
      ch = lCmdLine[i];
      if ((' ' == ch) || ('\t' == ch)) {
        lCmdLine[i] = '\0'; /* split it */
        i++;
      } else if ('\0' == ch) {
        i++;
        isEol = TRUE;
      } else {
        isArg = TRUE;
      }
    }
    if (isArg) {
      lCmdArgv[argc] = &lCmdLine[i];
      argc++;
    }

    while ((i < SHELL_CMDLINE_MAX) && (TRUE == isArg) && (FALSE == isEol)) {
      ch = lCmdLine[i];
      if ((' ' == ch) || ('\t' == ch)) {
        isArg = FALSE;
      } else if ('\0' == ch) {
        isEol = TRUE;
      } else {
        i++;
      }
    }
  }

  if (argc > 0) {
    for (cmdIt = __ssymtab_start; (cmdIt < __ssymtab_end) && (NULL == cmd); cmdIt++) {
      if (0 == strcmp(cmdIt->cmdName, lCmdArgv[0])) {
        cmd = cmdIt;
      }
    }

    if (cmd) {
      i = cmd->cmdFunc(argc, lCmdArgv);
      if (i != 0) {
        ASLOG(ERROR, ("command error %d!\n", i));
      }
    } else {
      ASLOG(ERROR, ("command %s is not found!\n", lCmdArgv[0]));
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
#if defined(_WIN32) || defined(linux)
void Shell_Register(const Shell_CmdType *cmd) {
  int number = __ssymtab_end - __ssymtab_start;
  if (number < ARRAY_SIZE(__ssymtab_start)) {
    __ssymtab_start[number] = *cmd;
    __ssymtab_end = &__ssymtab_start[number + 1];
  }
}
#endif

void Shell_Init(void) {
#if defined(_WIN32) || defined(linux)
  if (NULL == lStdinThread) {
    lStdinThread = OSAL_ThreadCreate(ProcessStdio, NULL);
  }
#endif
  Shell_Prompt();
}

void Shell_Input(char ch) {
  rb_size_t r;
  if (ch == '\r') {
    /* ignore */
  } else {
    EnterCritical();
#ifdef RB_PUSH_FAST
    r = RB_PushChar(&rb_shin, &ch);
#else
    r = RB_PUSH(shin, &ch, 1);
#endif
    ExitCritical();
    if (1 != r) {
      ASLOG(ERROR, ("shell input buffer overflow!\n"));
    }
  }
}

void Shell_MainFunction(void) {
  rb_size_t r;
  char ch;

  EnterCritical();
  r = RB_POP(shin, &ch, 1);
  ExitCritical();
  if (1 == r) {
    if (lCmdPos >= SHELL_CMDLINE_MAX) {
      lCmdPos = 0;
    }

    /* for putty, backspace key value is 0x7f */
    if ((ch == '\b') || (ch == 0x7f)) {
      lCmdPos--;
      Shell_PutC(ch);
    } else if (ch == '\n') {
      Shell_PutC(ch);
      if (lCmdPos > 0) {
        lCmdLine[lCmdPos] = '\0';
        Shell_RunCmd();
        lCmdPos = 0;
      }
      Shell_Prompt();
    } else {
      lCmdLine[lCmdPos++] = ch;
      Shell_PutC(ch);
    }
  }
}
