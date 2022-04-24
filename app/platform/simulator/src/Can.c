/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of CAN Driver AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Can.h"
#include "CanIf_Can.h"
#include "canlib.h"
#include "Can_Lcfg.h"
/* ================================ [ MACROS    ] ============================================== */
/* this simulation just alow only one HTH/HRH for each CAN controller */
#define CAN_MAX_HOH 32

#define CAN_CONFIG (&Can_Config)


/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern Can_ConfigType Can_Config;
/* ================================ [ DATAS     ] ============================================== */
static uint32_t lOpenFlag = 0;
static uint32_t lWriteFlag = 0;
static uint32_t lswPduHandle[32];
static int lBusIdMap[32];
/* ================================ [ LOCALS    ] ============================================== */
__attribute__((weak)) void CanIf_RxIndication(const Can_HwType *Mailbox,
                                              const PduInfoType *PduInfoPtr) {
}
__attribute__((weak)) void CanIf_TxConfirmation(PduIdType CanTxPduId) {
}
/* ================================ [ FUNCTIONS ] ============================================== */
void Can_Init(const Can_ConfigType *Config) {
  (void)Config;
  lOpenFlag = 0;
  lWriteFlag = 0;
}

Std_ReturnType Can_SetControllerMode(uint8_t Controller, Can_ControllerStateType Transition) {
  Std_ReturnType ret = E_NOT_OK;
  Can_ChannelConfigType *config;
  int rv;

  if (Controller < CAN_CONFIG->numOfChannels) {
    config = &CAN_CONFIG->channelConfigs[Controller];
    switch (Transition) {
    case CAN_CS_STARTED:
      lBusIdMap[Controller] = can_open(config->device, config->port, config->baudrate);
      if (lBusIdMap[Controller] >= 0) {
        ret = E_OK;
        lOpenFlag |= (1 << Controller);
        lWriteFlag = 0;
      }
      break;
    case CAN_CS_STOPPED:
    case CAN_CS_SLEEP:
      rv = can_close(lBusIdMap[Controller]);
      if (TRUE == rv) {
        ret = E_OK;
        lOpenFlag &= ~(1 << Controller);
      }
      break;
    default:
      break;
    }
  }

  return ret;
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo) {
  Std_ReturnType ret = E_OK;
  int r;

  if (lOpenFlag & (1 << Hth)) {
    if (0 == (lWriteFlag & (1 << Hth))) {
      r = can_write(lBusIdMap[Hth], PduInfo->id, PduInfo->length, PduInfo->sdu);
      if (TRUE == r) {
        lWriteFlag |= (1 << Hth);
        lswPduHandle[Hth] = PduInfo->swPduHandle;
      } else {
        ret = E_NOT_OK;
      }
    } else {
      ret = CAN_BUSY;
    }
  }

  return ret;
}

void Can_DeInit(void) {
}

void Can_MainFunction_Write(void) {
  int i;
  PduIdType swPduHandle;

  for (i = 0; i < CAN_MAX_HOH; i++) {
    if (lWriteFlag & (1 << i)) {
      swPduHandle = lswPduHandle[i];
      lWriteFlag &= ~(1 << i);
      CanIf_TxConfirmation(swPduHandle);
    }
  }
}

void Can_MainFunction_Read(void) {
  int i;
  int r;
  uint32_t canid;
  uint8_t dlc;
  uint8_t data[64];
  Can_HwType Mailbox;
  PduInfoType PduInfo;

  for (i = 0; i < CAN_MAX_HOH; i++) {
    if (lOpenFlag & (1 << i)) {
      canid = (uint32_t)-1;
      dlc = sizeof(data);
      r = can_read(i, &canid, &dlc, data);
      if (TRUE == r) {
        Mailbox.CanId = canid;
        Mailbox.ControllerId = i;
        Mailbox.Hoh = i;
        PduInfo.SduLength = dlc;
        PduInfo.SduDataPtr = data;
        PduInfo.MetaDataPtr = NULL;
        CanIf_RxIndication(&Mailbox, &PduInfo);
      }
    }
  }
}
