/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "kernel_internal.h"
#if (OS_PTHREAD_NUM > 0)
#include "pthread.h"
#include "signal.h"
#include "OsMem.h"
#endif
#include "Std_Debug.h"
#ifdef USE_SHELL
#include "shell.h"
#endif
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
#ifdef USE_SHELL
extern void statOsTask(void);
extern void statOsAlarm(void);
extern void statOsCounter(void);
#endif
/* ================================ [ DATAS     ] ============================================== */
OSServiceIdType _errorhook_svcid;
_ErrorHook_Par _errorhook_par1, _errorhook_par2, _errorhook_par3;

#ifdef USE_SMP
TaskVarType *RunningVars[CPU_CORE_NUMBER];
TaskVarType *ReadyVars[CPU_CORE_NUMBER];
unsigned int CallLevels[CPU_CORE_NUMBER];
#else
TaskVarType *RunningVar;
TaskVarType *ReadyVar;
unsigned int CallLevel;
#endif

TickType OsTickCounter;

static AppModeType appMode;
/* ================================ [ LOCALS    ] ============================================== */
static void Os_MiscInit(void) {
#ifdef USE_SMP
  memset(RunningVars, 0, sizeof(RunningVars));
  memset(ReadyVars, 0, sizeof(ReadyVars));
  memset(CallLevels, 0, sizeof(CallLevels));
#else
  RunningVar = NULL;
  ReadyVar = NULL;
  CallLevel = TCL_NULL;
#endif

  OsTickCounter = 0;

#if (OS_PTHREAD_NUM > 0)
  Os_MemInit();
  Os_SleepInit();
#ifdef USE_PTHREAD_SIGNAL
  Os_SignalInit();
#endif
#endif

  Sched_Init();
}
#ifdef USE_SHELL
int statOsFunc(int argc, const char *argv[]) {
  if ((1 == argc) || (0 == strcmp(argv[1], "task"))) {
    statOsTask();
  }

  if ((1 == argc) || (0 == strcmp(argv[1], "alarm"))) {
    statOsAlarm();
  }

  if ((1 == argc) || (0 == strcmp(argv[1], "counter"))) {
    statOsCounter();
  }

  return 0;
}
#ifdef USE_PTHREAD_SIGNAL
static int killOsFunc(int argc, const char *argv[]) {
  int ercd;
  int pid;
  int sig = SIGKILL;
  pthread_t tid;

  pid = strtoul(argv[1], NULL, 10);

  if (3 == argc) {
    sig = strtoul(argv[2], NULL, 10);
  }

  if (pid < OS_PTHREAD_NUM) {
    tid = (pthread_t)TaskVarArray[TASK_NUM + pid].pConst;
    if (tid > (pthread_t)1) {
      ercd = pthread_kill(tid, sig);
    } else {
      ercd = -EACCES;
    }
  } else {
    ercd = -ENOENT;
  }

  return ercd;
}
#endif

#ifdef USE_SHELL
SHELL_REGISTER(ps,
               "ps <task/alarm/counter>\n"
               "  Show the status of operationg system\n",
               statOsFunc);
#ifdef USE_PTHREAD_SIGNAL
SHELL_REGISTER(kill,
               "\n  kill pid [sig]\n"
               "  kill signal sign to the posix thread by pid,\n"
               "  if sig is not specified, will kill SIGKILL(9) by default\n",
               killOsFunc);
#endif
#endif
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
/* |------------------+------------------------------------------------------| */
/* | Syntax:          | void StartOS ( AppModeType <Mode> )                  | */
/* |------------------+------------------------------------------------------| */
/* | Parameter (In):  | Mode:application mode                                | */
/* |------------------+------------------------------------------------------| */
/* | Parameter (Out): | none                                                 | */
/* |------------------+------------------------------------------------------| */
/* | Description:     | The user can call this system service to start the   | */
/* |                  | operating system in a specific mode, see chapter 5   | */
/* |                  | (os223.doc), Application modes.                      | */
/* |------------------+------------------------------------------------------| */
/* | Particularities: | Only allowed outside of the operating system,        | */
/* |                  | therefore implementation specific restrictions may   | */
/* |                  | apply. See also chapter 11.3, System start-up,       | */
/* |                  | especially with respect to systems where OSEK and    | */
/* |                  | OSEKtime coexist. This call does not need to return. | */
/* |------------------+------------------------------------------------------| */
/* | Conformance:     | BCC1, BCC2, ECC1, ECC2                               | */
/* |------------------+------------------------------------------------------| */
FUNC(void, __weak) Os_PortStartFirstDispatch(void) {
  Os_PortStartDispatch();
}

void StartOS(AppModeType Mode) {
  DECLARE_SMP_PROCESSOR_ID();

  appMode = Mode;

  DisableInterrupt();

  Os_MiscInit();
  Os_PortInit();
  Os_TaskInit(Mode);
  Os_ResourceInit();
#if (COUNTER_NUM > 0)
  Os_CounterInit();
#endif
#if (ALARM_NUM > 0)
  Os_AlarmInit(Mode);
#endif

  OSStartupHook();
#ifdef USE_SMP
  Os_PortSpinLock();
#endif
  Sched_GetReady();
  Os_PortStartFirstDispatch();
  while (1)
    ;
}

void ShutdownOS(StatusType Error) {
  OSShutdownHook(Error);
  DisableInterrupt();
  while (1)
    ;
}

AppModeType GetActiveApplicationMode(void) {
  return appMode;
}

void OsTick(void) {
  OsTickCounter++;

#if (OS_PTHREAD_NUM > 0)
  Os_SleepTick();
#endif
}

#ifdef USE_SMP
imask_t Os_LockKernel(void) {
  imask_t imask;
  DECLARE_SMP_PROCESSOR_ID();

  imask = Std_EnterCritical();

  if (RunningVar != NULL) {
    if (0 == RunningVar->lock) {
      Os_PortSpinLock();
    }
    RunningVar->lock++;
    /* bug if too much nested lock */
    asAssert(RunningVar->lock != 0xFF);
  }

  return imask;
}

void Os_UnLockKernel(imask_t imask) {
  DECLARE_SMP_PROCESSOR_ID();

  if (RunningVar != NULL) {
    asAssert(RunningVar->lock > 0);
    RunningVar->lock--;
    if (0 == RunningVar->lock) {
      Os_PortSpinUnLock();
    }
  }

  Std_ExitCritical(imask);
}

/* This can only be called in context switching area where OS spin lock is locked */
void Os_RestoreKernelLock(void) {
  DECLARE_SMP_PROCESSOR_ID();

  if (RunningVar != NULL) {
    if (0 == RunningVar->lock) {
      // asAssert((NULL == RunningVars[cpuid ? 0 : 1]) || (0 == RunningVars[cpuid ? 0 : 1]->lock));

      if (!(((NULL == RunningVars[cpuid ? 0 : 1]) || (0 == RunningVars[cpuid ? 0 : 1]->lock)))) {
        while (1)
          ;
      }
      Os_PortSpinUnLock();
    }
  }
}
#endif
