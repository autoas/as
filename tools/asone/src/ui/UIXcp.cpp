/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023-2024 Parai Wang <parai@foxmail->xcp>
 */
/* ================================ { INCLUDES  ] ============================================== */
#include "UIXcp.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QMainwindow>
#include <QTextCursor>
#include <sstream>
#include <iomanip>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <dlfcn.h>
#include <mutex>
#include <iostream>

#include "canlib.h"
#include "Std_Bit.h"
#include "Log.hpp"

#include "Std_Timer.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

using namespace as;
/* ================================ { MACROS    ] ============================================== */
#define XCP_PID_CMD_CAL_DOWNLOAD 0xF0
#define XCP_PID_CMD_PGM_PROGRAM 0xD0
/* ================================ { TYPES     ] ============================================== */
/* ================================ { DECLARES  ] ============================================== */
/* ================================ { DATAS     ] ============================================== */
/* ================================ { LOCALS    ] ============================================== */
static std::string format(const uint8_t *data, int len) {
  static const std::map<uint8_t, std::string> nrcMap = {
    {0x00, "Command processor synchronisation"},
    {0x10, "Command was not executed"},
    {0x11, "Command rejected because DAQ is running"},
    {0x12, "Command rejected because PGM is running"},
    {0x20, "Unknown command or not implemented optional command"},
    {0x21, "Command syntax invalid"},
    {0x22, "Command syntax valid but command parameter(s) out of range"},
    {0x23, "The memory location is write protected"},
    {0x24, "The memory location is not accessible"},
    {0x25, "Access denied, Seed & Key is required"},
    {0x26, "Selected page not available"},
    {0x27, "Selected page mode not available"},
    {0x29, "Sequence error"},
    {0x2A, "DAQ configuration not valid"},
    {0x30, "Memory overflow error"},
    {0x31, "Generic error"},
    {0x32, "The slave internal program verify routine detects an error"},
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

  if (2 == len) {
    if (0xFE == data[0]) {
      auto it = nrcMap.find(data[1]);
      if (it != nrcMap.end()) {
        ss << " nrc = " << it->second;
      }
    }
  }

  return ss.str();
}

static std::string format(std::vector<uint8_t> data) {
  return format(data.data(), (int)data.size());
}
/* ================================ { FUNCTIONS ] ============================================== */
class Xcp {
public:
  Xcp(json &js) {
    auto protocol = get<std::string>(js, "protocol", "CAN");
    auto device = get<std::string>(js, "device", "simulator_v2");
    uint32_t port = get<uint32_t>(js, "port", 0);
    uint32_t baudrate = get<uint32_t>(js, "baudrate", 500000);
    m_LL_DL = get<uint32_t>(js, "LL_DL", 8);

    m_rxid = get2<uint32_t>(js, "rxid", m_rxid);
    m_txid = get2<uint32_t>(js, "txid", m_txid);

    if (protocol == "CAN") {
      m_busid = can_open(device.c_str(), port, baudrate);
    } else {
      throw std::runtime_error("invalid protocol " + protocol);
    }

    if (m_busid < 0) {
      throw std::runtime_error("failed to create xcp");
    }

    LOG(INFO, "XCP create: %s:%s:%d txid=0x%x rxid=0x%x\n", protocol.c_str(), device.c_str(), port,
        m_txid, m_rxid);
  }

  void empty_rx_queue() {
    uint8_t data[64];
    uint32_t canid;
    uint8_t dlc;
    bool r = true;

    std::unique_lock<std::mutex> lck(m_Lock);
    while (true == r) {
      canid = m_rxid;
      dlc = sizeof(data);
      r = can_read(m_busid, &canid, &dlc, data);
      if (true == r) {
        LOG(INFO, "Xcp not handled response: %s\n", format(data, (int)dlc).c_str());
      }
    }
  }

  int receive(std::vector<uint8_t> &res) {
    int ret = 0;
    std::unique_lock<std::mutex> lck(m_Lock);
    uint8_t data[64];
    uint32_t canid;
    uint8_t dlc;
    bool r = true;

    Std_TimerType timer;
    Std_TimerStart(&timer);
    res.clear();

    while (0 == ret) {
      canid = m_rxid;
      dlc = sizeof(data);
      r = can_read(m_busid, &canid, &dlc, data);
      if (true == r) {
        LOG(INFO, "Xcp response: %s\n", format(data, (int)dlc).c_str());
        for (int i = 0; i < (int)dlc; i++) {
          res.push_back(data[i]);
        }
        if (res[0] != 0xFF) {
          ret = E_NOT_OK;
        }
        break;
      } else {
        auto elapsed = Std_GetTimerElapsedTime(&timer);
        if (elapsed > 1000000) {
          ret = -ETIMEDOUT;
          break;
        } else {
          /* continue to wait a response */
          usleep(1000);
        }
      }
    }

    return ret;
  }

  int transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res) {
    int ret = 0;
    std::unique_lock<std::mutex> lck(m_Lock);
    uint8_t data[64];
    uint32_t canid;
    uint8_t dlc;
    bool r = true;

    if (res.data() != req.data()) {
      res.clear();
    }
    LOG(INFO, "Xcp request: %s\n", format(req).c_str());
    r = can_write(m_busid, m_txid, (uint8_t)req.size(), req.data());
    if (true != r) {
      ret = EFAULT;
    }

    /* do write only if res == req */
    if (res.data() == req.data()) {
      return ret;
    }

    Std_TimerType timer;
    Std_TimerStart(&timer);

    while (0 == ret) {
      canid = m_rxid;
      dlc = sizeof(data);
      r = can_read(m_busid, &canid, &dlc, data);
      if (true == r) {
        LOG(INFO, "Xcp response: %s\n", format(data, (int)dlc).c_str());
        for (int i = 0; i < (int)dlc; i++) {
          res.push_back(data[i]);
        }
        if (res[0] != 0xFF) {
          ret = E_NOT_OK;
        }
        break;
      } else {
        auto elapsed = Std_GetTimerElapsedTime(&timer);
        if (elapsed > 1000000) {
          ret = -ETIMEDOUT;
          break;
        } else {
          /* continue to wait a response */
          usleep(1000);
        }
      }
    }

    return ret;
  }

  ~Xcp() {
    if (m_busid >= 0) {
      can_close(m_busid);
    }
  }

  int BusId() {
    return m_busid;
  }

  uint32_t RxId() {
    return m_rxid;
  }

  uint32_t TxId() {
    return m_txid;
  }

private:
  int m_busid = -1;
  uint32_t m_rxid = 0xFB0;
  uint32_t m_txid = 0xFA0;
  uint32_t m_LL_DL = 8;

  std::mutex m_Lock;
};

UIService::UIService(json &js, UIXcp *xcp)
  : UIServiceBase(js["name"].get<std::string>(), xcp->luaScript()), m_Xcp(xcp) {
  auto vbox = createUI(js);
  setLayout(vbox);

  connect(m_btnEnter, SIGNAL(clicked()), this, SLOT(on_btnEnter_clicked()));
}

void UIService::on_btnEnter_clicked(void) {
  BitArray TxBits(m_TxBitSize + 8);
  TxBits.put(m_SID, 8);
  for (auto &data : m_TxDatas) {
    data->put(TxBits);
  }

  auto &req = TxBits.bytes();
  std::vector<uint8_t> res;
  int r = 0;

  m_Xcp->empty_rx_queue();

  if ((XCP_PID_CMD_CAL_DOWNLOAD == m_SID) || (XCP_PID_CMD_PGM_PROGRAM == m_SID)) {
    if ((size_t)(req[1] + 2) == req.size()) {
      r = m_Xcp->transmit(req, res);
    } else {
      /* OK, depends on the download/program next to send the request */
      res.push_back(0xFF);
    }
  } else {
    r = m_Xcp->transmit(req, res);
  }

  if (r != 0) {
    std::string msg = "error " + std::to_string(r) + " , response " + format(res);
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  } else {
    auto N = ((m_RxBitSize + 15) / 8);
    if (res.size() >= N) {
      try {
        BitArray RxBits(&(res.data()[1]), res.size() - 1);
        for (auto &data : m_RxDatas) {
          data->get(RxBits);
        }
        std::string msg = m_Name + " OK with response " + format(res);
        QMessageBox::information(this, "Info", tr(msg.c_str()));
      } catch (const std::exception &e) {
        std::string text = "Exception for " + m_Name + ": ";
        text += e.what();
        QMessageBox::critical(this, "Error", tr(text.c_str()));
      }
    } else {
      std::string msg = "response " + format(res) + " for " + m_Name + " too short, expect " +
                        std::to_string(N) + " bytes";
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
    }
  }
}

UIService::~UIService() {
}

UIMeasure::UIMeasure(json &js, UIXcp *xcp, BitArray::Endian endian)
  : UIServiceBase(js["name"].get<std::string>(), xcp->luaScript()), m_Xcp(xcp), m_Endian(endian) {
  m_Addr = js["addr"].get<uint32_t>();
  for (auto &djs : js["datas"]) {
    auto data = parseData(djs);
    data->optional = false;
    m_BitSize += data->bitSize * data->size;
    m_Datas.push_back(data);
  }

  auto vbox = new QVBoxLayout();

  int row = 0;
  int col = 0;
  auto grid = new QGridLayout();
  for (auto &data : m_Datas) {
    data->ui = new UIData(data);
    if (data->visible) {
      auto label = data->name + ":";
      grid->addWidget(new QLabel(tr(label.c_str())), row, col);
      grid->addWidget(data->ui, row, col + 1);
      col += 2;
      if ((col >= 8) || (true == data->newline)) {
        row += 1;
        col = 0;
      }
    }
  }

  if (col != 0) {
    row += 1;
  }

  m_btnRead = new QPushButton("Read");
  m_btnRead->setMaximumWidth(100);
  m_btnWrite = new QPushButton("Write");
  m_btnWrite->setMaximumWidth(100);
  grid->addWidget(m_btnRead, row, 0);
  grid->addWidget(m_btnWrite, row, 1);
  vbox->addLayout(grid);

  setLayout(vbox);
  connect(m_btnRead, SIGNAL(clicked()), this, SLOT(on_btnRead_clicked()));
  connect(m_btnWrite, SIGNAL(clicked()), this, SLOT(on_btnWrite_clicked()));
}

void UIMeasure::on_btnRead_clicked(void) {
  uint32_t number = (m_BitSize + 7) / 8;
  uint32_t numsz;
  uint32_t numoff = 0;
  std::vector<uint8_t> res;

  m_Xcp->empty_rx_queue();

  while (numoff < number) {
    numsz = number - numoff;
    if (numsz > 255) {
      numsz = 255;
    }
    BitArray TxBits(64, m_Endian);
    TxBits.put(0xF4, 8);             /* Short Upload */
    TxBits.put(numsz, 8);            /* Number */
    TxBits.put(0, 8);                /* reserved */
    TxBits.put(0, 8);                /* extension */
    TxBits.put(m_Addr + numoff, 32); /* address */

    auto &req = TxBits.bytes();
    std::vector<uint8_t> res2;
    int r = 0;

    r = m_Xcp->transmit(req, res2);
    if (r != 0) {
      std::string msg = "error " + std::to_string(r) + " , response " + format(res);
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
      return;
    }
    for (size_t i = 1; i < res2.size(); i++) {
      res.push_back(res2[i]);
    }
    while (res.size() < (numoff + numsz)) {
      r = m_Xcp->receive(res2);
      if (r != 0) {
        std::string msg = "error " + std::to_string(r) + " , response " + format(res2);
        QMessageBox::critical(this, "Error", tr(msg.c_str()));
        return;
      }
      for (size_t i = 1; i < res2.size(); i++) {
        res.push_back(res2[i]);
      }
    }

    numoff += numsz;
  }

  if (res.size() >= number) {
    try {
      BitArray RxBits(res.data(), res.size(), m_Endian);
      for (auto &data : m_Datas) {
        data->get(RxBits);
      }
      // std::string msg = m_Name + " OK with response " + format(res);
      // QMessageBox::information(this, "Info", tr(msg.c_str()));
    } catch (const std::exception &e) {
      std::string text = "Exception for " + m_Name + ": ";
      text += e.what();
      QMessageBox::critical(this, "Error", tr(text.c_str()));
    }
  } else {
    std::string msg = "response " + format(res) + " for " + m_Name + " too short, expect " +
                      std::to_string(number) + " bytes";
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  }
}

void UIMeasure::on_btnWrite_clicked(void) {
  BitArray VarBits(m_BitSize, m_Endian);
  for (auto &data : m_Datas) {
    data->put(VarBits);
  }
  auto &var = VarBits.bytes();

  uint32_t number = var.size();
  uint32_t numsz;
  uint32_t numoff = 0;

  int r = 0;
  std::vector<uint8_t> res;

  m_Xcp->empty_rx_queue();

  while (numoff < number) {
    numsz = number - numoff;
    if (numsz > 255) {
      numsz = 255;
    }
    BitArray TxBits(64, m_Endian);
    TxBits.put(0xF6, 8);             /* SET MTA */
    TxBits.put(0, 16);               /* reserved */
    TxBits.put(0, 8);                /* extension */
    TxBits.put(m_Addr + numoff, 32); /* address */

    auto &req = TxBits.bytes();

    r = m_Xcp->transmit(req, res);
    if (r != 0) {
      std::string msg = "error " + std::to_string(r) + " , response " + format(res);
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
      return;
    }

    TxBits.clear();
    TxBits.put(0xF0, 8);  /* Download */
    TxBits.put(numsz, 8); /* Number */
    uint32_t doSz = numsz;
    if (doSz > 6) {
      doSz = 6;
    }
    for (uint32_t i = 0; i < doSz; i++) {
      TxBits.put(var[numoff + i], 8);
    }

    req = TxBits.bytes();
    if (doSz >= numsz) {
      r = m_Xcp->transmit(req, res);
    } else {
      uint32_t offset = doSz;
      r = m_Xcp->transmit(req, req);
      while ((0 == r) && (offset < numsz)) {
        std::this_thread::sleep_for(20ms);
        doSz = numsz - offset;
        if (doSz > 6) {
          doSz = 6;
        }
        TxBits.clear();
        TxBits.put(0xEF, 8); /* Download Next */
        TxBits.put(doSz, 8); /* Number */
        for (size_t i = 0; i < doSz; i++) {
          TxBits.put(var[numoff + i + offset], 8);
        }
        offset += doSz;
        auto &req = TxBits.bytes();
        if (offset >= numsz) {
          r = m_Xcp->transmit(req, res);
        } else {
          r = m_Xcp->transmit(req, req);
        }
      }

      if (r != 0) {
        std::string msg = "error " + std::to_string(r) + " , response " + format(res);
        QMessageBox::critical(this, "Error", tr(msg.c_str()));
        return;
      }
    }

    numoff += numsz;
  }

  std::string msg = m_Name + " download " + format(var) + " OK";
  QMessageBox::information(this, "Info", tr(msg.c_str()));
}

UIMeasure::~UIMeasure() {
}

UIGroup::UIGroup(json &js, UIXcp *xcp, UITreeItem<UIXcp> *topItem, int tabIndex) {
  auto wd = new QWidget();
  auto vbox = new QVBoxLayout();

  for (auto &it : js.items()) {
    auto name = it.key();
    auto service = it.value();
    try {
      LOG(INFO, "UIXcp add %s\n", name.c_str());
      if ("Services" == name) {
        for (auto &jss : service) {
          auto name2 = jss["name"].get<std::string>();
          auto wd = new UIService(jss, xcp);
          vbox->addWidget(wd);
          auto item = new UITreeItem<UIXcp>(name2, this, wd);
          item->setTabIndex(tabIndex);
          topItem->addChild(item);
        }
      }
    } catch (const std::exception &e) {
      std::string text = "Exception for " + name + ": ";
      text += e.what();
      QMessageBox::critical(this, "Error", tr(text.c_str()));
    }
  }
  wd->setLayout(vbox);
  setWidget(wd);
  setWidgetResizable(true);
}

UIGroup::UIGroup(json &js, UIXcp *xcp, UITreeItem<UIXcp> *topItem, int tabIndex,
                 BitArray::Endian endian) {
  auto wd = new QWidget();
  auto vbox = new QVBoxLayout();

  for (auto &var : js) {
    auto name = var["name"].get<std::string>();
    auto wd = new UIMeasure(var, xcp, endian);
    vbox->addWidget(wd);
    auto item = new UITreeItem<UIXcp>(name, this, wd);
    item->setTabIndex(tabIndex);
    topItem->addChild(item);
  }

  wd->setLayout(vbox);
  setWidget(wd);
  setWidgetResizable(true);
}

UIGroup::~UIGroup() {
}

UIXcp::UIXcp() : QWidget() {
  auto vbox = new QVBoxLayout();

  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("load XCP json:"), 0, 0);
  m_leXcpJs = new QLineEdit();
  m_leXcpJs->setReadOnly(true);
  grid->addWidget(m_leXcpJs, 0, 1);
  m_btnOpenXcpJs = new QPushButton("...");
  grid->addWidget(m_btnOpenXcpJs, 0, 2);

  grid->addWidget(new QLabel("load Ecu json:"), 1, 0);
  m_leEcuJs = new QLineEdit();
  m_leEcuJs->setReadOnly(true);
  grid->addWidget(m_leEcuJs, 1, 1);
  m_btnOpenEcuJs = new QPushButton("...");
  grid->addWidget(m_btnOpenEcuJs, 1, 2);

  grid->addWidget(new QLabel("target:"), 2, 0);
  m_cmbxTarget = new QComboBox();
  m_cmbxTarget->setEditable(true);
  grid->addWidget(m_cmbxTarget, 2, 1);
  m_btnStart = new QPushButton("start");
  grid->addWidget(m_btnStart, 2, 2);

  vbox->addLayout(grid);

  auto qSplitter = new QSplitter(Qt::Horizontal);
  m_Tree = new QTreeWidget();
  m_Tree->setMaximumWidth(300);
  m_Tree->setHeaderLabel("Service");
  m_tabWidget = new QTabWidget();
  qSplitter->insertWidget(0, m_Tree);
  qSplitter->insertWidget(1, m_tabWidget);

  vbox->addWidget(qSplitter);

  setLayout(vbox);
  connect(m_btnOpenXcpJs, SIGNAL(clicked()), this, SLOT(on_btnOpenXcpJs_clicked()));
  connect(m_btnOpenEcuJs, SIGNAL(clicked()), this, SLOT(on_btnOpenEcuJs_clicked()));
  connect(m_btnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
  connect(m_Tree, SIGNAL(itemSelectionChanged()), this, SLOT(on_Tree_itemSelectionChanged()));

  std::string defJs = "./diagnostic.json";
  if (0 != access(defJs.c_str(), F_OK | R_OK)) {
    defJs = "../../../../tools/asone/examples/xcp.json";
  }

  auto r = load(defJs, m_Json);
  if (r) {
    auto target = m_Json["target"].dump();
    m_cmbxTarget->addItem(tr(target.c_str()));
    m_leXcpJs->setText(tr(defJs.c_str()));
  }

  defJs = "XcpEcu.Json";
  r = load(defJs, m_EcuJson);
  if (r) {
    m_leEcuJs->setText(tr(defJs.c_str()));
  }
}

bool UIXcp::loadScript(void) {
  bool r = false;
  auto luaScript = m_Dir + "/" + get<std::string>(m_Json, "lua", "");
  try {
    m_Lua = std::make_shared<AsLuaScript>(luaScript);
    r = true;
  } catch (const std::exception &e) {
    QString text = "Exception: ";
    text += e.what();
    QMessageBox::critical(this, "Error", text);
  }
  return r;
}

bool UIXcp::loadUI(void) {
  bool r = true;

  m_tabWidget->clear();
  m_Tree->clear();

  if (false == m_Json.contains("groups")) {
    QMessageBox::critical(this, "Error", tr("json was not loaded"));
    return false;
  }

  r = loadScript();
  if (false == r) {
    return r;
  }

  for (auto &it : m_Json["groups"].items()) {
    auto name = it.key();
    auto group = it.value();
    auto item = new UITreeItem<UIXcp>(name);
    item->setTabIndex(m_tabWidget->count());
    m_tabWidget->addTab(new UIGroup(group, this, item, m_tabWidget->count()), tr(name.c_str()));
    m_Tree->addTopLevelItem(item);
  }

  if (m_EcuJson.contains("xcp_vars")) {
    auto elf = m_EcuJson["elf"].get<std::string>();
    auto endianStr = m_EcuJson["endian"].get<std::string>();
    BitArray::Endian endian;
    if ("LITTLE" == endianStr) {
      endian = BitArray::Endian::LITTLE;
    } else {
      endian = BitArray::Endian::BIG;
    }
    std::string appName = basename((char *)elf.c_str());
    auto ecuTree = new UITreeItem<UIXcp>(appName);
    m_Tree->addTopLevelItem(ecuTree);
    for (auto &it : m_EcuJson["xcp_vars"].items()) {
      auto fileName = it.key();
      auto vars = it.value();
      auto varsTree = new UITreeItem<UIXcp>(fileName);
      varsTree->setTabIndex(m_tabWidget->count());
      m_tabWidget->addTab(new UIGroup(vars, this, varsTree, m_tabWidget->count(), endian),
                          tr(fileName.c_str()));
      ecuTree->addChild(varsTree);
    }
  }

  try {
    auto s = m_cmbxTarget->currentText().toStdString();
    auto tgt = json::parse(s);
    m_Xcp = std::make_shared<Xcp>(tgt);
  } catch (const std::exception &e) {
    QString text = "Exception: ";
    text += e.what();

    r = false;
  }

  if (true == r) {
    /* do Lua xcp init */
    lua_arg_t inArgs[3];
    inArgs[0].type = LUA_ARG_TYPE_SINT32;
    inArgs[0].s32 = m_Xcp->BusId();
    inArgs[1].type = LUA_ARG_TYPE_UINT32;
    inArgs[1].u32 = m_Xcp->TxId();
    inArgs[2].type = LUA_ARG_TYPE_UINT32;
    inArgs[2].u32 = m_Xcp->RxId();
    auto ret = m_Lua->call("xcp_init", inArgs, 3, nullptr, 0);
    if (0 != ret) {
      QMessageBox::critical(this, "Error", "Xcp Lua has no API xcp_init");
    }
  }

  return r;
}

bool UIXcp::load(std::string cfgPath, json &js) {
  bool ret = true;

  FILE *fp = fopen(cfgPath.c_str(), "rb");
  if (nullptr == fp) {
    std::string msg("config file <" + cfgPath + "> not exists");
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
    ret = false;
  }

  if (&js == &m_Json) {
    m_Dir = dirname((char *)cfgPath.c_str());
  }

  if (true == ret) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::unique_ptr<char[]> text(new char[size + 1]);
    fread(text.get(), size, 1, fp);
    char *eol = (char *)text.get();
    eol[size] = 0;
    try {
      js = json::parse(text.get());
    } catch (std::exception &e) {
      ret = false;
      std::string msg("config file " + cfgPath + " error: " + e.what());
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
    }
    fclose(fp);
  }

  return ret;
}

void UIXcp::on_btnOpenXcpJs_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "XCP json config", "", "json config (*.json)");

  auto r = load(fileName.toStdString(), m_Json);
  if (r) {
    auto target = m_Json["target"].dump();
    m_cmbxTarget->addItem(tr(target.c_str()));
    m_leXcpJs->setText(fileName);
  } else {
    m_leXcpJs->setText("");
  }
}

void UIXcp::on_btnOpenEcuJs_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "Euc json config", "", "json config (*.json)");
  auto r = load(fileName.toStdString(), m_EcuJson);
  if (r) {
    m_leEcuJs->setText(fileName);
  } else {
    m_leEcuJs->setText("");
  }
}

void UIXcp::empty_rx_queue() {
  if (m_Xcp != nullptr) {
    m_Xcp->empty_rx_queue();
  }
}

int UIXcp::transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res) {
  int r = -1;
  if (m_Xcp != nullptr) {
    r = m_Xcp->transmit(req, res);
  } else {
    QMessageBox::critical(this, "Error", tr("xcp not created"));
  }

  return r;
}

int UIXcp::receive(std::vector<uint8_t> &res) {
  int r = -1;
  if (m_Xcp != nullptr) {
    r = m_Xcp->receive(res);
  } else {
    QMessageBox::critical(this, "Error", tr("xcp not created"));
  }

  return r;
}

void UIXcp::timerEvent(QTimerEvent *event) {
  (void)event;
}

void UIXcp::on_btnStart_clicked(void) {
  if (m_btnStart->text().toStdString() == "start") {
    auto r = loadUI();
    if (r) {
      m_btnStart->setText("stop");
    }
  } else {
    m_tabWidget->clear();
    m_Xcp = nullptr;
    m_btnStart->setText("start");
  }
}

void UIXcp::on_Tree_itemSelectionChanged(void) {
  UITreeItem<UIXcp> *item = (UITreeItem<UIXcp> *)m_Tree->currentItem();
  auto index = item->tabIndex();
  if (-1 != index) {
    m_tabWidget->setCurrentIndex(index);
    auto uiGroup = item->group();
    auto wd = item->service();
    if (nullptr != uiGroup) {
      uiGroup->ensureWidgetVisible(wd);
    }
  }
}

QTreeWidget *UIXcp::tree(void) {
  return m_Tree;
}

UIXcp::~UIXcp() {
}

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  QMainWindow w;
  w.setCentralWidget(new UIXcp());
  w.show();
  return a.exec();
}

extern "C" std::string name(void) {
  return "Xcp";
}

extern "C" QWidget *create(void) {
  return new UIXcp();
}
