/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifndef KERNEL_H_
#define KERNEL_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
#define E_OS_ACCESS ((StatusType)1)
#define E_OS_CALLEVEL ((StatusType)2)
#define E_OS_ID ((StatusType)3)
#define E_OS_LIMIT ((StatusType)4)
#define E_OS_NOFUNC ((StatusType)5)
#define E_OS_RESOURCE ((StatusType)6)
#define E_OS_STATE ((StatusType)7)
#define E_OS_VALUE ((StatusType)8)

#define OSDEFAULTAPPMODE ((AppModeType)1)

/* OSEK task state: lower 4 bits */
#define SUSPENDED ((StatusType)0)
#define RUNNING ((StatusType)1)
#define READY ((StatusType)2)
#define WAITING ((StatusType)3)

#define OSEK_TASK_STATE_MASK ((StatusType)0x0F)

/* PTHREAD state bit mask: higher 4 bits */
#define PTHREAD_STATE_SLEEPING ((StatusType)0x10)
#define PTHREAD_STATE_WAITING ((StatusType)0x20)

#define INVALID_TASK ((TaskType)-1)
#define INVALID_RESOURCE ((ResourceType)-1)
#define INVALID_ALARM ((AlarmType)-1)

/*
 *  Macro for declare Task/Alarm/ISR Entry
 */
#define TASK(TaskName) void TaskMain##TaskName(void)
#define ISR(ISRName) void ISRMain##ISRName(void)
#define ALARM(AlarmCallBackName) void AlarmMain##AlarmCallBackName(void)

#define RES_SCHEDULER ((ResourceType)0) /* default resources for OS */

#define DeclareTask(name) extern const TaskType name
#define DeclareCounter(name) extern const CounterType name
#define DeclareAlarm(name) extern const AlarmType name
#define DeclareEvent(name) extern const EventMaskType name
#define DeclareResource(name) extern const ResourceType name

/*
 *  OS service API IDs
 */
#define OSServiceId_ActivateTask ((OSServiceIdType)0)
#define OSServiceId_TerminateTask ((OSServiceIdType)1)
#define OSServiceId_ChainTask ((OSServiceIdType)2)
#define OSServiceId_Schedule ((OSServiceIdType)3)
#define OSServiceId_GetTaskID ((OSServiceIdType)4)
#define OSServiceId_GetTaskState ((OSServiceIdType)5)
#define OSServiceId_EnableAllInterrupts ((OSServiceIdType)6)
#define OSServiceId_DisableAllInterrupts ((OSServiceIdType)7)
#define OSServiceId_ResumeAllInterrupts ((OSServiceIdType)8)
#define OSServiceId_SuspendAllInterrupts ((OSServiceIdType)9)
#define OSServiceId_ResumeOSInterrupts ((OSServiceIdType)10)
#define OSServiceId_SuspendOSInterrupts ((OSServiceIdType)11)
#define OSServiceId_GetResource ((OSServiceIdType)12)
#define OSServiceId_ReleaseResource ((OSServiceIdType)13)
#define OSServiceId_SetEvent ((OSServiceIdType)14)
#define OSServiceId_ClearEvent ((OSServiceIdType)15)
#define OSServiceId_GetEvent ((OSServiceIdType)16)
#define OSServiceId_WaitEvent ((OSServiceIdType)17)
#define OSServiceId_GetAlarmBase ((OSServiceIdType)18)
#define OSServiceId_GetAlarm ((OSServiceIdType)19)
#define OSServiceId_SetRelAlarm ((OSServiceIdType)20)
#define OSServiceId_SetAbsAlarm ((OSServiceIdType)21)
#define OSServiceId_CancelAlarm ((OSServiceIdType)22)
#define OSServiceId_GetActiveApplicationMode ((OSServiceIdType)23)
#define OSServiceId_StartOS ((OSServiceIdType)24)
#define OSServiceId_ShutdownOS ((OSServiceIdType)25)
#define OSServiceId_IncrementCounter ((OSServiceIdType)26)

/*
 *  OS Error Process Macors
 */
#define OSErrorGetServiceId() (_errorhook_svcid)

#define OSError_ActivateTask_TaskID() (_errorhook_par1.tskid)
#define OSError_ChainTask_TaskID() (_errorhook_par1.tskid)
#define OSError_GetTaskID_TaskID() (_errorhook_par1.p_tskid)
#define OSError_GetTaskState_TaskID() (_errorhook_par1.tskid)
#define OSError_GetTaskState_State() (_errorhook_par2.p_state)
#define OSError_GetResource_ResID() (_errorhook_par1.resid)
#define OSError_ReleaseResource_ResID() (_errorhook_par1.resid)
#define OSError_SetEvent_TaskID() (_errorhook_par1.tskid)
#define OSError_SetEvent_Mask() (_errorhook_par2.mask)
#define OSError_ClearEvent_Mask() (_errorhook_par1.mask)
#define OSError_GetEvent_TaskID() (_errorhook_par1.tskid)
#define OSError_GetEvent_Mask() (_errorhook_par2.p_mask)
#define OSError_WaitEvent_Mask() (_errorhook_par1.mask)
#define OSError_GetAlarmBase_AlarmID() (_errorhook_par1.almid)
#define OSError_GetAlarmBase_Info() (_errorhook_par2.p_info)
#define OSError_GetAlarm_AlarmID() (_errorhook_par1.almid)
#define OSError_GetAlarm_Tick() (_errorhook_par2.p_tick)
#define OSError_SetRelAlarm_AlarmID() (_errorhook_par1.almid)
#define OSError_SetRelAlarm_Increment() (_errorhook_par2.incr)
#define OSError_SetRelAlarm_Cycle() (_errorhook_par3.cycle)
#define OSError_SetAbsAlarm_AlarmID() (_errorhook_par1.almid)
#define OSError_SetAbsAlarm_Start() (_errorhook_par2.start)
#define OSError_SetAbsAlarm_Cycle() (_errorhook_par3.cycle)
#define OSError_CancelAlarm_AlarmID() (_errorhook_par1.almid)
#define OSError_IncrementCounter_CounterID() (_errorhook_par1.cntid)

#ifndef OS_TICKS_PER_SECOND
#define OS_TICKS_PER_SECOND 1000
#endif
#ifndef USECONDS_PER_TICK
#define USECONDS_PER_TICK (1000000 / OS_TICKS_PER_SECOND)
#endif

#define TICK_MAX ((TickType)0xFFFFFFFFul)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8 StatusType;
typedef uint32 EventMaskType;
typedef EventMaskType *EventMaskRefType;
typedef uint8 TaskType;
typedef TaskType *TaskRefType;
typedef uint8 TaskStateType;
typedef TaskStateType *TaskStateRefType;
typedef uint32 AppModeType; /*! each bit is a mode */

typedef uint32 TickType;
typedef TickType *TickRefType;
typedef uint8 IsrType;     /* ISR ID */
typedef uint8 CounterType; /* Counter ID */

typedef uint8 AlarmType;

typedef struct {
  TickType maxallowedvalue;
  TickType ticksperbase;
  TickType mincycle;
} AlarmBaseType;
typedef AlarmBaseType *AlarmBaseRefType;

typedef uint8 ResourceType;

typedef uint8 OSServiceIdType; /* OS service API ID */

typedef union {
  TaskType tskid;
  TaskRefType p_tskid;
  TaskStateRefType p_state;
  ResourceType resid;
  EventMaskType mask;
  EventMaskRefType p_mask;
  AlarmType almid;
  AlarmBaseRefType p_info;
  TickRefType p_tick;
  TickType incr;
  TickType cycle;
  TickType start;
  AppModeType mode;
  CounterType cntid;
} _ErrorHook_Par;

typedef void (*FP)(void);
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
extern OSServiceIdType _errorhook_svcid;
extern _ErrorHook_Par _errorhook_par1, _errorhook_par2, _errorhook_par3;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
StatusType ActivateTask(TaskType TaskID);
StatusType TerminateTask(void);
StatusType ChainTask(TaskType TaskID);
StatusType Schedule(void);
StatusType GetTaskID(TaskRefType pTaskType);
StatusType GetTaskState(TaskType TaskID, TaskStateRefType pState);

StatusType SignalCounter(CounterType CounterID);
StatusType GetAlarmBase(AlarmType AlarmID, AlarmBaseRefType pInfo);
StatusType GetAlarm(AlarmType AlarmID, TickRefType pTick);
StatusType SetRelAlarm(AlarmType AlarmID, TickType Increment, TickType Cycle);
StatusType SetAbsAlarm(AlarmType AlarmID, TickType Start, TickType Cycle);
StatusType CancelAlarm(AlarmType AlarmID);

StatusType SetEvent(TaskType TaskID, EventMaskType pMask);
StatusType ClearEvent(EventMaskType Mask);
StatusType GetEvent(TaskType TaskID, EventMaskRefType pEvent);
StatusType WaitEvent(EventMaskType Mask);

StatusType GetResource(ResourceType ResID);
StatusType ReleaseResource(ResourceType ResID);

void StartOS(AppModeType Mode);
void ShutdownOS(StatusType Error);
AppModeType GetActiveApplicationMode(void);

void ShutdownHook(StatusType Error);
void StartupHook(void);
void ErrorHook(StatusType Error);
void PreTaskHook(void);
void PostTaskHook(void);

void EnterISR(void);
void LeaveISR(void);

void EnableInterrupt(void);
void DisableInterrupt(void);

void DisableAllInterrupts(void);
void EnableAllInterrupts(void);

void SuspendAllInterrupts(void);
void ResumeAllInterrupts(void);

void SuspendOSInterrupts(void);
void ResumeOSInterrupts(void);

void Os_Sleep(TickType tick);
#endif /* KERNEL_H_ */
