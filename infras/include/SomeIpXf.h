/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
#ifndef _SOMEIP_XF_H_
#define _SOMEIP_XF_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "Std_Types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
#define E_SER_GENERIC_ERROR ((Std_ReturnType)0x81)
#define E_NO_DATA ((Std_ReturnType)0x01)
#define E_SER_WRONG_PROTOCOL_VERSION ((Std_ReturnType)0x87)
#define E_SER_WRONG_PROTOCOL_VERSION ((Std_ReturnType)0x87)
#define E_SER_WRONG_INTERFACE_VERSION ((Std_ReturnType)0x88)
#define E_SER_MALFORMED_MESSAGE ((Std_ReturnType)0x89)
#define E_SER_WRONG_MESSAGE_TYPE ((Std_ReturnType)0x8a)
/* ================================ [ TYPES     ] ============================================== */
typedef struct SomeIpXf_StructDefinition_s SomeIpXf_StructDefinitionType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
/* @SWS_SomeIpXf_00138: support of encoding of basic types */
/* @SWS_SomeIpXf_00144: support of decoding of basic types */
/* byte types: uint8_t sint8_t char boolean */
int32_t SomeIpXf_EncodeByte(uint8_t *buffer, uint32_t bufferSize, uint8_t data);
int32_t SomeIpXf_DecodeByte(const uint8_t *buffer, uint32_t bufferSize, uint8_t *data);

/* short types: uint16_t sint16_t */
int32_t SomeIpXf_EncodeShort(uint8_t *buffer, uint32_t bufferSize, uint16_t data);
int32_t SomeIpXf_DecodeShort(const uint8_t *buffer, uint32_t bufferSize, uint16_t *data);

/* long types: uint32_t sint32_t float */
int32_t SomeIpXf_EncodeLong(uint8_t *buffer, uint32_t bufferSize, uint32_t data);
int32_t SomeIpXf_DecodeLong(const uint8_t *buffer, uint32_t bufferSize, uint32_t *data);

/* long long types: uint64_t sint64_t double */
int32_t SomeIpXf_EncodeLongLong(uint8_t *buffer, uint32_t bufferSize, uint64_t data);
int32_t SomeIpXf_DecodeLongLong(const uint8_t *buffer, uint32_t bufferSize, uint64_t *data);

int32_t SomeIpXf_EncodeStruct(uint8_t *buffer, uint32_t bufferSize, const void *pStruct,
                              const SomeIpXf_StructDefinitionType *structDef);
int32_t SomeIpXf_DecodeStruct(const uint8_t *buffer, uint32_t structSize, void *pStruct,
                              const SomeIpXf_StructDefinitionType *structDef);

int32_t SomeIpXf_EncodeByteArray(uint8_t *buffer, uint32_t bufferSize, const uint8_t *data,
                                 uint32_t length);
int32_t SomeIpXf_DecodeByteArray(const uint8_t *buffer, uint32_t bufferSize, uint8_t *data,
                                 uint32_t length);

/* short types: uint16_t sint16_t */
int32_t SomeIpXf_EncodeShortArray(uint8_t *buffer, uint32_t bufferSize, const uint16_t *data,
                                  uint32_t length);
int32_t SomeIpXf_DecodeShortArray(const uint8_t *buffer, uint32_t bufferSize, uint16_t *data,
                                  uint32_t length);

/* long types: uint32_t sint32_t float */
int32_t SomeIpXf_EncodeLongArray(uint8_t *buffer, uint32_t bufferSize, const uint32_t *data,
                                 uint32_t length);
int32_t SomeIpXf_DecodeLongArray(const uint8_t *buffer, uint32_t bufferSize, uint32_t *data,
                                 uint32_t length);

/* long long types: uint64_t sint64_t double */
int32_t SomeIpXf_EncodeLongLongArray(uint8_t *buffer, uint32_t bufferSize, const uint64_t *data,
                                     uint32_t length);
int32_t SomeIpXf_DecodeLongLongArray(const uint8_t *buffer, uint32_t bufferSize, uint64_t *data,
                                     uint32_t length);

int32_t SomeIpXf_EncodeStructArray(uint8_t *buffer, uint32_t bufferSize, const void *pStruct,
                                   const SomeIpXf_StructDefinitionType *pStructDef,
                                   uint32_t length);
int32_t SomeIpXf_DecodeStructArray(const uint8_t *buffer, uint32_t bufferSize, void *pStruct,
                                   const SomeIpXf_StructDefinitionType *pStructDef,
                                   uint32_t *length);
#ifdef __cplusplus
}
#endif
#endif /* _SOMEIP_XF_H_ */
