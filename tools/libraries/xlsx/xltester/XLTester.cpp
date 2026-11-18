/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "XLTester.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "canlib.h"

#include <libgen.h>

namespace as {
/* ================================ [ MACROS    ] ============================================== */
#define XLT_MSG_VERBOSE 0
#define XLT_MSG_DEBUG 1
#define XLT_MSG_INFO 2
#define XLT_MSG_WARN 3
#define XLT_MSG_ERROR 4

#define XLT_MSG_DEFAULT XLT_MSG_INFO

#define XLT_LOG(level, fmt, ...) AddMsg(XLT_MSG_##level, #level ": " fmt, ##__VA_ARGS__)

#define XLT_VERBOSE(fmt, ...) XLT_LOG(VERBOSE, fmt, ##__VA_ARGS__)
#define XLT_DEBUG(fmt, ...) XLT_LOG(DEBUG, fmt, ##__VA_ARGS__)
#define XLT_INFO(fmt, ...) XLT_LOG(INFO, fmt, ##__VA_ARGS__)
#define XLT_WARN(fmt, ...) XLT_LOG(WARN, fmt, ##__VA_ARGS__)
#define XLT_ERROR(fmt, ...) XLT_LOG(ERROR, fmt, ##__VA_ARGS__)
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static std::string Xlt_Get(XLT_ConfigType &cfg, std::string key, std::string dft) {
  std::string value = dft;
  auto it = cfg.find(key);
  if (it != cfg.end()) {
    value = it->second;
  }
  return value;
}

static int Xlt_StrToInt(std::string strV) {
  int value;

  if ((0 == strncmp(strV.c_str(), "0x", 2)) || (0 == strncmp(strV.c_str(), "0X", 2))) {
    value = (int)strtoul(strV.c_str(), NULL, 16);
  } else {
    value = atoi(strV.c_str());
  }

  return value;
}

std::vector<std::string> Xlt_StrSplit(const std::string &str, char delim) {
  std::stringstream ss(str);
  std::string item;
  std::vector<std::string> elems;
  while (std::getline(ss, item, delim)) {
    if (!item.empty()) {
      elems.push_back(item);
    }
  }
  return elems;
}

/* the string array must in hex format */
static int Xlt_StrToArrayU8(std::string strV, std::vector<uint8_t> &u8Arr) {
  int ret = 0;

  std::vector<std::string> strArr = Xlt_StrSplit(strV, ' ');
  u8Arr.reserve(strArr.size());
  for (auto &str : strArr) {
    u8Arr.push_back((uint8_t)strtoul(str.c_str(), NULL, 16));
  }

  return ret;
}

static int Xlt_Get(XLT_ConfigType &cfg, std::string key, int dft) {
  int value = dft;
  auto it = cfg.find(key);
  if (it != cfg.end()) {
    auto strV = it->second;
    value = Xlt_StrToInt(strV);
  }
  return value;
}

static std::string Xlt_Format(const uint8_t *data, int len) {
  static const std::map<uint8_t, std::string> nrcMap = {
    {0x00, "positive response"},
    {0x10, "general reject"},
    {0x11, "service not supported"},
    {0x12, "sub function not supported"},
    {0x13, "incorrect message length or invalid format"},
    {0x21, "busy repeat request"},
    {0x22, "conditions not correct"},
    {0x24, "request sequence error"},
    {0x31, "request out of range"},
    {0x33, "secutity access denied"},
    {0x35, "invalid key"},
    {0x36, "exceed number of attempts"},
    {0x37, "required time delay not expired"},
    {0x72, "general programming failure"},
    {0x73, "wrong block sequence counter"},
    {0x78, "response pending"},
    {0x7e, "sub function not supported in active session"},
    {0x7f, "service not supported in active session"},
    {0x81, "rpm too high"},
    {0x82, "rpm to low"},
    {0x83, "engine is running"},
    {0x84, "engine is not running"},
    {0x85, "engine run time too low"},
    {0x86, "temperature too high"},
    {0x87, "temperature too low"},
    {0x88, "vehicle speed too high"},
    {0x89, "vehicle speed too low"},
    {0x8a, "throttle pedal too high"},
    {0x8b, "throttle pedal too low"},
    {0x8c, "transmission range not in neutral"},
    {0x8d, "transmission range not in gear"},
    {0x8f, "brake switch not closed"},
    {0x90, "shifter lever not in park"},
    {0x91, "torque converter clutch locked"},
    {0x92, "voltage too high"},
    {0x93, "voltage too low"},
  };
  std::stringstream ss;
  ss << "[";
  for (int i = 0; i < len; i++) {
    ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
    if ((i + 1) < len) {
      ss << " ";
    }
  }
  ss << "]";

  if (3 == len) {
    if (0x7F == data[0]) {
      auto it = nrcMap.find(data[2]);
      if (it != nrcMap.end()) {
        ss << " nrc = " << it->second;
      }
    }
  }

  return ss.str();
}

static std::string Xlt_Format(std::vector<uint8_t> data) {
  return Xlt_Format(data.data(), (int)data.size());
}

static std::string Xlt_Hex(const uint8_t *data, int len) {
  std::stringstream ss;
  for (int i = 0; i < len; i++) {
    ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
    if ((i + 1) < len) {
      ss << " ";
    }
  }

  return ss.str();
}

static std::string Xlt_Hex(std::vector<uint8_t> data) {
  return Xlt_Hex(data.data(), (int)data.size());
}
/* ================================ [ FUNCTIONS ] ============================================== */
XLTester::XLTester() {
}

XLTester::~XLTester() {
  if (m_Thread.joinable()) {
    m_Thread.join();
  }

  if (nullptr != m_IsoTp) {
    IsoTp_Destroy(m_IsoTp);
    m_IsoTp = nullptr;
  }

  if (m_CanBusId >= 0) {
    CanL_Close(m_CanBusId);
    m_CanBusId = -1;
  }
}

void XLTester::AddMsg(int level, const char *fmt, ...) {
  va_list args;
  if (level >= XLT_MSG_DEFAULT) {
    std::lock_guard<std::mutex> guard(m_Lock);
    va_start(args, fmt);
    if (m_LogSize < (int)sizeof(m_Logs)) {
      m_LogSize += vsnprintf(&m_Logs[m_LogSize], sizeof(m_Logs) - m_LogSize, fmt, args);
    } else {
      printf("XLTester logs area too small, please enlarge it\n");
      vprintf(fmt, args);
    }
    va_end(args);
  } else {
    // va_start(args, fmt);
    // vprintf(fmt, args);
    // va_end(args);
  }
}

std::string XLTester::GetMsg() {
  std::string msg;
  std::lock_guard<std::mutex> guard(m_Lock);
  if (m_LogSize > 0) {
    msg = m_Logs;
    m_LogSize = 0;
  }
  return msg;
}

int XLTester::GetProgress() {
  if (m_Progress >= XLTESTER_PROGRESS_DONE) {
    if (m_Thread.joinable()) {
      m_Thread.join();
    }
  }
  return m_Progress;
}

int XLTester::InitIsoTp() {
  int ret = 0;

  auto protocol = Xlt_Get(m_DcmCfg, "Protocol", "CAN");
  auto device = Xlt_Get(m_DcmCfg, "Device", "simulator_v2");
  uint32_t port = Xlt_Get(m_DcmCfg, "Port", 0);
  uint32_t baudrate = Xlt_Get(m_DcmCfg, "Baudrate", 500000);
  uint32_t rxid = 0x732, txid = 0x731;
  uint16_t N_TA = Xlt_Get(m_DcmCfg, "N_TargetAddress", 0xFFFF);
  uint32_t delayUs = Xlt_Get(m_DcmCfg, "DelayUs", 0);
  auto LinkLayerDataLength = Xlt_Get(m_DcmCfg, "LinkLayerDataLength", 8);
  if (protocol == "LIN") {
    rxid = 0x3d;
    txid = 0x3c;
  }

  rxid = Xlt_Get(m_DcmCfg, "PhysicalResponseAddress", rxid);
  txid = Xlt_Get(m_DcmCfg, "PhysicalRequestAddress", txid);

  m_Params.Baudrate = (uint32_t)baudrate;
  m_Params.Port = port;
  m_Params.LinkLayerDataLength = LinkLayerDataLength;
  m_Params.N_TA = N_TA;

  if (protocol == "CAN") {
    strcpy(m_Params.Device, device.c_str());
    m_Params.Protocol = ISOTP_OVER_CAN;
    m_Params.U.CAN.RxCanId = (uint32_t)rxid;
    m_Params.U.CAN.TxCanId = (uint32_t)txid;
    m_Params.U.CAN.BlockSize = Xlt_Get(m_DcmCfg, "BlockSize", 8);
    m_Params.U.CAN.STmin = Xlt_Get(m_DcmCfg, "STmin", 0);
    m_FuncAddr = Xlt_Get(m_DcmCfg, "FunctionalRequestAddress", 0x7df);
    if (m_CanBusId >= 0) {
      CanL_Close(m_CanBusId);
      m_CanBusId = -1;
    }
    m_CanBusId = CanL_Open(device.c_str(), port, (uint32_t)baudrate);
    if (m_CanBusId < 0) {
      XLT_ERROR("CAN device %s port %d baudrate %d open failed\n", device.c_str(), port, baudrate);
      ret = EACCES;
    }
  } else if (protocol == "LIN") {
    strcpy(m_Params.Device, device.c_str());
    m_Params.Protocol = ISOTP_OVER_LIN;
    m_Params.U.LIN.RxId = (uint32_t)rxid;
    m_Params.U.LIN.TxId = (uint32_t)txid;
    m_Params.U.LIN.Timeout = Xlt_Get(m_DcmCfg, "STmin", 100);
    m_Params.U.LIN.DelayUs = delayUs;
  } else if (protocol == "DoIP") {
    auto ipStr = Xlt_Get(m_DcmCfg, "Ip", "224.244.224.245");
    strcpy(m_Params.Device, ipStr.c_str());
    m_Params.Protocol = ISOTP_OVER_DOIP;
    m_Params.Port = Xlt_Get(m_DcmCfg, "Port", 13400);
    m_Params.U.DoIP.SourceAddress = Xlt_Get(m_DcmCfg, "SourceAddress", 0xbeef);
    m_Params.U.DoIP.TargetAddress = Xlt_Get(m_DcmCfg, "TargetAddress", 0xdead);
    m_Params.U.DoIP.ActivationType = Xlt_Get(m_DcmCfg, "ActivationType", 0xda);
  } else {
    XLT_ERROR("invalid protocol %s\n", protocol.c_str());
    ret = EINVAL;
  }

  if (0 == ret) {
    if (nullptr != m_IsoTp) {
      IsoTp_Destroy(m_IsoTp);
      m_IsoTp = nullptr;
    }
    m_IsoTp = IsoTp_Create(&m_Params);
    if (nullptr == m_IsoTp) {
      XLT_ERROR("Fail to create Dcm\n");
      ret = EFAULT;
    } else {
      XLT_INFO("Dcm created: %s:%s:%d Physical Address = 0x%x, 0x%x\n", protocol.c_str(),
               device.c_str(), port, txid, rxid);
    }
  }

  return ret;
}

int XLTester::Lookup(XLWorksheet &sheet, std::string value, int row, int &col) {
  int ret = EEXIST;
  int numColumns = sheet.range().numColumns();

  for (int c = 1; c <= numColumns; c++) {
    std::string val = GetCellValue(sheet, row, c);
    if (val == value) {
      XLT_DEBUG("found %s at (%d, %d)\n", value.c_str(), row, c);
      col = c;
      ret = 0;
      break;
    }
  }

  return ret;
}

int XLTester::GetTestCases(TestList &testList) {
  int ret = 0;
  XLWorksheet caseSheet;
  int numRows;
  int startRow = 1;
  int testCaseCol = 1, testTypeCol = 2, addressCol = 3, requestCol = 4, expectedResponseCol = 5,
      repeatCol = 5, actualResponseCol = 7, delayMsCol = 8, resultCol = 9, commentsCol = 10;

  try {
    bool bFound = false;
    caseSheet = m_Doc->workbook().worksheet(testList.Name);
    numRows = caseSheet.range().numRows();
    for (int row = 1; row <= numRows; row++) {
      ret = Lookup(caseSheet, "Test Case", row, testCaseCol);
      ret += Lookup(caseSheet, "Test Type", row, testTypeCol);
      ret += Lookup(caseSheet, "Address", row, addressCol);
      ret += Lookup(caseSheet, "Request", row, requestCol);
      ret += Lookup(caseSheet, "Expected Response", row, expectedResponseCol);
      ret += Lookup(caseSheet, "Repeat", row, repeatCol);
      ret += Lookup(caseSheet, "Actual Response", row, actualResponseCol);
      ret += Lookup(caseSheet, "Delay(ms)", row, delayMsCol);
      ret += Lookup(caseSheet, "Result", row, resultCol);
      ret += Lookup(caseSheet, "Comments", row, commentsCol);
      if (0 == ret) {
        startRow = row + 1;
        bFound = true;
        break;
      }
    }
    if (false == bFound) {
      XLT_ERROR("Can't found valid title for: %s\n", testList.Name.c_str());
      ret = EINVAL;
    } else {
      XLT_DEBUG("titile position: %d %d %d %d %d %d %d %d %d %d\n", testCaseCol, testTypeCol,
                addressCol, requestCol, expectedResponseCol, repeatCol, actualResponseCol,
                delayMsCol, resultCol, commentsCol);
    }
  } catch (const std::exception &e) {
    XLT_ERROR("Fail to get test cases for %s: %s\n", testList.Name.c_str(), e.what());
    ret = EFAULT;
  }

  if (0 == ret) {
    testList.Sheet = caseSheet;
    testList.ActualResponseCol = actualResponseCol;
    testList.ResultCol = resultCol;
    for (int row = startRow; row <= numRows; row++) {
      TestCase tstCase;
      tstCase.Name = GetCellValue(caseSheet, row, testCaseCol);
      tstCase.Type = GetCellValue(caseSheet, row, testTypeCol);
      tstCase.Address = GetCellValue(caseSheet, row, addressCol);
      tstCase.Request = GetCellValue(caseSheet, row, requestCol);
      tstCase.ExpectedResponse = GetCellValue(caseSheet, row, expectedResponseCol);
      tstCase.Comments = GetCellValue(caseSheet, row, commentsCol);
      tstCase.DelayMs = Xlt_StrToInt(GetCellValue(caseSheet, row, delayMsCol));
      tstCase.Row = row;

      auto repeatStr = GetCellValue(caseSheet, row, repeatCol);
      if ("" != repeatStr) {
        tstCase.Repeat = Xlt_StrToInt(repeatStr);
        if (tstCase.Repeat <= 0) {
          ret = EINVAL;
          XLT_ERROR("%s: repeat \"%s\" is invalid\n", testList.Name.c_str(), repeatStr.c_str());
        }
      }

      if (("" != tstCase.Type) && ("" != tstCase.Request)) {
        testList.Cases.push_back(tstCase);
        XLT_DEBUG("%s: %s, %s, %s, %s, %s, %s\n", testList.Name.c_str(), tstCase.Name.c_str(),
                  tstCase.Type.c_str(), tstCase.Address.c_str(), tstCase.Request.c_str(),
                  tstCase.ExpectedResponse.c_str(), tstCase.Comments.c_str());
      }
    }
  }

  return ret;
}

int XLTester::Init(std::string filePath) {
  int ret = 0;
  XLWorksheet configSheet;
  int numRows, numColumns;

  m_Progress = 0;
  m_DcmCfg.clear();
  m_TestLists.clear();

  try {
    m_Doc = std::make_shared<XLDocument>(filePath);
    XLT_DEBUG("Open Excel: %s OK\n", filePath.c_str());
  } catch (const std::exception &e) {
    XLT_ERROR("Fail to open Excel: %s, %s\n", filePath.c_str(), e.what());
    ret = EFAULT;
  }

  if (0 == ret) {
    try {
      configSheet = m_Doc->workbook().worksheet("Config");
      numRows = configSheet.range().numRows();
      numColumns = configSheet.range().numColumns();
      XLT_DEBUG("Get config sheet: rows,cols = %d,%d\n", numRows, numColumns);
      for (int row = 1; (row <= numRows) && (0 == ret); row++) {
        for (int col = 1; (col < numColumns) && (0 == ret); col++) {
          std::string key = GetCellValue(configSheet, row, col);
          std::string value = GetCellValue(configSheet, row, col + 1);
          if (("Configuration" == key) && ("Dcm" == value)) {
            XLT_DEBUG("Get config Dcm @ row,col = %d,%d\n", row, col);
            for (row = row + 1; row <= numRows; row++) {
              std::string key = GetCellValue(configSheet, row, col);
              std::string value = GetCellValue(configSheet, row, col + 1);
              if (("" == key) || ("" == value)) {
                break;
              }
              XLT_INFO("Dcm: %s = %s\n", key.c_str(), value.c_str());
              m_DcmCfg[key] = value;
            }
            break;
          } else if (("TestList" == key) && ("TestType" == value)) {
            for (row = row + 1; (row <= numRows) && (0 == ret); row++) {
              std::string key = GetCellValue(configSheet, row, col);
              std::string value = GetCellValue(configSheet, row, col + 1);
              std::string onOff = GetCellValue(configSheet, row, col + 2);
              if (("" == key) || ("" == value)) {
                break;
              }
              if ("OFF" == onOff) {
                continue;
              }
              XLT_INFO("TestList for %s, type %s\n", key.c_str(), value.c_str());
              TestList testList;
              testList.Name = key;
              testList.Type = value;
              ret = GetTestCases(testList);
              if (0 == ret) {
                m_TestLists.push_back(testList);
              }
            }
            break;
          } else {
            /* OK */
          }
        }
      }
    } catch (const std::exception &e) {
      XLT_ERROR("Fail to get config sheet: %s\n", e.what());
      ret = EFAULT;
    }
  }

  if (0 == ret) {
    if (m_DcmCfg.empty()) {
      XLT_ERROR("No Dcm Cfg provided\n");
      ret = EFAULT;
    } else {
      ret = InitIsoTp();
    }
  }

  if (0 == ret) {
    auto dir = std::string(dirname((char *)filePath.c_str()));
    auto luaScript = dir + "/" + Xlt_Get(m_DcmCfg, "Lua", "Tester.lua");
    try {
      m_Lua = std::make_shared<AsLuaScript>(luaScript);
    } catch (const std::exception &e) {
      XLT_ERROR("Failed to load lua script <%s>: %s\n", luaScript.c_str(), e.what());
      ret = EFAULT;
    }
  }

  if (0 == ret) {
    int totalCases = 0;
    for (auto &tstList : m_TestLists) {
      totalCases += tstList.Cases.size();
    }

    m_Step = XLTESTER_PROGRESS_DONE / totalCases;
  }

  if (0 != ret) {
    /* resource cleanup */
    m_Lua = nullptr;
    if (nullptr != m_IsoTp) {
      IsoTp_Destroy(m_IsoTp);
      m_IsoTp = nullptr;
    }

    if (m_CanBusId >= 0) {
      CanL_Close(m_CanBusId);
      m_CanBusId = -1;
    }
  }

  return ret;
}

int XLTester::ExecuteCanCase(TestCase &tstCase) {
  int ret = 0;
  uint32_t canId;
  int repeat = tstCase.Repeat;
  std::vector<uint8_t> req;
  bool bLuaRequest = false;
  std::string api;
  std::string request;

  if (0 == strncmp(tstCase.Request.c_str(), "lua.", 4)) {
    bLuaRequest = true;
    api = std::string(&tstCase.Request.c_str()[4]);
    request = tstCase.Address;
  } else if (0 == strncmp(tstCase.Address.c_str(), "lua.", 4)) {
    bLuaRequest = true;
    api = std::string(&tstCase.Address.c_str()[4]);
    request = tstCase.Request;
  } else {
    canId = (uint32_t)Xlt_StrToInt(tstCase.Address);
    ret = Xlt_StrToArrayU8(tstCase.Request, req);
  }

  for (int i = 0; (i < repeat) && (0 == ret); i++) {
    if (true == bLuaRequest) {
      Lua_ArgType inArgs[3];
      inArgs[0].Type = LUA_ARG_TYPE_SINT32;
      inArgs[0].S32 = m_CanBusId;

      inArgs[1].Type = LUA_ARG_TYPE_STRING;
      inArgs[1].String = request;

      inArgs[2].Type = LUA_ARG_TYPE_STRING;
      inArgs[2].String = tstCase.ExpectedResponse;

      Lua_ArgType outArgs[2];
      outArgs[0].Type = LUA_ARG_TYPE_SINT32;
      outArgs[1].Type = LUA_ARG_TYPE_STRING; /* the actural response text */
      ret = m_Lua->Call(api.c_str(), inArgs, 3, outArgs, 2);
      if (0 == ret) {
        ret = outArgs[0].S32;
        if ("" != outArgs[1].String) {
          tstCase.ActualResponse += std::to_string(i) + ": " + outArgs[1].String + "\n";
        }
      } else {
        XLT_ERROR("Failed to call lua API %s: %d\n", api.c_str(), ret);
      }
    } else {
      auto bRet = CanL_Write(m_CanBusId, canId, (uint8_t)req.size(), req.data());
      if (true != bRet) {
        XLT_ERROR("%d time: can write(%d, %x, [%s]) failed\n", i, m_CanBusId, canId,
                  Xlt_Hex(req).c_str());
        ret = EFAULT;
      } else {
        if (i < (repeat - 1)) {
          if (tstCase.DelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(tstCase.DelayMs));
          }
        }
      }
    }
  }

  if (0 != ret) {
    tstCase.Result = false;
  } else {
    tstCase.Result = true;
  }

  return ret;
}

int XLTester::UdsRequestService(std::vector<uint8_t> &req, std::vector<uint8_t> &res,
                                std::string address) {
  int r = 0;
  res.clear();
  XLT_DEBUG("Dcm request: %s\n", Xlt_Format(req).c_str());

  uint32_t funcAddr = m_FuncAddr;

  if ("Functional" == address) {
    /* switch to functional addressing mode */
    IsoTp_IoCtl(m_IsoTp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
  }

  r = IsoTp_Transmit(m_IsoTp, (uint8_t *)req.data(), req.size(), nullptr, 0);
  while (0 == r) {
    int rlen = IsoTp_Receive(m_IsoTp, m_Buffer, sizeof(m_Buffer));
    if (rlen > 0) {
      XLT_DEBUG("Dcm response: %s\n", Xlt_Format(m_Buffer, rlen).c_str());
      if ((req[0] | 0x40) == m_Buffer[0]) {
        r = 0;
        for (int i = 0; i < rlen; i++) {
          res.push_back(m_Buffer[i]);
        }
        break;
      } else {
        if ((3 == rlen) && (0x7F == m_Buffer[0]) && (m_Buffer[1] == req[0]) &&
            (0x78 == m_Buffer[2])) {
          /* response is pending as server is busy */
          continue;
        } else {
          for (int i = 0; i < rlen; i++) {
            res.push_back(m_Buffer[i]);
          }
          r = 0; /* negative response */
          break;
        }
      }
    } else {
      r = -EIO;
    }
  }

  if ("Functional" == address) {
    /* switch back to physical addressing mode */
    IsoTp_IoCtl(m_IsoTp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
  }

  return r;
}

int XLTester::LuaRequest(std::string api, std::vector<uint8_t> &req) {
  int ret = 0;

  Lua_ArgType outArgs[2];
  outArgs[0].Type = LUA_ARG_TYPE_SINT32;  /* ret */
  outArgs[1].Type = LUA_ARG_TYPE_UINT8_N; /* req */

  ret = m_Lua->Call(api.c_str(), nullptr, 0, outArgs, 2);
  if (0 == ret) {
    ret = outArgs[0].S32;
    if (0 == ret) {
      req = outArgs[1].U8N;
    }
  } else {
    XLT_ERROR("Failed to call lua API %s: %d\n", api.c_str(), ret);
  }

  return ret;
}

int XLTester::LuaResponse(std::string api, std::vector<uint8_t> res) {
  int ret = 0;

  Lua_ArgType inArgs[1];
  inArgs[0].Type = LUA_ARG_TYPE_UINT8_N;
  inArgs[0].U8N = res;

  Lua_ArgType outArgs[1];
  outArgs[0].Type = LUA_ARG_TYPE_SINT32;

  ret = m_Lua->Call(api.c_str(), inArgs, 1, outArgs, 1);
  if (0 == ret) {
    ret = outArgs[0].S32;
  } else {
    XLT_ERROR("Failed to call lua API %s: %d\n", api.c_str(), ret);
  }

  return ret;
}

int XLTester::ExecuteUdsCase(TestCase &tstCase) {
  int ret = 0;

  std::vector<uint8_t> req;
  std::vector<uint8_t> res;

  if (0 == strncmp(tstCase.Request.c_str(), "lua.", 4)) {
    ret = LuaRequest(std::string(&tstCase.Request.c_str()[4]), req);
  } else {
    ret = Xlt_StrToArrayU8(tstCase.Request, req);
  }

  if (0 == ret) {
    ret = UdsRequestService(req, res, tstCase.Address);
    if (0 == ret) {
      tstCase.ActualResponse = Xlt_Hex(res);
      if (0 == strncmp(tstCase.ExpectedResponse.c_str(), "lua.", 4)) {
        ret = LuaResponse(std::string(&tstCase.ExpectedResponse.c_str()[4]), res);
      } else {
        auto strArrR = Xlt_StrSplit(tstCase.ActualResponse, ' ');
        auto strArrG = Xlt_StrSplit(tstCase.ExpectedResponse, ' ');
        if (strArrR.size() != strArrG.size()) {
          ret = EFAULT;
        } else {
          for (size_t i = 0; i < strArrG.size(); i++) {
            if (("xx" == strArrG[i]) || ("XX" == strArrG[i])) {
            } else if (strtoul(strArrR[i].c_str(), NULL, 16) !=
                       strtoul(strArrG[i].c_str(), NULL, 16)) {
              ret = EFAULT;
              break;
            }
          }
        }
      }
    }
  } else {
    XLT_ERROR("  name=\"%s\", request=\"%s\" contains invalid data\n", tstCase.Name.c_str(),
              tstCase.Request.c_str());

    ret = EINVAL;
  }

  if (0 != ret) {
    tstCase.Result = false;
  } else {
    tstCase.Result = true;
  }

  return ret;
}

int XLTester::ExecuteTestCase(TestCase &tstCase) {
  int ret = 0;

  if ("UDS" == tstCase.Type) {
    ret = ExecuteUdsCase(tstCase);
  } else if ("CAN" == tstCase.Type) {
    ret = ExecuteCanCase(tstCase);
  } else {
    XLT_ERROR("  name=\"%s\", type=\"%s\" not supported\n", tstCase.Name.c_str(),
              tstCase.Type.c_str());
    ret = EINVAL;
  }

  XLT_INFO("  name=\"%s\", type=\"%s\", address=%s, request=\"%s\", repeat=%d, expected=\"%s\", "
           "actual=\"%s\", %s\n",
           tstCase.Name.c_str(), tstCase.Type.c_str(), tstCase.Address.c_str(),
           tstCase.Request.c_str(), tstCase.Repeat, tstCase.ExpectedResponse.c_str(),
           tstCase.ActualResponse.c_str(), tstCase.Result ? "PASS" : "FAIL");

  return ret;
}

void XLTester::ThreadMain() {
  XLT_INFO("Starting Test ...\n");

  uint32_t failNum = 0;
  uint32_t passNum = 0;

  for (auto &tstList : m_TestLists) {
    XLT_INFO("Starting Test for %s: %d number of cases\n", tstList.Name.c_str(),
             (int)tstList.Cases.size());
    for (auto &tstCase : tstList.Cases) {
      (void)ExecuteTestCase(tstCase);
      if (tstCase.DelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(tstCase.DelayMs));
      }
      m_Progress += m_Step;

      try {
        /* update the value field */
        auto cellActualRes = tstList.Sheet.cell(tstCase.Row, tstList.ActualResponseCol);
        cellActualRes.value() = tstCase.ActualResponse;

        auto cellResult = tstList.Sheet.cell(tstCase.Row, tstList.ResultCol);
        cellResult.value() = tstCase.Result ? "PASS" : "FAIL";
        if (false == tstCase.Result) {
          failNum++;
        } else {
          passNum++;
        }
      } catch (const std::exception &e) {
        XLT_ERROR("Fail to update sheet %s for case %s: %s\n", tstList.Name.c_str(),
                  tstCase.Name.c_str(), e.what());
      }
    }
  }

  try {
    m_Doc->saveAs("TestReport.xlsx");
    m_Doc->close();
    XLT_INFO("Generate Report TestReport.xlsx OK, %u FAIL, %u PASS\n", failNum, passNum);
  } catch (const std::exception &e) {
    XLT_ERROR("Fail to generate TestReport.xlsx: %s\n", e.what());
  }

  XLT_INFO("Test Done\n");
  m_Progress = XLTESTER_PROGRESS_DONE;

  m_Lua = nullptr;
  if (nullptr != m_IsoTp) {
    IsoTp_Destroy(m_IsoTp);
    m_IsoTp = nullptr;
  }

  if (m_CanBusId >= 0) {
    CanL_Close(m_CanBusId);
    m_CanBusId = -1;
  }
}

int XLTester::Start(std::string filePath) {
  int ret = Init(filePath);

  if (0 == ret) {
    m_Progress = 0;
    if (m_Thread.joinable()) {
      m_Thread.join();
    }
    m_Thread = std::thread(&XLTester::ThreadMain, this);
  }

  return ret;
}

std::string XLTester::GetCellValue(XLWorksheet &sheet, int row, int col) {
  std::string value = "";
  int ret = 0;
  XLCell cell;
  try {
    cell = sheet.cell(row, col);
  } catch (const std::exception &e) {
    XLT_ERROR("cell(%d, %d): %s\n", row, col, e.what());
    ret = EFAULT;
  }

  if (0 == ret) {
    switch (cell.value().type()) {
    case XLValueType::Boolean:
      if (cell.value().get<bool>()) {
        value = "true";
      } else {
        value = "false";
      }
      break;
    case XLValueType::Integer:
      value = std::to_string(cell.value().get<int64_t>());
      break;
    case XLValueType::Float:
      value = std::to_string(cell.value().get<double>());
      break;
    case XLValueType::String:
      value = cell.value().get<std::string>();
      break;
    default:
      ret = EFAULT;
      break;
    }
    XLT_VERBOSE("cell(%d, %d)=\"%s\", type=%d\n", row, col, value.c_str(),
                (int)cell.value().type());
  }

  return value;
}
} /* namespace as */
