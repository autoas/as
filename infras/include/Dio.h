/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of DIO Driver AUTOSAR CP Release 4.4.0
 */
#ifndef _DIO_H_
#define _DIO_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* @SWS_Dio_00182 */
typedef uint16_t Dio_ChannelType;

/* @SWS_Dio_00183 */
typedef uint32_t Dio_PortType;

/* @SWS_Dio_00184 */
typedef struct {
  uint32_t mask;
  uint8_t offset;
  Dio_PortType port;
} Dio_ChannelGroupType;

/* @SWS_Dio_00185 */
typedef uint8_t Dio_LevelType;

/* @SWS_Dio_00186 */
typedef uint32_t Dio_PortLevelType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_Dio_00133 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId);

/* @SWS_Dio_00134 */
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);

/* @SWS_Dio_00135 */
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);

/* @SWS_Dio_00136 */
void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);
/* @SWS_Dio_00137 */
Dio_PortLevelType Dio_ReadChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr);
/* @SWS_Dio_00138 */
void Dio_WriteChannelGroup(const Dio_ChannelGroupType *ChannelGroupIdPtr, Dio_PortLevelType Level);
#endif /* _DIO_H_ */
