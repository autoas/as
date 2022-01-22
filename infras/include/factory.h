/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 */
#ifndef _FACTORY_H
#define _FACTORY_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* depend on event happens */
#define FACTORY_E_EVENT 10
/* Stop this factory */
#define FACTORY_E_STOP 11

#define FACTORY_MAX_MACHINES 80
/* from 20 to 99, map machine 0 to 79, switch to that machine */
#define FACTORY_E_SWITCH_TO 20

#define FACTORY_MAX_NODES 150
/* from 100 to 249, map node 0 to 149, go to that node */
#define FACTORY_E_GOTO 100

#define FACTORY_IDLE 0
#define FACTORY_RUNNING 1
#define FACTORY_WAITING 2

#define factory_switch(machine) (Std_ReturnType)(FACTORY_E_SWITCH_TO + FEE_MACHINE_##machine)
#define factory_goto(node) (Std_ReturnType)(FACTORY_E_GOTO + FEE_NODE_##node)
/* ================================ [ TYPES     ] ============================================== */
typedef struct {
  uint8_t state;
  uint8_t machineId;
  uint8_t nodeId;
} factory_context_t;

typedef struct factory_s factory_t;

typedef Std_ReturnType (*factory_event_t)(void);
typedef Std_ReturnType (*factory_main_t)(void);

typedef struct {
  const char *name;
  factory_main_t main;
  const factory_event_t *events;
  uint8_t numOfEvents;
} machine_node_t;

typedef struct machine_s {
  const char *name;
  const machine_node_t *nodes;
  uint8_t numOfNodes;
} machine_t;

typedef enum
{
  MACHINE_STOP,
  MACHINE_FAIL,
} machine_state_t;

typedef struct factory_s {
  const char *name;
  factory_context_t *context;
  const machine_t *machines;
  uint8_t numOfMachines;
  void (*stateNotification)(uint8_t machineId, machine_state_t state);
} factory_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
void factory_init(const factory_t *factory);
void factory_cancel(const factory_t *factory);

Std_ReturnType factory_main(const factory_t *factory);
Std_ReturnType factory_on_event(const factory_t *factory, uint8_t eventId);

Std_ReturnType factory_start_machine(const factory_t *factory, uint8_t machineId);

uint8_t factory_get_state(const factory_t *factory);
#endif /* _FACTORY_H */
