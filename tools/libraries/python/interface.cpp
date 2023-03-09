/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <pybind11/pybind11.h>
#include <string>

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "canlib.h"
#include "linlib.h"
#include "isotp.h"
#include "devlib.h"
#include "loader.h"
#include "srec.h"
#include "Log.hpp"

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

  py::object read(int canid) {
    uint32_t rcanid = (uint32_t)canid;
    uint8_t data[64];
    uint8_t dlc = sizeof(data);
    bool r = can_read(busid, &rcanid, &dlc, data);
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

  bool write(int canid, py::bytes b) {
    std::string str = b;
    bool r = can_write(busid, (uint32_t)canid, (uint8_t)str.size(), (uint8_t *)str.data());
    return r;
  }

private:
  int busid;
};

class lin {
public:
  lin(std::string device, uint32_t port, uint32_t baudrate, bool enhanced, int timeout)
    : enhanced(enhanced), timeout(timeout) {
    busid = lin_open(device.c_str(), port, baudrate);
    if (busid < 0) {
      throw std::runtime_error("failed to create lin " + device + " port " + std::to_string(port) +
                               " baudrate " + std::to_string(baudrate));
    }
    uint32_t _tmo = timeout * 1000;
    dev_ioctl(busid, 0, &_tmo, sizeof(_tmo));
  }

  ~lin() {
    (void)lin_close(busid);
  }

  bool is_opened() {
    return (busid >= 0);
  }

  py::object read(int id, int dlc) {
    uint8_t data[64];
    bool r = lin_read(busid, (uint8_t)id, (uint8_t)dlc, data, enhanced, timeout);
    py::object obj;
    if (true == r) {
      obj = py::bytes((char *)data, dlc);
    } else {
      obj = py::none();
    }
    return obj;
  }

  bool write(int id, py::bytes b) {
    std::string str = b;
    bool r = lin_write(busid, (uint8_t)id, (uint8_t)str.size(), (uint8_t *)str.data(), enhanced);
    return r;
  }

private:
  int busid;
  bool enhanced;
  int timeout;
};

class isotp {
public:
  isotp(py::kwargs kwargs) {
    std::string protocol = get<std::string, py::str>(kwargs, "protocol", "CAN");
    device = get<std::string, py::str>(kwargs, "device", "simulator");
    uint32_t port = get<uint32_t, py::int_>(kwargs, "port", 0);
    uint32_t baudrate = get<uint32_t, py::int_>(kwargs, "baudrate", 500000);
    uint32_t rxid = 0x732, txid = 0x731;
    if (protocol == "LIN") {
      rxid = 0x3d;
      txid = 0x3c;
    }
    rxid = get<uint32_t, py::int_>(kwargs, "rxid", rxid);
    txid = get<uint32_t, py::int_>(kwargs, "txid", txid);
    uint32_t ll_dl = get<uint32_t, py::int_>(kwargs, "LL_DL", 8);
    params.baudrate = (uint32_t)baudrate;
    params.port = port;
    params.ll_dl = ll_dl;
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
      params.U.LIN.RxId = (uint8_t)rxid;
      params.U.LIN.TxId = (uint8_t)txid;
      params.U.LIN.timeout = get<uint32_t, py::int_>(kwargs, "STmin", 100);
    } else {
      throw std::runtime_error("invalid protocol " + protocol);
    }

    tp = isotp_create(&params);
    if (nullptr == tp) {
      throw std::runtime_error("failed to create isotp");
    }
  }

  bool transmit(py::bytes b) {
    std::string str = b;
    int r = isotp_transmit(tp, (uint8_t *)str.data(), (uint8_t)str.size(), nullptr, 0);
    return (0 == r);
  }

  py::object receive() {
    int r = isotp_receive(tp, buffer, sizeof(buffer));

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
  std::string device;
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
  int m_LogLevel = L_LOG_INFO;
  uint32_t m_FuncAddr = 0;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
static void __attribute__((constructor)) AsPyInit(void) {
  Log::setLogFile(".AsPy.log");
}
/* ================================ [ FUNCTIONS ] ============================================== */
PYBIND11_MODULE(AsPy, m) {
  m.doc() = "pybind11 AsPy library";
  py::class_<can>(m, "can")
    .def(py::init<std::string, uint32_t, uint32_t>(), py::arg("device") = "simulator",
         py::arg("port") = 0, py::arg("baudrate") = 1000000)
    .def("is_opened", &can::is_opened)
    .def("read", &can::read, py::arg("canid"))
    .def("write", &can::write, py::arg("canid"), py::arg("data"));
  py::class_<lin>(m, "lin")
    .def(py::init<std::string, uint32_t, uint32_t, bool, int>(), py::arg("device") = "simulator",
         py::arg("port") = 0, py::arg("baudrate") = 500000, py::arg("enhanced") = true,
         py::arg("timeout") = 100)
    .def("is_opened", &lin::is_opened)
    .def("read", &lin::read, py::arg("id"), py::arg("dlc") = 8)
    .def("write", &lin::write, py::arg("id"), py::arg("data"));
  py::class_<isotp>(m, "isotp")
    .def(py::init<py::kwargs>(), ISOTP_KWARGS)
    .def("transmit", &isotp::transmit, py::arg("data"))
    .def("receive", &isotp::receive);
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
}
