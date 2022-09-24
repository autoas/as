/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "GEN/Rte_Telltale.h"
#include "Dio.h"
/* ================================ [ MACROS    ] ============================================== */
#define RTE_WRITE_PORT_FOR_TELLTALE(name)                                                          \
  Std_ReturnType Rte_Write_Telltale_Telltale_##name##State(OnOff_T data) {                         \
    Std_ReturnType ercd = E_NOT_OK;                                                                \
    if (data < eTelltaleStatusMax) {                                                               \
      TelltaleStatus[eTelltale##name] = data;                                                      \
      ercd = E_OK;                                                                                 \
    }                                                                                              \
    return ercd;                                                                                   \
  }

#define TELLTALE_MGR_PERIOD 10
#define mMS2Ticks(t) (((t) + TELLTALE_MGR_PERIOD - 1) / TELLTALE_MGR_PERIOD)
/* ================================ [ TYPES     ] ============================================== */
enum
{
  eTelltaleTPMS = 0,
  eTelltaleLowOil,
  eTelltalePosLamp,
  eTelltaleTurnLeft,
  eTelltaleTurnRight,
  eTelltaleAutoCruise,
  eTelltaleHighBeam,
  eTelltaleSeatbeltDriver,
  eTelltaleSeatbeltPassenger,
  eTelltaleAirbag,
  eTelltaleMax
};

enum
{
  eTelltaleStatusOff = 0,
  eTelltaleStatusOn,
  eTelltaleStatus1Hz,
  eTelltaleStatus2Hz,
  eTelltaleStatus3Hz,
  eTelltaleStatusMax
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static uint16_t TellatleTimer[eTelltaleStatusMax];
static Dio_LevelType TellatleLevel[eTelltaleStatusMax];
static const Dio_ChannelType TellatleChannel[eTelltaleMax] = {
  0, /* TPMS */
  1, /* LowOil */
  2, /* PosLamp */
  3, /* TurnLeft */
  4, /* TurnRight */
  5, /* AutoCruise */
  6, /* HighBeam */
  7, /* SeatbeltDriver */
  8, /* SeatbeltPassenger */
  9, /* Airbag */
};

static const uint16_t TellatleHzCfg[eTelltaleStatusMax][2] = {
  /* { duty, period } */
  {mMS2Ticks(1000), mMS2Ticks(1000) - 1}, /* eTelltaleStatusOff */
  {mMS2Ticks(0), mMS2Ticks(0) - 1},       /* eTelltaleStatusOn */
  {mMS2Ticks(500), mMS2Ticks(1000) - 1},  /* eTelltaleStatus1Hz */
  {mMS2Ticks(250), mMS2Ticks(500) - 1},   /* eTelltaleStatus2Hz */
  {mMS2Ticks(167), mMS2Ticks(333) - 1},   /* eTelltaleStatus3Hz */
};
/* ================================ [ LOCALS    ] ============================================== */
static OnOff_T TelltaleStatus[eTelltaleMax];
/* ================================ [ FUNCTIONS ] ============================================== */
RTE_WRITE_PORT_FOR_TELLTALE(TPMS)
RTE_WRITE_PORT_FOR_TELLTALE(LowOil)
RTE_WRITE_PORT_FOR_TELLTALE(PosLamp)
RTE_WRITE_PORT_FOR_TELLTALE(TurnLeft)
RTE_WRITE_PORT_FOR_TELLTALE(TurnRight)
RTE_WRITE_PORT_FOR_TELLTALE(AutoCruise)
RTE_WRITE_PORT_FOR_TELLTALE(HighBeam)
RTE_WRITE_PORT_FOR_TELLTALE(SeatbeltDriver)
RTE_WRITE_PORT_FOR_TELLTALE(SeatbeltPassenger)
RTE_WRITE_PORT_FOR_TELLTALE(Airbag)

void Swc_TelltaleManager(void) {
  int i;
  for (i = 0; i < eTelltaleStatusMax; i++) {
    TellatleTimer[i]++;
    if (TellatleTimer[i] < TellatleHzCfg[i][0]) { /* in the low duty */
      TellatleLevel[i] = STD_LOW;                 /* off the Telltale */
    } else {
      TellatleLevel[i] = STD_HIGH;                  /* on the Telltale */
      if (TellatleTimer[i] > TellatleHzCfg[i][1]) { /* reach the period */
        TellatleTimer[i] = 0;
        TellatleLevel[i] = STD_LOW; /* off the Telltale */
      }
    }
  }
  /* refresh Telltale */
  for (i = 0; i < eTelltaleMax; i++) {
    Dio_WriteChannel(TellatleChannel[i], TellatleLevel[TelltaleStatus[i]]);
  }
}