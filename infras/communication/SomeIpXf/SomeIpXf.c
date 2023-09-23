/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of SOME/IP Transformer AUTOSAR CP Release 4.4.0
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "SomeIpXf_Priv.h"
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_SOMEIPXF 0
#define AS_LOG_SOMEIPXFE 2
/* ================================ [ TYPES     ] ============================================== */
#define STRUCT_PTR(dtype, pStruct, offset) ((dtype *)(((uint8_t *)(pStruct)) + offset))
#define STRUCT_VAL(dtype, pStruct, offset) (*STRUCT_PTR(dtype, pStruct, offset))
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
#if AS_LOG_SOMEIPXF > 0 || defined(linux) || defined(_WIN32)
static const char *lDataTypeName[] = {
  "byte",       "short",       "long",       "long long",       "struct",
  "byte array", "short array", "long array", "long long array", "struct array",
};
#endif
/* ================================ [ LOCALS    ] ============================================== */
#if AS_LOG_SOMEIPXF > 0 || defined(linux) || defined(_WIN32)
const char *getDataTypeName(SomIpXf_DataElementTypeType dataType) {
  const char *name = "unknown";
  if (dataType < ARRAY_SIZE(lDataTypeName)) {
    name = lDataTypeName[dataType];
  }
  return name;
}
#endif
static uint32_t SomeIpXf_GetDataLength(const void *pStruct,
                                       const SomeIpXf_DataElementType *dataElement) {
  uint32_t length = dataElement->dataSize;

  switch (dataElement->dataType) {
  case SOMEIPXF_DATA_ELEMENT_TYPE_BYTE_ARRAY:
    break;
  case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT_ARRAY:
    length /= sizeof(uint16_t);
    break;
  case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_ARRAY:
    length /= sizeof(uint32_t);
    break;
  case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG_ARRAY:
    length /= sizeof(uint64_t);
    break;
  case SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT_ARRAY:
    length /= dataElement->pStructDef->structSize;
    break;
  default:
    length = 1;
    break;
  }

  if (0 != dataElement->dataLenOffset) {
    if (length < 256) {
      length = (uint32_t)STRUCT_VAL(uint8_t, pStruct, dataElement->dataLenOffset);
    } else if (length < 65536) {
      length = (uint32_t)STRUCT_VAL(uint16_t, pStruct, dataElement->dataLenOffset);
    } else {
      length = STRUCT_VAL(uint32_t, pStruct, dataElement->dataLenOffset);
    }
  }

  asAssert(length < (UINT32_MAX / 2));

  return length;
}

static int32_t SomeIpXf_DecodeLength(const uint8_t *buffer, uint8_t sizeOfStructLengthField,
                                     uint32_t *length) {

  if (1 == sizeOfStructLengthField) {
    *length = buffer[0];
  } else if (2 == sizeOfStructLengthField) {
    *length = ((uint32_t)buffer[0] << 8) + buffer[1];
  } else if (4 == sizeOfStructLengthField) {
    *length = ((uint32_t)buffer[0] << 24) + ((uint32_t)buffer[1] << 16) +
              ((uint32_t)buffer[2] << 8) + buffer[3];
  } else {
  }

  return sizeOfStructLengthField;
}

static boolean SomeIpXf_HasThisField(const void *pStruct,
                                     const SomeIpXf_DataElementType *dataElement) {
  boolean hasIt;

  if (0 != dataElement->hasOffset) {
    hasIt = STRUCT_VAL(boolean, pStruct, dataElement->hasOffset);
  } else {
    hasIt = TRUE;
  }

  return hasIt;
}

static uint16_t SomeIpXf_GetTag(const SomeIpXf_DataElementType *dataElement) {
  uint16_t tag = 0;

  switch (dataElement->sizeOfDataLengthField) {
  case 1:
    tag = 0x5000;
    break;
  case 2:
    tag = 0x6000;
    break;
  case 4:
    tag = 0x7000;
    break;
  default:
    /* config generator will ensure sizeOfDataLengthField is 0 for basic types */
    switch (dataElement->dataType) {
    case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT:
      tag = 0x1000;
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG:
      tag = 0x2000;
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG:
      tag = 0x3000;
      break;
    default:
      break;
    }
    break;
  }
  tag = tag | (dataElement->tag & 0xFFF);

  return tag;
}

static boolean SomeIpXf_IsTagMatched(const SomeIpXf_DataElementType *dataElement, uint16_t tag,
                                     uint8_t *sizeOfDataLengthField) {
  boolean matched = TRUE;

  if ((tag & 0xFFF) == dataElement->tag) {
    if (dataElement->sizeOfDataLengthField) {
      if (((tag & 0xF000) == 0x4000) || ((tag & 0xF000) == 0x5000)) {
        *sizeOfDataLengthField = 1;
      } else if ((tag & 0xF000) == 0x6000) {
        *sizeOfDataLengthField = 2;
      } else if ((tag & 0xF000) == 0x7000) {
        *sizeOfDataLengthField = 4;
      } else {
        matched = FALSE;
      }
    } else {
      switch (dataElement->dataType) {
      case SOMEIPXF_DATA_ELEMENT_TYPE_BYTE:
        if ((tag & 0xF000) != 0x0000) {
          matched = FALSE;
        }
        break;
      case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT:
        if ((tag & 0xF000) != 0x1000) {
          matched = FALSE;
        }
        break;
      case SOMEIPXF_DATA_ELEMENT_TYPE_LONG:
        if ((tag & 0xF000) != 0x2000) {
          matched = FALSE;
        }
        break;
      case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG:
        if ((tag & 0xF000) != 0x3000) {
          matched = FALSE;
        }
        break;
      default:
        matched = FALSE;
        break;
      }
    }
  } else {
    matched = FALSE;
  }

  return matched;
}

static int32_t SomeIpXf_EncodeTagAndLengthIfHave(uint8_t *buffer, uint32_t bufferSize,
                                                 const SomeIpXf_DataElementType *dataElement) {
  int32_t offset = 0;
  uint16_t tag = 0;

  if (SOMEIPXF_TAG_NOT_USED != dataElement->tag) {
    tag = SomeIpXf_GetTag(dataElement);
    if (bufferSize >= 2) {
      buffer[0] = tag >> 8;
      buffer[1] = tag & 0xFF;
    }
    offset += 2;
    offset += dataElement->sizeOfDataLengthField;
  } else if (0 != dataElement->dataLenOffset) {
    offset += dataElement->sizeOfDataLengthField;
  } else {
    /* pass */
  }

  if ((uint32_t)offset > bufferSize) {
    ASLOG(SOMEIPXFE, ("no space for tag and lenght field\n"));
    offset = -E_NO_DATA;
  }

  return offset;
}

static void SomeIpXf_SetLenghtIfhave(void *pStruct, const SomeIpXf_DataElementType *dataElement,
                                     uint32_t length) {
  uint32_t max = 0;
  if (0 != dataElement->dataLenOffset) {
    switch (dataElement->dataType) {
    case SOMEIPXF_DATA_ELEMENT_TYPE_BYTE_ARRAY:
      max = dataElement->dataSize;
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT_ARRAY:
      max = dataElement->dataSize / sizeof(uint16_t);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_ARRAY:
      max = dataElement->dataSize / sizeof(uint32_t);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG_ARRAY:
      max = dataElement->dataSize / sizeof(uint64_t);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT_ARRAY:
      max = dataElement->dataSize / dataElement->pStructDef->structSize;
      break;
    default:
      break;
    }

    if (max > 0) {
      if (max < 256) {
        STRUCT_VAL(uint8_t, pStruct, dataElement->dataLenOffset) = (uint8_t)length;
      } else if (max < 65536) {
        STRUCT_VAL(uint16_t, pStruct, dataElement->dataLenOffset) = (uint16_t)length;
      } else {
        STRUCT_VAL(uint32_t, pStruct, dataElement->dataLenOffset) = length;
      }
    }
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int32_t SomeIpXf_EncodeByte(uint8_t *buffer, uint32_t bufferSize, uint8_t data) {
  int32_t offset = 1;

  if (bufferSize >= 1) {
    ASLOG(SOMEIPXF, ("encode byte=%02X to %p\n", data, buffer));
    buffer[0] = data;
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_DecodeByte(const uint8_t *buffer, uint32_t bufferSize, uint8_t *data) {
  int32_t offset = 1;

  if (bufferSize >= 1) {
    data[0] = buffer[0];
    ASLOG(SOMEIPXF, ("decode byte=%02X from %p\n", data[0], buffer));
  } else {
    offset = -E_NO_DATA;
  }

  return offset;
}

int32_t SomeIpXf_EncodeShort(uint8_t *buffer, uint32_t bufferSize, uint16_t data) {
  int32_t offset = 2;

  if (bufferSize >= 2) {
    ASLOG(SOMEIPXF, ("encode short=%04X to %p\n", data, buffer));
    buffer[0] = (data >> 8) & 0xFF;
    buffer[1] = data & 0xFF;
  } else {
    offset = -E_NO_DATA;
  }

  return offset;
}

int32_t SomeIpXf_DecodeShort(const uint8_t *buffer, uint32_t bufferSize, uint16_t *data) {
  int32_t offset = 2;

  if (bufferSize >= 2) {
    data[0] = ((uint16_t)buffer[0] << 8) + buffer[1];
    ASLOG(SOMEIPXF, ("decode short=%04X from %p\n", data[0], buffer));
  } else {
    offset = -E_NO_DATA;
  }

  return offset;
}

int32_t SomeIpXf_EncodeLong(uint8_t *buffer, uint32_t bufferSize, uint32_t data) {
  int32_t offset = 4;

  if (bufferSize >= 4) {
    ASLOG(SOMEIPXF, ("encode long=%08X to %p\n", data, buffer));
    buffer[0] = (data >> 24) & 0xFF;
    buffer[1] = (data >> 16) & 0xFF;
    buffer[2] = (data >> 8) & 0xFF;
    buffer[3] = data & 0xFF;
  } else {
    offset = -E_NO_DATA;
  }

  return offset;
}

int32_t SomeIpXf_DecodeLong(const uint8_t *buffer, uint32_t bufferSize, uint32_t *data) {
  int32_t offset = 4;

  if (bufferSize >= 4) {
    data[0] = ((uint32_t)buffer[0] << 24) + ((uint32_t)buffer[1] << 16) +
              ((uint32_t)buffer[2] << 8) + buffer[3];
    ASLOG(SOMEIPXF, ("decode long=%08X from %p\n", data[0], buffer));
  } else {
    offset = -E_NO_DATA;
  }

  return offset;
}

int32_t SomeIpXf_EncodeLongLong(uint8_t *buffer, uint32_t bufferSize, uint64_t data) {
  int32_t offset = 8;

  if (bufferSize >= 8) {
    ASLOG(SOMEIPXF, ("encode longlong=%08llX to %p\n", data, buffer));
    buffer[0] = (data >> 56) & 0xFF;
    buffer[1] = (data >> 48) & 0xFF;
    buffer[2] = (data >> 40) & 0xFF;
    buffer[3] = (data >> 32) & 0xFF;
    buffer[4] = (data >> 24) & 0xFF;
    buffer[5] = (data >> 16) & 0xFF;
    buffer[6] = (data >> 8) & 0xFF;
    buffer[7] = data & 0xFF;
  } else {
    offset = -E_NO_DATA;
  }

  return offset;
}

int32_t SomeIpXf_DecodeLongLong(const uint8_t *buffer, uint32_t bufferSize, uint64_t *data) {
  int32_t offset = 8;

  if (bufferSize >= 8) {
    data[0] = ((uint64_t)buffer[0] << 56) + ((uint64_t)buffer[1] << 48) +
              ((uint64_t)buffer[2] << 40) + ((uint64_t)buffer[3] << 32) +
              ((uint32_t)buffer[4] << 24) + ((uint64_t)buffer[5] << 16) +
              ((uint64_t)buffer[6] << 8) + buffer[7];
    ASLOG(SOMEIPXF, ("decode longlong=%08llX from %p\n", data[0], buffer));
  } else {
    offset = -E_NO_DATA;
  }

  return offset;
}

int32_t SomeIpXf_EncodeStruct(uint8_t *buffer, uint32_t bufferSize, const void *pStruct,
                              const SomeIpXf_StructDefinitionType *structDef) {
  uint32_t i;
  int32_t offset = 0;
  int32_t length = 0;
  int32_t r = 0;
  boolean hasIt;
  const SomeIpXf_DataElementType *dataElement;

  /* @SWS_SomeIpXf_00218: reserve space for the length field */
  offset += structDef->sizeOfStructLengthField;

  if ((uint32_t)offset >= bufferSize) {
    ASLOG(SOMEIPXFE, ("no space for struct length field\n"));
    r = -E_NO_DATA;
  }

  ASLOG(SOMEIPXF, ("encode struct %s @%p to %p\n", structDef->name, pStruct, buffer));

  for (i = 0; (i < structDef->numOfDataElements) && (r >= 0); i++) {
    dataElement = &structDef->dataElements[i];
    hasIt = SomeIpXf_HasThisField(pStruct, dataElement);
    if (FALSE == hasIt) {
      continue;
    }
    length = (int32_t)SomeIpXf_GetDataLength(pStruct, dataElement);
    r = SomeIpXf_EncodeTagAndLengthIfHave(buffer + offset, bufferSize - (uint32_t)offset,
                                          dataElement);
    if (r < 0) {
      continue;
    } else {
      offset += r;
    }
    ASLOG(SOMEIPXF,
          ("encode data %s type=%s\n", dataElement->name, getDataTypeName(dataElement->dataType)));
    switch (dataElement->dataType) {
    case SOMEIPXF_DATA_ELEMENT_TYPE_BYTE:
      r = SomeIpXf_EncodeByte(buffer + offset, bufferSize - (uint32_t)offset,
                              STRUCT_VAL(uint8_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT:
      r = SomeIpXf_EncodeShort(buffer + offset, bufferSize - (uint32_t)offset,
                               STRUCT_VAL(uint16_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG:
      r = SomeIpXf_EncodeLong(buffer + offset, bufferSize - (uint32_t)offset,
                              STRUCT_VAL(uint32_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG:
      r = SomeIpXf_EncodeLongLong(buffer + offset, bufferSize - (uint32_t)offset,
                                  STRUCT_VAL(uint64_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT:
      r = SomeIpXf_EncodeStruct(buffer + offset, bufferSize - (uint32_t)offset,
                                STRUCT_PTR(void, pStruct, dataElement->dataOffset),
                                dataElement->pStructDef);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_BYTE_ARRAY:
      r = SomeIpXf_EncodeByteArray(buffer + offset, bufferSize - (uint32_t)offset,
                                   STRUCT_PTR(uint8_t, pStruct, dataElement->dataOffset), length);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT_ARRAY:
      r = SomeIpXf_EncodeShortArray(buffer + offset, bufferSize - (uint32_t)offset,
                                    STRUCT_PTR(uint16_t, pStruct, dataElement->dataOffset), length);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_ARRAY:
      r = SomeIpXf_EncodeLongArray(buffer + offset, bufferSize - (uint32_t)offset,
                                   STRUCT_PTR(uint32_t, pStruct, dataElement->dataOffset), length);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG_ARRAY:
      r = SomeIpXf_EncodeLongLongArray(buffer + offset, bufferSize - (uint32_t)offset,
                                       STRUCT_PTR(uint64_t, pStruct, dataElement->dataOffset),
                                       length);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT_ARRAY:
      r = SomeIpXf_EncodeStructArray(buffer + offset, bufferSize - (uint32_t)offset,
                                     STRUCT_PTR(void, pStruct, dataElement->dataOffset),
                                     dataElement->pStructDef, length);
      break;
    default:
      r = -E_SER_GENERIC_ERROR;
      break;
    }

    if (r < 0) {
      ASLOG(SOMEIPXFE,
            ("struct %s field %s encode error %d\n", structDef->name, dataElement->name, r));
      continue;
    }

    length = r;

    if ((0 != dataElement->dataLenOffset) || (SOMEIPXF_TAG_NOT_USED != dataElement->tag)) {
      if (0 == dataElement->sizeOfDataLengthField) {
        /* no length field */
      } else if (1 == dataElement->sizeOfDataLengthField) {
        asAssert(length < 256);
        (void)SomeIpXf_EncodeByte(buffer + offset - dataElement->sizeOfDataLengthField, 1,
                                  (uint8_t)length);
      } else if (2 == dataElement->sizeOfDataLengthField) {
        asAssert(length < 65536);
        (void)SomeIpXf_EncodeShort(buffer + offset - dataElement->sizeOfDataLengthField, 2,
                                   (uint16_t)length);
      } else {
        asAssert(4 == dataElement->sizeOfDataLengthField);
        (void)SomeIpXf_EncodeLong(buffer + offset - dataElement->sizeOfDataLengthField, 4,
                                  (uint32_t)length);
      }
    }

    offset += length;
  }

  if (r >= 0) {
    if (1 == structDef->sizeOfStructLengthField) {
      asAssert((offset - 1) < 256);
      (void)SomeIpXf_EncodeByte(buffer, 1, (uint8_t)(offset - 1));
    } else if (2 == structDef->sizeOfStructLengthField) {
      asAssert((offset - 2) < 65536);
      (void)SomeIpXf_EncodeShort(buffer, 2, (uint16_t)(offset - 2));
    } else if (4 == structDef->sizeOfStructLengthField) {
      (void)SomeIpXf_EncodeLong(buffer, 4, offset - 4);
    } else {
      /* pass */
    }
  } else {
    offset = r;
  }

  return offset;
}

int32_t SomeIpXf_DecodeStruct(const uint8_t *buffer, uint32_t bufferSize, void *pStruct,
                              const SomeIpXf_StructDefinitionType *structDef) {
  uint32_t i;
  int32_t offset = 0;
  int32_t r = 0;
  uint16_t tag = 0;
  uint32_t structSize = 0;
  uint32_t dataSize;
  const SomeIpXf_DataElementType *dataElement;
  boolean hasTag = (SOMEIPXF_TAG_NOT_USED != structDef->dataElements[0].tag);
  uint8_t sizeOfDataLengthField = 0;

  offset += SomeIpXf_DecodeLength(buffer, structDef->sizeOfStructLengthField, &structSize);
  if (structSize > bufferSize) {
    r = -E_NO_DATA;
  } else if (0 == structSize) {
    structSize = bufferSize;
  } else {
    /* pass */
  }

  ASLOG(SOMEIPXF,
        ("decode struct %s @%p from %p len=%u\n", structDef->name, pStruct, buffer, structSize));

  if (hasTag && (r >= 0)) {
    for (i = 0; i < structDef->numOfDataElements; i++) {
      dataElement = &structDef->dataElements[i];
      if (0 != dataElement->hasOffset) {
        STRUCT_VAL(boolean, pStruct, dataElement->hasOffset) = FALSE;
      }
    }
  }

  for (i = 0; ((offset - (int32_t)structDef->sizeOfStructLengthField) < (int32_t)structSize) &&
              (i < structDef->numOfDataElements) && (r >= 0);) {
    sizeOfDataLengthField = 0;
    if (hasTag) {
      tag = ((uint32_t)buffer[offset + 0] << 8) + buffer[offset + 1];
      offset += 2;
      dataElement = NULL;
      for (i = 0; i < structDef->numOfDataElements; i++) {
        if (SomeIpXf_IsTagMatched(&structDef->dataElements[i], tag, &sizeOfDataLengthField)) {
          dataElement = &structDef->dataElements[i];
          break;
        }
      }
      if (NULL == dataElement) {
        if ((tag & 0xF000) == 0x0000) {
          offset += 1;
        } else if ((tag & 0xF000) == 0x1000) {
          offset += 2;
        } else if ((tag & 0xF000) == 0x2000) {
          offset += 4;
        } else if ((tag & 0xF000) == 0x3000) {
          offset += 8;
        } else if (((tag & 0xF000) == 0x4000) || ((tag & 0xF000) == 0x5000)) {
          offset += 1 + SomeIpXf_DecodeLength(buffer + offset, 1, &dataSize);
          offset += (int32_t)dataSize;
        } else if ((tag & 0xF000) == 0x6000) {
          offset += 2 + SomeIpXf_DecodeLength(buffer + offset, 2, &dataSize);
          offset += (int32_t)dataSize;
        } else if ((tag & 0xF000) == 0x7000) {
          offset += 4 + SomeIpXf_DecodeLength(buffer + offset, 4, &dataSize);
          offset += (int32_t)dataSize;
        } else {
          r = -E_SER_MALFORMED_MESSAGE;
        }
        ASLOG(SOMEIPXF, ("skip data with tag=%X as not found\n", tag));
        if ((offset - (int32_t)structDef->sizeOfStructLengthField) > (int32_t)structSize) {
          r = -E_SER_MALFORMED_MESSAGE;
        }
        continue;
      } else {
        if (0 != dataElement->hasOffset) {
          STRUCT_VAL(boolean, pStruct, dataElement->hasOffset) = TRUE;
        }
      }
      i = 0;
    } else {
      dataElement = &structDef->dataElements[i];
      sizeOfDataLengthField = dataElement->sizeOfDataLengthField;
    }
    if (0 != dataElement->dataLenOffset) {
      offset += SomeIpXf_DecodeLength(buffer + offset, sizeOfDataLengthField, &dataSize);
    } else {
      dataSize = dataElement->dataSize;
    }
    if (dataSize > dataElement->dataSize) {
      r = -E_SER_WRONG_INTERFACE_VERSION;
      continue;
    }

    ASLOG(SOMEIPXF, ("decode data %s type=%s len=%u tag=%X\n", dataElement->name,
                     getDataTypeName(dataElement->dataType), dataSize, tag));
    switch (dataElement->dataType) {
    case SOMEIPXF_DATA_ELEMENT_TYPE_BYTE:
      r = SomeIpXf_DecodeByte(buffer + offset, bufferSize - (uint32_t)offset,
                              STRUCT_PTR(uint8_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT:
      r = SomeIpXf_DecodeShort(buffer + offset, bufferSize - (uint32_t)offset,
                               STRUCT_PTR(uint16_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG:
      r = SomeIpXf_DecodeLong(buffer + offset, bufferSize - (uint32_t)offset,
                              STRUCT_PTR(uint32_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG:
      r = SomeIpXf_DecodeLongLong(buffer + offset, bufferSize - (uint32_t)offset,
                                  STRUCT_PTR(uint64_t, pStruct, dataElement->dataOffset));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT:
      r = SomeIpXf_DecodeStruct(buffer + offset, bufferSize - (uint32_t)offset,
                                STRUCT_PTR(void, pStruct, dataElement->dataOffset),
                                dataElement->pStructDef);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_BYTE_ARRAY:
      r = SomeIpXf_DecodeByteArray(buffer + offset, bufferSize - (uint32_t)offset,
                                   STRUCT_PTR(uint8_t, pStruct, dataElement->dataOffset), dataSize);
      SomeIpXf_SetLenghtIfhave(pStruct, dataElement, dataSize);
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_SHORT_ARRAY:
      r = SomeIpXf_DecodeShortArray(buffer + offset, bufferSize - (uint32_t)offset,
                                    STRUCT_PTR(uint16_t, pStruct, dataElement->dataOffset),
                                    dataSize / sizeof(uint16_t));
      SomeIpXf_SetLenghtIfhave(pStruct, dataElement, dataSize / sizeof(uint16_t));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_ARRAY:
      r = SomeIpXf_DecodeLongArray(buffer + offset, bufferSize - (uint32_t)offset,
                                   STRUCT_PTR(uint32_t, pStruct, dataElement->dataOffset),
                                   dataSize / sizeof(uint32_t));
      SomeIpXf_SetLenghtIfhave(pStruct, dataElement, dataSize / sizeof(uint32_t));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_LONG_LONG_ARRAY:
      r = SomeIpXf_DecodeLongLongArray(buffer + offset, bufferSize - (uint32_t)offset,
                                       STRUCT_PTR(uint64_t, pStruct, dataElement->dataOffset),
                                       dataSize / sizeof(uint64_t));
      SomeIpXf_SetLenghtIfhave(pStruct, dataElement, dataSize / sizeof(uint64_t));
      break;
    case SOMEIPXF_DATA_ELEMENT_TYPE_STRUCT_ARRAY:
      r = SomeIpXf_DecodeStructArray(buffer + offset, bufferSize - (uint32_t)offset,
                                     STRUCT_PTR(void, pStruct, dataElement->dataOffset),
                                     dataElement->pStructDef, &dataSize);
      SomeIpXf_SetLenghtIfhave(pStruct, dataElement, dataSize);
      break;
    default:
      r = -E_SER_GENERIC_ERROR;
      break;
    }
    if (r < 0) {
      ASLOG(SOMEIPXFE,
            ("struct %s field %s encode error %d\n", structDef->name, dataElement->name, r));
      continue;
    }

    offset += r;

    if (FALSE == hasTag) {
      i++;
    }
  }

  if (r < 0) {
    offset = r;
  }

  return offset;
}

int32_t SomeIpXf_EncodeByteArray(uint8_t *buffer, uint32_t bufferSize, const uint8_t *data,
                                 uint32_t length) {
  int32_t offset = length;
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("encode byte array len=%u: [%02X %02X .. %02X %02X] %c%c..%c%c\n", length,
                     data[0], data[1], data[length - 2], data[length - 1], data[0], data[1],
                     data[length - 2], data[length - 1]));
    memcpy(buffer, data, length);
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_DecodeByteArray(const uint8_t *buffer, uint32_t bufferSize, uint8_t *data,
                                 uint32_t length) {
  int32_t offset = length;
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("decode byte array len=%u: [%02X %02X .. %02X %02X] %c%c..%c%c\n", length,
                     buffer[0], buffer[1], buffer[length - 2], buffer[length - 1], buffer[0],
                     buffer[1], buffer[length - 2], buffer[length - 1]));
    memcpy(data, buffer, length);
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_EncodeShortArray(uint8_t *buffer, uint32_t bufferSize, const uint16_t *data,
                                  uint32_t length) {
  uint32_t i;
  int32_t offset = length * sizeof(uint16_t);
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("encode short array len=%u\n", length));
    for (i = 0; i < length; i++) {
      SomeIpXf_EncodeShort(buffer + i * sizeof(uint16_t), sizeof(uint16_t), data[i]);
    }
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_DecodeShortArray(const uint8_t *buffer, uint32_t bufferSize, uint16_t *data,
                                  uint32_t length) {
  uint32_t i;
  int32_t offset = length * sizeof(uint16_t);
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("decode short array len=%u\n", length));
    for (i = 0; i < length; i++) {
      SomeIpXf_DecodeShort(buffer + i * sizeof(uint16_t), sizeof(uint16_t), &data[i]);
    }
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_EncodeLongArray(uint8_t *buffer, uint32_t bufferSize, const uint32_t *data,
                                 uint32_t length) {
  uint32_t i;
  int32_t offset = length * sizeof(uint32_t);
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("encode long array len=%u\n", length));
    for (i = 0; i < length; i++) {
      SomeIpXf_EncodeLong(buffer + i * sizeof(uint32_t), sizeof(uint32_t), data[i]);
    }
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_DecodeLongArray(const uint8_t *buffer, uint32_t bufferSize, uint32_t *data,
                                 uint32_t length) {
  uint32_t i;
  int32_t offset = length * sizeof(uint32_t);
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("decode long array len=%u\n", length));
    for (i = 0; i < length; i++) {
      SomeIpXf_DecodeLong(buffer + i * sizeof(uint32_t), sizeof(uint32_t), &data[i]);
    }
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_EncodeLongLongArray(uint8_t *buffer, uint32_t bufferSize, const uint64_t *data,
                                     uint32_t length) {
  uint32_t i;
  int32_t offset = length * sizeof(uint64_t);
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("encode long long array len=%u\n", length));
    for (i = 0; i < length; i++) {
      SomeIpXf_EncodeLongLong(buffer + i * sizeof(uint64_t), sizeof(uint64_t), data[i]);
    }
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_DecodeLongLongArray(const uint8_t *buffer, uint32_t bufferSize, uint64_t *data,
                                     uint32_t length) {
  uint32_t i;
  int32_t offset = length * sizeof(uint64_t);
  if (bufferSize >= (uint32_t)offset) {
    ASLOG(SOMEIPXF, ("decode long long array len=%u\n", length));
    for (i = 0; i < length; i++) {
      SomeIpXf_DecodeLongLong(buffer + i * sizeof(uint64_t), sizeof(uint64_t), &data[i]);
    }
  } else {
    offset = -E_NO_DATA;
  }
  return offset;
}

int32_t SomeIpXf_EncodeStructArray(uint8_t *buffer, uint32_t bufferSize, const void *pStruct,
                                   const SomeIpXf_StructDefinitionType *pStructDef,
                                   uint32_t length) {
  uint32_t i;
  int32_t offset = 0;
  int32_t r = 0;
  ASLOG(SOMEIPXF, ("encode struct array len=%u\n", length));
  for (i = 0; (i < length) && (r >= 0); i++) {
    r = SomeIpXf_EncodeStruct(buffer + offset, bufferSize - (uint32_t)offset,
                              ((uint8_t *)pStruct) + pStructDef->structSize * i, pStructDef);
    if (r >= 0) {
      offset += r;
    }
  }
  if (r < 0) {
    offset = r;
  }
  return offset;
}

int32_t SomeIpXf_DecodeStructArray(const uint8_t *buffer, uint32_t bufferSize, void *pStruct,
                                   const SomeIpXf_StructDefinitionType *pStructDef,
                                   uint32_t *length) {
  uint32_t i;
  int32_t offset = 0;
  int32_t r = 0;

  if (bufferSize >= *length) {
    ASLOG(SOMEIPXF, ("decode struct array len=%u\n", *length));
    for (i = 0; ((uint32_t)offset < *length) && (r >= 0); i++) {
      r = SomeIpXf_DecodeStruct(buffer + offset, *length - offset,
                                ((uint8_t *)pStruct) + pStructDef->structSize * i, pStructDef);
      if (r >= 0) {
        offset += r;
      }
    }
  } else {
    r = -E_NO_DATA;
  }

  if (r < 0) {
    offset = r;
  } else {
    *length = i;
  }
  return offset;
}
