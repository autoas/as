/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "J1939Tp_Priv.h"
#include "J1939Tp.h"
#include "J1939Tp_Cfg.h"
#include "CanIf_Cfg.h"
/* ================================ [ MACROS    ] ============================================== */
#define J1939TP_NUM_PDU_PER_CHANNEL 4
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
extern void CanIf_J1939TpReconfig(uint8_t Channel, J1939Tp_ParamType *params);
extern void PduR_J1939TpReConfig(uint8_t Channel);
/* ================================ [ DATAS     ] ============================================== */
static J1939Tp_RxChannelContextType J1939Tp_RxChannelContexts[J1939TP_MAX_CHANNELS];
static uint8_t u8RxData[J1939TP_MAX_CHANNELS][64];
static J1939Tp_RxChannelType J1939Tp_RxChannels[J1939TP_MAX_CHANNELS];
static J1939Tp_TxChannelContextType J1939Tp_TxChannelContexts[J1939TP_MAX_CHANNELS];
static uint8_t u8TxData[J1939TP_MAX_CHANNELS][64];
static J1939Tp_TxChannelType J1939Tp_TxChannels[J1939TP_MAX_CHANNELS];

static J1939Tp_RxPduInfoType
  J1939Tp_RxPduInfos_C[J1939TP_MAX_CHANNELS * J1939TP_NUM_PDU_PER_CHANNEL];
static J1939Tp_TxPduInfoType
  J1939Tp_TxPduInfos_C[J1939TP_MAX_CHANNELS * J1939TP_NUM_PDU_PER_CHANNEL];

static J1939Tp_PduInfoType J1939Tp_RxPduInfos[J1939TP_MAX_CHANNELS * J1939TP_NUM_PDU_PER_CHANNEL];
static J1939Tp_PduInfoType J1939Tp_TxPduInfos[J1939TP_MAX_CHANNELS * J1939TP_NUM_PDU_PER_CHANNEL];
const J1939Tp_ConfigType J1939Tp_Config = {
  J1939Tp_RxChannels,
  J1939Tp_TxChannels,
  J1939Tp_RxPduInfos,
  J1939Tp_TxPduInfos,
  ARRAY_SIZE(J1939Tp_RxChannels),
  ARRAY_SIZE(J1939Tp_TxChannels),
  ARRAY_SIZE(J1939Tp_RxPduInfos),
  ARRAY_SIZE(J1939Tp_TxPduInfos),
};
/* ================================ [ LOCALS    ] ============================================== */
INITIALIZER(_isotp_j1939tp_init) {
  int i;
  for (i = 0; i < J1939TP_MAX_CHANNELS; i++) {
    J1939Tp_TxChannels[i].context = &J1939Tp_TxChannelContexts[i];
    J1939Tp_TxChannels[i].data = u8TxData[i];
    J1939Tp_TxChannels[i].TxPgPGN = 0;
    J1939Tp_TxChannels[i].PduR_TxPduId = i;
    J1939Tp_TxChannels[i].CanIf_TxCmNPdu = CANTP_MAX_CHANNELS + 3 * i + 0;
    J1939Tp_TxChannels[i].CanIf_TxDtNPdu = CANTP_MAX_CHANNELS + 3 * i + 1;
    J1939Tp_TxChannels[i].CanIf_TxDirectNPdu = CANTP_MAX_CHANNELS + 3 * i + 2;
    J1939Tp_TxChannels[i].STMin = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(50);
    J1939Tp_TxChannels[i].Tr = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(200);
    J1939Tp_TxChannels[i].T1 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(750);
    J1939Tp_TxChannels[i].T2 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(1250);
    J1939Tp_TxChannels[i].T3 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(1250);
    J1939Tp_TxChannels[i].T4 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(1050);
    J1939Tp_TxChannels[i].TxProtocol = J1939TP_PROTOCOL_CMDT;
    J1939Tp_TxChannels[i].TxMaxPacketsPerBlock = 8;
    J1939Tp_TxChannels[i].LL_DL = 8;
    J1939Tp_TxChannels[i].S = 0;
    J1939Tp_TxChannels[i].ADType = 0;
    J1939Tp_TxChannels[i].padding = 0x55;
  }

  for (i = 0; i < J1939TP_MAX_CHANNELS; i++) {
    J1939Tp_RxChannels[i].context = &J1939Tp_RxChannelContexts[i];
    J1939Tp_RxChannels[i].data = u8RxData[i];
    J1939Tp_RxChannels[i].PduR_RxPduId = i;
    J1939Tp_RxChannels[i].CanIf_TxCmNPdu = CANTP_MAX_CHANNELS + 3 * i + 3;
    J1939Tp_RxChannels[i].Tr = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(200);
    J1939Tp_RxChannels[i].T1 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(750);
    J1939Tp_RxChannels[i].T2 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(1250);
    J1939Tp_RxChannels[i].T3 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(1250);
    J1939Tp_RxChannels[i].T4 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(1050);
    J1939Tp_RxChannels[i].RxProtocol = J1939TP_PROTOCOL_CMDT;
    J1939Tp_RxChannels[i].RxPacketsPerBlock = 8;
    J1939Tp_RxChannels[i].LL_DL = 8;
    J1939Tp_RxChannels[i].S = 0;
    J1939Tp_RxChannels[i].ADType = 0;
    J1939Tp_RxChannels[i].padding = 0x55;
  }

  for (i = 0; i < J1939TP_MAX_CHANNELS; i++) {
    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i].ChannelType = J1939TP_TX_CHANNEL;
    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i].TxPduInfo =
      &J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i];
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i].PacketType = J1939TP_PACKET_CM;
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i].TxChannel = i;

    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].ChannelType = J1939TP_TX_CHANNEL;
    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].TxPduInfo =
      &J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 1];
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].PacketType = J1939TP_PACKET_DT;
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].TxChannel = i;

    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].ChannelType = J1939TP_TX_CHANNEL;
    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].TxPduInfo =
      &J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 2];
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].PacketType = J1939TP_PACKET_DIRECT;
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].TxChannel = i;

    /* For a Tx Channel, it needs a CM_CTS RX */
    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].ChannelType = J1939TP_TX_CHANNEL;
    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].TxPduInfo =
      &J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 3];
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].PacketType = J1939TP_PACKET_CM;
    J1939Tp_TxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].TxChannel = i;
  }

  for (i = 0; i < J1939TP_MAX_CHANNELS; i++) {
    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i].ChannelType = J1939TP_RX_CHANNEL;
    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i].RxPduInfo =
      &J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i];
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i].PacketType = J1939TP_PACKET_CM;
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i].RxChannel = i;

    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].ChannelType = J1939TP_RX_CHANNEL;
    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].RxPduInfo =
      &J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 1];
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].PacketType = J1939TP_PACKET_DT;
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 1].RxChannel = i;

    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].ChannelType = J1939TP_RX_CHANNEL;
    J1939Tp_RxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].RxPduInfo =
      &J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 2];
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].PacketType = J1939TP_PACKET_DIRECT;
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 2].RxChannel = i;

    /* For a Rx Channel, it needs a CM_CTS TX */
    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].ChannelType = J1939TP_RX_CHANNEL;
    J1939Tp_TxPduInfos[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].RxPduInfo =
      &J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 3];
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].PacketType = J1939TP_PACKET_CM;
    J1939Tp_RxPduInfos_C[J1939TP_NUM_PDU_PER_CHANNEL * i + 3].RxChannel = i;
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
void J1939Tp_ReConfig(uint8_t Channel, J1939Tp_ParamType *params) {
  if (Channel < J1939TP_MAX_CHANNELS) {
    J1939Tp_TxChannels[Channel].STMin = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->STMin);
    J1939Tp_TxChannels[Channel].Tr = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->Tr);
    J1939Tp_TxChannels[Channel].T1 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T1);
    J1939Tp_TxChannels[Channel].T2 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T2);
    J1939Tp_TxChannels[Channel].T3 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T3);
    J1939Tp_TxChannels[Channel].T4 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T4);
    J1939Tp_TxChannels[Channel].TxProtocol = params->protocol;
    J1939Tp_TxChannels[Channel].TxMaxPacketsPerBlock = params->TxMaxPacketsPerBlock;
    J1939Tp_TxChannels[Channel].LL_DL = params->ll_dl;

    J1939Tp_RxChannels[Channel].Tr = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->Tr);
    J1939Tp_RxChannels[Channel].T1 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T1);
    J1939Tp_RxChannels[Channel].T2 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T2);
    J1939Tp_RxChannels[Channel].T3 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T3);
    J1939Tp_RxChannels[Channel].T4 = J1939TP_CONVERT_MS_TO_MAIN_CYCLES(params->T4);
    J1939Tp_RxChannels[Channel].RxProtocol = params->protocol;
    J1939Tp_RxChannels[Channel].RxPacketsPerBlock = params->TxMaxPacketsPerBlock;
    J1939Tp_RxChannels[Channel].LL_DL = params->ll_dl;

    PduR_J1939TpReConfig(Channel);
    CanIf_J1939TpReconfig(Channel, params);
  }
}
