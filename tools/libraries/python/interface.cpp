/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <pybind11/pybind11.h>
#include <string>
#include <thread>
#include <chrono>

#include <stdio.h>
#include <set>
#include <stdarg.h>
#include "canlib.h"
#include "isotp.h"
#include "Log.hpp"

#include "Std_Compiler.h"
#include "linlib.h"
#include "BitArray.hpp"
#include "devlib.h"
#include "loader.h"
#include "srec.h"
#include "aslua.hpp"

using namespace as;
namespace py = pybind11;
/* ================================ [ MACROS    ] ============================================== */
#define ISOTP_KWARGS                                                                               \
  "\tProtocol: str, 'CAN' or 'LIN', default 'CAN'\n"                                               \
  "\tLinkLayerDataLength: int, Link Layer Data Length, default 8\n"                                \
  "\tDevice: str, 'simulator', ..., default 'simulator'\n"                                         \
  "\tPort: int, default 0\n"                                                                       \
  "\tBaudrate: int, default 500000\n"                                                              \
  "\tTxId: int, default 0x731 for 'CAN', 0x3C for 'LIN'\n"                                         \
  "\tRxId: int, default 0x732 for 'CAN', 0x3D for 'LIN'\n"                                         \
  "\tN_TargetAddress: int, default 0xFFFF\n"                                                       \
  "\tDelayUs: int, unit us\n"                                                                      \
  "\tBlockSize: int, default 8\n"                                                                  \
  "\tSTmin: int, default 0 for CAN, 100 for LIN, unit ms\n"

#define LIN_KWARGS                                                                                 \
  "\tDevice: str, default 'simulator'\n"                                                           \
  "\tPort: int, default 0\n"                                                                       \
  "\tBaudrate: int, default 500000\n"                                                              \
  "\tEnhanced: bool, default true\n"                                                               \
  "\tTimeout: int, unit ms\n"                                                                      \
  "\tDelayUs: int, unit us\n"

/* Legacy keyword names, kept working for one release. The canonical names are PascalCase
 * (see doc/CN/CCodingStyle.md 3.7); the aliases exist so that out-of-tree scripts do not silently
 * fall back to the defaults the way they did when the rename landed. */
struct KwargAlias {
  const char *Legacy;
  const char *Canonical;
};

static const KwargAlias lKwargAliases[] = {
  {"protocol", "Protocol"},
  {"device", "Device"},
  {"port", "Port"},
  {"baudrate", "Baudrate"},
  {"version", "Version"},
  {"N_TA", "N_TargetAddress"},
  {"delayUs", "DelayUs"},
  {"LL_DL", "LinkLayerDataLength"},
  {"rxid", "RxId"},
  {"txid", "TxId"},
  {"block_size", "BlockSize"},
  {"enhanced", "Enhanced"},
};

static void Dev_WarnLegacyKwarg(const std::string &canonical, const char *legacy) {
  static std::set<std::string> warned;
  if (warned.insert(canonical).second) {
    fprintf(stderr, "[DEPRECATED] keyword '%s' is spelled '%s' now; the old name still works\n",
            legacy, canonical.c_str());
  }
}

template <typename To, typename Ti> To AsPy_Get(py::kwargs &kwargs, std::string key, To dft) {
  To r = dft;
  try {
    r = Ti(kwargs[key.c_str()]);
    return r;
  } catch (const std::exception &e) {
    /* the canonical name is absent, fall through to the legacy spelling */
  }
  for (const auto &alias : lKwargAliases) {
    if (key == alias.Canonical) {
      if (kwargs.contains(alias.Legacy)) {
        r = Ti(kwargs[alias.Legacy]);
        Dev_WarnLegacyKwarg(key, alias.Legacy);
      }
      break;
    }
  }
  return r;
}
/* ================================ [ TYPES     ] ============================================== */
class Can {
public:
  Can(std::string device, uint32_t port, uint32_t baudrate) {
    m_BusId = CanL_Open(device.c_str(), port, baudrate);
    if (m_BusId < 0) {
      throw std::runtime_error("failed to create can " + device + " port " + std::to_string(port) +
                               " baudrate " + std::to_string(baudrate));
    }
  }

  ~Can() {
    (void)CanL_Close(m_BusId);
  }

  bool IsOpened() {
    return (m_BusId >= 0);
  }

  py::object Read(uint32_t canid, uint32_t timeoutMs = 0) {
    uint32_t rcanid = canid;
    uint8_t data[64];
    uint8_t dlc = sizeof(data);
    bool r = FALSE;
    r = CanL_Read(m_BusId, &rcanid, &dlc, data);
    while (FALSE == r) {
      if (timeoutMs > 0) {
        timeoutMs--;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      } else {
        break;
      }
      rcanid = canid;
      dlc = sizeof(data);
      r = CanL_Read(m_BusId, &rcanid, &dlc, data);
    }
    py::list L;
    L.append(py::bool_(r));
    if (true == r) {
      L.append(py::int_(rcanid));
      L.append(py::bytes((char *)data, dlc));
    } else {
      L.append(py::none());
      L.append(py::none());
    }
    return L;
  }

  bool Write(uint32_t canid, py::bytes b) {
    std::string str = b;
    bool r = CanL_Write(m_BusId, canid, (uint8_t)str.size(), (uint8_t *)str.data());
    return r;
  }

private:
  int m_BusId;
};

class Lin {
public:
  Lin(py::kwargs kwargs) {
    auto device = AsPy_Get<std::string, py::str>(kwargs, "Device", "simulator");
    uint32_t port = AsPy_Get<uint32_t, py::int_>(kwargs, "Port", 0);
    uint32_t baudrate = AsPy_Get<uint32_t, py::int_>(kwargs, "Baudrate", 0);
    m_Enhanced = AsPy_Get<bool, py::bool_>(kwargs, "Enhanced", true);
    m_Timeout = AsPy_Get<uint32_t, py::int_>(kwargs, "Timeout", 100);
    m_DelayUs = AsPy_Get<uint32_t, py::int_>(kwargs, "DelayUs", 0);
    m_BusId = LinL_Open(device.c_str(), port, baudrate);
    if (m_BusId < 0) {
      throw std::runtime_error("failed to create lin " + device + " port " + std::to_string(port) +
                               " baudrate " + std::to_string(baudrate));
    }
    uint32_t _tmo = m_Timeout * 1000;
    Dev_IoCtl(m_BusId, DEV_IOCTL_SET_TIMEOUT, &_tmo, sizeof(_tmo));
    Dev_IoCtl(m_BusId, DEV_IOCTL_SET_DELAY, &m_DelayUs, sizeof(m_DelayUs));
  }

  ~Lin() {
    (void)LinL_Close(m_BusId);
  }

  bool IsOpened() {
    return (m_BusId >= 0);
  }

  py::object Read(LinL_IdType id, int dlc) {
    uint8_t data[64];
    bool r = LinL_Read(m_BusId, id, (uint8_t)dlc, data, m_Enhanced, m_Timeout);
    py::list L;
    L.append(py::bool_(r));
    if (true == r) {
      L.append(py::int_(id));
      L.append(py::bytes((char *)data, dlc));
    } else {
      L.append(py::none());
      L.append(py::none());
    }
    return L;
  }

  bool Write(LinL_IdType id, py::bytes b) {
    std::string str = b;
    bool r = LinL_Write(m_BusId, id, (uint8_t)str.size(), (uint8_t *)str.data(), m_Enhanced);
    return r;
  }

private:
  int m_BusId;
  bool m_Enhanced;
  uint32_t m_Timeout;
  uint32_t m_DelayUs;
};

class Isotp {
public:
  Isotp(py::kwargs kwargs) {
    std::string protocol = AsPy_Get<std::string, py::str>(kwargs, "Protocol", "CAN");
    auto device = AsPy_Get<std::string, py::str>(kwargs, "Device", "simulator");
    uint32_t port = AsPy_Get<uint32_t, py::int_>(kwargs, "Port", 0);
    uint32_t baudrate = AsPy_Get<uint32_t, py::int_>(kwargs, "Baudrate", 500000);
    IsoTp_CanVersionType version =
      (IsoTp_CanVersionType)AsPy_Get<uint32_t, py::int_>(kwargs, "Version", 2);
    uint32_t rxid = 0x732, txid = 0x731;
    uint16_t N_TA = AsPy_Get<uint16_t, py::int_>(kwargs, "N_TargetAddress", 0xFFFF);
    uint32_t linkLayerDataLength;
    uint32_t delayUs = AsPy_Get<uint32_t, py::int_>(kwargs, "DelayUs", 0);
    if (protocol == "LIN") {
      rxid = 0x3d;
      txid = 0x3c;
    }
    rxid = AsPy_Get<uint32_t, py::int_>(kwargs, "RxId", rxid);
    txid = AsPy_Get<uint32_t, py::int_>(kwargs, "TxId", txid);
    linkLayerDataLength = AsPy_Get<uint32_t, py::int_>(kwargs, "LinkLayerDataLength", 8);
    m_Params.Baudrate = (uint32_t)baudrate;
    m_Params.Port = port;
    m_Params.LinkLayerDataLength = linkLayerDataLength;
    m_Params.N_TA = N_TA;
    if (protocol == "CAN") {
      strcpy(m_Params.Device, device.c_str());
      m_Params.Protocol = ISOTP_OVER_CAN;
      m_Params.U.CAN.RxCanId = (uint32_t)rxid;
      m_Params.U.CAN.TxCanId = (uint32_t)txid;
      m_Params.U.CAN.BlockSize = AsPy_Get<uint32_t, py::int_>(kwargs, "BlockSize", 8);
      m_Params.U.CAN.STmin = AsPy_Get<uint32_t, py::int_>(kwargs, "STmin", 0);
      m_Params.U.CAN.Version = version;
    } else if (protocol == "LIN") {
      strcpy(m_Params.Device, device.c_str());
      m_Params.Protocol = ISOTP_OVER_LIN;
      m_Params.U.LIN.RxId = (uint32_t)rxid;
      m_Params.U.LIN.TxId = (uint32_t)txid;
      m_Params.U.LIN.Timeout = AsPy_Get<uint32_t, py::int_>(kwargs, "STmin", 100);
      m_Params.U.LIN.DelayUs = delayUs;
    } else if (protocol == "DoIP") {
      auto ipStr = AsPy_Get<std::string, py::str>(kwargs, "Ip", "224.244.224.245");
      strcpy(m_Params.Device, ipStr.c_str());
      m_Params.Protocol = ISOTP_OVER_DOIP;
      m_Params.Port = AsPy_Get<int, py::int_>(kwargs, "Port", 13400);
      m_Params.U.DoIP.SourceAddress = AsPy_Get<uint16_t, py::int_>(kwargs, "SourceAddress", 0xbeef);
      m_Params.U.DoIP.TargetAddress = AsPy_Get<uint16_t, py::int_>(kwargs, "TargetAddress", 0xdead);
      m_Params.U.DoIP.ActivationType = AsPy_Get<uint8_t, py::int_>(kwargs, "ActivationType", 0x00);
    } else {
      throw std::runtime_error("invalid protocol " + protocol);
    }

    m_Tp = IsoTp_Create(&m_Params);
    if (nullptr == m_Tp) {
      throw std::runtime_error("failed to create isotp");
    }
  }

  bool Transmit(py::bytes b, uint32_t timeoutMs = 5000) {
    std::string str = b;
    int r;
    uint32_t timeoutUs = timeoutMs * 1000;

    (void)IsoTp_IoCtl(m_Tp, ISOTP_IOCTL_SET_TIMEOUT, &timeoutUs, sizeof(uint32_t));

    r = IsoTp_Transmit(m_Tp, (uint8_t *)str.data(), str.size(), nullptr, 0);
    return (0 == r);
  }

  py::object Receive(uint32_t timeoutMs = 5000) {
    int r;
    uint32_t timeoutUs = timeoutMs * 1000;

    (void)IsoTp_IoCtl(m_Tp, ISOTP_IOCTL_SET_TIMEOUT, &timeoutUs, sizeof(uint32_t));

    r = IsoTp_Receive(m_Tp, m_Buffer, sizeof(m_Buffer));

    if (r > 0) {
      return py::bytes((const char *)m_Buffer, r);
    }

    return py::none();
  }

  ~Isotp() {
    if (m_Tp) {
      IsoTp_Destroy(m_Tp);
    }
  }
  IsoTp_Type *GetIsoTp() {
    return m_Tp;
  }

private:
  IsoTp_Type *m_Tp = nullptr;
  uint8_t m_Buffer[4096];
  IsoTp_ParameterType m_Params;
};

class Dev {
public:
  Dev(std::string device, std::string option) {
    m_Fd = Dev_Open(device.c_str(), option.c_str());
    if (m_Fd < 0) {
      throw std::runtime_error("failed to create dev " + device + " option " + option);
    }
  }

  ~Dev() {
    Dev_Close(m_Fd);
  }

  int Write(py::bytes b) {
    std::string str = b;
    return Dev_Write(m_Fd, (uint8_t *)str.data(), str.size());
  }

  py::object Read() {
    int r = Dev_Read(m_Fd, m_Data, sizeof(m_Data));
    py::object obj;
    if (r > 0) {
      obj = py::bytes((char *)m_Data, r);
    } else {
      obj = py::none();
    }
    return obj;
  }

private:
  int m_Fd;
  uint8_t m_Data[128];
};

class Loader {
public:
  Loader(py::kwargs kwargs) {
    m_IsoTp = new Isotp(kwargs);
    m_App = py::str(kwargs["app"]);
    m_Fls = py::str(kwargs["fls"]);
    m_LogLevel = AsPy_Get<int, py::int_>(kwargs, "logLevel", L_LOG_INFO);
    m_Choice = AsPy_Get<std::string, py::str>(kwargs, "choice", "FBL");
    m_SignType = (Srec_SignType)AsPy_Get<int, py::int_>(kwargs, "signType", SREC_SIGN_CRC16);
    m_FuncAddr = (uint32_t)AsPy_Get<int, py::int_>(kwargs, "funcAddr", 0x7DF);

    m_AppSrec = Srec_Open(m_App.c_str());
    if (nullptr == m_AppSrec) {
      throw std::runtime_error("failed to open app image " + m_App);
    }

    m_FlsSrec = Srec_Open(m_Fls.c_str());
    if (nullptr == m_FlsSrec) {
      throw std::runtime_error("failed to open flash driver image " + m_Fls);
    }
  }

  ~Loader() {
    if (nullptr != m_Loader) {
      Loader_Destroy(m_Loader);
    }

    if (nullptr != m_IsoTp) {
      delete m_IsoTp;
    }

    if (nullptr != m_AppSrec) {
      Srec_Close(m_AppSrec);
    }

    if (nullptr != m_FlsSrec) {
      Srec_Close(m_FlsSrec);
    }
  }

  bool Start() {
    Loader_ArgsType args;
    args.Isotp = m_IsoTp->GetIsoTp();
    args.AppSRec = m_AppSrec;
    args.FlsSRec = m_FlsSrec;
    args.Choice = m_Choice.c_str();
    args.SignType = m_SignType;
    args.FuncAddr = m_FuncAddr;
    m_Loader = Loader_Create(&args);
    if (nullptr == m_Loader) {
      throw std::runtime_error("failed to start loader");
    }
    Loader_SetLogLevel(m_Loader, m_LogLevel);
    return true;
  }

  void Stop() {
    if (nullptr != m_Loader) {
      Loader_Destroy(m_Loader);
      m_Loader = nullptr;
    }

    if (nullptr != m_IsoTp) {
      delete m_IsoTp;
      m_IsoTp = nullptr;
    }
  }

  py::object Poll() {
    int progress = 0;
    char *log = NULL;
    int r = -1;
    py::list L;
    if (nullptr != m_Loader) {
      r = Loader_Poll(m_Loader, &progress, &log);
    }
    L.append(py::bool_(0 == r));
    L.append(py::float_(progress / 100.0f));
    if (NULL != log) {
      L.append(py::str(log));
      free(log);
    } else {
      L.append(py::none());
    }
    return L;
  }

private:
  Isotp *m_IsoTp = nullptr;
  std::string m_App;
  std::string m_Fls;
  Srec_FileType *m_AppSrec = nullptr;
  Srec_FileType *m_FlsSrec = nullptr;
  Loader_Type *m_Loader = nullptr;
  std::string m_Choice = "FBL";
  Srec_SignType m_SignType = SREC_SIGN_CRC16;
  int m_LogLevel = L_LOG_INFO;
  uint32_t m_FuncAddr = 0;
};

class PyBitArray {
public:
  PyBitArray(py::bytes data = py::bytes(), std::string endianS = "BIG") {
    BitArray::Endian endian;
    if ("BIG" == endianS) {
      endian = BitArray::Endian::BIG;
    } else {
      endian = BitArray::Endian::LITTLE;
    }

    std::string b = data;
    if (b.empty()) {
      m_BitArray = std::make_shared<BitArray>(endian);
    } else {

      m_BitArray = std::make_shared<BitArray>((uint8_t *)b.c_str(), b.size(), endian);
    }
  }

  void Put(uint32_t u32, uint8_t nBits) {
    m_BitArray->Put(u32, nBits);
  }

  uint32_t Get(uint8_t nBits) {
    return m_BitArray->Get(nBits);
  }

  py::bytes ToBytes() {
    auto data = m_BitArray->Bytes();
    return py::bytes((char *)data.data(), data.size());
  }

  py::bytes LeftBytes() {
    auto data = m_BitArray->LeftBytes();
    return py::bytes((char *)data.data(), data.size());
  }

  void Drop(uint32_t nBits) {
    m_BitArray->Drop(nBits);
  }

  ~PyBitArray() {
  }

private:
  std::shared_ptr<BitArray> m_BitArray;
};

class PyLua {
public:
  PyLua(std::string scriptFile) {
    m_Lua = std::make_shared<AsLuaScript>(scriptFile);
  }

  py::object TableGet(std::string api) {
    py::list L;
    Lua_ArgType outArgs[3];

    outArgs[0].Type = LUA_ARG_TYPE_UINT8_N;  /* get new bytes data */
    outArgs[1].Type = LUA_ARG_TYPE_STRING_N; /* bytes data headers */
    outArgs[2].Type = LUA_ARG_TYPE_STRING_N; /* bytes data values */
    auto r = m_Lua->Call(api, nullptr, 0, outArgs, 3);
    if (0 == r) {
      L.append(py::bytes((char *)outArgs[0].U8N.data(), outArgs[0].U8N.size()));
      py::list Lh;
      for (auto &h : outArgs[1].StringN) {
        Lh.append(py::str(h));
      }
      L.append(Lh);
      py::list Lv;
      for (auto &v : outArgs[2].StringN) {
        Lv.append(py::str(v));
      }
      L.append(Lv);
    } else {
      throw std::runtime_error("Failed to get table content");
    }
    return L;
  }

  py::object TableDecode(std::string api, py::bytes data) {
    py::list L;
    Lua_ArgType inArgs[1];
    Lua_ArgType outArgs[4];

    inArgs[0].Type = LUA_ARG_TYPE_UINT8_N;
    std::string b = data;
    for (size_t i = 0; i < b.size(); i++) {
      inArgs[0].U8N.push_back((uint8_t)b[i]);
    }

    outArgs[0].Type = LUA_ARG_TYPE_BOOL;     /* decoding results */
    outArgs[1].Type = LUA_ARG_TYPE_STRING_N; /* decoding headers */
    outArgs[2].Type = LUA_ARG_TYPE_STRING_N; /* decoding values */
    outArgs[3].Type = LUA_ARG_TYPE_UINT32;   /* consumed bits */
    auto r = m_Lua->Call(api, inArgs, 1, outArgs, 4);
    if (0 == r) {
      if (outArgs[0].Bool) {
        py::list Lh;
        for (auto &h : outArgs[1].StringN) {
          Lh.append(py::str(h));
        }
        L.append(Lh);
        py::list Lv;
        for (auto &v : outArgs[2].StringN) {
          Lv.append(py::str(v));
        }
        L.append(Lv);
        L.append(py::int_(outArgs[3].U32));
      } else {
        throw std::runtime_error("Failed to decode table content:" + outArgs[1].StringN[0]);
      }
    } else {
      throw std::runtime_error("Failed to decode table content");
    }
    return L;
  }

  ~PyLua() {
  }

private:
  std::shared_ptr<AsLuaScript> m_Lua;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
PYBIND11_MODULE(AsPy, m) {
  Log::SetName("AsPy");
  m.doc() = "pybind11 AsPy library";
  py::class_<Can>(m, "can")
    .def(py::init<std::string, uint32_t, uint32_t>(), py::arg("device") = "simulator",
         py::arg("port") = 0, py::arg("baudrate") = 500000)
    .def("is_opened", &Can::IsOpened)
    .def("read", &Can::Read, py::arg("canid"), py::arg("timeoutMs") = 0)
    .def("write", &Can::Write, py::arg("canid"), py::arg("data"));
  py::class_<Lin>(m, "lin")
    .def(py::init<py::kwargs>(), LIN_KWARGS)
    .def("is_opened", &Lin::IsOpened)
    .def("read", &Lin::Read, py::arg("id"), py::arg("dlc") = 8)
    .def("write", &Lin::Write, py::arg("id"), py::arg("data"));
  py::class_<Isotp>(m, "isotp")
    .def(py::init<py::kwargs>(), ISOTP_KWARGS)
    .def("transmit", &Isotp::Transmit, py::arg("data"), py::arg("timeoutMs") = 5000)
    .def("receive", &Isotp::Receive, py::arg("timeoutMs") = 5000);
  py::class_<Loader>(m, "loader")
    .def(py::init<py::kwargs>(), "\tapp: str, 'the app image'\n"
                                 "\tfls: str, 'the flash driver image'\n"
                                 "\tlogLevel: int, 'the loader log level'\n" ISOTP_KWARGS)
    .def("start", &Loader::Start)
    .def("stop", &Loader::Stop)
    .def("poll", &Loader::Poll);
  py::class_<Dev>(m, "dev")
    .def(py::init<std::string, std::string>(), py::arg("device"), py::arg("options"))
    .def("read", &Dev::Read)
    .def("write", &Dev::Write, py::arg("data"));
  py::class_<PyBitArray>(m, "bitarray")
    .def(py::init<py::bytes, std::string>(), py::arg("data") = py::bytes(),
         py::arg("endian") = "BIG")
    .def("put", &PyBitArray::Put, py::arg("u32"), py::arg("nBits"))
    .def("get", &PyBitArray::Get, py::arg("nBits"))
    .def("tobytes", &PyBitArray::ToBytes)
    .def("left", &PyBitArray::LeftBytes)
    .def("drop", &PyBitArray::Drop, py::arg("nBits"));
  py::class_<PyLua>(m, "lua")
    .def(py::init<std::string>())
    .def("table_get", &PyLua::TableGet, py::arg("api"))
    .def("table_decode", &PyLua::TableDecode, py::arg("api"), py::arg("data"));
}
