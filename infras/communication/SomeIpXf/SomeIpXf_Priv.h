/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
#ifndef _SOMEIP_XF_PRIV_H_
#define _SOMEIP_XF_PRIV_H_
/* ================================ [ INCLUDES  ] ============================================== */
#include "SomeIpXf.h"
/* ================================ [ MACROS    ] ============================================== */
#define SOMEIPXF_DATA_ELEMENT_TYPE_BYTE ((SomIpXf_DataElementTypeType)0x00)
#define SOMEIPXF_DATA_ELEMENT_TYPE_SHORT ((SomIpXf_DataElementTypeType)0x01)
#define SOMEIPXF_DATA_ELEMENT_TYPE_LONG ((SomIpXf_DataElementTypeType)0x02)
#define SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG ((SomIpXf_DataElementTypeType)0x03)
#define SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT ((SomIpXf_DataElementTypeType)0x04)
#define SOMEIPXF_DATA_ELEMENT_TYPE_BYTE_ARRAY ((SomIpXf_DataElementTypeType)0x05)
#define SOMEIPXF_DATA_ELEMENT_TYPE_SHORT_ARRAY ((SomIpXf_DataElementTypeType)0x06)
#define SOMEIPXF_DATA_ELEMENT_TYPE_LONG_ARRAY ((SomIpXf_DataElementTypeType)0x07)
#define SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG_ARRAY ((SomIpXf_DataElementTypeType)0x08)
#define SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT_ARRAY ((SomIpXf_DataElementTypeType)0x09)

#define SOMEIPXF_TAG_NOT_USED ((uint16_t)0xFFFF)
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t SomIpXf_DataElementTypeType;

/* For this implementataion, the dataSize or structSize must be smaller than UINT32_MAX/2,
 * That means this SomeIpXf support data serialized size maximum to 2GB */
typedef struct {
  const char *name;
  const SomeIpXf_StructDefinitionType *pStructDef; /* for struct or struct array */
  uint32_t dataSize;
  uint32_t dataOffset;
  uint32_t dataLenOffset; /* for array */
  uint32_t hasOffset;     /* for optional */
  uint16_t tag;           /* @SWS_SomeIpXf_00268 */
  SomIpXf_DataElementTypeType dataType;
  uint8_t sizeOfDataLengthField;
} SomeIpXf_DataElementType;

typedef struct SomeIpXf_StructDefinition_s {
  const char *name;
  const SomeIpXf_DataElementType *dataElements;
  uint32_t structSize;
  uint16_t numOfDataElements;
  uint8_t sizeOfStructLengthField;
} SomeIpXf_StructDefinitionType;

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* _SOMEIP_XF_PRIV_H_ */
