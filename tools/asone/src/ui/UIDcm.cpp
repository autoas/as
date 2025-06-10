/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail->dcm>
 */
/* ================================ { INCLUDES  ] ============================================== */
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
#include <dlfcn.h>
#include <mutex>
#include <iostream>

#include "isotp.h"
#include "Std_Bit.h"
#include "Log.hpp"

using namespace as;
/* ================================ { MACROS    ] ============================================== */

/* ================================ { TYPES     ] ============================================== */
/* ================================ { DECLARES  ] ============================================== */
/* ================================ { DATAS     ] ============================================== */
/* ================================ { LOCALS    ] ============================================== */
static std::string format(const uint8_t *data, int len) {
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

static std::string format(std::vector<uint8_t> data) {
  return format(data.data(), (int)data.size());
}
/* ================================ { FUNCTIONS ] ============================================== */
class Dcm {
public:
  Dcm(json &js) {
    auto protocol = get<std::string>(js, "protocol", "CAN");
    auto device = get<std::string>(js, "device", "simulator_v2");
    uint32_t port = get<uint32_t>(js, "port", 0);
    uint32_t baudrate = get<uint32_t>(js, "baudrate", 500000);
    uint32_t rxid = 0x732, txid = 0x731;
    uint16_t N_TA = get<uint16_t>(js, "N_TA", 0xFFFF);
    uint32_t delayUs = get<uint32_t>(js, "delayUs", 0);
    auto LL_DL = get<uint32_t>(js, "LL_DL", 8);
    if (protocol == "LIN") {
      rxid = 0x3d;
      txid = 0x3c;
    }

    rxid = get2<uint32_t>(js, "rxid", rxid);
    txid = get2<uint32_t>(js, "txid", txid);

    m_Params.baudrate = (uint32_t)baudrate;
    m_Params.port = port;
    m_Params.ll_dl = LL_DL;
    m_Params.N_TA = N_TA;

    if (protocol == "CAN") {
      strcpy(m_Params.device, device.c_str());
      m_Params.protocol = ISOTP_OVER_CAN;
      m_Params.U.CAN.RxCanId = (uint32_t)rxid;
      m_Params.U.CAN.TxCanId = (uint32_t)txid;
      m_Params.U.CAN.BlockSize = get<uint32_t>(js, "block_size", 8);
      m_Params.U.CAN.STmin = get<uint32_t>(js, "STmin", 0);
    } else if (protocol == "LIN") {
      strcpy(m_Params.device, device.c_str());
      m_Params.protocol = ISOTP_OVER_LIN;
      m_Params.U.LIN.RxId = (uint32_t)rxid;
      m_Params.U.LIN.TxId = (uint32_t)txid;
      m_Params.U.LIN.timeout = get<uint32_t>(js, "STmin", 100);
      m_Params.U.LIN.delayUs = delayUs;
    } else if (protocol == "DoIP") {
      auto ipStr = get<std::string>(js, "ip", "224.244.224.245");
      strcpy(m_Params.device, ipStr.c_str());
      m_Params.protocol = ISOTP_OVER_DOIP;
      m_Params.port = get<int>(js, "port", 13400);
      m_Params.U.DoIP.sourceAddress = get<uint16_t>(js, "sa", 0xbeef);
      m_Params.U.DoIP.targetAddress = get<uint16_t>(js, "ta", 0xdead);
      m_Params.U.DoIP.activationType = get<uint8_t>(js, "at", 0xda);
    } else {
      throw std::runtime_error("invalid protocol " + protocol);
    }

    m_IsoTp = isotp_create(&m_Params);
    if (nullptr == m_IsoTp) {
      throw std::runtime_error("failed to create isotp");
    }

    LOG(INFO, "DCM create: %s:%s:%d txid=0x%x rxid=0x%x\n", protocol.c_str(), device.c_str(), port,
        txid, rxid);
  }

  int transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res) {
    std::unique_lock<std::mutex> lck(m_Lock);

    res.clear();
    LOG(INFO, "Dcm request: %s\n", format(req).c_str());
    int r = isotp_transmit(m_IsoTp, (uint8_t *)req.data(), req.size(), nullptr, 0);

    if ((req.size() >= 2) && ((0x3e == req[0])) && ((req[1] & 0x80) != 0)) {
      /* suppress positive response */
      res.push_back(req[0] | 0x40);
      return 0;
    }

    while (0 == r) {
      int rlen = isotp_receive(m_IsoTp, m_Buffer, sizeof(m_Buffer));
      if (rlen > 0) {
        LOG(INFO, "Dcm response: %s\n", format(m_Buffer, rlen).c_str());
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
      isotp_destory(m_IsoTp);
    }
  }

private:
  isotp_t *m_IsoTp = nullptr;

  uint8_t m_Buffer[4096];
  isotp_parameter_t m_Params;

  std::mutex m_Lock;
};

UISessionControl::UISessionControl(json &js, UIDcm *dcm)
  : QGroupBox("session control"), m_Dcm(dcm) {
  m_cmbxSessions = new QComboBox();
  for (auto &it : js.items()) {
    auto sesName = it.key();
    uint8_t sesId = (uint8_t)toInteger(it.value().get<std::string>());
    m_SessionMap[sesName] = sesId;
    m_cmbxSessions->addItem(tr(sesName.c_str()));
  }
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("Session:"), 0, 0);
  m_btnEnter = new QPushButton("Enter");
  grid->addWidget(m_cmbxSessions, 0, 1);
  grid->addWidget(m_btnEnter, 0, 2);
  setLayout(grid);
  connect(m_btnEnter, SIGNAL(clicked()), this, SLOT(on_btnEnter_clicked()));
}

void UISessionControl::on_btnEnter_clicked(void) {
  auto session = m_cmbxSessions->currentText().toStdString();
  auto sesId = m_SessionMap[session];
  std::vector<uint8_t> req;
  std::vector<uint8_t> res;
  req.push_back(0x10);
  req.push_back(sesId);
  auto r = m_Dcm->transmit(req, res);
  if (r != 0) {
    std::string msg = "error " + std::to_string(r) + ", response " + format(res);
    QMessageBox::critical(this, "Error", tr(msg.c_str()));
  } else {
    std::string msg = "session control OK with response " + format(res);
    QMessageBox::information(this, "Info", tr(msg.c_str()));
  }
}

UISessionControl::~UISessionControl() {
}

UISecurityAccess::UISecurityAccess(json &js, UIDcm *dcm)
  : QGroupBox("security access"), m_Dcm(dcm) {
  m_cmbxSecurityLevels = new QComboBox();
  for (auto &it : js.items()) {
    auto name = it.key();
    Info info;
    auto &jsLvl = it.value();
    info.level = get2(jsLvl, "level", 0);
    if (info.level == 0) {
      throw std::runtime_error("found invalid security level for " + name);
    }
    if (jsLvl.contains("lua")) {
      info.m_Scripts = "";
      for (auto &jsCode : jsLvl["lua"]) {
        info.m_Scripts += jsCode.get<std::string>() + "\n";
      }
      LOG(INFO, "lua script for security level %s:\n%s\n", name.c_str(), info.m_Scripts.c_str());
    } else if (jsLvl.contains("dll")) {
      auto dllPath = jsLvl["dll"].get<std::string>();
      info.dll = dlopen(dllPath.c_str(), RTLD_NOW);
      if (nullptr != info.dll) {
        info.calculate_key = (calculate_key_t)dlsym(info.dll, "CalculateKey");
      }
      if ((nullptr == info.dll) || (nullptr == info.calculate_key)) {
        throw std::runtime_error("invalid dll " + dllPath + " for level " + name);
      }
    } else {
      throw std::runtime_error("no unlock method specified for level " + name);
    }
    m_SecurityMap[name] = info;
    m_cmbxSecurityLevels->addItem(tr(name.c_str()));
  }
  auto grid = new QGridLayout();
  grid->addWidget(new QLabel("Security Level:"), 0, 0);
  m_btnUnlock = new QPushButton("Unlock");
  grid->addWidget(m_cmbxSecurityLevels, 0, 1);
  grid->addWidget(m_btnUnlock, 0, 2);
  setLayout(grid);
  connect(m_btnUnlock, SIGNAL(clicked()), this, SLOT(on_btnUnlock_clicked()));
}

std::vector<uint8_t> UISecurityAccess::CalculateKey(Info &info, std::vector<uint8_t> res) {
  std::vector<uint8_t> key;

  if (nullptr != info.calculate_key) {
    key = info.calculate_key(res);
  } else {
    lua_arg_t inArgs[1];
    lua_arg_t outArgs[1];

    inArgs[0].type = LUA_ARG_TYPE_UINT8_N;
    inArgs[0].u8N = res;

    outArgs[0].type = LUA_ARG_TYPE_UINT8_N;
    auto r = aslua_call(info.m_Scripts.c_str(), "CalculateKey", inArgs, 1, outArgs, 1);
    if (0 == r) {
      key = outArgs[0].u8N;
    }
  }

  return key;
}

void UISecurityAccess::on_btnUnlock_clicked(void) {
  auto levelName = m_cmbxSecurityLevels->currentText().toStdString();
  auto info = m_SecurityMap[levelName];
  std::vector<uint8_t> req;
  std::vector<uint8_t> res;
  req.push_back(0x27);
  req.push_back(info.level);
  auto r = m_Dcm->transmit(req, res);
  if (r != 0) {
    std::string msg = "error " + std::to_string(r) + " , response " + format(res);
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
    req.push_back(info.level + 1);
    auto key = CalculateKey(info, res);
    if (key.size() > 0) {
      for (auto k : key) {
        req.push_back(k);
      }
      auto r = m_Dcm->transmit(req, res);
      if (r != 0) {
        std::string msg = "error " + std::to_string(r) + " , response " + format(res);
        QMessageBox::critical(this, "Error", tr(msg.c_str()));
      } else {
        std::string msg = "security access OK with response " + format(res);
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
  : UIServiceBase(js["name"].get<std::string>(), dcm->luaScript()), m_Dcm(dcm) {
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
  auto r = m_Dcm->transmit(req, res);

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
          auto name2 = jss["name"].get<std::string>();
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
  m_leDcmJs = new QLineEdit();
  m_leDcmJs->setReadOnly(true);
  grid->addWidget(m_leDcmJs, 0, 1);
  m_btnOpenDcmJs = new QPushButton("...");
  grid->addWidget(m_btnOpenDcmJs, 0, 2);

  grid->addWidget(new QLabel("target:"), 1, 0);
  m_cmbxTarget = new QComboBox();
  m_cmbxTarget->setEditable(true);
  grid->addWidget(m_cmbxTarget, 1, 1);
  m_btnStart = new QPushButton("start");
  grid->addWidget(m_btnStart, 1, 2);
  m_cbxTesterPresent = new QCheckBox("Tester Present");
  grid->addWidget(m_cbxTesterPresent, 2, 2);

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
  connect(m_btnOpenDcmJs, SIGNAL(clicked()), this, SLOT(on_btnOpenDcmJs_clicked()));
  connect(m_btnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));
  connect(m_cbxTesterPresent, SIGNAL(stateChanged(int)), this,
          SLOT(on_cbxTesterPresent_stateChanged(int)));
  connect(m_Tree, SIGNAL(itemSelectionChanged()), this, SLOT(on_Tree_itemSelectionChanged()));

  std::string defJs = "./diagnostic.json";
  if (0 != access(defJs.c_str(), F_OK | R_OK)) {
    defJs = "../../../../tools/asone/examples/diagnostic.json";
  }

  auto r = load(defJs);
  if (r) {
    auto target = m_Json["target"].dump();
    m_cmbxTarget->addItem(tr(target.c_str()));
    m_leDcmJs->setText(tr(defJs.c_str()));
    m_cmbxTarget->addItem(
      "{\"protocol\": \"LIN\", \"device\": \"spi/CH341_SPI/2,1\", \"port\": 0, \"txid\": \"0x3c\", "
      "\"rxid\": \"0x3d\", \"N_TA\": 0, \"baudrate\": 544000, \"delayUs\": 20000, \"LL_DL\": 8}");
    m_cmbxTarget->addItem(
      "{\"protocol\": \"LIN\", \"device\": \"i2c/CH341_I2C\", \"port\": 0, \"txid\": \"0x51d0\", "
      "\"rxid\": \"0x51d1\", \"N_TA\": 0, \"baudrate\": 100000, \"delayUs\": 20000, \"LL_DL\": 8}");
    m_cmbxTarget->addItem(
      "{\"protocol\": \"LIN\", \"device\": \"lvds\", \"port\": 8, \"txid\": \"0x3c\", "
      "\"rxid\": \"0x3d\", \"N_TA\": 0, \"baudrate\": 115200, \"delayUs\": 20000, \"LL_DL\": 64}");
  }
}

bool UIDcm::loadScript(void) {
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

bool UIDcm::loadUI(void) {
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
    auto item = new UITreeItem<UIDcm>(name);
    m_tabWidget->addTab(new UIGroup(group, this, item), tr(name.c_str()));
    m_Tree->addTopLevelItem(item);
  }

  try {
    auto s = m_cmbxTarget->currentText().toStdString();
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

bool UIDcm::load(std::string cfgPath) {
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

  auto r = load(fileName.toStdString());
  if (r) {
    auto target = m_Json["target"].dump();
    m_cmbxTarget->addItem(tr(target.c_str()));
    m_leDcmJs->setText(fileName);
  } else {
    m_leDcmJs->setText("");
  }
}

int UIDcm::transmit(std::vector<uint8_t> &req, std::vector<uint8_t> &res) {
  int r = -1;
  if (m_Dcm != nullptr) {
    r = m_Dcm->transmit(req, res);
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
    m_Dcm->transmit(req, res);
  }
}

void UIDcm::on_btnStart_clicked(void) {
  if (m_btnStart->text().toStdString() == "start") {
    auto r = loadUI();
    if (r) {
      m_btnStart->setText("stop");
    }
  } else {
    m_tabWidget->clear();
    m_Dcm = nullptr;
    m_cbxTesterPresent->setCheckState(Qt::Unchecked);
    m_btnStart->setText("start");
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
    m_tabWidget->setCurrentIndex(index);
    auto uiGroup = item->group();
    auto wd = item->service();
    uiGroup->ensureWidgetVisible(wd);
  } else {
    m_tabWidget->setCurrentIndex(index);
  }
}

QTreeWidget *UIDcm::tree(void) {
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

extern "C" std::string name(void) {
  return "Dcm";
}

extern "C" QWidget *create(void) {
  return new UIDcm();
}
