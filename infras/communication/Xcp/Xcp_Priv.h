/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 *
 * ref: [0] Specification of Module XCP AUTOSAR CP R23-11
 *  [1]
 * https://docs.fzxhub.com/%E6%A0%87%E5%AE%9A%E5%8D%8F%E8%AE%AE%E8%AF%A6%E8%A7%A3/4.XCP%E5%8D%8F%E8%AE%AE%E5%89%96%E6%9E%90/
 *  [2] https://blog.csdn.net/djkeyzx/article/details/134166684
 *  [3] https://blog.csdn.net/w_melody/article/details/134463026
 */
#ifndef XCP_PRIV_H
#define XCP_PRIV_H
/* ================================ [ INCLUDES  ] ============================================== */
#include "ComStack_Types.h"
#include "sys/queue.h"
/* ================================ [ MACROS    ] ============================================== */
#define DET_THIS_MODULE_ID MODULE_ID_XCP
/* @[1] CMD-STD                                Is Optional */
#define XCP_PID_CMD_STD_CONNECT 0xFFu             /* no */
#define XCP_PID_CMD_STD_DISCONNECT 0xFEu          /* no */
#define XCP_PID_CMD_STD_GET_STATUS 0xFDu          /* no */
#define XCP_PID_CMD_STD_SYNCH 0xFCu               /* no */
#define XCP_PID_CMD_STD_GET_COMM_MODE_INFO 0xFBu  /* yes */
#define XCP_PID_CMD_STD_GET_ID 0xFAu              /* yes */
#define XCP_PID_CMD_STD_SET_REQUEST OxF9u         /* yes */
#define XCP_PID_CMD_STD_GET_SEED 0xF8u            /* yes */
#define XCP_PID_CMD_STD_UNLOCK 0xF7u              /* yes */
#define XCP_PID_CMD_STD_SET_MTA 0xF6u             /* yes */
#define XCP_PID_CMD_STD_UPLOAD 0xF5u              /* yes */
#define XCP_PID_CMD_STD_SHORT_UPLOAD 0xF4u        /* yes */
#define XCP_PID_CMD_STD_BUILD_CHECKSUM 0xF3u      /* yes */
#define XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD 0xF2u /* yes */
#define XCP_PID_CMD_STD_USER_CMD 0xF1u            /* yes */

/* @[1] CMD-CAL                          Is Optional */
#define XCP_PID_CMD_CAL_DOWNLOAD 0xF0u       /* no*/
#define XCP_PID_CMD_CAL_DOWNLOAD_NEXT 0xEFu  /* yes */
#define XCP_PID_CMD_CAL_DOWNLOAD_MAX 0xEEu   /* yes */
#define XCP_PID_CMD_CAL_SHORT_DOWNLOAD 0xEDu /* yes */
#define XCP_PID_CMD_CAL_MODIFY_BITS 0xECu    /* yes */

/* @[1] CMD-PAG                                  Is Optional */
#define XCP_PID_CMD_PAG_SET_CAL_PAGE 0xEBu           /* no*/
#define XCP_PID_CMD_PAG_GET_CAL_PAGE 0xEAu           /* no*/
#define XCP_PID_CMD_PAG_GET_PAG_PROCESSOR_INFO 0xE9u /* yes */
#define XCP_PID_CMD_PAG_GET_SEGMENT_INFO 0xE8u       /* yes */
#define XCP_PID_CMD_PAG_GET_PAGE_INFO 0xE7u          /* yes */
#define XCP_PID_CMD_PAG_SET_SEGMENT_MODE 0xE6u       /* yes */
#define XCP_PID_CMD_PAG_GET_SEGMENT_MODE 0xE5u       /* yes */
#define XCP_PID_CMD_PAG_COPY_CAL_PAGE 0xE4u          /* yes */

/* @[1] CMD-DAQ                                   Is Optional */
#define XCP_PID_CMD_DAQ_CLEAR_DAQ_LIST 0xE3u          /* no*/
#define XCP_PID_CMD_DAQ_SET_DAQ_PTR 0xE2u             /* no*/
#define XCP_PID_CMD_DAQ_WRITE_DAQ 0xE1u               /* no*/
#define XCP_PID_CMD_DAQ_SET_DAQ_LIST_MODE 0xE0u       /* no*/
#define XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE 0xDFu       /* no*/
#define XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST 0xDEu     /* no*/
#define XCP_PID_CMD_DAQ_START_STOP_SYNCH 0xDDu        /* no*/
#define XCP_PID_CMD_DAQ_GET_DAQ_CLOCK 0xDCu           /* yes */
#define XCP_PID_CMD_DAQ_READ_DAQ 0xDBu                /* yes */
#define XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO 0xDAu  /* yes */
#define XCP_PID_CMD_DAQ_GET_DAQ_RESOLUTION_INFO 0xD9u /* yes */
#define XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO 0xD8u       /* yes */
#define XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO 0xD7u      /* yes */
#define XCP_PID_CMD_DAQ_FREE_DAQ 0xD6u                /* yes */
#define XCP_PID_CMD_DAQ_ALLOC_DAQ 0xD5u               /* yes */
#define XCP_PID_CMD_DAQ_ALLOC_ODT 0xD4u               /* yes */
#define XCP_PID_CMD_DAQ_ALLOC_ODT_ENTRY 0xD3u         /* yes */

/* @[1] CMD-PGM                                   Is Optional */
#define XCP_PID_CMD_PGM_PROGRAM_START 0xD2u          /* no*/
#define XCP_PID_CMD_PGM_PROGRAM_CLEAR 0xD1u          /* no*/
#define XCP_PID_CMD_PGM_PROGRAM 0xD0u                /* no*/
#define XCP_PID_CMD_PGM_PROGRAM_RESET 0xCFu          /* no*/
#define XCP_PID_CMD_PGM_GET_PGM_PROCESSOR_INFO 0xCEu /* yes */
#define XCP_PID_CMD_PGM_GET_SECTOR_INFO 0xCDu        /* yes */
#define XCP_PID_CMD_PGM_PROGRAM_PREPARE 0xCCu        /* yes */
#define XCP_PID_CMD_PGM_PROGRAM_FORMAT 0xCBu         /* yes */
#define XCP_PID_CMD_PGM_PROGRAM_NEXT 0xCAu           /* yes */
#define XCP_PID_CMD_PGM_PROGRAM_MAX 0xC9u            /* yes */
#define XCP_PID_CMD_PGM_PROGRAM_VERIFY 0xC8u         /* yes */

/* Packet Id from slave to master */
#define XCP_PID_RES 0xFFu  /* Command Response packet */
#define XCP_PID_ERR 0xFEu  /* Error packet */
#define XCP_PID_EV 0xFDu   /* Event packet */
#define XCP_PID_SERV 0xFCu /* Service Request packet */

#define XCP_DAT_LIST_TYPE_DAQ ((Xcp_DaqListTypeType)0)
#define XCP_DAT_LIST_TYPE_DAQ_STIM ((Xcp_DaqListTypeType)1)
#define XCP_DAT_LIST_TYPE_STIM ((Xcp_DaqListTypeType)2)

#define XCP_EVENT_CHANNEL_TYPE_DAQ ((Xcp_EventChannelTypeType)0)
#define XCP_EVENT_CHANNEL_TYPE_DAQ_STIM ((Xcp_EventChannelTypeType)1)
#define XCP_EVENT_CHANNEL_TYPE_STIM ((Xcp_EventChannelTypeType)2)

#define XCP_CONSISTENCY_ON_DAQ ((Xcp_EventChannelConsistencyType)0)
#define XCP_CONSISTENCY_ON_EVENT ((Xcp_EventChannelConsistencyType)1)
#define XCP_CONSISTENCY_ON_ODT ((Xcp_EventChannelConsistencyType)2)

#ifndef XCP_PATCKET_MAX_SIZE
#define XCP_PATCKET_MAX_SIZE 8u
#endif

#ifndef XCP_CACHE_SIZE
#define XCP_CACHE_SIZE 32u
#endif

#define XCP_MTA_EXT_MEMORY 0u /* RAM */
#define XCP_MTA_EXT_FLASH 1u
#define XCP_MTA_EXT_DIO_PORT 2u
#define XCP_MTA_EXT_DIO_CHANNEL 3u
#define XCP_MTA_EXT_MAX 4u

#define XCP_TIMESTAMP_UNIT_1NS 0x00u
#define XCP_TIMESTAMP_UNIT_10NS 0x01u
#define XCP_TIMESTAMP_UNIT_100NS 0x02u
#define XCP_TIMESTAMP_UNIT_1US 0x03u
#define XCP_TIMESTAMP_UNIT_10US 0x04u
#define XCP_TIMESTAMP_UNIT_100US 0x05u
#define XCP_TIMESTAMP_UNIT_1MS 0x06u
#define XCP_TIMESTAMP_UNIT_10MS 0x07u
#define XCP_TIMESTAMP_UNIT_100MS 0x08u
#define XCP_TIMESTAMP_UNIT_1S 0x09u

#ifndef XCP_CONST
#define XCP_CONST
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef uint8_t Xcp_DaqListTypeType;
typedef uint8_t Xcp_EventChannelTypeType;

/* @ECUC_Xcp_00065 */
typedef struct {
  uint8_t Pid;            /* @ECUC_Xcp_00066, Range: 0 - 251 */
  uint8_t Dto2PduMapping; /* @ECUC_Xcp_00067 */
} Xcp_DtoType;

/* @ECUC_Xcp_00061 */
typedef struct {
  uint16_t Address;  /* @ECUC_Xcp_00063 */
  uint8_t BitOffset; /* @ECUC_Xcp_00179 */
  uint8_t Length;    /* @ECUC_Xcp_00064 */
  uint8_t Extension;
} Xcp_OdtEntryType;

/* @ECUC_Xcp_00055 */
typedef struct {
  Xcp_OdtEntryType *OdtEntries;
  uint8_t EntryMaxSize; /* @ECUC_Xcp_00060: range 0 ~ 254 */
} Xcp_OdtType;

/* @ECUC_Xcp_00050 */
typedef struct {
  Xcp_OdtType *Odts;
  uint8_t MaxOdt; /* ECUC_Xcp_00053: range 0 ~ 252 */
} Xcp_DaqListType;

typedef uint8_t Xcp_EventChannelConsistencyType;

/* @ECUC_Xcp_00150 */
typedef struct {
  /* uint16_t *TriggeredDaqListRef; @ECUC_Xcp_00151 */
  /* uint16_t Number; @ECUC_Xcp_00152, Index number of the event channel */
  uint16_t TimeCycle; /* For this simple implementation, TimeCycle in the Xcp Main Function cycle */
  /* uint8_t MaxDaqList; @ECUC_Xcp_00153 */
  /* Xcp_EventChannelConsistencyType ConsistencyType; @ECUC_Xcp_00171 */
  /* uint8_t Priority;   @ECUC_Xcp_00154 */
  /* uint8_t TimeCycle;  @ECUC_Xcp_00173 */
  /* uint8_t TimeUnit;   @ECUC_Xcp_00174 */
  /* Xcp_EventChannelTypeType Type;  @ECUC_Xcp_00172 */
} Xcp_EventChannelType;

/* @ECUC_Xcp_00188 */
typedef struct {
  uint16_t Length;
  uint8_t ReferencePageAddress;
  uint8_t WorkingPageAddress;
} Xcp_SegmentType;

/* @ECUC_Xcp_00192 */
typedef struct {
  uint8_t Address;
} Xcp_PageType;

typedef Std_ReturnType (*Xcp_DspServiceFncType)(Xcp_MsgContextType *msgContext,
                                                Xcp_NegativeResponseCodeType *nrc);

typedef Std_ReturnType (*Xcp_CallbackGetConnectPermissionFncType)(
  uint8_t mode, Xcp_NegativeResponseCodeType *nrc);

typedef Std_ReturnType (*Xcp_CallbackDisconnectionFncType)(void);

typedef Std_ReturnType (*Xcp_CallbackGetSeedFncType)(uint8_t mode, uint8_t resource, uint8_t *seed,
                                                     uint16_t *seedLen,
                                                     Xcp_NegativeResponseCodeType *nrc);

typedef Std_ReturnType (*Xcp_CallbackCompareKeyFncType)(uint8_t *key, uint16_t keyLen,
                                                        Xcp_NegativeResponseCodeType *nrc);

typedef struct {
  Xcp_CallbackGetConnectPermissionFncType GetConnectPermissionFnc;
} Xcp_ConnectConfigType;

typedef struct {
  Xcp_CallbackDisconnectionFncType DisconnectFnc;
} Xcp_DisconnectConfigType;

typedef struct {
  Xcp_CallbackGetSeedFncType GetSeedFnc;
} Xcp_GetSeedConfigType;

typedef struct {
  Xcp_CallbackCompareKeyFncType CompareKeyFnc;
} Xcp_UnlockConfigType;

typedef Std_ReturnType (*Xcp_GetProgramResetPermissionFncType)(Xcp_OpStatusType OpStatus,
                                                               Xcp_NegativeResponseCodeType *nrc);

typedef struct {
  Xcp_GetProgramResetPermissionFncType GetProgramResetPermissionFnc;
  uint16_t delay;
} Xcp_ProgramResetConfigType;

typedef Std_ReturnType (*Xcp_GetProgramStartPermissionFncType)(Xcp_OpStatusType OpStatus,
                                                               Xcp_NegativeResponseCodeType *nrc);

typedef struct {
  Xcp_GetProgramStartPermissionFncType GetProgramStartPermissionFnc;
  uint8_t maxBs;
  uint8_t STmin;
  uint8_t queSz;
} Xcp_ProgramStartConfigType;

typedef struct {
  Xcp_DspServiceFncType dspServiceFnc;
  P2CONST(void, AUTOMATIC, XCP_CONST) config;
  uint8_t Pid;
  uint8_t resource; /* the protected resource that need to be unlocked for this service */
} Xcp_ServiceType;

typedef struct {
  uint16_t timer;
  uint8_t state;
  /* uint8_t mode; Now DAQ only, timestamp and PID_OFF not supported */
  uint8_t prescaler;
  uint8_t evChl;
  uint8_t curPid;
} Xcp_DaqListContextType;

struct Xcp_Config_s {
  P2CONST(Xcp_ServiceType, AUTOMATIC, XCP_CONST) services;
  P2CONST(Xcp_DaqListType *, AUTOMATIC, XCP_CONST) daqList;
  Xcp_DaqListContextType *daqContexts;
  P2CONST(Xcp_EventChannelType, AUTOMATIC, XCP_CONST) evChls;
  Xcp_OdtType *dynOdtSlots;
  Xcp_OdtEntryType *dynOdtEntrySlots;
  uint16_t numOfStaticDaqList;
  uint16_t numOfDaqList;
  uint16_t numOfDynOdtSlots;
  uint16_t numOfDynOdtEntrySlots;
  PduIdType CanIfTxPduId;
  uint8_t numOfServices;
  uint8_t numOfEvChls;
  uint8_t resource;
};

typedef struct Xcp_Packet_s {
  STAILQ_ENTRY(Xcp_Packet_s) entry;
  Xcp_MsgLenType length;
  uint8_t payload[XCP_PATCKET_MAX_SIZE];
  Xcp_OpStatusType opStatus;
} Xcp_PacketType;

typedef STAILQ_HEAD(Xcp_PacketListHead_s, Xcp_Packet_s) Xcp_PacketListType;

typedef struct {
  uint32_t address;
  uint8_t extension;
} Xcp_MtaContextType;

typedef struct {
  uint32_t address;
  uint8_t extension;
  uint8_t length;
  uint8_t offset;
} Xcp_DownloadContextType;

typedef struct {
  uint32_t address;
  uint8_t extension;
  uint8_t length;
  uint8_t offset;
} Xcp_ProgramContextType;

typedef struct {
  uint16_t daqListNumber;
  uint8_t odtNumber;
  uint8_t odtEntryNumber;
} Xcp_SetDaqPtrContextType;

typedef struct {
  Xcp_PacketListType rxPackets;
  Xcp_PacketListType txPackets;
  P2CONST(Xcp_ServiceType, AUTOMATIC, XCP_CONST) curService;
  uint32_t cache[XCP_CACHE_SIZE / sizeof(uint32_t)];
  Xcp_MsgContextType msgContext;
#ifdef XCP_USE_SERVICE_PGM_PROGRAM_RESET
  uint16_t timer2Reset;
#endif
  uint8_t activeChannel;   /* active channel that connected */
  uint8_t lockedResource;  /* protected resource */
  uint8_t requestResource; /* the cached get seed requested resource */
  uint8_t cacheOwner;      /* record the cache last used by which service */
#ifdef XCP_USE_SERVICE_PGM_PROGRAM_START
  uint8_t pgmFlags;
#endif
} Xcp_ContextType;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
Xcp_ContextType *Xcp_GetContext(void);
P2CONST(Xcp_ConfigType, AUTOMATIC, XCP_CONST) Xcp_GetConfig(void);
P2CONST(void, AUTOMATIC, XCP_CONST) Xcp_GetCurServiceConfig(void);

boolean Xcp_HasFreePacket(void);
Xcp_PacketType *Xcp_AllocPacket(void);
void Xcp_FreePacket(Xcp_PacketType *packet);

Std_ReturnType Xcp_MtaRead(uint8_t extension, uint32_t address, uint8_t *data, Xcp_MsgLenType len,
                           Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_MtaWrite(uint8_t extension, uint32_t address, uint8_t *data, Xcp_MsgLenType len,
                            Xcp_NegativeResponseCodeType *nrc);

Std_ReturnType Xcp_DspConnect(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspDisconnect(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspGetSeed(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspUnlock(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspGetStatus(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspSetMTA(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspUpload(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspShortUpload(Xcp_MsgContextType *msgContext,
                                  Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspDownload(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspDownloadNext(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc);

Std_ReturnType Xcp_DspProgramStart(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspProgramClear(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspProgram(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspProgramNext(Xcp_MsgContextType *msgContext,
                                  Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspProgramReset(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc);

void Xcp_DspDaqInit(void);

void Xcp_MainFunction_Daq(void);
void Xcp_MainFunction_DaqWrite(void);

Std_ReturnType Xcp_DspFreeDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspAllocDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspAllocODT(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspAllocODTEntry(Xcp_MsgContextType *msgContext,
                                    Xcp_NegativeResponseCodeType *nrc);

Std_ReturnType Xcp_DspClearDAQList(Xcp_MsgContextType *msgContext,
                                   Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspStartStopDAQList(Xcp_MsgContextType *msgContext,
                                       Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspDAQStartStopSynch(Xcp_MsgContextType *msgContext,
                                        Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspSetDAQListMode(Xcp_MsgContextType *msgContext,
                                     Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspSetDAQPtr(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspWriteDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspReadDAQ(Xcp_MsgContextType *msgContext, Xcp_NegativeResponseCodeType *nrc);

Std_ReturnType Xcp_DspGetDAQProcessorInfo(Xcp_MsgContextType *msgContext,
                                          Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspGetDAQResolutionInfo(Xcp_MsgContextType *msgContext,
                                           Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspGetDAQListInfo(Xcp_MsgContextType *msgContext,
                                     Xcp_NegativeResponseCodeType *nrc);
Std_ReturnType Xcp_DspGetDAQEventInfo(Xcp_MsgContextType *msgContext,
                                      Xcp_NegativeResponseCodeType *nrc);

uint32_t Xcp_GetU32(const uint8_t *data);
void Xcp_SetU32(uint8_t *data, uint32_t u32);
uint16_t Xcp_GetU16(const uint8_t *data);
void Xcp_SetU16(uint8_t *data, uint16_t u16);

void Xcp_RxIndication(uint8_t channel, const PduInfoType *PduInfoPtr);
#endif /* XCP_PRIV_H */
