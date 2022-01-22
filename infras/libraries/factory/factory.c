/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "factory.h"
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_FACTORY 0
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static Std_ReturnType factory_health_check(const factory_t *factory) {
  Std_ReturnType ret = E_NOT_OK;
  uint8_t machineId;
  uint8_t nodeId;

  machineId = factory->context->machineId;
  if (machineId < factory->numOfMachines) {
    nodeId = factory->context->nodeId;
    if (nodeId < factory->machines[machineId].numOfNodes) {
      ret = E_OK;
    } else {
      factory_init(factory);
      ASLOG(ERROR, ("%s: Invalid NodeId %d for %s\n", factory->name, nodeId,
                    factory->machines[machineId].name));
    }
  } else {
    factory_init(factory);
    ASLOG(ERROR, ("%s: Invalid machineId %d\n", factory->name, machineId));
  }

  return ret;
}

Std_ReturnType factory_post(const factory_t *factory, uint8_t machineId, uint8_t nodeId,
                            Std_ReturnType ercd) {
  Std_ReturnType ret = ercd;

  if (E_OK == ercd) {
    /* pass */
  } else if (FACTORY_E_EVENT == ercd) {
    factory->context->state = FACTORY_WAITING;
    ret = E_OK;
  } else if (FACTORY_E_STOP == ercd) {
    ASLOG(FACTORY, ("%s: %s stopped\n", factory->name, factory->machines[machineId].name));
    factory_init(factory);
    factory->stateNotification(machineId, MACHINE_STOP);
    ret = E_OK;
  } else if ((ercd >= FACTORY_E_SWITCH_TO) &&
             (ercd < (FACTORY_E_SWITCH_TO + FACTORY_MAX_MACHINES))) {
    factory->stateNotification(machineId, MACHINE_STOP);
    machineId = ercd - FACTORY_E_SWITCH_TO;
    if (machineId < factory->numOfMachines) {
      factory->context->machineId = machineId;
      factory->context->nodeId = 0;
      ASLOG(FACTORY, ("%s: switch to %s\n", factory->name, factory->machines[machineId].name));
      ret = FACTORY_E_SWITCH_TO;
    } else {
      ASLOG(ERROR, ("%s: Invalid machineId %d\n", factory->name, machineId));
      factory_init(factory);
      ret = E_NOT_OK;
    }
  } else if ((ercd >= FACTORY_E_GOTO) && (ercd < (FACTORY_E_GOTO + FACTORY_MAX_NODES))) {
    nodeId = ercd - FACTORY_E_GOTO;
    if (nodeId < factory->machines[machineId].numOfNodes) {
      factory->context->nodeId = nodeId;
      ASLOG(FACTORY, ("%s: goto %s:%s\n", factory->name, factory->machines[machineId].name,
                      factory->machines[machineId].nodes[nodeId].name));
      ret = FACTORY_E_GOTO;
    } else {
      ASLOG(ERROR, ("%s: Invalid NodeId %d for %s\n", factory->name, nodeId,
                    factory->machines[machineId].name));
      factory_init(factory);
      ret = E_NOT_OK;
    }
  } else {
    ASLOG(ERROR,
          ("%s: %s %s failed with ercd %d\n", factory->name, factory->machines[machineId].name,
           factory->machines[machineId].nodes[nodeId].name, ercd));
    factory_init(factory);
    factory->stateNotification(machineId, MACHINE_FAIL);
  }

  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
void factory_init(const factory_t *factory) {
  factory->context->state = FACTORY_IDLE;
  factory->context->machineId = 0;
  factory->context->nodeId = 0;
}

void factory_cancel(const factory_t *factory) {
  Std_ReturnType ret;
  uint8_t machineId;

  ret = factory_health_check(factory);
  if (E_OK == ret) {
    machineId = factory->context->machineId;
    factory_init(factory);
    factory->stateNotification(machineId, MACHINE_FAIL);
  }
}

Std_ReturnType factory_main(const factory_t *factory) {
  Std_ReturnType ret = E_OK;
  uint8_t machineId;
  uint8_t nodeId;

  if (FACTORY_RUNNING == factory->context->state) {
    ret = factory_health_check(factory);
    if (E_OK == ret) {
      machineId = factory->context->machineId;
      nodeId = factory->context->nodeId;
      ASLOG(FACTORY, ("%s: %s %s main\n", factory->name, factory->machines[machineId].name,
                      factory->machines[machineId].nodes[nodeId].name));
      ret = factory->machines[machineId].nodes[nodeId].main();
      ret = factory_post(factory, machineId, nodeId, ret);
    }
  }

  return ret;
}

Std_ReturnType factory_on_event(const factory_t *factory, uint8_t eventId) {
  Std_ReturnType ret = E_NOT_OK;
  uint8_t machineId;
  uint8_t nodeId;

  if (FACTORY_WAITING == factory->context->state) {
    ret = factory_health_check(factory);
    if (E_OK == ret) {
      machineId = factory->context->machineId;
      nodeId = factory->context->nodeId;
      if (eventId < factory->machines[machineId].nodes[nodeId].numOfEvents) {
        ASLOG(FACTORY, ("%s: %s %s on event %d\n", factory->name, factory->machines[machineId].name,
                        factory->machines[machineId].nodes[nodeId].name, eventId));
        ret = factory->machines[machineId].nodes[nodeId].events[eventId]();
        ret = factory_post(factory, machineId, nodeId, ret);
        if ((FACTORY_E_SWITCH_TO == ret) || (FACTORY_E_GOTO == ret)) {
          factory->context->state = FACTORY_RUNNING;
          ret = factory_main(factory);
        } else {
          /* do nothing as pass or fail */
        }
      }
    } else {
      ASLOG(ERROR,
            ("%s: %s %s on invalid event %d\n", factory->name,
             factory->machines[factory->context->machineId].name,
             factory->machines[factory->context->machineId].nodes[factory->context->nodeId].name,
             eventId));
      factory_init(factory);
      ret = E_NOT_OK;
    }
  }

  return ret;
}

Std_ReturnType factory_start_machine(const factory_t *factory, uint8_t machineId) {
  Std_ReturnType ret = E_NOT_OK;

  if (machineId < factory->numOfMachines) {
    if (FACTORY_IDLE == factory->context->state) {
      factory->context->state = FACTORY_RUNNING;
      factory->context->machineId = machineId;
      factory->context->nodeId = 0;
      ret = E_OK;
      ASLOG(FACTORY, ("%s: start %s\n", factory->name, factory->machines[machineId].name));
    } else {
      ASLOG(ERROR, ("%s: %s start failed\n", factory->name, factory->machines[machineId].name));
    }
  } else {
    ASLOG(ERROR, ("%s: invalid machineId %d\n", factory->name, machineId));
  }

  return ret;
}

uint8_t factory_get_state(const factory_t *factory) {
  return factory->context->state;
}
