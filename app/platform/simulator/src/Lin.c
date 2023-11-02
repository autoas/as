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
#include "Lin_Lcfg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_LIN 0
#define AS_LOG_LINI 1
#define AS_LOG_LINE 2

#define LIN_BIT(v, pos) (((v) >> (pos)) & 0x01)

#define LIN_CONFIG (&Lin_Config)

#ifndef LIN_MAX_DATA_SIZE
#define LIN_MAX_DATA_SIZE 64
#endif

#define LIN_TYPE_INVALID ((uint8_t)'I')
#define LIN_TYPE_BREAK ((uint8_t)'B')
#define LIN_TYPE_SYNC ((uint8_t)'S')
#define LIN_TYPE_HEADER ((uint8_t)'H')
#define LIN_TYPE_DATA ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA ((uint8_t)'F')
#define LIN_TYPE_EXT_HEADER ((uint8_t)'h')
#define LIN_TYPE_EXT_HEADER_AND_DATA ((uint8_t)'f')
/* ================================ [ TYPES     ] ============================================== */
typedef enum {
  LIN_STATE_IDLE,
  LIN_STATE_ONLINE,
  LIN_STATE_ONLY_HEADER_TRANSMITTING,
  LIN_STATE_HEADER_TRANSMITTING,
  LIN_STATE_FULL_TRANSMITTING,
  LIN_STATE_WAITING_RESPONSE,
  LIN_STATE_ONLY_HEADER_TRANSMITTED,
  LIN_STATE_RESPONSE_TRANSMITTED,
  LIN_STATE_FULL_TRANSMITTED,
  LIN_STATE_RESPONSE_RECEIVED,
  LIN_STATE_WAITING_DATA,
  LIN_STATE_DATA_TRANSMITTING
} Lin_StateType;

typedef struct {
  Lin_FrameCsModelType Cs;
  uint8_t type;
  Lin_FramePidType pid;
  uint8_t dlc;
  uint8_t data[LIN_MAX_DATA_SIZE];
  uint8_t checksum;
} Lin_FrameType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
extern Lin_ConfigType Lin_Config;

static int linChannelToFdMap[LIN_CHANNEL_NUM];
static Lin_StateType linState[LIN_CHANNEL_NUM];
static Lin_FrameType linFrame[LIN_CHANNEL_NUM];
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
__attribute__((weak)) Std_ReturnType LinIf_HeaderIndication(NetworkHandleType Channel,
                                                            Lin_PduType *PduPtr) {
  return E_OK;
}
__attribute__((weak)) void LinIf_RxIndication(NetworkHandleType Channel, uint8 *Lin_SduPtr) {
}

void Lin_Init(const Lin_ConfigType *ConfigPtr) {
  int i;
  (void)ConfigPtr;

  for (i = 0; i < LIN_CHANNEL_NUM; i++) {
    linChannelToFdMap[i] = -1;
    linState[i] = LIN_STATE_IDLE;
  }
}

Std_ReturnType Lin_SetControllerMode(uint8_t Channel, Lin_ControllerStateType Transition) {
  Std_ReturnType ret = E_NOT_OK;
  Lin_ChannelConfigType *config;
  int fd;
  char option[64];

  if (Channel < LIN_CONFIG->numOfChannels) {
    config = &LIN_CONFIG->channelConfigs[Channel];
    switch (Transition) {
    case LIN_CS_STARTED:
      fd = linChannelToFdMap[Channel];
      if (fd < 0) {
        snprintf(option, sizeof(option), "%u", config->baudrate);
        fd = dev_open(config->device, option);
        if (fd >= 0) {
          linChannelToFdMap[Channel] = fd;
          linState[Channel] = LIN_STATE_ONLINE;
        } else {
          ASLOG(LINE, ("failed to open %s with error %d\n", config->device, fd));
        }
      } else {
        ASLOG(LINE, ("device %s already opened\n", config->device));
      }
      break;
    case LIN_CS_STOPPED:
    case LIN_CS_SLEEP:
      fd = linChannelToFdMap[Channel];
      if (fd >= 0) {
        dev_close(fd);
        linChannelToFdMap[Channel] = -1;
      } else {
        ASLOG(LINE, ("channel %d not opened\n", Channel));
      }
      break;
    default:
      break;
    }
  }

  return ret;
}

Std_ReturnType Lin_SendFrame(uint8_t Channel, const Lin_PduType *PduInfoPtr) {
  int r = -1, fd;
  Std_ReturnType ercd = E_OK;
  uint8_t data[LIN_MAX_DATA_SIZE + 8];
  int len = 0;
  Lin_StateType state;
  fd = linChannelToFdMap[Channel];
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
        linFrame[Channel].pid = PduInfoPtr->Pid;
        linFrame[Channel].dlc = PduInfoPtr->Dl;
        linFrame[Channel].Cs = PduInfoPtr->Cs;
        len = 6;
      } else {
        data[0] = LIN_TYPE_HEADER;
        data[1] = Lin_GetPid(PduInfoPtr);
        data[2] = PduInfoPtr->Dl;
        linFrame[Channel].pid = data[1];
        linFrame[Channel].dlc = PduInfoPtr->Dl;
        linFrame[Channel].Cs = PduInfoPtr->Cs;
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
        linState[Channel] = state;
      }
    }
  } else {
    ercd = E_NOT_OK;
  }
  return ercd;
}

Lin_StatusType Lin_GetStatus(uint8_t Channel, uint8_t **Lin_SduPtr) {
  Lin_StatusType status = LIN_NOT_OK;
  *Lin_SduPtr = NULL;
  switch (linState[Channel]) {
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
    *Lin_SduPtr = linFrame[Channel].data;
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
  for (i = 0; i < LIN_CHANNEL_NUM; i++) {
    fd = linChannelToFdMap[i];
    if (fd < 0)
      continue;
    size = dev_read(fd, data, sizeof(data));
    if ((2 == size) && (LIN_TYPE_HEADER == data[0])) {
      linFrame[i].type = LIN_TYPE_HEADER;
      linFrame[i].pid = data[1];
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
            linState[i] = LIN_STATE_DATA_TRANSMITTING;
          }
        } else if (LIN_FRAMERESPONSE_RX == linPdu.Drc) {
          linFrame[i].dlc = linPdu.Dl;
          linFrame[i].Cs = linPdu.Cs;
          linState[i] = LIN_STATE_WAITING_DATA;
        } else {
          /* ignore the response */
          linState[i] = LIN_STATE_ONLINE;
        }
      }
    } else if ((size > 2) && (LIN_TYPE_DATA == data[0])) {
      linFrame[i].type = LIN_TYPE_DATA;
      linFrame[i].dlc = size - 2;
      memcpy(linFrame[i].data, &data[1], size - 2);
      linFrame[i].checksum = data[size - 1];
      ret = Lin_IsFrameGood(&linFrame[i]);
      if (ret == E_OK) {
        if (linState[i] == LIN_STATE_ONLY_HEADER_TRANSMITTING) {
          /* slave to slave response */
          linState[i] = LIN_STATE_ONLY_HEADER_TRANSMITTED;
        } else if ((linState[i] == LIN_STATE_WAITING_RESPONSE) ||
                   (linState[i] == LIN_STATE_HEADER_TRANSMITTING)) {
          linState[i] = LIN_STATE_RESPONSE_RECEIVED;
        } else if (linState[i] == LIN_STATE_WAITING_DATA) {
          LinIf_RxIndication(i, linFrame[i].data);
        } else {
          ASLOG(LINE, ("frame received in state %d\n", linState[i]));
        }
      } else {
        ASLOG(LINE, ("invalid frame data received in state %d\n", linState[i]));
      }
    } else if ((LIN_TYPE_HEADER_AND_DATA == data[0]) && (size > 3)) {
      linFrame[i].type = LIN_TYPE_HEADER_AND_DATA;
      linFrame[i].pid = (uint8_t)data[1];
      linFrame[i].dlc = size - 3;
      memcpy(linFrame[i].data, &data[2], size - 3);
      linFrame[i].checksum = data[size - 1];
      linPdu.Pid = data[1] & 0x3F;
      ret = LinIf_HeaderIndication(i, &linPdu);
      if (E_OK == ret) {
        linFrame[i].Cs = linPdu.Cs;
        ret = Lin_IsFrameGood(&linFrame[i]);
        if (E_OK == ret) {
          LinIf_RxIndication(i, linFrame[i].data);
        }
      } else {
        ASLOG(LINE, ("invalid frame full received in state %d\n", linState[i]));
      }
    } else if ((5 == size) && (LIN_TYPE_EXT_HEADER == data[0])) {
      linFrame[i].type = LIN_TYPE_EXT_HEADER;
      linFrame[i].pid =
        ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
      linPdu.Pid = linFrame[i].pid;
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
            linState[i] = LIN_STATE_DATA_TRANSMITTING;
          }
        } else if (LIN_FRAMERESPONSE_RX == linPdu.Drc) {
          linFrame[i].dlc = linPdu.Dl;
          linFrame[i].Cs = linPdu.Cs;
          linState[i] = LIN_STATE_WAITING_DATA;
        } else {
          /* ignore the response */
          linState[i] = LIN_STATE_ONLINE;
        }
      }
    } else if ((LIN_TYPE_EXT_HEADER_AND_DATA == data[0]) && (size > 6)) {
      linFrame[i].type = LIN_TYPE_EXT_HEADER_AND_DATA;
      linFrame[i].pid =
        ((uint32_t)data[1] << 24) + ((uint32_t)data[2] << 16) + ((uint32_t)data[3] << 8) + data[4];
      linFrame[i].dlc = size - 6;
      memcpy(linFrame[i].data, &data[5], size - 6);
      linFrame[i].checksum = data[size - 1];
      linPdu.Pid = data[1];
      ret = LinIf_HeaderIndication(i, &linPdu);
      if (E_OK == ret) {
        linFrame[i].Cs = linPdu.Cs;
        ret = Lin_IsFrameGood(&linFrame[i]);
        if (E_OK == ret) {
          LinIf_RxIndication(i, linFrame[i].data);
        }
      } else {
        ASLOG(LINE, ("invalid frame full received in state %d\n", linState[i]));
      }
    } else {
      if (size > 0) {
        ASLOG(LINE, ("invalid frame received\n"));
      }
    }

    if (size > 0) {
      ASLOG(LIN, ("%d RX: %c %02X @ %u us\n", i, data[0], data[1], (uint32_t)Std_GetTime()));
    }
    switch (linState[i]) {
    case LIN_STATE_ONLY_HEADER_TRANSMITTING:
      linState[i] = LIN_STATE_ONLY_HEADER_TRANSMITTED;
      break;
    case LIN_STATE_HEADER_TRANSMITTING:
      linState[i] = LIN_STATE_WAITING_RESPONSE;
      break;
    case LIN_STATE_FULL_TRANSMITTING:
      linState[i] = LIN_STATE_FULL_TRANSMITTED;
      break;
    default:
      break;
    }
  }
}