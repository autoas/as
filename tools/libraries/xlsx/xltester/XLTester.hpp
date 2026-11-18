/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef XLTESTER_HPP
#define XLTESTER_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <OpenXLSX.hpp>

#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include <stdint.h>
#include <errno.h>
#include "isotp.h"

#include "aslua.hpp"

using namespace OpenXLSX;

namespace as {
/* ================================ [ MACROS    ] ============================================== */
#define XLTESTER_MSG_SIZE (512 * 1024)

#define XLTESTER_PROGRESS_DONE 10000
/* ================================ [ TYPES     ] ============================================== */
typedef std::map<std::string, std::string> XLT_ConfigType;

class XLTester {
public:
  XLTester();
  ~XLTester();

public:
  int Start(std::string filePath);
  std::string GetMsg();
  int GetProgress(); /* return progress if no error, else negative error code */

private:
  int UdsRequestService(std::vector<uint8_t> &req, std::vector<uint8_t> &res, std::string address);
  int InitIsoTp();
  int Init(std::string filePath);
  void AddMsg(int level, const char *fmt, ...);
  std::string GetCellValue(XLWorksheet &sheet, int row, int col);

  void ThreadMain();

private:
  struct TestCase {
    std::string Name;
    std::string Type;
    std::string Address;
    std::string Request;
    std::string ExpectedResponse; /* For CAN message, let's use this to define the repeat times */
    std::string ActualResponse;
    std::string Comments;
    int Repeat = 1;
    int DelayMs;
    bool Result;
    /* position of the Result cell */
    int Row;
  };

  struct TestList {
    XLWorksheet Sheet;
    std::string Name;
    std::string Type;
    int ActualResponseCol;
    int ResultCol;
    std::vector<TestCase> Cases;
  };

private:
  int Lookup(XLWorksheet &sheet, std::string key, int row, int &col);
  int GetTestCases(TestList &testList);
  int ExecuteCanCase(TestCase &tstCase);
  int ExecuteUdsCase(TestCase &tstCase);
  int ExecuteTestCase(TestCase &tstCase);

  int LuaRequest(std::string api, std::vector<uint8_t> &req);
  int LuaResponse(std::string api, std::vector<uint8_t> res);

private:
  std::shared_ptr<XLDocument> m_Doc;

  XLT_ConfigType m_DcmCfg;
  std::mutex m_Lock;
  char m_Logs[XLTESTER_MSG_SIZE];
  int m_LogSize = 0;      /* log size */
  int m_Progress = 0; /* unit 0.01% */
  int m_Step = 1;     /* add on value to progress if a test case done */

  std::vector<TestList> m_TestLists;

  std::thread m_Thread;

  IsoTp_ParameterType m_Params;
  uint32_t m_FuncAddr;
  IsoTp_Type *m_IsoTp = nullptr;
  uint8_t m_Buffer[4096];

  int m_CanBusId = -1;

  std::shared_ptr<AsLuaScript> m_Lua = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* XLTESTER_HPP */
