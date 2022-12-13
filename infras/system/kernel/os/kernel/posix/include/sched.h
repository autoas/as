/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2017 Parai Wang <parai@foxmail.com>
 */
#ifndef _ASKAR_SCHED_H_
#define _ASKAR_SCHED_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include <sys/queue.h>
/* ================================ [ MACROS    ] ============================================== */
/* Scheduling algorithms.  */
#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2

#define PTHREAD_EXPLICIT_SCHED 0
#define PTHREAD_INHERIT_SCHED 1
/* ================================ [ TYPES     ] ============================================== */
struct sched_param {
  int sched_priority;
};

struct TaskVar;
typedef TAILQ_HEAD(TaskList, TaskVar) TaskListType;

typedef unsigned long sigset_t;

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void sched_yield(void);
#endif /* _ASKAR_SCHED_H_ */
