/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail->dcm>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "UIDcm.hpp"
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
#include "PAL.h"
#include <mutex>
#include <iostream>

#include "isotp.h"
#include "Std_Bit.h"
#include "Log.hpp"

using namespace as;
/* ================================ [ MACROS    ] ============================================== */

/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static std::string Dcm_Format(const uint8_t *data, int len) {
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

static std::string Dcm_Format(std::vector<uint8_t> data) {
  return Dcm_Format(data.data(), (int)data.size());
}
/* ================================ [ FUNCTIONS ] ============================================== */
class Dcm {
public:
  Dcm(json &js) {
    auto protocol = JsonGet<std::string>(js, "Protocol", "CAN");
    auto device = JsonGet<std::string>(js, "Device", "simulator_v2");
    uint32_t port = JsonGet<uint32_t>(js, "Port", 0);
    uint32_t baudrate = JsonGet<uint32_t>(js, "Baudrate", 500000);
    uint32_t rxid = 0x732, txid = 0x731;
    uint16_t nTargetAddress = JsonGet<uint16_t>(js, "N_TargetAddress", 0xFFFF);
    uint32_t delayUs = JsonGet<uint32_t>(js, "DelayUs", 0);
    auto LinkLayerDataLength = JsonGet<uint32_t>(js, "LinkLayerDataLength", 8);
    if (protocol == "LIN") {
      rxid = 0x3d;
      txid = 0x3c;
    }

    rxid = JsonGetInt<uint32_t>(js, "RxId", rxid);
    txid = JsonGetInt<uint32_t>(js, "TxId", txid);

    m_Params.Baudrate = (uint32_t)baudrate;
    m_Params.Port = port;
    m_Params.LinkLayerDataLength = LinkLayerDataLength;
    m_Params.N_TA = nTargetAddress;

    if (protocol == "CAN") {
      strcpy(m_Params.Device, device.c_str());
      m_Params.Protocol = ISOTP_OVER_CAN;
      m_Params.U.CAN.RxCanId = (uint32_t)rxid;
      m_Params.U.CAN.TxCanId = (uint32_t)txid;
      m_Params.U.CAN.BlockSize = JsonGet<uint32_t>(js, "BlockSize", 8);
      m_Params.U.CAN.STmin = JsonGet<uint32_t>(js, "STmin", 0);
      m_Params.U.CAN.Version = (IsoTp_CanVersionType)JsonGet<uint32_t>(js, "Version", ISOTP_CAN_V2);
    } else if (protocol == "LIN") {
      strcpy(m_Params.Device, device.c_str());
      m_Params.Protocol = ISOTP_OVER_LIN;
      m_Params.U.LIN.RxId = (uint32_t)rxid;
      m_Params.U.LIN.TxId = (uint32_t)txid;
      m_Params.U.LIN.Timeout = JsonGet<uint32_t>(js, "STmin", 100);
      m_Params.U.LIN.DelayUs = delayUs;
    } else if (protocol == "DoIP") {
      auto ipStr = JsonGet<std::string>(js, "Ip", "224.244.224.245");
      strcpy(m_Params.Device, ipStr.c_str());
      m_Params.Protocol = ISOTP_OVER_DOIP;
      m_Params.Port = JsonGet<int>(js, "Port", 13400);
      m_Params.U.DoIP.SourceAddress = JsonGet<uint16_t>(js, "SourceAddress", 0xbeef);
      m_Params.U.DoIP.TargetAddress = JsonGet<uint16_t>(js, "TargetAddress", 0xdead);
      m_Params.U.DoIP.ActivationType = JsonGet<uint8_t>(js, "ActivationType", 0xda);
    } else {
      throw std::runtime_error("invalid protocol " + protocol);
    }

    m_IsoTp = IsoTp_Create(&m_Params);
    if (nullptr == m_IsoTp) {
      throw std::runtime_error("failed to create isotp");
    }

    LOG(INFO, "DCM create: %s:%s:%d txid=0x%x rxid=0x%x\n", protocol.c_str(), device.c_str(), port,
        txid, rxid);
  }

  int Transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res) {
    std::unique_lock<std::mutex> lck(m_Lock);

    res.clear();
    LOG(INFO, "Dcm request: %s\n", Dcm_Format(req).c_str());
    int r = IsoTp_Transmit(m_IsoTp, (uint8_t *)req.data(), req.size(), nullptr, 0);

    if ((req.size() >= 2) && ((0x3e == req[0])) && ((req[1] & 0x80) != 0)) {
      /* suppress positive response */
      res.push_back(req[0] | 0x40);
      return 0;
    }

    while (0 == r) {
      int rlen = IsoTp_Receive(m_IsoTp, m_Buffer, sizeof(m_Buffer));
      if (rlen > 0) {
        LOG(INFO, "Dcm response: %s\n", Dcm_Format(m_Buffer, rlen).c_str());
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
            r = -EINVAL;
          }
        }
      } else {
        r = -EIO;
      }
    }

    return r;
  }

  ~Dcm() {
    if (m_IsoTp) {
      IsoTp_Destroy(m_IsoTp);
    }
  }

private:
  IsoTp_Type *m_IsoTp = nullptr;

  uint8_t m_Buffer[4096];
  IsoTp_ParameterType m_Params;

  std::mutex m_Lock;
};

UISessionControl::UISessionControl(json &js, UIDcm *dcm)
  : QGroupBox("session control"), m_Dcm(dcm) {
  m_CmbxSessions = new QComboBox();
  for (auto &it : js.items()) {
    auto sesName = it.key();
    uint8_t sesId = (uint8_t)ToInteger(it.value().get<std::string>());
    m_SessionMap[sesName] = sesId;
    m_CmbxSessions->addItem(tr(sesName.c_str()));
  }
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("Session:"), 0, 0);
  m_BtnEnter = new QPushButton("Enter");
  grid->addWidget(m_CmbxSessions, 0, 1);
  grid->addWidget(m_BtnEnter, 0, 2);
  setLayout(grid);
  connect(m_BtnEnter, SIGNAL(clicked()), this, SLOT(on_btnEnter_clicked()));
}

void UISessionControl::on_btnEnter_clicked(void) {
  auto session = m_CmbxSessions->currentText().toStdString();
  auto sesId = m_SessionMap[session];
  std::vector<uint8_t> req;
  std::vector<uint8_t> res;
  req.push_back(0x10);
  req.push_back(sesId);
  auto r = m_Dcm->Transmit(req, res);
  if (r != 0) {
    std::string msg = "error " + std::to_string(r) + ", response " + Dcm_Format(res);
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  } else {
    std::string msg = "session control OK with response " + Dcm_Format(res);
    QMessageBox::information(this, "Info", tr(msg.c_str()));
  }
}

UISessionControl::~UISessionControl() {
}

UISecurityAccess::UISecurityAccess(json &js, UIDcm *dcm)
  : QGroupBox("security access"), m_Dcm(dcm) {
  m_CmbxSecurityLevels = new QComboBox();
  for (auto &it : js.items()) {
    auto name = it.key();
    Info info;
    auto &jsLvl = it.value();
    info.Level = JsonGetInt(jsLvl, "Level", 0);
    if (info.Level == 0) {
      throw std::runtime_error("found invalid security level for " + name);
    }
    if (jsLvl.contains("Lua")) {
      info.Scripts = "";
      for (auto &jsCode : jsLvl["Lua"]) {
        info.Scripts += jsCode.get<std::string>() + "\n";
      }
      LOG(INFO, "lua script for security level %s:\n%s\n", name.c_str(), info.Scripts.c_str());
    } else if (jsLvl.contains("dll")) {
      auto dllPath = jsLvl["dll"].get<std::string>();
      info.Dll = PAL_DlOpen(dllPath.c_str());
      if (nullptr != info.Dll) {
        info.CalculateKey = (CalculateKeyType)PAL_DlSym(info.Dll, "CalculateKey");
      }
      if ((nullptr == info.Dll) || (nullptr == info.CalculateKey)) {
        throw std::runtime_error("invalid dll " + dllPath + " for level " + name);
      }
    } else {
      throw std::runtime_error("no unlock method specified for level " + name);
    }
    m_SecurityMap[name] = info;
    m_CmbxSecurityLevels->addItem(tr(name.c_str()));
  }
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("Security Level:"), 0, 0);
  m_BtnUnlock = new QPushButton("Unlock");
  grid->addWidget(m_CmbxSecurityLevels, 0, 1);
  grid->addWidget(m_BtnUnlock, 0, 2);
  setLayout(grid);
  connect(m_BtnUnlock, SIGNAL(clicked()), this, SLOT(on_btnUnlock_clicked()));
}

std::vector<uint8_t> UISecurityAccess::CalculateKey(Info &info, std::vector<uint8_t> res) {
  std::vector<uint8_t> key;

  if (nullptr != info.CalculateKey) {
    key = info.CalculateKey(res);
  } else {
    Lua_ArgType inArgs[1];
    Lua_ArgType outArgs[1];

    inArgs[0].Type = LUA_ARG_TYPE_UINT8_N;
    inArgs[0].U8N = res;

    outArgs[0].Type = LUA_ARG_TYPE_UINT8_N;
    auto r = AsLua_Call(info.Scripts.c_str(), "CalculateKey", inArgs, 1, outArgs, 1);
    if (0 == r) {
      key = outArgs[0].U8N;
    }
  }

  return key;
}

void UISecurityAccess::on_btnUnlock_clicked(void) {
  auto levelName = m_CmbxSecurityLevels->currentText().toStdString();
  auto info = m_SecurityMap[levelName];
  std::vector<uint8_t> req;
  std::vector<uint8_t> res;
  req.push_back(0x27);
  req.push_back(info.Level);
  auto r = m_Dcm->Transmit(req, res);
  if (r != 0) {
    std::string msg = "error " + std::to_string(r) + " , response " + Dcm_Format(res);
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  }

  if (0 == r) {
    bool unlocked = true;
    for (size_t i = 2; i < res.size(); i++) {
      if (res[i] != 0) {
        unlocked = false;
      }
    }
    if (unlocked) {
      std::string msg = "SecurityAccess okay with 0 seed, already unlocked !";
      QMessageBox::information(this, "Info", tr(msg.c_str()));
      return;
    }
  }

  if (0 == r) {
    req.clear();
    req.push_back(0x27);
    req.push_back(info.Level + 1);
    auto key = CalculateKey(info, res);
    if (key.size() > 0) {
      for (auto k : key) {
        req.push_back(k);
      }
      auto r = m_Dcm->Transmit(req, res);
      if (r != 0) {
        std::string msg = "error " + std::to_string(r) + " , response " + Dcm_Format(res);
        QMessageBox::critical(this, "Error", tr(msg.c_str()));
      } else {
        std::string msg = "security access OK with response " + Dcm_Format(res);
        QMessageBox::information(this, "Info", tr(msg.c_str()));
      }
    } else {
      QMessageBox::critical(this, "Error", tr("failed to calculate key"));
    }
  }
}

UISecurityAccess::~UISecurityAccess() {
}

UIService::UIService(json &js, UIDcm *dcm)
  : UIServiceBase(js["Name"].get<std::string>(), dcm->LuaScript()), m_Dcm(dcm) {
  auto vbox = CreateUi(js);
  setLayout(vbox);

  connect(m_BtnEnter, SIGNAL(clicked()), this, SLOT(on_btnEnter_clicked()));
}

void UIService::on_btnEnter_clicked(void) {
  BitArray TxBits(m_TxBitSize + 8);
  TxBits.Put(m_SID, 8);
  for (auto &data : m_TxDatas) {
    data->Put(TxBits);
  }

  auto &req = TxBits.Bytes();
  std::vector<uint8_t> res;
  auto r = m_Dcm->Transmit(req, res);

  if (r != 0) {
    std::string msg = "error " + std::to_string(r) + " , response " + Dcm_Format(res);
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  } else {
    auto N = ((m_RxBitSize + 15) / 8);
    if (res.size() >= N) {
      try {
        BitArray RxBits(&(res.data()[1]), res.size() - 1);
        for (auto &data : m_RxDatas) {
          data->Get(RxBits);
        }
        std::string msg = m_Name + " OK with response " + Dcm_Format(res);
        QMessageBox::information(this, "Info", tr(msg.c_str()));
      } catch (const std::exception &e) {
        std::string text = "Exception for " + m_Name + ": ";
        text += e.what();
        QMessageBox::critical(this, "Error", tr(text.c_str()));
      }
    } else {
      std::string msg = "response " + Dcm_Format(res) + " for " + m_Name + " too short, expect " +
                        std::to_string(N) + " bytes";
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
    }
  }
}

UIService::~UIService() {
}

UIGroup::UIGroup(json &js, UIDcm *dcm, UITreeItem<UIDcm> *topItem) {
  auto wd = new QWidget();
  auto vbox = new QVBoxLayout();

  for (auto &it : js.items()) {
    auto name = it.key();
    auto service = it.value();
    try {
      LOG(INFO, "UIDcm add %s\n", name.c_str());
      if ("SessionControl" == name) {
        auto wd = new UISessionControl(service, dcm);
        vbox->addWidget(wd);
        auto item = new UITreeItem<UIDcm>(name, this, wd);
        topItem->addChild(item);
      } else if ("SecurityAccess" == name) {
        auto wd = new UISecurityAccess(service, dcm);
        vbox->addWidget(wd);
        auto item = new UITreeItem<UIDcm>(name, this, wd);
        topItem->addChild(item);
      } else if ("Services" == name) {
        for (auto &jss : service) {
          auto name2 = jss["Name"].get<std::string>();
          auto wd = new UIService(jss, dcm);
          vbox->addWidget(wd);
          auto item = new UITreeItem<UIDcm>(name2, this, wd);
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

UIGroup::~UIGroup() {
}

UIDcm::UIDcm() : QWidget() {
  auto vbox = new QVBoxLayout();

  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("load DCM json:"), 0, 0);
  m_LeDcmJs = new QLineEdit();
  m_LeDcmJs->setReadOnly(true);
  grid->addWidget(m_LeDcmJs, 0, 1);
  m_BtnOpenDcmJs = new QPushButton("...");
  grid->addWidget(m_BtnOpenDcmJs, 0, 2);

  grid->addWidget(new QLabel("target:"), 1, 0);
  m_CmbxTarget = new QComboBox();
  m_CmbxTarget->setEditable(true);
  grid->addWidget(m_CmbxTarget, 1, 1);
  m_BtnStart = new QPushButton("Start");
  grid->addWidget(m_BtnStart, 1, 2);
  m_CbxTesterPresent = new QCheckBox("Tester Present");
  grid->addWidget(m_CbxTesterPresent, 2, 2);

  vbox->addLayout(grid);

  auto qSplitter = new QSplitter(Qt::Horizontal);
  m_Tree = new QTreeWidget();
  m_Tree->setMaximumWidth(300);
  m_Tree->setHeaderLabel("Service");
  m_TabWidget = new QTabWidget();
  qSplitter->insertWidget(0, m_Tree);
  qSplitter->insertWidget(1, m_TabWidget);

  vbox->addWidget(qSplitter);

  setLayout(vbox);
  connect(m_BtnOpenDcmJs, SIGNAL(clicked()), this, SLOT(on_btnOpenDcmJs_clicked()));
  connect(m_BtnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
  connect(m_CbxTesterPresent, SIGNAL(stateChanged(int)), this,
          SLOT(on_cbxTesterPresent_stateChanged(int)));
  connect(m_Tree, SIGNAL(itemSelectionChanged()), this, SLOT(on_Tree_itemSelectionChanged()));

  std::string defJs = "./diagnostic.json";
  if (0 != access(defJs.c_str(), F_OK | R_OK)) {
    defJs = "../../../../tools/asone/examples/diagnostic.json";
  }

  auto r = Load(defJs);
  if (r) {
    auto target = m_Json["Target"].dump();
    m_CmbxTarget->addItem(tr(target.c_str()));
    m_LeDcmJs->setText(tr(defJs.c_str()));
    m_CmbxTarget->addItem(
      "{\"Protocol\": \"LIN\", \"Device\": \"spi/CH341_SPI/2,1\", \"Port\": 0, \"TxId\": \"0x3c\", "
      "\"RxId\": \"0x3d\", \"N_TargetAddress\": 0, \"Baudrate\": 544000, \"DelayUs\": 20000, "
      "\"LinkLayerDataLength\": 8}");
    m_CmbxTarget->addItem(
      "{\"Protocol\": \"LIN\", \"Device\": \"i2c/CH341_I2C\", \"Port\": 0, \"TxId\": \"0x51d0\", "
      "\"RxId\": \"0x51d1\", \"N_TargetAddress\": 0, \"Baudrate\": 100000, \"DelayUs\": 20000, "
      "\"LinkLayerDataLength\": 8}");
    m_CmbxTarget->addItem(
      "{\"Protocol\": \"LIN\", \"Device\": \"lvds\", \"Port\": 8, \"TxId\": \"0x3c\", "
      "\"RxId\": \"0x3d\", \"N_TargetAddress\": 0, \"Baudrate\": 115200, \"DelayUs\": 20000, "
      "\"LinkLayerDataLength\": 64}");
  }
}

bool UIDcm::LoadScript(void) {
  bool r = false;
  auto LuaScript = m_Dir + "/" + JsonGet<std::string>(m_Json, "Lua", "");
  try {
    m_Lua = std::make_shared<AsLuaScript>(LuaScript);
    r = true;
  } catch (const std::exception &e) {
    QString text = "Exception: ";
    text += e.what();
    QMessageBox::critical(this, "Error", text);
  }
  return r;
}

bool UIDcm::LoadUi(void) {
  bool r = true;

  m_TabWidget->clear();
  m_Tree->clear();

  if (false == m_Json.contains("Groups")) {
    QMessageBox::critical(this, "Error", tr("json was not loaded"));
    return false;
  }

  r = LoadScript();
  if (false == r) {
    return r;
  }

  for (auto &it : m_Json["Groups"].items()) {
    auto name = it.key();
    auto group = it.value();
    auto item = new UITreeItem<UIDcm>(name);
    m_TabWidget->addTab(new UIGroup(group, this, item), tr(name.c_str()));
    m_Tree->addTopLevelItem(item);
  }

  try {
    auto s = m_CmbxTarget->currentText().toStdString();
    auto tgt = json::parse(s);
    m_Dcm = std::make_shared<Dcm>(tgt);
  } catch (const std::exception &e) {
    QString text = "Exception: ";
    text += e.what();
    QMessageBox::critical(this, "Error", text);
    r = false;
  }

  return r;
}

bool UIDcm::Load(std::string cfgPath) {
  bool ret = true;

  FILE *fp = fopen(cfgPath.c_str(), "rb");
  if (nullptr == fp) {
    std::string msg("config file <" + cfgPath + "> not exists");
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
    ret = false;
  }

  m_Dir = dirname((char *)cfgPath.c_str());

  if (true == ret) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::unique_ptr<char[]> text(new char[size + 1]);
    fread(text.get(), size, 1, fp);
    char *eol = (char *)text.get();
    eol[size] = 0;
    try {
      m_Json = json::parse(text.get());
    } catch (std::exception &e) {
      ret = false;
      std::string msg("config file " + cfgPath + " error: " + e.what());
      QMessageBox::critical(this, "Error", tr(msg.c_str()));
    }
    fclose(fp);
  }

  return ret;
}

void UIDcm::on_btnOpenDcmJs_clicked(void) {
  auto fileName = QFileDialog::getOpenFileName(this, "json config", "", "json config (*.json)");

  auto r = Load(fileName.toStdString());
  if (r) {
    auto target = m_Json["Target"].dump();
    m_CmbxTarget->addItem(tr(target.c_str()));
    m_LeDcmJs->setText(fileName);
  } else {
    m_LeDcmJs->setText("");
  }
}

int UIDcm::Transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res) {
  int r = -1;
  if (m_Dcm != nullptr) {
    r = m_Dcm->Transmit(req, res);
  } else {
    QMessageBox::critical(this, "Error", tr("dcm not created"));
  }

  return r;
}

void UIDcm::timerEvent(QTimerEvent *event) {
  (void)event;
  std::vector<uint8_t> req = {0x3e, 0x80};
  std::vector<uint8_t> res;
  if (m_Dcm != nullptr) {
    m_Dcm->Transmit(req, res);
  }
}

void UIDcm::on_btnStart_clicked(void) {
  if (m_BtnStart->text().toStdString() == "Start") {
    auto r = LoadUi();
    if (r) {
      m_BtnStart->setText("Stop");
    }
  } else {
    m_TabWidget->clear();
    m_Dcm = nullptr;
    m_CbxTesterPresent->setCheckState(Qt::Unchecked);
    m_BtnStart->setText("Start");
  }
}

void UIDcm::on_cbxTesterPresent_stateChanged(int state) {
  if (state) {
    m_TPtimer = startTimer(3000);
  } else {
    killTimer(m_TPtimer);
  }
}

void UIDcm::on_Tree_itemSelectionChanged(void) {
  UITreeItem<UIDcm> *item = (UITreeItem<UIDcm> *)m_Tree->currentItem();
  auto index = m_Tree->indexOfTopLevelItem(item);
  if (-1 == index) {
    auto topItem = item->parent();
    index = m_Tree->indexOfTopLevelItem(topItem);
    m_TabWidget->setCurrentIndex(index);
    auto uiGroup = item->Group();
    auto wd = item->Service();
    uiGroup->ensureWidgetVisible(wd);
  } else {
    m_TabWidget->setCurrentIndex(index);
  }
}

QTreeWidget *UIDcm::Tree(void) {
  return m_Tree;
}

UIDcm::~UIDcm() {
}

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  QMainWindow w;
  w.setCentralWidget(new UIDcm());
  w.show();
  return a.exec();
}

extern "C" std::string Name(void) {
  return "Dcm";
}

extern "C" QWidget *Create(void) {
  return new UIDcm();
}
