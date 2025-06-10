/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* NOTE: this is a mess, just demo */
/* ================================ [ INCLUDES  ] ============================================== */
#include "Lin.h"
#include "LinIf.h"
#include "devlib.h"
#include "Std_Debug.h"
#include "Std_Timer.h"
#include "Lin_Cfg.h"
#include "Lin_Priv.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LIN 0
#define AS_LOG_LINI 1
#define AS_LOG_LINE 2

#define LIN_BIT(v, pos) (((v) >> (pos)) & 0x01)

#define LIN_CONFIG (&Lin_Config)

#define LIN_TYPE_INVALID ((uint8_t)'I')
#define LIN_TYPE_BREAK ((uint8_t)'B')
#define LIN_TYPE_SYNC ((uint8_t)'S')
#define LIN_TYPE_HEADER ((uint8_t)'H')
#define LIN_TYPE_DATA ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA ((uint8_t)'F')
#define LIN_TYPE_EXT_HEADER ((uint8_t)'h')
#define LIN_TYPE_EXT_HEADER_AND_DATA ((uint8_t)'f')
/* ================================ [ TYPES     ] ============================================== */

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
extern Lin_ConfigType Lin_Config;
/* ================================ [ LOCALS    ] ============================================== */
static Lin_FramePidType Lin_GetPid(const Lin_PduType *PduInfoPtr) {
  Lin_FramePidType id = PduInfoPtr->Pid;
  uint8_t pid;
  uint8_t p0;
  uint8_t p1;

  if (id > 0x3F) {
    /* extended id or already is pid */
    pid = id;
  } else {
    /* calculate the pid for standard LIN id case */
    pid = id & 0x3F;
    p0 = LIN_BIT(pid, 0) ^ LIN_BIT(pid, 1) ^ LIN_BIT(pid, 2) ^ LIN_BIT(pid, 4);
    p1 = ~(LIN_BIT(pid, 1) ^ LIN_BIT(pid, 3) ^ LIN_BIT(pid, 4) ^ LIN_BIT(pid, 5));
    pid = pid | (p0 << 6) | (p1 << 7);
  }

  /* for the LIN_USE_EXT_ID. keep the high 3 bytes unchanged */
  return (Lin_FramePidType)pid | (id & 0xFFFFFF00);
}

static Std_ReturnType Lin_CheckPid(Lin_FramePidType Pid_) {
  Std_ReturnType ret = E_OK;
  uint8_t pid = Pid_ & 0x3F;
  uint8_t p0 = LIN_BIT(pid, 0) ^ LIN_BIT(pid, 1) ^ LIN_BIT(pid, 2) ^ LIN_BIT(pid, 4);
  uint8_t p1 = ~(LIN_BIT(pid, 1) ^ LIN_BIT(pid, 3) ^ LIN_BIT(pid, 4) ^ LIN_BIT(pid, 5));
  pid = pid | (p0 << 6) | (p1 << 7);

  if (pid != (uint8_t)(Pid_ & 0xFF)) {
    ret = E_NOT_OK;
  }

  /* TODO: what for ID between (0x3F, 0xFF) */
  if (Pid_ > 0xFF) {
    ret = E_OK;
  }

  return ret;
}

static void Lin_GetData(const Lin_PduType *PduInfoPtr, uint8_t *data, uint8_t *p_checksum) {
  int i;
  uint8_t checksum = 0;
  Lin_FramePidType pid;

  if (LIN_ENHANCED_CS == PduInfoPtr->Cs) {
    pid = Lin_GetPid(PduInfoPtr);
    checksum += (pid >> 24) & 0xFF;
    checksum += (pid >> 16) & 0xFF;
    checksum += (pid >> 8) & 0xFF;
    checksum += pid & 0xFF;
  }

  for (i = 0; i < PduInfoPtr->Dl; i++) {
    data[i] = PduInfoPtr->SduPtr[i];
    checksum += PduInfoPtr->SduPtr[i];
  }

  *p_checksum = ~checksum;
}

Std_ReturnType Lin_IsFrameGood(const Lin_FrameType *frame) {
  Std_ReturnType ret;
  int i;
  uint8_t checksum = 0;

  ret = Lin_CheckPid(frame->pid);
  if (E_OK == ret) {
    if (LIN_ENHANCED_CS == frame->Cs) {
      checksum += (frame->pid >> 24) & 0xFF;
      checksum += (frame->pid >> 16) & 0xFF;
      checksum += (frame->pid >> 8) & 0xFF;
      checksum += frame->pid & 0xFF;
    }
    for (i = 0; i < frame->dlc; i++) {
      checksum += frame->data[i];
    }
    checksum = ~checksum;
    if (frame->checksum != checksum) {
      ASLOG(LINE, ("invalid Checksum\n"));
      ret = E_NOT_OK;
    }
  } else {
    ASLOG(LINE, ("invalid Pid\n"));
    ret = E_NOT_OK;
  }

  return ret;
}
/* ================================ [ FUNCTIONS ] ============================================== */
__weak Std_ReturnType LinIf_HeaderIndication(NetworkHandleType Channel, Lin_PduType *PduPtr) {
  return E_OK;
}
__weak void LinIf_RxIndication(NetworkHandleType Channel, uint8 *Lin_SduPtr) {
}

Std_ReturnType LinAc_Init(uint8_t Controller, const Lin_ChannelConfigType *config) {
  Std_ReturnType ret = E_NOT_OK;
  int fd;
  char option[64];
  Lin_CtrlAcContextType *context;

  context = &config->context->acCtx;
  if (context->fd < 0) {
    snprintf(option, sizeof(option), "%u", config->baudrate);
    fd = dev_open(config->device, option);
    if (fd >= 0) {
      context->fd = fd;
      context->state = LIN_STATE_ONLINE;
      ret = E_OK;
    } else {
      ASLOG(LINE, ("failed to open %s with error %d\n", config->device, fd));
    }
  } else {
    ASLOG(LINE, ("device %s already opened\n", config->device));
  }

  return ret;
}

Std_ReturnType LinAc_DeInit(uint8_t Controller, const Lin_ChannelConfigType *config) {
  Std_ReturnType ret = E_NOT_OK;
  int fd;
  Lin_CtrlAcContextType *context;
  context = &config->context->acCtx;

  fd = context->fd;
  if (fd >= 0) {
    dev_close(fd);
    context->fd = -1;
    ret = E_OK;
  } else {
    ASLOG(LINE, ("channel %d not opened\n", Controller));
  }

  return ret;
}

Std_ReturnType LinAc_SetSleepMode(uint8_t Controller, const Lin_ChannelConfigType *config) {
  return LinAc_DeInit(Controller, config);
}

Std_ReturnType LinAc_SetupPinMode(const Lin_CtrlPinType *pin) {
  return E_OK;
}

Std_ReturnType LinAc_WritePin(const Lin_CtrlPinType *pin, uint8_t value) {
  return E_OK;
}

Std_ReturnType Lin_SendFrame(uint8_t Channel, const Lin_PduType *PduInfoPtr) {
  int r = -1, fd;
  Std_ReturnType ercd = E_OK;
  uint8_t data[LIN_MAX_DATA_SIZE + 8];
  int len = 0;
  Lin_StateType state;
  const Lin_ChannelConfigType *config;
  Lin_CtrlAcContextType *context;

  config = &LIN_CONFIG->channelConfigs[Channel];
  context = &config->context->acCtx;
  fd = context->fd;
  if (fd >= 0) {
    if (LIN_FRAMERESPONSE_TX == PduInfoPtr->Drc) {
      if (PduInfoPtr->Pid > 0x3F) {
        data[0] = LIN_TYPE_EXT_HEADER_AND_DATA;
        data[1] = (PduInfoPtr->Pid >> 24) & 0xFF;
        data[2] = (PduInfoPtr->Pid >> 16) & 0xFF;
        data[3] = (PduInfoPtr->Pid >> 8) & 0xFF;
        data[4] = PduInfoPtr->Pid & 0xFF;
        Lin_GetData(PduInfoPtr, &data[5], &data[5 + PduInfoPtr->Dl]);
        len = 6 + PduInfoPtr->Dl;
      } else {
        data[0] = LIN_TYPE_HEADER_AND_DATA;
        data[1] = Lin_GetPid(PduInfoPtr);
        Lin_GetData(PduInfoPtr, &data[2], &data[2 + PduInfoPtr->Dl]);
        len = 3 + PduInfoPtr->Dl;
      }
      state = LIN_STATE_FULL_TRANSMITTING;
    } else if ((LIN_FRAMERESPONSE_RX == PduInfoPtr->Drc) ||
               (LIN_FRAMERESPONSE_IGNORE == PduInfoPtr->Drc)) {
      if (PduInfoPtr->Pid > 0x3F) {
        data[0] = LIN_TYPE_EXT_HEADER;
        data[1] = (PduInfoPtr->Pid >> 24) & 0xFF;
        data[2] = (PduInfoPtr->Pid >> 16) & 0xFF;
        data[3] = (PduInfoPtr->Pid >> 8) & 0xFF;
        data[4] = PduInfoPtr->Pid & 0xFF;
        data[5] = PduInfoPtr->Dl;
        context->frame.pid = PduInfoPtr->Pid;
        context->frame.dlc = PduInfoPtr->Dl;
        context->frame.Cs = PduInfoPtr->Cs;
        len = 6;
      } else {
        data[0] = LIN_TYPE_HEADER;
        data[1] = Lin_GetPid(PduInfoPtr);
        data[2] = PduInfoPtr->Dl;
        context->frame.pid = data[1];
        context->frame.dlc = PduInfoPtr->Dl;
        context->frame.Cs = PduInfoPtr->Cs;
        len = 3;
      }
      if (LIN_FRAMERESPONSE_RX == PduInfoPtr->Drc) {
        state = LIN_STATE_HEADER_TRANSMITTING;
      } else {
        state = LIN_STATE_ONLY_HEADER_TRANSMITTING;
      }
    } else {
      ercd = E_NOT_OK;
    }

    if (E_OK == ercd) {
      ASLOG(LIN, ("%d TX: %c %02X @ %u us\n", Channel, data[0], data[1], (uint32_t)Std_GetTime()));
      r = dev_write(fd, data, len);
      if (len != r) {
        ercd = E_NOT_OK;
      } else {
        context->state = state;
      }
    }
  } else {
    ercd = E_NOT_OK;
  }
  return ercd;
}

Lin_StatusType Lin_GetStatus(uint8_t Channel, uint8_t **Lin_SduPtr) {
  Lin_StatusType status = LIN_NOT_OK;
  const Lin_ChannelConfigType *config;
  Lin_CtrlAcContextType *context;

  config = &LIN_CONFIG->channelConfigs[Channel];
  context = &config->context->acCtx;
  *Lin_SduPtr = NULL;
  switch (context->state) {
  case LIN_STATE_ONLINE:
    status = LIN_OPERATIONAL;
    break;
  case LIN_STATE_HEADER_TRANSMITTING:
  case LIN_STATE_WAITING_RESPONSE:
    status = LIN_RX_NO_RESPONSE;
    break;
  case LIN_STATE_ONLY_HEADER_TRANSMITTING:
  case LIN_STATE_FULL_TRANSMITTING:
    status = LIN_TX_BUSY;
    break;
  case LIN_STATE_ONLY_HEADER_TRANSMITTED:
  case LIN_STATE_RESPONSE_TRANSMITTED:
  case LIN_STATE_FULL_TRANSMITTED:
    status = LIN_TX_OK;
    break;
  case LIN_STATE_RESPONSE_RECEIVED:
    *Lin_SduPtr = context->frame.data;
    status = LIN_RX_OK;
    break;
  default:
    break;
  }
  return status;
}

void Lin_MainFunction(void) {
}

void Lin_MainFunction_Read(void) {
  int i;
  int fd;
  int r;
  Std_ReturnType ret;
  uint8_t data[LIN_MAX_DATA_SIZE + 4];
  Lin_PduType linPdu;
  size_t size;
  const Lin_ChannelConfigType *config;
  Lin_CtrlAcContextType *context;

  for (i = 0; i < LIN_CHANNEL_NUM; i++) {
    config = &LIN_CONFIG->channelConfigs[i];
    context = &config->context->acCtx;
    fd = context->fd;
    if (fd < 0)
      continue;
    size = dev_read(fd, data, sizeof(data));
    if ((2 == size) && (LIN_TYPE_HEADER == data[0])) {
      context->frame.type = LIN_TYPE_HEADER;
      context->frame.pid = data[1];
      linPdu.Pid = data[1] & 0x3F;
      linPdu.Dl = 0;
      linPdu.Drc = LIN_FRAMERESPONSE_IGNORE;
      ret = LinIf_HeaderIndication(i, &linPdu);
      if (E_OK == ret) {
        if (LIN_FRAMERESPONSE_TX == linPdu.Drc) {
          data[0] = LIN_TYPE_DATA;
          Lin_GetData(&linPdu, &data[1], &data[1 + linPdu.Dl]);
          r = dev_write(fd, data, linPdu.Dl + 2);
          if ((linPdu.Dl + 2) != r) {
            ASLOG(LINE, ("failed to slave response %x\n", linPdu.Pid));
          } else {
            context->state = LIN_STATE_DATA_TRANSMITTING;
          }
        } else if (LIN_FRAMERESPONSE_RX == linPdu.Drc) {
          context->frame.dlc = linPdu.Dl;
          context->frame.Cs = linPdu.Cs;
          context->state = LIN_STATE_WAITING_DATA;
        } else {
          /* ignore the response */
          context->state = LIN_STATE_ONLINE;
        }
      }
    } else if ((size > 2) && (LIN_TYPE_DATA == data[0])) {
      context->frame.type = LIN_TYPE_DATA;
      context->frame.dlc = size - 2;
      memcpy(context->frame.data, &data[1], size - 2);
      context->frame.checksum = data[size - 1];
      ret = Lin_IsFrameGood(&context->frame);
      if (ret == E_OK) {
        if (context->state == LIN_STATE_ONLY_HEADER_TRANSMITTING) {
          /* slave to slave response */
          context->state = LIN_STATE_ONLY_HEADER_TRANSMITTED;
        } else if ((context->state == LIN_STATE_WAITING_RESPONSE) ||
                   (context->state == LIN_STATE_HEADER_TRANSMITTING)) {
          context->state = LIN_STATE_RESPONSE_RECEIVED;
        } else if (context->state == LIN_STATE_WAITING_DATA) {
          LinIf_RxIndication(i, context->frame.data);
        } else {
          ASLOG(LINE, ("frame received in state %d\n", context->state));
        }
      } else {
        ASLOG(LINE, ("invalid frame data received in state %d\n", context->state));
      }
    } else if ((LIN_TYPE_HEADER_AND_DATA == data[0]) && (size > 3)) {
      context->frame.type = LIN_TYPE_HEADER_AND_DATA;
      context->frame.pid = (uint8_t)data[1];
      context->frame.dlc = size - 3;
      memcpy(context->frame.data, &data[2], size - 3);
      context->frame.checksum = data[size - 1];
      linPdu.Pid = data[1] & 0x3F;
      ret = LinIf_HeaderIndication(i, &linPdu);
      if (E_OK == ret) {
        context->frame.Cs = linPdu.Cs;
        ret = Lin_IsFrameGood(&context->frame);
        if (E_OK == ret) {
          LinIf_RxIndication(i, context->frame.data);
        }
      } else {
        ASLOG(LINE, ("invalid frame full received in state %d\n", context->state));
      }
    } else if ((5 == size) && (LIN_TYPE_EXT_HEADER == data[0])) {
      context->frame.type = LIN_TYPE_EXT_HEADER;
      context->frame.pid =
        ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
      linPdu.Pid = context->frame.pid;
      linPdu.Dl = 0;
      linPdu.Drc = LIN_FRAMERESPONSE_IGNORE;
      ret = LinIf_HeaderIndication(i, &linPdu);
      if (E_OK == ret) {
        if (LIN_FRAMERESPONSE_TX == linPdu.Drc) {
          data[0] = LIN_TYPE_DATA;
          Lin_GetData(&linPdu, &data[1], &data[1 + linPdu.Dl]);
          r = dev_write(fd, data, linPdu.Dl + 2);
          if ((linPdu.Dl + 2) != r) {
            ASLOG(LINE, ("failed to slave response %x\n", linPdu.Pid));
          } else {
            context->state = LIN_STATE_DATA_TRANSMITTING;
          }
        } else if (LIN_FRAMERESPONSE_RX == linPdu.Drc) {
          context->frame.dlc = linPdu.Dl;
          context->frame.Cs = linPdu.Cs;
          context->state = LIN_STATE_WAITING_DATA;
        } else {
          /* ignore the response */
          context->state = LIN_STATE_ONLINE;
        }
      }
    } else if ((LIN_TYPE_EXT_HEADER_AND_DATA == data[0]) && (size > 6)) {
      context->frame.type = LIN_TYPE_EXT_HEADER_AND_DATA;
      context->frame.pid =
        ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
      context->frame.dlc = size - 6;
      memcpy(context->frame.data, &data[5], size - 6);
      context->frame.checksum = data[size - 1];
      linPdu.Pid = data[1];
      ret = LinIf_HeaderIndication(i, &linPdu);
      if (E_OK == ret) {
        context->frame.Cs = linPdu.Cs;
        ret = Lin_IsFrameGood(&context->frame);
        if (E_OK == ret) {
          LinIf_RxIndication(i, context->frame.data);
        }
      } else {
        ASLOG(LINE, ("invalid frame full received in state %d\n", context->state));
      }
    } else {
      if (size > 0) {
        ASLOG(LINE, ("invalid frame received\n"));
      }
    }

    if (size > 0) {
      ASLOG(LIN, ("%d RX: %c %02X @ %u us\n", i, data[0], data[1], (uint32_t)Std_GetTime()));
    }
    switch (context->state) {
    case LIN_STATE_ONLY_HEADER_TRANSMITTING:
      context->state = LIN_STATE_ONLY_HEADER_TRANSMITTED;
      break;
    case LIN_STATE_HEADER_TRANSMITTING:
      context->state = LIN_STATE_WAITING_RESPONSE;
      break;
    case LIN_STATE_FULL_TRANSMITTING:
      context->state = LIN_STATE_FULL_TRANSMITTED;
      break;
    default:
      break;
    }
  }
}
