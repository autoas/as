/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2024 Parai Wang <parai@foxmail.com>
 */
#ifndef __XL_TESTER_HPP__
#define __XL_TESTER_HPP__
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
  int start(std::string filePath);
  std::string getMsg();
  int getProgress(); /* return progress if no error, else negative error code */

private:
  int udsRequestService(std::vector<uint8_t> &req, std::vector<uint8_t> &res, std::string address);
  int initIsoTp();
  int init(std::string filePath);
  void addMsg(int level, const char *fmt, ...);
  std::string getCellValue(XLWorksheet &sheet, int row, int col);

  void threadMain();

private:
  struct TestCase {
    std::string name;
    std::string type;
    std::string address;
    std::string request;
    std::string expectedResponse; /* For CAN message, let's use this to define the repeat times */
    std::string actualResponse;
    std::string comments;
    int repeat = 1;
    int delayMs;
    bool result;
    /* position of the Result cell */
    int row;
  };

  struct TestList {
    XLWorksheet sheet;
    std::string name;
    std::string type;
    int actualResponseCol;
    int resultCol;
    std::vector<TestCase> cases;
  };

private:
  int lookup(XLWorksheet &sheet, std::string key, int row, int &col);
  int getTestCases(TestList &testList);
  int executeCANCase(TestCase &tstCase);
  int executeUDSCase(TestCase &tstCase);
  int executeTestCase(TestCase &tstCase);

  int luaRequest(std::string api, std::vector<uint8_t> &req);
  int luaResponse(std::string api, std::vector<uint8_t> res);

private:
  std::shared_ptr<XLDocument> m_doc;

  XLT_ConfigType m_dcmCfg;
  std::mutex m_lock;
  char m_logs[XLTESTER_MSG_SIZE];
  int m_lsz = 0;      /* log size */
  int m_progress = 0; /* unit 0.01% */
  int m_step = 1;     /* add on value to progress if a test case done */

  std::vector<TestList> m_testLists;

  std::thread m_thread;

  isotp_parameter_t m_Params;
  uint32_t m_funcAddr;
  isotp_t *m_IsoTp = nullptr;
  uint8_t m_Buffer[4096];

  int m_canBusId = -1;

  std::shared_ptr<AsLuaScript> m_Lua = nullptr;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
} /* namespace as */
#endif /* __XL_TESTER_HPP__ */
