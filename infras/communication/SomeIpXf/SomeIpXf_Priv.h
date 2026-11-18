/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
#ifndef SOMEIPXF_PRIV_H
#define SOMEIPXF_PRIV_H
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
  const char *Name;
  const SomeIpXf_StructDefinitionType *PStructDef; /* for struct or struct array */
  uint32_t DataSize;
  uint32_t DataOffset;
  uint32_t DataLenOffset; /* for array */
  uint32_t HasOffset;     /* for optional */
  uint16_t Tag;           /* @SWS_SomeIpXf_00268 */
  SomIpXf_DataElementTypeType DataType;
  uint8_t SizeOfDataLengthField;
} SomeIpXf_DataElementType;

struct SomeIpXf_StructDefinition_s {
  const char *Name;
  const SomeIpXf_DataElementType *DataElements;
  uint32_t StructSize;
  uint16_t NumOfDataElements;
  uint8_t SizeOfStructLengthField;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
#endif /* SOMEIPXF_PRIV_H */
