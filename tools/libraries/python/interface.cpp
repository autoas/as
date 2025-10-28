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
  "\tprotocol: str, 'CAN' or 'LIN', default 'CAN'\n"                                               \
  "\tLL_DL: int, Link Layer Data Length, default 8\n"                                              \
  "\tdevice: str, 'simulator', ..., default 'simulator'\n"                                         \
  "\tport: int, default 0\n"                                                                       \
  "\tbaudrate: int, default 500000\n"                                                              \
  "\ttxid: int, default 0x731 for 'CAN', 0x3C for 'LIN'\n"                                         \
  "\trxid: int, default 0x732 for 'CAN', 0x3D for 'LIN'\n"                                         \
  "\tblock_size: int, default 8\n"                                                                 \
  "\tSTmin: int, default 0 for CAN, 100 for LIN, unit ms\n"

#define LIN_KWARGS                                                                                 \
  "\tdevice: str, default 'simulator'\n"                                                           \
  "\tport: int, default 0\n"                                                                       \
  "\tbaudrate: int, default 500000\n"                                                              \
  "\tenhanced: bool, default true\n"                                                               \
  "\ttimeout: int, unit ms\n"                                                                      \
  "\tdelayUs: int, unit us\n"

template <typename To, typename Ti> To get(py::kwargs &kwargs, std::string key, To dft) {
  To r = dft;
  try {
    r = Ti(kwargs[key.c_str()]);
  } catch (const std::exception &e) {
  }
  return r;
}
/* ================================ [ TYPES     ] ============================================== */
class can {
public:
  can(std::string device, uint32_t port, uint32_t baudrate) {
    busid = can_open(device.c_str(), port, baudrate);
    if (busid < 0) {
      throw std::runtime_error("failed to create can " + device + " port " + std::to_string(port) +
                               " baudrate " + std::to_string(baudrate));
    }
  }

  ~can() {
    (void)can_close(busid);
  }

  bool is_opened() {
    return (busid >= 0);
  }

  py::object read(uint32_t canid, uint32_t timeoutMs = 0) {
    uint32_t rcanid = canid;
    uint8_t data[64];
    uint8_t dlc = sizeof(data);
    bool r = FALSE;
    r = can_read(busid, &rcanid, &dlc, data);
    while (FALSE == r) {
      if (timeoutMs > 0) {
        timeoutMs--;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      } else {
        break;
      }
      rcanid = canid;
      dlc = sizeof(data);
      r = can_read(busid, &rcanid, &dlc, data);
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

  bool write(uint32_t canid, py::bytes b) {
    std::string str = b;
    bool r = can_write(busid, canid, (uint8_t)str.size(), (uint8_t *)str.data());
    return r;
  }

private:
  int busid;
};

class lin {
public:
  lin(py::kwargs kwargs) {
    auto device = get<std::string, py::str>(kwargs, "device", "simulator");
    uint32_t port = get<uint32_t, py::int_>(kwargs, "port", 0);
    uint32_t baudrate = get<uint32_t, py::int_>(kwargs, "baudrate", 0);
    enhanced = get<bool, py::bool_>(kwargs, "enhanced", true);
    timeout = get<uint32_t, py::int_>(kwargs, "timeout", 100);
    delayUs = get<uint32_t, py::int_>(kwargs, "delayUs", 0);
    busid = lin_open(device.c_str(), port, baudrate);
    if (busid < 0) {
      throw std::runtime_error("failed to create lin " + device + " port " + std::to_string(port) +
                               " baudrate " + std::to_string(baudrate));
    }
    uint32_t _tmo = timeout * 1000;
    dev_ioctl(busid, DEV_IOCTL_SET_TIMEOUT, &_tmo, sizeof(_tmo));
    dev_ioctl(busid, DEV_IOCTL_SET_DELAY, &delayUs, sizeof(delayUs));
  }

  ~lin() {
    (void)lin_close(busid);
  }

  bool is_opened() {
    return (busid >= 0);
  }

  py::object read(lin_id_t id, int dlc) {
    uint8_t data[64];
    bool r = lin_read(busid, id, (uint8_t)dlc, data, enhanced, timeout);
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

  bool write(lin_id_t id, py::bytes b) {
    std::string str = b;
    bool r = lin_write(busid, id, (uint8_t)str.size(), (uint8_t *)str.data(), enhanced);
    return r;
  }

private:
  int busid;
  bool enhanced;
  uint32_t timeout;
  uint32_t delayUs;
};

class isotp {
public:
  isotp(py::kwargs kwargs) {
    std::string protocol = get<std::string, py::str>(kwargs, "protocol", "CAN");
    auto device = get<std::string, py::str>(kwargs, "device", "simulator");
    uint32_t port = get<uint32_t, py::int_>(kwargs, "port", 0);
    uint32_t baudrate = get<uint32_t, py::int_>(kwargs, "baudrate", 500000);
    uint32_t rxid = 0x732, txid = 0x731;
    uint16_t N_TA = get<uint16_t, py::int_>(kwargs, "N_TA", 0xFFFF);
    uint32_t ll_dl;
    uint32_t delayUs = get<uint32_t, py::int_>(kwargs, "delayUs", 0);
    if (protocol == "LIN") {
      rxid = 0x3d;
      txid = 0x3c;
    }
    rxid = get<uint32_t, py::int_>(kwargs, "rxid", rxid);
    txid = get<uint32_t, py::int_>(kwargs, "txid", txid);
    ll_dl = get<uint32_t, py::int_>(kwargs, "LL_DL", 8);
    params.baudrate = (uint32_t)baudrate;
    params.port = port;
    params.ll_dl = ll_dl;
    params.N_TA = N_TA;
    if (protocol == "CAN") {
      strcpy(params.device, device.c_str());
      params.protocol = ISOTP_OVER_CAN;
      params.U.CAN.RxCanId = (uint32_t)rxid;
      params.U.CAN.TxCanId = (uint32_t)txid;
      params.U.CAN.BlockSize = get<uint32_t, py::int_>(kwargs, "block_size", 8);
      params.U.CAN.STmin = get<uint32_t, py::int_>(kwargs, "STmin", 0);
    } else if (protocol == "LIN") {
      strcpy(params.device, device.c_str());
      params.protocol = ISOTP_OVER_LIN;
      params.U.LIN.RxId = (uint32_t)rxid;
      params.U.LIN.TxId = (uint32_t)txid;
      params.U.LIN.timeout = get<uint32_t, py::int_>(kwargs, "STmin", 100);
      params.U.LIN.delayUs = delayUs;
    } else if (protocol == "DoIP") {
      auto ipStr = get<std::string, py::str>(kwargs, "ip", "224.244.224.245");
      strcpy(params.device, ipStr.c_str());
      params.protocol = ISOTP_OVER_DOIP;
      params.port = get<int, py::int_>(kwargs, "port", 13400);
      params.U.DoIP.sourceAddress = get<uint16_t, py::int_>(kwargs, "sa", 0xbeef);
      params.U.DoIP.targetAddress = get<uint16_t, py::int_>(kwargs, "ta", 0xdead);
      params.U.DoIP.activationType = get<uint8_t, py::int_>(kwargs, "at", 0x00);
    } else {
      throw std::runtime_error("invalid protocol " + protocol);
    }

    tp = isotp_create(&params);
    if (nullptr == tp) {
      throw std::runtime_error("failed to create isotp");
    }
  }

  bool transmit(py::bytes b, uint32_t timeoutMs = 5000) {
    std::string str = b;
    int r;
    uint32_t timeoutUs = timeoutMs * 1000;

    (void)isotp_ioctl(tp, ISOTP_IOCTL_SET_TIMEOUT, &timeoutUs, sizeof(uint32_t));

    r = isotp_transmit(tp, (uint8_t *)str.data(), str.size(), nullptr, 0);
    return (0 == r);
  }

  py::object receive(uint32_t timeoutMs = 5000) {
    int r;
    uint32_t timeoutUs = timeoutMs * 1000;

    (void)isotp_ioctl(tp, ISOTP_IOCTL_SET_TIMEOUT, &timeoutUs, sizeof(uint32_t));

    r = isotp_receive(tp, buffer, sizeof(buffer));

    if (r > 0) {
      return py::bytes((const char *)buffer, r);
    }

    return py::none();
  }

  ~isotp() {
    if (tp) {
      isotp_destory(tp);
    }
  }
  isotp_t *get_isotp() {
    return tp;
  }

private:
  isotp_t *tp = nullptr;
  uint8_t buffer[4096];
  isotp_parameter_t params;
};

class dev {
public:
  dev(std::string device, std::string option) {
    fd = dev_open(device.c_str(), option.c_str());
    if (fd < 0) {
      throw std::runtime_error("failed to create dev " + device + " option " + option);
    }
  }

  ~dev() {
    dev_close(fd);
  }

  int write(py::bytes b) {
    std::string str = b;
    return dev_write(fd, (uint8_t *)str.data(), str.size());
  }

  py::object read() {
    int r = dev_read(fd, data, sizeof(data));
    py::object obj;
    if (r > 0) {
      obj = py::bytes((char *)data, r);
    } else {
      obj = py::none();
    }
    return obj;
  }

private:
  int fd;
  uint8_t data[128];
};

class loader {
public:
  loader(py::kwargs kwargs) {
    m_IsoTp = new isotp(kwargs);
    m_App = py::str(kwargs["app"]);
    m_Fls = py::str(kwargs["fls"]);
    m_LogLevel = get<int, py::int_>(kwargs, "logLevel", L_LOG_INFO);
    m_Choice = get<std::string, py::str>(kwargs, "choice", "FBL");
    m_SignType = (srec_sign_type_t)get<int, py::int_>(kwargs, "signType", SREC_SIGN_CRC16);
    m_FuncAddr = (uint32_t)get<int, py::int_>(kwargs, "funcAddr", 0x7DF);

    m_AppSrec = srec_open(m_App.c_str());
    if (nullptr == m_AppSrec) {
      throw std::runtime_error("failed to open app image " + m_App);
    }

    m_FlsSrec = srec_open(m_Fls.c_str());
    if (nullptr == m_FlsSrec) {
      throw std::runtime_error("failed to open flash driver image " + m_Fls);
    }
  }

  ~loader() {
    if (nullptr != m_Loader) {
      loader_destory(m_Loader);
    }

    if (nullptr != m_IsoTp) {
      delete m_IsoTp;
    }

    if (nullptr != m_AppSrec) {
      srec_close(m_AppSrec);
    }

    if (nullptr != m_FlsSrec) {
      srec_close(m_FlsSrec);
    }
  }

  bool start() {
    loader_args_t args;
    args.isotp = m_IsoTp->get_isotp();
    args.appSRec = m_AppSrec;
    args.flsSRec = m_FlsSrec;
    args.choice = m_Choice.c_str();
    args.signType = m_SignType;
    args.funcAddr = m_FuncAddr;
    m_Loader = loader_create(&args);
    if (nullptr == m_Loader) {
      throw std::runtime_error("failed to start loader");
    }
    loader_set_log_level(m_Loader, m_LogLevel);
    return true;
  }

  void stop() {
    if (nullptr != m_Loader) {
      loader_destory(m_Loader);
      m_Loader = nullptr;
    }

    if (nullptr != m_IsoTp) {
      delete m_IsoTp;
      m_IsoTp = nullptr;
    }
  }

  py::object poll() {
    int progress = 0;
    char *log = NULL;
    int r = -1;
    py::list L;
    if (nullptr != m_Loader) {
      r = loader_poll(m_Loader, &progress, &log);
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
  isotp *m_IsoTp = nullptr;
  std::string m_App;
  std::string m_Fls;
  srec_t *m_AppSrec = nullptr;
  srec_t *m_FlsSrec = nullptr;
  loader_t *m_Loader = nullptr;
  std::string m_Choice = "FBL";
  srec_sign_type_t m_SignType = SREC_SIGN_CRC16;
  int m_LogLevel = L_LOG_INFO;
  uint32_t m_FuncAddr = 0;
};

class bitarray {
public:
  bitarray(py::bytes data = py::bytes(), std::string endianS = "BIG") {
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

  void put(uint32_t u32, uint8_t nBits) {
    m_BitArray->put(u32, nBits);
  }

  uint32_t get(uint8_t nBits) {
    return m_BitArray->get(nBits);
  }

  py::bytes tobytes() {
    auto data = m_BitArray->bytes();
    return py::bytes((char *)data.data(), data.size());
  }

  py::bytes left_bytes() {
    auto data = m_BitArray->left_bytes();
    return py::bytes((char *)data.data(), data.size());
  }

  void drop(uint32_t nBits) {
    m_BitArray->drop(nBits);
  }

  ~bitarray() {
  }

private:
  std::shared_ptr<BitArray> m_BitArray;
};

class pylua {
public:
  pylua(std::string scriptFile) {
    m_Lua = std::make_shared<AsLuaScript>(scriptFile);
  }

  py::object table_get(std::string api) {
    py::list L;
    lua_arg_t outArgs[3];

    outArgs[0].type = LUA_ARG_TYPE_UINT8_N;  /* get new bytes data */
    outArgs[1].type = LUA_ARG_TYPE_STRING_N; /* bytes data headers */
    outArgs[2].type = LUA_ARG_TYPE_STRING_N; /* bytes data values */
    auto r = m_Lua->call(api, nullptr, 0, outArgs, 3);
    if (0 == r) {
      L.append(py::bytes((char *)outArgs[0].u8N.data(), outArgs[0].u8N.size()));
      py::list Lh;
      for (auto &h : outArgs[1].stringN) {
        Lh.append(py::str(h));
      }
      L.append(Lh);
      py::list Lv;
      for (auto &v : outArgs[2].stringN) {
        Lv.append(py::str(v));
      }
      L.append(Lv);
    } else {
      throw std::runtime_error("Failed to get table content");
    }
    return L;
  }

  py::object table_decode(std::string api, py::bytes data) {
    py::list L;
    lua_arg_t inArgs[1];
    lua_arg_t outArgs[4];

    inArgs[0].type = LUA_ARG_TYPE_UINT8_N;
    std::string b = data;
    for (size_t i = 0; i < b.size(); i++) {
      inArgs[0].u8N.push_back((uint8_t)b[i]);
    }

    outArgs[0].type = LUA_ARG_TYPE_BOOL;     /* decoding results */
    outArgs[1].type = LUA_ARG_TYPE_STRING_N; /* decoding headers */
    outArgs[2].type = LUA_ARG_TYPE_STRING_N; /* decoding values */
    outArgs[3].type = LUA_ARG_TYPE_UINT32;   /* consumed bits */
    auto r = m_Lua->call(api, inArgs, 1, outArgs, 4);
    if (0 == r) {
      if (outArgs[0].b) {
        py::list Lh;
        for (auto &h : outArgs[1].stringN) {
          Lh.append(py::str(h));
        }
        L.append(Lh);
        py::list Lv;
        for (auto &v : outArgs[2].stringN) {
          Lv.append(py::str(v));
        }
        L.append(Lv);
        L.append(py::int_(outArgs[3].u32));
      } else {
        throw std::runtime_error("Failed to decode table content:" + outArgs[1].stringN[0]);
      }
    } else {
      throw std::runtime_error("Failed to decode table content");
    }
    return L;
  }

  ~pylua() {
  }

private:
  std::shared_ptr<AsLuaScript> m_Lua;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
PYBIND11_MODULE(AsPy, m) {
  Log::setName("AsPy");
  m.doc() = "pybind11 AsPy library";
  py::class_<can>(m, "can")
    .def(py::init<std::string, uint32_t, uint32_t>(), py::arg("device") = "simulator",
         py::arg("port") = 0, py::arg("baudrate") = 500000)
    .def("is_opened", &can::is_opened)
    .def("read", &can::read, py::arg("canid"), py::arg("timeoutMs") = 0)
    .def("write", &can::write, py::arg("canid"), py::arg("data"));
  py::class_<lin>(m, "lin")
    .def(py::init<py::kwargs>(), LIN_KWARGS)
    .def("is_opened", &lin::is_opened)
    .def("read", &lin::read, py::arg("id"), py::arg("dlc") = 8)
    .def("write", &lin::write, py::arg("id"), py::arg("data"));
  py::class_<isotp>(m, "isotp")
    .def(py::init<py::kwargs>(), ISOTP_KWARGS)
    .def("transmit", &isotp::transmit, py::arg("data"), py::arg("timeoutMs") = 5000)
    .def("receive", &isotp::receive, py::arg("timeoutMs") = 5000);
  py::class_<loader>(m, "loader")
    .def(py::init<py::kwargs>(), "\tapp: str, 'the app image'\n"
                                 "\tfls: str, 'the flash driver image'\n"
                                 "\tlogLevel: int, 'the loader log level'\n" ISOTP_KWARGS)
    .def("start", &loader::start)
    .def("stop", &loader::stop)
    .def("poll", &loader::poll);
  py::class_<dev>(m, "dev")
    .def(py::init<std::string, std::string>(), py::arg("device"), py::arg("options"))
    .def("read", &dev::read)
    .def("write", &dev::write, py::arg("data"));
  py::class_<bitarray>(m, "bitarray")
    .def(py::init<py::bytes, std::string>(), py::arg("data") = py::bytes(),
         py::arg("endian") = "BIG")
    .def("put", &bitarray::put, py::arg("u32"), py::arg("nBits"))
    .def("get", &bitarray::get, py::arg("nBits"))
    .def("tobytes", &bitarray::tobytes)
    .def("left", &bitarray::left_bytes)
    .def("drop", &bitarray::drop, py::arg("nBits"));
  py::class_<pylua>(m, "lua")
    .def(py::init<std::string>())
    .def("table_get", &pylua::table_get, py::arg("api"))
    .def("table_decode", &pylua::table_decode, py::arg("api"), py::arg("data"));
}
