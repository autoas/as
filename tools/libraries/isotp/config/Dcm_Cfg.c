/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Internal.h"
#include <Std_Debug.h>
#include <string.h>
#include "CanTp.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef DCM_DEFAULT_RXBUF_SIZE
#define DCM_DEFAULT_RXBUF_SIZE 4095
#endif

#ifndef DCM_DEFAULT_TXBUF_SIZE
#define DCM_DEFAULT_TXBUF_SIZE 4095
#endif

#define DCM_S3SERVER_CFG_TIMEOUT_MS 5000
#define DCM_P2SERVER_CFG_TIMEOUT_MS 500

#define DCM_RESET_DELAY_CFG_MS 100 /* give time for the response */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static uint8_t rxBuffer[DCM_DEFAULT_RXBUF_SIZE];
static uint8_t txBuffer[DCM_DEFAULT_TXBUF_SIZE];
const Dcm_ConfigType Dcm_Config = {
  rxBuffer,
  txBuffer,
  sizeof(rxBuffer),
  sizeof(txBuffer),
};
/* ================================ [ LOCALS    ] ============================================== */
static int lTxRequested = FALSE;
static int lRxResponce = FALSE;
/* ================================ [ FUNCTIONS ] ============================================== */
int Dcm_IsResponseReady(void) {
  int IsResponseReady = FALSE;
  Dcm_ContextType *context = Dcm_GetContext();

  if (DCM_BUFFER_FULL == context->rxBufferState) {
    if (FALSE == lRxResponce) {
      lRxResponce = TRUE;
      IsResponseReady = TRUE;
    }
  }

  return IsResponseReady;
}

int Dcm_IsTxCompleted(void) {
  int IsTxCompleted = FALSE;
  Dcm_ContextType *context = Dcm_GetContext();
  if (lTxRequested) {
    if (DCM_BUFFER_IDLE == context->txBufferState) {
      IsTxCompleted = TRUE;
      lTxRequested = FALSE;
    }
  }

  return IsTxCompleted;
}

int Dcm_Receive(uint8_t *buffer, PduLengthType length, int *functional) {
  int r = -1;
  Dcm_ContextType *context = Dcm_GetContext();

  if ((DCM_BUFFER_FULL == context->rxBufferState) && (context->RxTpSduLength <= length)) {
    r = (int)context->RxTpSduLength;
    memcpy(buffer, Dcm_Config.rxBuffer, r);
    context->rxBufferState = DCM_BUFFER_IDLE;
    lRxResponce = FALSE;
    if (NULL != functional) {
      if (DCM_P2P_PDU == context->curPduId) {
        *functional = FALSE;
      } else {
        *functional = TRUE;
      }
    }
  }

  return r;
}

Std_ReturnType Dcm_Transmit(const uint8_t *buffer, PduLengthType length, int functional) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();

  if ((DCM_BUFFER_IDLE == context->txBufferState) && (Dcm_Config.txBufferSize >= length)) {
    r = E_OK;
    if (functional) {
      context->curPduId = DCM_P2A_PDU;
    } else {
      context->curPduId = DCM_P2P_PDU;
    }
    memcpy(Dcm_Config.txBuffer, buffer, (size_t)length);
    context->TxTpSduLength = (PduLengthType)length;
    lTxRequested = TRUE;
    lRxResponce = FALSE;
    context->txBufferState = DCM_BUFFER_FULL;
  } else {
    ASLOG(ERROR, ("Dcm_Transmit FAILED\n"));
  }

  return r;
}

void Dcm_SessionChangeIndication(Dcm_SesCtrlType sesCtrlTypeActive,
                                 Dcm_SesCtrlType sesCtrlTypeNew) {
}

Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr) {
  return CanTp_Transmit(TxPduId, PduInfoPtr);
}