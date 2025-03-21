# SSAS - Simple Smart Automotive Software
# Copyright (C) 2023 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def get_session(obj):
    if len(obj.get("sessions", [])) == 0:
        return "DCM_ANY_SESSION_MASK"
    cstr = ""
    for ss in obj["sessions"]:
        cstr += "|DCM_%s_MASK" % (toMacro(ss))
    return cstr[1:]


def get_security(obj, cfg):
    if len(obj.get("securities", [])) == 0:
        return "DCM_ANY_SECURITY_MASK"
    cstr = ""
    smap = {x["name"]: x for x in cfg["securities"]}
    for sec in obj["securities"]:
        cstr += "|DCM_SEC_LEVEL%s_MASK" % (smap[sec]["level"])
    return cstr[1:]


def get_misc(obj, isService=True):
    cstr = ""
    access = obj.get("access", ["physical"])
    if len(access) == 0:
        print("WARNING: no access for:", obj)
        access = ["physical"]
    for x in access:
        cstr += "|DCM_MISC_%s" % (x.upper())
    if isService and ServiceMap[obj["id"]]["subfunc"]:
        cstr += "|DCM_MISC_SUB_FUNCTION"
    return cstr[1:]


def get_number_of_DDDID(cfg):
    for sx in cfg["services"]:
        if sx["id"] == 0x2C:
            return sx.get("number", 2)
    return 0


def gen_dummy_api(C, service, cfg):
    pass


def gen_dummy_config(C, service, cfg):
    pass


def gen_session_control_api(C, service, cfg):
    C.write("Std_ReturnType %s(Dcm_SesCtrlType sesCtrlTypeActive,\n" % (service["API"]))
    C.write("                  Dcm_SesCtrlType sesCtrlTypeNew,\n")
    C.write("                  Dcm_NegativeResponseCodeType *nrc);\n\n")


def gen_session_control_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_SesCtrlType, DCM_CONST) Dcm_SesCtrls[] = {\n")
    for x in cfg["sessions"]:
        C.write("  DCM_%s_SESSION,\n" % (toMacro(x["name"])))
    C.write("};\n\n")
    C.write("static CONSTANT(Dcm_SessionControlConfigType, DCM_CONST) Dcm_SessionControlConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("  Dcm_SesCtrls,\n")
    C.write("  ARRAY_SIZE(Dcm_SesCtrls),\n")
    C.write("};\n\n")


def gen_ecu_reset_api(C, service, cfg):
    C.write(
        "Std_ReturnType %s(Dcm_OpStatusType opStatus, Dcm_NegativeResponseCodeType *errorCode);\n\n" % (service["API"])
    )


def gen_ecu_reset_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_EcuResetConfigType, DCM_CONST) Dcm_EcuResetConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("  DCM_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (service.get("delay", 100)))
    C.write("};\n\n")


def gen_read_did_api(C, service, cfg):
    for did in service["DIDs"]:
        C.write("Std_ReturnType %s(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,\n" % (did["API"]))
        C.write("                   Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_read_scaling_did_api(C, service, cfg):
    for did in service["DIDs"]:
        C.write("Std_ReturnType %s(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,\n" % (did["API"]))
        C.write("                   Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_read_did_config(C, service, cfg):
    numOfDDDID = get_number_of_DDDID(cfg)
    C.write("static Dcm_ReadDIDContextType Dcm_ReadDIDContexts[%d];\n" % (len(service["DIDs"]) + numOfDDDID))
    C.write("static CONSTANT(Dcm_ReadDIDType, DCM_CONST) Dcm_ReadDIDs[] = {\n")
    for i, did in enumerate(service["DIDs"]):
        C.write("  {\n")
        C.write("    &Dcm_ReadDIDContexts[%s],\n" % (i))
        C.write("    &Dcm_rDIDConfigs[DCM_RDID_%X_INDEX],\n" % (toNum(did["id"])))
        C.write("  },\n")
    for i in range(numOfDDDID):
        C.write("  {\n")
        C.write("    &Dcm_ReadDIDContexts[%s],\n" % (i + len(service["DIDs"])))
        C.write("    &Dcm_rDDDIDConfigs[%s],\n" % (i))
        C.write("  },\n")
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_ReadDIDConfigType, DCM_CONST) Dcm_ReadDataByIdentifierConfig = {\n")
    C.write("  Dcm_ReadDIDs,\n")
    C.write("  ARRAY_SIZE(Dcm_ReadDIDs),\n")
    C.write("};\n\n")


def gen_read_scaling_did_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_rDIDConfigType, DCM_CONST) Dcm_rScalingDIDConfigs[] = {\n")
    for did in service["DIDs"]:
        C.write("  {\n")
        C.write("    0x%X,\n" % (toNum(did["id"])))
        C.write("    %s,\n" % (did["size"]))
        C.write("    %s,\n" % (did["API"]))
        C.write("    {\n")
        C.write("      %s,\n" % (get_session(did)))
        C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
        C.write("      %s,\n" % (get_security(did, cfg)))
        C.write("#endif\n")
        C.write("      %s,\n" % (get_misc(did, False)))
        C.write("    },\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dcm_ReadScalingDIDConfigType, DCM_CONST) Dcm_ReadScalingDataByIdentifierConfig = {\n")
    C.write("  Dcm_rScalingDIDConfigs,\n")
    C.write("  ARRAY_SIZE(Dcm_rScalingDIDConfigs),\n")
    C.write("};\n\n")


def gen_read_periodic_did_api(C, service, cfg):
    for did in service["DIDs"]:
        C.write("Std_ReturnType %s(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,\n" % (did["API"]))
        C.write("                   Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_read_periodic_did_config(C, service, cfg):
    numOfDDDID = get_number_of_DDDID(cfg)
    C.write(
        "static Dcm_ReadPeriodicDIDContextType Dcm_ReadPeriodicDIDContexts[%d];\n" % (len(service["DIDs"]) + numOfDDDID)
    )
    C.write("static CONSTANT(Dcm_ReadPeriodicDIDType, DCM_CONST) Dcm_ReadPeriodicDIDs[] = {\n")
    for i, did in enumerate(service["DIDs"]):
        C.write("  {\n")
        C.write("    &Dcm_ReadPeriodicDIDContexts[%s],\n" % (i))
        C.write("    &Dcm_rDIDConfigs[DCM_RDID_%X_INDEX],\n" % (toNum(did["id"])))
        C.write("  },\n")
    for i in range(numOfDDDID):
        C.write("  {\n")
        C.write("    &Dcm_ReadPeriodicDIDContexts[%s],\n" % (i + len(service["DIDs"])))
        C.write("    &Dcm_rDDDIDConfigs[%s],\n" % (i))
        C.write("  },\n")
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_ReadPeriodicDIDConfigType, DCM_CONST) Dcm_ReadDataByPeriodicIdentifierConfig = {\n")
    C.write("  Dcm_ReadPeriodicDIDs,\n")
    C.write("  ARRAY_SIZE(Dcm_ReadPeriodicDIDs),\n")
    C.write("};\n\n")


def gen_security_access_api(C, service, cfg):
    for x in cfg["securities"]:
        C.write("Std_ReturnType %s(uint8_t *seed, Dcm_NegativeResponseCodeType *errorCode);\n" % (x["API"]["seed"]))
        C.write(
            "Std_ReturnType %s(const uint8_t *key, Dcm_NegativeResponseCodeType *errorCode);\n\n" % (x["API"]["key"])
        )


def gen_security_access_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_SecLevelConfigType, DCM_CONST) Dcm_SecLevelConfigs[] = {\n")
    for x in cfg["securities"]:
        C.write("  {\n")
        C.write("    %s,\n" % (x["API"]["seed"]))
        C.write("    %s,\n" % (x["API"]["key"]))
        C.write("    DCM_SEC_LEVEL%s,\n" % (x["level"]))
        C.write("    %s,\n" % (x.get("size", 4)))
        C.write("    %s,\n" % (x.get("size", 4)))
        C.write("    %s,\n" % (get_session(x)))
        C.write("  },\n")
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_SecurityAccessConfigType, DCM_CONST) Dcm_SecurityAccessConfig = {\n")
    C.write("  Dcm_SecLevelConfigs,\n")
    C.write("  ARRAY_SIZE(Dcm_SecLevelConfigs),\n")
    C.write("};\n\n")


def gen_write_did_api(C, service, cfg):
    for did in service["DIDs"]:
        C.write("Std_ReturnType %s(Dcm_OpStatusType opStatus, uint8_t *data, uint16_t length,\n" % (did["API"]))
        C.write("                   Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_write_did_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_WriteDIDType, DCM_CONST) Dcm_WriteDIDs[] = {\n")
    for did in service["DIDs"]:
        C.write("  {\n")

        C.write("    0x%X,\n" % (toNum(did["id"])))
        C.write("    %s,\n" % (did["size"]))
        C.write("    %s,\n" % (did["API"]))
        C.write("    {\n")
        C.write("      %s,\n" % (get_session(did)))
        C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
        C.write("      %s,\n" % (get_security(did, cfg)))
        C.write("#endif\n")
        C.write("      %s,\n" % (get_misc(did, False)))
        C.write("    },\n")
        C.write("  },\n")
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_WriteDIDConfigType, DCM_CONST) Dcm_WriteDataByIdentifierConfig = {\n")
    C.write("  Dcm_WriteDIDs,\n")
    C.write("  ARRAY_SIZE(Dcm_WriteDIDs),\n")
    C.write("};\n\n")


def gen_routine_control_api(C, service, cfg):
    for x in service["routines"]:
        for api in [x["API"]["start"], x["API"].get("stop", "NULL"), x["API"].get("result", "NULL")]:
            if api == "NULL":
                continue
            C.write("Std_ReturnType %s(const uint8_t *dataIn, Dcm_OpStatusType OpStatus,\n" % (api))
            C.write("                          uint8_t *dataOut, uint16_t *currentDataLength,\n")
            C.write("                          Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_routine_control_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_RoutineControlType, DCM_CONST) Dcm_RoutineControls[] = {\n")
    for x in service["routines"]:
        C.write("  {\n")
        C.write("    0x%X,\n" % (toNum(x["id"])))
        C.write("    %s,\n" % (x["API"]["start"]))
        C.write("    %s,\n" % (x["API"].get("stop", "NULL")))
        C.write("    %s,\n" % (x["API"].get("result", "NULL")))
        C.write("    {\n")
        C.write("      %s,\n" % (get_session(x)))
        C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
        C.write("      %s,\n" % (get_security(x, cfg)))
        C.write("#endif\n")
        C.write("      %s,\n" % (get_misc(x, False)))
        C.write("    },\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dcm_RoutineControlConfigType, DCM_CONST) Dcm_RoutineControlConfig = {\n")
    C.write("  Dcm_RoutineControls,\n")
    C.write("  ARRAY_SIZE(Dcm_RoutineControls),\n")
    C.write("};\n\n")


def gen_request_download_api(C, service, cfg):
    C.write("Std_ReturnType %s(Dcm_OpStatusType OpStatus, uint8_t DataFormatIdentifier,\n" % (service["API"]))
    C.write("                                     uint8_t MemoryIdentifier, uint32_t MemoryAddress,\n")
    C.write("                                     uint32_t MemorySize, uint32_t *BlockLength,\n")
    C.write("                                     Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_request_download_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_RequestDownloadConfigType, DCM_CONST) Dcm_RequestDownloadConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("};\n")


def gen_request_upload_api(C, service, cfg):
    C.write("Std_ReturnType %s(Dcm_OpStatusType OpStatus, uint8_t DataFormatIdentifier,\n" % (service["API"]))
    C.write("                                     uint8_t MemoryIdentifier, uint32_t MemoryAddress,\n")
    C.write("                                     uint32_t MemorySize, uint32_t *BlockLength,\n")
    C.write("                                     Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_request_upload_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_RequestUploadConfigType, DCM_CONST) Dcm_RequestUploadConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("};\n")


def gen_transfer_data_api(C, service, cfg):
    if service["API"].get("write", "NULL") != "NULL":
        C.write("Dcm_ReturnWriteMemoryType %s(Dcm_OpStatusType OpStatus,\n" % (service["API"]["write"]))
        C.write("                             uint8_t MemoryIdentifier,\n")
        C.write("                             uint32_t MemoryAddress, uint32_t MemorySize,\n")
        C.write("                             const Dcm_RequestDataArrayType MemoryData,\n")
        C.write("                             Dcm_NegativeResponseCodeType *errorCode);\n\n")
    if service["API"].get("read", "NULL") != "NULL":
        C.write("Dcm_ReturnReadMemoryType %s(Dcm_OpStatusType OpStatus,\n" % (service["API"]["read"]))
        C.write("                            uint8_t MemoryIdentifier,\n")
        C.write("                            uint32_t MemoryAddress, uint32_t MemorySize,\n")
        C.write("                            Dcm_RequestDataArrayType MemoryData,\n")
        C.write("                            Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_transfer_data_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_TransferDataConfigType, DCM_CONST) Dcm_TransferDataConfig = {\n")
    C.write("  %s,\n" % (service["API"]["write"]))
    C.write("  %s,\n" % (service["API"]["read"]))
    C.write("};\n\n")


def gen_request_transfer_exit_api(C, service, cfg):
    C.write("Std_ReturnType %s(Dcm_OpStatusType OpStatus,\n" % (service["API"]))
    C.write("                  Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_request_transfer_exit_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_TransferExitConfigType, DCM_CONST) Dcm_RequestTransferExitConfig = {\n")
    C.write("  %s,\n" % (service["API"]))
    C.write("};\n\n")


def gen_read_dtc_config(C, service, cfg):
    C.write("#ifdef USE_DEM\n")
    C.write("static CONSTANT(Dcm_ReadDTCSubFunctionConfigType, DCM_CONST) Dcm_ReadDTCSubFunctions[] = {\n")
    ReadDtcSupportedSubFunctions = [
        {"name": "ReportNumberOfDTCByStatusMask", "type": "0x01"},
        {"name": "ReportDTCByStatusMask", "type": "0x02"},
        {"name": "ReportDTCSnapshotIdentification", "type": "0x03"},
        {"name": "ReportDTCSnapshotRecordByDTCNumber", "type": "0x04"},
        {"name": "ReportDTCExtendedDataRecordByDTCNumber", "type": "0x06"},
        {"name": "ReportSupportedDTC", "type": "0x0A"},
        {"name": "ReportMirrorMemoryDTCByStatusMask", "type": "0x0F"},
        {"name": "ReportMirrorMemoryDTCExtendedDataRecordByDTCNumber", "type": "0x10"},
        {"name": "ReportNumberOfMirrorMemoryDTCByStatusMask", "type": "0x11"},
    ]
    ReadDtcSubFunctions = cfg.get("ReadDtcSubFunctions", ReadDtcSupportedSubFunctions)
    if len(ReadDtcSubFunctions) == 0:
        ReadDtcSubFunctions = service.get("functions", ReadDtcSupportedSubFunctions)
    if len(ReadDtcSubFunctions) == 0:
        ReadDtcSubFunctions = ReadDtcSupportedSubFunctions
    for subf in ReadDtcSubFunctions:
        if "MirrorMemory" in subf["name"]:
            C.write("  #ifdef DEM_USE_MIRROR_MEMORY\n")
        C.write("  {\n")
        C.write("    Dem_Dsp%s,\n" % (subf["name"]))
        C.write("    {\n")
        C.write("      %s,\n" % (get_session(subf)))
        C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
        C.write("      %s,\n" % (get_security(subf, cfg)))
        C.write("#endif\n")
        C.write("      %s,\n" % (get_misc(subf, False)))
        C.write("    },\n")
        C.write("    %s,\n" % (subf["type"]))
        C.write("  },\n")
        if "MirrorMemory" in subf["name"]:
            C.write("  #endif\n")
    C.write("};\n")
    C.write("static CONSTANT(Dcm_ReadDTCInfoConfigType, DCM_CONST) Dcm_ReadDTCInformationConfig = {\n")
    C.write("  Dcm_ReadDTCSubFunctions,\n")
    C.write("  ARRAY_SIZE(Dcm_ReadDTCSubFunctions),\n")
    C.write("};\n")
    C.write("#endif\n\n")


def gen_ioctl_api(C, service, cfg):
    for ioctl in service["IOCTLs"]:
        for x in ioctl["actions"]:
            C.write("Std_ReturnType %s(uint8_t *ControlRecord, uint16_t length,\n" % (x["API"]))
            C.write("                  uint8_t *resData, uint16_t *resDataLen,\n")
            C.write("                  uint8_t *nrc);\n\n")


def gen_ioctl_config(C, service, cfg):
    C.write("static Dcm_IOControlContextType Dcm_IOCtrlContexts[%s];\n" % (len(service["IOCTLs"])))
    C.write("static CONSTANT(Dcm_IOControlType, DCM_CONST) Dcm_IOCtrls[] = {\n")
    for i, ioctl in enumerate(service["IOCTLs"]):
        C.write("  {\n")
        C.write("    &Dcm_IOCtrlContexts[%s],\n" % (i))
        C.write("    0x%X,\n" % (toNum(ioctl["id"])))
        actionMap = {toNum(x["id"]): x for x in ioctl["actions"]}
        for id, name in enumerate(
            ["ReturnControlToEcu", "ResetToDefault", "FreezeCurrentState", "ShortTermAdjustment"]
        ):
            if id in actionMap:
                C.write("    %s, /* %s */\n" % (actionMap[id]["API"], name))
            else:
                C.write("    NULL, /* %s */\n" % (name))
        C.write("    {\n")
        C.write("      %s,\n" % (get_session(ioctl)))
        C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
        C.write("      %s,\n" % (get_security(ioctl, cfg)))
        C.write("#endif\n")
        C.write("      %s,\n" % (get_misc(ioctl, False)))
        C.write("    },\n")
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dcm_IOControlConfigType, DCM_CONST) Dcm_IOControlByIdentifierConfig = {\n")
    C.write("  Dcm_IOCtrls,\n")
    C.write("  ARRAY_SIZE(Dcm_IOCtrls),\n")
    C.write("};\n\n")


def gen_communication_control_api(C, service, cfg):
    for x in service["functions"]:
        C.write("Std_ReturnType %s(uint8_t comType,\n" % (x["API"]))
        C.write("                  Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_communication_control_config(C, service, cfg):
    C.write("static CONSTANT(Dcm_ComCtrlType, DCM_CONST) Dcm_ComCtrls[] = {\n")
    for x in service["functions"]:
        C.write("  {\n")
        C.write("    %s,\n" % (x["id"]))
        C.write("    %s,\n" % (x["API"]))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dcm_CommunicationControlConfigType, DCM_CONST) Dcm_CommunicationControlConfig = {\n")
    C.write("  Dcm_ComCtrls,\n")
    C.write("  ARRAY_SIZE(Dcm_ComCtrls),\n")
    C.write("};\n\n")


def gen_authentication_api(C, service, cfg):
    for x in service["functions"]:
        if "API" in x:  # user defined authentication
            C.write("Std_ReturnType %s(Dcm_OpStatusType OpStatus,\n" % (x["API"]))
            C.write("                  const uint8_t *dataIn, uint16_t dataInLen,\n")
            C.write("                  uint8_t *dataOut, uint16_t *dataOutLen,\n")
            C.write("                  Dcm_NegativeResponseCodeType *errorCode);\n\n")


def gen_authentication_config(C, service, cfg):
    AuthDefaultApi = {
        0x00: "Dcm_DspDeAuthentication",
        0x01: "Dcm_DspVerifyCertificateUnidirectional",
        0x02: "Dcm_DspVerifyCertificateBidirectional",
        0x03: "Dcm_DspProofOfOwnership",
        0x04: "Dcm_DspTransmitCertificate",
        0x05: "Dcm_DspRequestChallengeForAuthentication",
        0x06: "Dcm_DspVerifyProofOfOwnershipUnidirectional",
        0x07: "Dcm_DspVerifyProofOfOwnershipBidirectional",
        0x08: "Dcm_DspAuthenticationConfiguration",
    }
    C.write("#ifdef USE_CRYPTO\n")
    C.write("static CONSTANT(Dcm_AuthenticationType, DCM_CONST) Dcm_Authentications[] = {\n")
    for x in service["functions"]:
        C.write("  {\n")
        C.write("    %s,\n" % (x["id"]))
        C.write("    %s,\n" % (x.get("API", AuthDefaultApi[toNum(x["id"])])))
        C.write("  },\n")
    C.write("};\n\n")
    C.write("static CONSTANT(Dcm_AuthenticationConfigType, DCM_CONST) Dcm_AuthenticationConfig = {\n")
    C.write("  Dcm_Authentications,\n")
    C.write("  ARRAY_SIZE(Dcm_Authentications),\n")
    C.write("};\n\n")
    C.write("#endif\n\n")


ServiceMap = {
    0x10: {
        "name": "DIAGNOSTIC_SESSION_CONTROL",
        "subfunc": True,
        "API": "SessionControl",
        "config": gen_session_control_config,
        "api": gen_session_control_api,
    },
    0x11: {
        "name": "ECU_RESET",
        "subfunc": True,
        "API": "EcuReset",
        "config": gen_ecu_reset_config,
        "api": gen_ecu_reset_api,
    },
    0x14: {
        "name": "CLEAR_DIAGNOSTIC_INFORMATION",
        "subfunc": False,
        "API": "ClearDTC",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0x19: {
        "name": "READ_DTC_INFORMATION",
        "subfunc": True,
        "API": "ReadDTCInformation",
        "config": gen_read_dtc_config,
        "api": gen_dummy_api,
    },
    0x22: {
        "name": "READ_DATA_BY_IDENTIFIER",
        "subfunc": False,
        "API": "ReadDataByIdentifier",
        "config": gen_read_did_config,
        "api": gen_read_did_api,
    },
    0x23: {
        "name": "READ_MEMORY_BY_ADDRESS",
        "subfunc": False,
        "API": "ReadMemoryByAddress",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0x24: {
        "name": "READ_SCALING_DATA_BY_IDENTIFIER",
        "subfunc": False,
        "API": "ReadScalingDataByIdentifier",
        "config": gen_read_scaling_did_config,
        "api": gen_read_scaling_did_api,
    },
    0x27: {
        "name": "SECURITY_ACCESS",
        "subfunc": True,
        "API": "SecurityAccess",
        "config": gen_security_access_config,
        "api": gen_security_access_api,
    },
    0x28: {
        "name": "COMMUNICATION_CONTROL",
        "subfunc": True,
        "API": "CommunicationControl",
        "config": gen_communication_control_config,
        "api": gen_communication_control_api,
    },
    0x29: {
        "name": "AUTHENTICATION",
        "subfunc": True,
        "API": "Authentication",
        "config": gen_authentication_config,
        "api": gen_authentication_api,
    },
    0x2A: {
        "name": "READ_DATA_BY_PERIODIC_IDENTIFIER",
        "subfunc": False,
        "API": "ReadDataByPeriodicIdentifier",
        "config": gen_read_periodic_did_config,
        "api": gen_read_periodic_did_api,
    },
    0x2C: {
        "name": "DYNAMICALLY_DEFINE_DATA_IDENTIFIER",
        "subfunc": True,
        "API": "DynamicallyDefineDataIdentifier",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0x2E: {
        "name": "WRITE_DATA_BY_IDENTIFIER",
        "subfunc": False,
        "API": "WriteDataByIdentifier",
        "config": gen_write_did_config,
        "api": gen_write_did_api,
    },
    0x2F: {
        "name": "INPUT_OUTPUT_CONTROL_BY_IDENTIFIER",
        "subfunc": False,
        "API": "IOControlByIdentifier",
        "config": gen_ioctl_config,
        "api": gen_ioctl_api,
    },
    0x31: {
        "name": "ROUTINE_CONTROL",
        "subfunc": True,
        "API": "RoutineControl",
        "config": gen_routine_control_config,
        "api": gen_routine_control_api,
    },
    0x34: {
        "name": "REQUEST_DOWNLOAD",
        "subfunc": False,
        "API": "RequestDownload",
        "config": gen_request_download_config,
        "api": gen_request_download_api,
    },
    0x35: {
        "name": "REQUEST_UPLOAD",
        "subfunc": False,
        "API": "RequestUpload",
        "config": gen_request_upload_config,
        "api": gen_request_upload_api,
    },
    0x36: {
        "name": "TRANSFER_DATA",
        "subfunc": False,
        "API": "TransferData",
        "config": gen_transfer_data_config,
        "api": gen_transfer_data_api,
    },
    0x37: {
        "name": "REQUEST_TRANSFER_EXIT",
        "subfunc": False,
        "API": "RequestTransferExit",
        "config": gen_request_transfer_exit_config,
        "api": gen_request_transfer_exit_api,
    },
    0x3D: {
        "name": "WRITE_MEMORY_BY_ADDRESS",
        "subfunc": False,
        "API": "WriteMemoryByAddress",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0x3E: {
        "name": "TESTER_PRESENT",
        "subfunc": True,
        "API": "TesterPresent",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
    0x85: {
        "name": "CONTROL_DTC_SETTING",
        "subfunc": True,
        "API": "ControlDTCSetting",
        "config": gen_dummy_config,
        "api": gen_dummy_api,
    },
}


def preprocess(cfg):
    for x in cfg["services"]:
        x["id"] = toNum(x["id"])
        if x["id"] == 0x2A:
            for did in x["DIDs"]:
                did["id"] = 0xF200 + (toNum(did["id"]) & 0xFF)
        elif x["id"] == 0x22:
            for did in x["DIDs"]:
                did["id"] = toNum(did["id"])
    for x in cfg["sessions"]:
        x["id"] = toNum(x["id"])


def get_all_readable_dids(cfg):
    DIDs = []
    for sx in cfg["services"]:
        if sx["id"] == 0x22:
            DIDs += sx["DIDs"]
        elif sx["id"] == 0x2A:
            DIDs += sx["DIDs"]
    return DIDs


def Gen_Dcm(cfg, dir):
    preprocess(cfg)
    H = open("%s/Dcm_Cfg.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef DCM_CFG_H\n")
    H.write("#define DCM_CFG_H\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("#ifdef USE_CANTP\n")
    H.write('#include "CanTp_Cfg.h"\n')
    H.write("#endif\n")
    H.write("#ifdef USE_LINTP\n")
    H.write('#include "LinTp_Cfg.h"\n')
    H.write("#endif\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("#ifndef DCM_CONST\n")
    H.write("#define DCM_CONST\n")
    H.write("#endif\n\n")
    chls = cfg.get("channels", [{"name": "P2P"}, {"name": "P2A"}])
    for idx, chl in enumerate(chls):
        H.write("#define DCM_%s_PDU %s\n" % (chl["name"], idx))
        H.write("#define DCM_%s_RX %s\n" % (chl["name"], idx))
        H.write("#define DCM_%s_TX %s\n" % (chl["name"], idx))
    iMask = 4
    for session in cfg["sessions"]:
        H.write("#ifndef DCM_%s_SESSION\n" % (toMacro(session["name"])))
        H.write("#define DCM_%s_SESSION %s\n" % (toMacro(session["name"]), hex(session["id"])))
        H.write("#endif\n")
        H.write("#ifndef DCM_%s_MASK\n" % (toMacro(session["name"])))
        if session["id"] > 4:
            H.write("#define DCM_%s_MASK %s\n" % (toMacro(session["name"]), hex(1 << iMask)))
            iMask += 1
        else:
            H.write("#define DCM_%s_MASK %s\n" % (toMacro(session["name"]), hex(1 << (session["id"] - 1))))
        H.write("#endif\n")
    H.write("\n")

    H.write("#ifndef DCM_MAIN_FUNCTION_PERIOD\n")
    H.write("#define DCM_MAIN_FUNCTION_PERIOD %s\n" % (cfg.get("MainFunctionPeriod", 10)))
    H.write("#endif\n")
    H.write("#define DCM_CONVERT_MS_TO_MAIN_CYCLES(x) \\\n")
    H.write("  ((x + DCM_MAIN_FUNCTION_PERIOD - 1) / DCM_MAIN_FUNCTION_PERIOD)\n\n")

    H.write("#define Dcm_DslCustomerSession2Mask(mask, sesCtrl) \\\n")
    for i, session in enumerate(cfg["sessions"]):
        if session["id"] > 4:
            H.write("  if (DCM_%s_SESSION == sesCtrl) { \\\n" % (toMacro(session["name"])))
            H.write("    mask = DCM_%s_MASK; \\\n" % (toMacro(session["name"])))
            H.write("  }\\\n")
    H.write("\n\n")

    for service in cfg["services"]:
        isDtcRelated = ServiceMap[service["id"]]["name"] in [
            "CLEAR_DIAGNOSTIC_INFORMATION",
            "READ_DTC_INFORMATION",
            "CONTROL_DTC_SETTING",
        ]
        isCryptoRelated = ServiceMap[service["id"]]["name"] in ["AUTHENTICATION"]
        if isDtcRelated:
            H.write("#ifdef USE_DEM\n")
        elif isCryptoRelated:
            H.write("#ifdef USE_CRYPTO\n")
        H.write("#define DCM_USE_SERVICE_%s\n" % (ServiceMap[service["id"]]["name"]))
        if isDtcRelated or isCryptoRelated:
            H.write("#endif\n")
    sec = cfg.get("security", {"NumAtt": 3, "DelayTime": 3000, "SeedProtection": True})
    H.write("%s#define DCM_USE_SECURITY_SEED_PROTECTION\n\n" % ("" if sec.get("SeedProtection", True) else "// "))
    maxSeedSize = 4
    for x in cfg["securities"]:
        s = x.get("size", 4)
        if s > maxSeedSize:
            maxSeedSize = s
    H.write("#define DCM_MAX_SEED_SIZE %s\n" % (maxSeedSize))
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* DCM_CFG_H */\n")
    H.close()

    C = open("%s/Dcm_Cfg.c" % (dir), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "Dcm.h"\n')
    C.write("#ifdef USE_NVM\n")
    C.write('#include "NvM_Cfg.h"\n')
    C.write("#endif\n")
    C.write("#ifdef USE_DEM\n")
    C.write('#include "Dem_Cfg.h"\n')
    C.write("#endif\n")
    C.write('#include "Dcm_Cfg.h"\n')
    C.write('#include "Dcm_Priv.h"\n')
    C.write("#include <string.h>\n")
    C.write("#ifdef USE_PDUR\n")
    C.write('#include "PduR_Cfg.h"\n')
    C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    for idx, chl in enumerate(chls):
        C.write("#ifndef PDUR_%s_TX\n" % (chl["name"]))
        C.write("#define PDUR_%s_TX %s\n" % (chl["name"], idx))
        C.write("#endif\n")
    rDIDs = get_all_readable_dids(cfg)
    numOfDDDID = get_number_of_DDDID(cfg)
    for i, did in enumerate(rDIDs):
        C.write("#define DCM_RDID_%X_INDEX %d\n" % (toNum(did["id"]), i))
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    ServiceVerification = cfg.get("ServiceVerification", "NULL")
    if ServiceVerification != "NULL":
        C.write(
            "Std_ReturnType %s(PduIdType RxPduId, uint8_t *payload, PduLengthType length, Dcm_NegativeResponseCodeType *nrc);\n\n"
            % (ServiceVerification)
        )
    for service in cfg["services"]:
        ServiceMap[service["id"]]["api"](C, service, cfg)
    for i in range(numOfDDDID):
        C.write("static Std_ReturnType Dcm_DspReadDDDID_%s(Dcm_OpStatusType opStatus, uint8_t *data,\n" % (i))
        C.write("                                          uint16_t length,\n")
        C.write("                                          Dcm_NegativeResponseCodeType *errorCode);\n\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#define DCM_START_SEC_CONST\n")
    C.write('#include "Dcm_MemMap.h"\n')
    C.write("static uint8_t rxBuffer[%s];\n" % (cfg["buffer"]["rx"]))
    C.write("static uint8_t txBuffer[%s];\n\n" % (cfg["buffer"]["tx"]))
    mems = cfg.get("memories", [])
    if len(mems):
        C.write(
            "#if defined(DCM_USE_SERVICE_READ_MEMORY_BY_ADDRESS) || defined(DCM_USE_SERVICE_WRITE_MEMORY_BY_ADDRESS)\n"
        )
        C.write("static CONSTANT(Dcm_DspMemoryRangeInfoType, DCM_CONST) Dcm_DspMemoryRangeInfos[] = {\n")
        for mem in mems:
            attr = "0"
            at = mem.get("attr", "rw")
            if "r" in at:
                attr = "DCM_MEM_ATTR_READ"
            if "w" in at:
                attr += "|DCM_MEM_ATTR_WRITE"
            C.write("  {\n")
            C.write("    %sUL,\n" % (mem["low"]))
            C.write("    %sUL,\n" % (mem["high"]))
            C.write("    %s,\n" % (attr))
            C.write("    {\n")
            C.write("      %s,\n" % (get_session(mem)))
            C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
            C.write("      %s,\n" % (get_security(mem, cfg)))
            C.write("#endif\n")
            C.write("      %s,\n" % (get_misc(mem, False)))
            C.write("    },\n")
            C.write("  },\n")
        C.write("};\n\n")
        fmts = cfg.get("memory.format", [])
        if len(fmts):
            C.write("static CONSTANT(uint8_t, DCM_CONST) Dcm_DspAddressAndLengthFormatIdentifiers[] = {\n")
            for x in fmts:
                C.write("  %s,\n" % (x))
            C.write("};\n\n")
        C.write("static CONSTANT(Dcm_DspMemoryConfigType, DCM_CONST) Dcm_DspMemoryConfig = {\n")
        if len(fmts):
            C.write("  Dcm_DspAddressAndLengthFormatIdentifiers,\n")
            C.write("  ARRAY_SIZE(Dcm_DspAddressAndLengthFormatIdentifiers),\n")
        else:
            C.write("  NULL,\n")
            C.write("  0,\n")
        C.write("  Dcm_DspMemoryRangeInfos,\n")
        C.write("  ARRAY_SIZE(Dcm_DspMemoryRangeInfos),\n")
        C.write("};\n\n")
        C.write("#endif\n\n")
    if numOfDDDID > 0:
        C.write("static Dcm_rDIDConfigType Dcm_rDDDIDConfigs[%s];\n" % (numOfDDDID))
        C.write("static Dcm_DDDIDContextType Dcm_DDDIDContexts[%s];\n" % (numOfDDDID))
        C.write("static CONSTANT(Dcm_DDDIDConfigType, DCM_CONST) Dcm_DDDIDConfigs[] = {\n")
        for i in range(numOfDDDID):
            C.write("  {\n")
            C.write("     &Dcm_rDDDIDConfigs[%s],\n" % (i))
            C.write("     &Dcm_DDDIDContexts[%s],\n" % (i))
            C.write("     Dcm_DspReadDDDID_%s,\n" % (i))
            C.write("  },\n")
        C.write("};\n\n")
    C.write("static CONSTANT(Dcm_rDIDConfigType, DCM_CONST) Dcm_rDIDConfigs[] = {\n")
    for did in rDIDs:
        C.write("  {\n")
        C.write("    0x%X,\n" % (did["id"]))
        C.write("    %s,\n" % (did["size"]))
        C.write("    %s,\n" % (did["API"]))
        C.write("    {\n")
        C.write("      %s,\n" % (get_session(did)))
        C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
        C.write("      %s,\n" % (get_security(did, cfg)))
        C.write("#endif\n")
        C.write("      %s,\n" % (get_misc(did, False)))
        C.write("    },\n")
        C.write("  },\n")
    C.write("};\n\n")
    for service in cfg["services"]:
        ServiceMap[service["id"]]["config"](C, service, cfg)
    C.write("static CONSTANT(Dcm_ServiceType, DCM_CONST) Dcm_UdsServices[] = {\n")
    for service in cfg["services"]:
        isDtcRelated = ServiceMap[service["id"]]["name"] in [
            "CLEAR_DIAGNOSTIC_INFORMATION",
            "READ_DTC_INFORMATION",
            "CONTROL_DTC_SETTING",
        ]
        isCryptoRelated = ServiceMap[service["id"]]["name"] in ["AUTHENTICATION"]
        if isDtcRelated:
            C.write("#ifdef USE_DEM\n")
        elif isCryptoRelated:
            C.write("#ifdef USE_CRYPTO\n")
        C.write("  {\n")
        C.write("    SID_%s,\n" % (ServiceMap[service["id"]]["name"]))
        C.write("    {\n")
        C.write("      %s,\n" % (get_session(service)))
        C.write("#ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
        C.write("      %s,\n" % (get_security(service, cfg)))
        C.write("#endif\n")
        C.write("      %s,\n" % (get_misc(service)))
        C.write("    },\n")
        C.write("    Dcm_Dsp%s,\n" % (ServiceMap[service["id"]]["API"]))
        if ServiceMap[service["id"]]["config"] == gen_dummy_config:
            C.write("    NULL,\n")
        else:
            C.write("    (const void DCM_CONST *)&Dcm_%sConfig,\n" % (ServiceMap[service["id"]]["API"]))
        C.write("  },\n")
        if isDtcRelated or isCryptoRelated:
            C.write("#endif\n")
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_ServiceTableType, DCM_CONST) Dcm_UdsServiceTable = {\n")
    C.write("  Dcm_UdsServices,\n")
    C.write("  ARRAY_SIZE(Dcm_UdsServices),\n")
    C.write("};\n\n")

    C.write("static CONSTP2CONST(Dcm_ServiceTableType, DCM_CONST, DCM_CONST) Dcm_ServiceTables[] = {\n")
    C.write("  &Dcm_UdsServiceTable,\n")
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_TimingConfigType, DCM_CONST) Dcm_TimingConfig = {\n")
    timings = cfg.get("timings", {})
    C.write("  DCM_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (timings.get("S3Server", 5000)))
    C.write("  DCM_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (timings.get("P2ServerAjust", 20)))
    C.write("  DCM_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (timings.get("P2StarServerAdjust", 100)))
    C.write("  DCM_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (timings.get("P2ServerMax", 50)))
    C.write("  DCM_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (timings.get("P2StarServerMax", 5000)))
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_DslDiagRespConfigType, DCM_CONST) Dcm_DslDiagRespConfig = {\n")
    C.write("  %s,\n" % (cfg.get("MaxNumRespPend", 8)))
    C.write("};\n\n")

    C.write("static CONSTANT(Dcm_ChannelType, DCM_CONST) Dcm_Channels[] = {\n")
    for chl in chls:
        C.write("  {\n")
        C.write("     PDUR_%s_TX,\n" % (chl["name"]))
        if "P2A" in chl["name"]:  # deduce type from the name
            C.write("     DCM_FUNCTIONAL_REQUEST,\n")
        else:
            C.write("     DCM_PHYSICAL_REQUEST,\n")
        C.write("  },\n")
    C.write("};\n\n")

    C.write("CONSTANT(Dcm_ConfigType, DCM_CONST) Dcm_Config = {\n")
    C.write("  %s,\n" % (cfg.get("ServiceVerification", "NULL")))
    C.write("  rxBuffer,\n")
    C.write("  txBuffer,\n")
    C.write("  sizeof(rxBuffer),\n")
    C.write("  sizeof(txBuffer),\n")
    C.write("  Dcm_Channels,\n")
    C.write("  ARRAY_SIZE(Dcm_Channels),\n")
    C.write("  Dcm_ServiceTables,\n")
    C.write("  ARRAY_SIZE(Dcm_ServiceTables),\n")
    C.write("  &Dcm_TimingConfig,\n")
    C.write("  &Dcm_DslDiagRespConfig,\n")
    C.write("  #ifdef DCM_USE_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER\n")
    C.write("  Dcm_DDDIDConfigs,\n")
    C.write("  ARRAY_SIZE(Dcm_DDDIDConfigs),\n")
    C.write("  Dcm_rDIDConfigs,\n")
    C.write("  ARRAY_SIZE(Dcm_rDIDConfigs),\n")
    C.write("  #endif\n")
    C.write("  #ifdef DCM_USE_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER\n")
    C.write("  &Dcm_ReadDataByPeriodicIdentifierConfig,\n")
    C.write("  #endif\n")
    C.write("  #ifdef DCM_USE_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER\n")
    C.write("  &Dcm_IOControlByIdentifierConfig,\n")
    C.write("  #endif\n")
    C.write(
        "  #if defined(DCM_USE_SERVICE_READ_MEMORY_BY_ADDRESS) || defined(DCM_USE_SERVICE_WRITE_MEMORY_BY_ADDRESS)\n"
    )
    C.write("  &Dcm_DspMemoryConfig,\n")
    C.write("  #endif\n")
    C.write("  #ifdef DCM_USE_SERVICE_SECURITY_ACCESS\n")
    C.write("  %s,\n" % (sec.get("NumAtt", 3)))
    C.write("  DCM_CONVERT_MS_TO_MAIN_CYCLES(%s),\n" % (sec.get("DelayTime", 3000)))
    C.write("  #ifdef USE_NVM\n")
    C.write("  NVM_BLOCKID_Dcm_NvmSecurityAccess\n")
    C.write("  #endif\n")
    C.write("  #endif\n")
    C.write("};\n\n")
    C.write("#define DCM_STOP_SEC_CONST\n")
    C.write('#include "Dcm_MemMap.h"\n')
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    for i in range(numOfDDDID):
        C.write("static Std_ReturnType Dcm_DspReadDDDID_%s(Dcm_OpStatusType opStatus, uint8_t *data,\n" % (i))
        C.write("                                          uint16_t length,\n")
        C.write("                                          Dcm_NegativeResponseCodeType *errorCode) {\n")
        C.write("  return Dcm_DspReadDDDID(&Dcm_DDDIDConfigs[%s], opStatus, data, length, errorCode);\n" % (i))
        C.write("}\n\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write("Std_ReturnType Dcm_Transmit(const uint8_t *buffer, PduLengthType length, int functional) {\n")
    C.write("  Std_ReturnType r = E_NOT_OK;\n")
    C.write("  Dcm_ContextType *context = Dcm_GetContext();\n\n")

    C.write("  if ((DCM_BUFFER_IDLE == context->txBufferState) && (Dcm_Config.txBufferSize >= length)) {\n")
    C.write("    r = E_OK;\n")
    C.write("    if (functional) {\n")
    C.write("      context->curPduId = DCM_P2A_PDU;\n")
    C.write("    } else {\n")
    C.write("      context->curPduId = DCM_P2P_PDU;\n")
    C.write("    }\n")
    C.write("    memcpy(Dcm_Config.txBuffer, buffer, (size_t)length);\n")
    C.write("    context->TxTpSduLength = (PduLengthType)length;\n")
    C.write("    context->txBufferState = DCM_BUFFER_FULL;\n")
    C.write("  }\n\n")

    C.write("  return r;\n")
    C.write("}\n")
    C.close()


def is_nan(nan):
    return nan != nan


def process_general(cfg, general):
    lastCat = float("nan")
    cfg["timings"] = {}
    sessionNames = []
    ApiPrefix = "App"
    for i in range(general.shape[0]):
        row = general.iloc[i]
        cat = row["Catalog"]
        attr = row["Attribue"]
        value = row["Value"]
        cat = lastCat if is_nan(cat) else cat
        if attr == "ApiPrefix":
            cfg["ApiPrefix"] = value
            ApiPrefix = value
            cfg["ServiceVerification"] = "App_DcmServiceVerification"
        elif cat in ["timings", "security", "buffer"]:
            if cat not in cfg:
                cfg[cat] = {}
            cfg[cat][attr] = value
        elif cat in ["channels"]:
            if cat not in cfg:
                cfg[cat] = []
            if not is_nan(attr):
                cfg[cat].append({"name": attr})
        elif cat in ["sessions"]:
            if cat not in cfg:
                cfg[cat] = []
            if type(attr) == str and attr.strip() != "":
                cfg[cat].append({"name": attr.strip(), "id": value})
                sessionNames.append(attr)
        elif cat in ["securities"]:
            if cat not in cfg:
                cfg[cat] = []
            if type(attr) == str and attr.strip() not in ["", "name"]:
                size = row["Unit"]
                sessions = []
                for idx, key in enumerate(["Comment"] + ["Unnamed: %s" % (5 + i) for i in range(len(sessionNames))]):
                    if key in row:
                        if "Y" == row[key]:
                            sessions.append(sessionNames[idx])
                cfg[cat].append(
                    {
                        "name": attr.strip(),
                        "level": value,
                        "size": size,
                        "sessions": sessions,
                        "API": {
                            "seed": "%s_Get%sLevelSeed" % (ApiPrefix, attr),
                            "key": "%s_Compare%sLevelKey" % (ApiPrefix, attr),
                        },
                    }
                )
        else:
            cfg[attr] = value

        if not is_nan(cat):
            lastCat = cat


def get_acs(attrs):
    access = []
    sessions = []
    securities = []
    for k, v in attrs.items():
        if k.startswith("access"):
            if v == "Y":
                access.append(k[7:])
        elif k.startswith("sessions"):
            if v == "Y":
                sessions.append(k[9:])
        elif k.startswith("securities"):
            if v == "Y":
                securities.append(k[11:])
    return access, sessions, securities


def process_services(cfg, services):
    ApiPrefix = cfg.get("ApiPrefix", "App")
    if "services" not in cfg:
        cfg["services"] = []
    row0 = services.iloc[0]
    keys0 = [row0.keys()[i] for i in range(len(row0.keys()))]
    keys1 = [row0[k] for k in keys0]
    keys = {}
    lastK0 = None
    for k0, k1 in zip(keys0, keys1):
        if not is_nan(k1) and k1.strip() != "":
            k0_ = k0
            if k0_.startswith("Unnamed: "):
                if lastK0 != None:
                    k0_ = lastK0
                else:
                    k0_ = None
            if k0_ == None:
                keys[k1] = k0
            else:
                keys[".".join([k0_.strip(), k1])] = k0
        if not k0.startswith("Unnamed: "):
            lastK0 = k0
    hasMemory = False
    for i in range(services.shape[0] - 1):
        row = services.iloc[i + 1]
        if row[keys["enabled"]] != "ON":
            continue
        access = []
        sessions = []
        securities = []
        for k1, k0 in keys.items():
            if k1.startswith("access"):
                if row[k0] == "Y":
                    access.append(k1[7:])
            elif k1.startswith("sessions"):
                if row[k0] == "Y":
                    sessions.append(k1[9:])
            elif k1.startswith("securities"):
                if row[k0] == "Y":
                    securities.append(k1[11:])
        service = {
            "name": row[keys["service name"]],
            "id": row[keys["id"]],
            "access": access,
            "sessions": sessions,
            "securities": securities,
        }
        if toNum(service["id"]) in [0x23, 0x3D]:
            hasMemory = True
        if toNum(service["id"]) == 0x10:  # Session Control
            service["API"] = "%s_GetSessionChangePermission" % (ApiPrefix)
        elif toNum(service["id"]) == 0x11:  # EcuReset
            service["delay"] = 100
            service["API"] = "%s_GetEcuResetPermission" % (ApiPrefix)
        elif toNum(service["id"]) == 0x28:  # communication control
            service["functions"] = [
                {"id": 0, "API": "%s_ComCtrlEnableRxAndTx" % (ApiPrefix)},
                {"id": 3, "API": "%s_ComCtrlDisableRxAndTx" % (ApiPrefix)},
            ]
        elif toNum(service["id"]) == 0x29:  # Authentication
            service["functions"] = [
                {"name": "deAuthenticate", "id": "0x00"},
                {"name": "verifyCertificateUnidirectional", "id": "0x01"},
                {"name": "verifyCertificateBidirectional", "id": "0x02"},
                {"name": "proofOfOwnership", "id": "0x03"},
                {"name": "transmitCertificate", "id": "0x04"},
                {"name": "requestChallengeForAuthentication", "id": "0x05"},
                {"name": "verifyProofOfOwnershipUnidirectional", "id": "0x06"},
                {"name": "verifyProofOfOwnershipBidirectional", "id": "0x07"},
                {"name": "authenticationConfiguration", "id": "0x08"},
            ]
        elif toNum(service["id"]) in [0x22, 0x24, 0x2A, 0x2E]:  # Read/Write DID
            DIDsName = {0x22: "DID", 0x24: "ScalingDID", 0x2A: "PeriodicDID", 0x2E: "DID"}
            typeName = DIDsName[toNum(service["id"])]
            DIDs = []
            for DID in cfg["matrixs"][typeName]:
                if toNum(service["id"]) in [0x24, 0x2A]:
                    DIDs.append(DID)
                elif toNum(service["id"]) == 0x22 and "r" in DID["DID.attribute"]:
                    DIDs.append(DID)
                elif toNum(service["id"]) == 0x2E and "w" in DID["DID.attribute"]:
                    DIDs.append(DID)
            service["DIDs"] = []
            for DID in DIDs:
                access, sessions, securities = get_acs(DID)
                did = {
                    "name": DID["DID.name"],
                    "id": DID["DID.ID"],
                    "size": DID["DID.length"],
                    "access": access,
                    "sessions": sessions,
                    "securities": securities,
                }
                if toNum(service["id"]) == 0x2E:
                    did["API"] = "%s_Write_%s_%s_%04X" % (ApiPrefix, typeName, DID["DID.name"], toNum(DID["DID.ID"]))
                else:
                    did["API"] = "%s_Read_%s_%s_%04X" % (ApiPrefix, typeName, DID["DID.name"], toNum(DID["DID.ID"]))
                service["DIDs"].append(did)
        elif toNum(service["id"]) == 0x2F:  # IO CTRL
            service["IOCTLs"] = []
            for x in cfg["matrixs"]["IOControl"]:
                access, sessions, securities = get_acs(x)
                ioctrl = {
                    "name": x["IO CTRL.name"],
                    "id": x["IO CTRL.ID"],
                    "actions": [],
                    "access": access,
                    "sessions": sessions,
                    "securities": securities,
                }
                for sub, action in [
                    (0, "ReturnControlToEcu"),
                    (1, "ResetToDefault"),
                    (2, "FreezeCurrentState"),
                    (3, "ShortTermAdjustment"),
                ]:
                    if x.get("Actions.%s" % (action)) == "Y":
                        ioctrl["actions"].append(
                            {
                                "id": sub,
                                "API": "%s_IOCtl_%s_%04X_%s" % (ApiPrefix, ioctrl["name"], toNum(ioctrl["id"]), action),
                            }
                        )
                service["IOCTLs"].append(ioctrl)
        elif toNum(service["id"]) == 0x31:  # Routines
            service["routines"] = []
            for x in cfg["matrixs"]["Routine"]:
                access, sessions, securities = get_acs(x)
                routine = {
                    "name": x["Rountine.name"],
                    "id": x["Rountine.ID"],
                    "access": access,
                    "sessions": sessions,
                    "securities": securities,
                }
                routine["API"] = {}
                for action in ["Start", "Stop", "Result"]:
                    if x.get("Actions.%s" % (action)) == "Y":
                        routine["API"][action.lower()] = "%s_%s_%04X_%s" % (
                            ApiPrefix,
                            routine["name"],
                            toNum(routine["id"]),
                            action,
                        )
                service["routines"].append(routine)
        cfg["services"].append(service)
    if hasMemory:
        memorys = []
        for x in cfg["matrixs"]["Memory"]:
            access, sessions, securities = get_acs(x)
            access = []
            sessions = []
            securities = []
            mem = {
                "name": x["memory.name"],
                "low": x["address.low"],
                "high": x["address.high"],
                "attr": x["address.atrribute"],
                "access": access,
                "sessions": sessions,
                "securities": securities,
            }
            memorys.append(mem)
        cfg["memories"] = memorys
        cfg["memory.format"] = [0x44]


def load_matrix(df):
    matrix = []
    row0 = df.iloc[0]
    keys0 = [row0.keys()[i] for i in range(len(row0.keys()))]
    keys1 = [row0[k] for k in keys0]
    keys = {}
    lastK0 = None
    for k0, k1 in zip(keys0, keys1):
        if not is_nan(k1) and k1.strip() != "":
            k0_ = k0
            if k0_.startswith("Unnamed: "):
                if lastK0 != None:
                    k0_ = lastK0
                else:
                    k0_ = None
            if k0_ == None:
                keys[k1] = k0
            else:
                keys[".".join([k0_.strip(), k1])] = k0
        if not k0.startswith("Unnamed: "):
            lastK0 = k0
    for i in range(df.shape[0] - 1):
        record = {}
        row = df.iloc[i + 1]
        for k1, k0 in keys.items():
            value = row[k0]
            if not is_nan(value):
                record[k1] = value
        matrix.append(record)
    return matrix


def load_matrixs(cfg, path):
    import pandas as pd

    matrixs = {}
    for mname in ["Memory", "DID", "PeriodicDID", "ScalingDID", "IOControl", "Routine"]:
        df = pd.read_excel(path, sheet_name=mname)
        matrixs[mname] = load_matrix(df)
    cfg["matrixs"] = matrixs


def load_excel(cfg, dir):
    import pandas as pd

    path = cfg["excel"]
    if not os.path.isfile(path):
        path = os.path.abspath(os.path.join(dir, "..", path))
    general = pd.read_excel(path, sheet_name="General")
    process_general(cfg, general)
    load_matrixs(cfg, path)
    services = pd.read_excel(path, sheet_name="Service")
    process_services(cfg, services)
    del cfg["matrixs"]
    del cfg["excel"]
    del cfg["ApiPrefix"]
    with open("%s/Dcm.json" % (dir), "w") as f:
        json.dump(cfg, f, indent=2)


def process(cfg, dir):
    if "excel" in cfg:
        load_excel(cfg, dir)


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    process(cfg, dir)
    Gen_Dcm(cfg, dir)
    GenMemMap("Dcm", dir)
