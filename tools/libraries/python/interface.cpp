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
#include "isotp.h"
#define WEAK_ALIAS_PRINTF
#include "Std_Debug.h"

namespace py = pybind11;
/* ================================ [ MACROS    ] ============================================== */
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
    uint8_t dlc;
    uint8_t data[64];
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
      params.device = device.c_str();
      params.protocol = ISOTP_OVER_CAN;
      params.U.CAN.RxCanId = (uint32_t)rxid;
      params.U.CAN.TxCanId = (uint32_t)txid;
      params.U.CAN.BlockSize = get<uint32_t, py::int_>(kwargs, "block_size", 8);
      params.U.CAN.STmin = get<uint32_t, py::int_>(kwargs, "STmin", 0);
    } else if (protocol == "LIN") {
      params.device = device.c_str();
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

private:
  template <typename To, typename Ti> To get(py::kwargs &kwargs, std::string key, To dft) {
    To r = dft;
    try {
      r = Ti(kwargs[key.c_str()]);
    } catch (const std::exception &e) {
    }
    return r;
  }

private:
  isotp_t *tp = nullptr;
  uint8_t buffer[4096];
  isotp_parameter_t params;
  std::string device;
};

/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
FILE *_stddebug = NULL;
/* ================================ [ LOCALS    ] ============================================== */
static void AsPyExit(void) {
  if (_stddebug) {
    fclose(_stddebug);
  }
}

static void __attribute__((constructor)) AsPyInit(void) {
  _stddebug = fopen(".AsPy.log", "w");
  atexit(AsPyExit);
}
/* ================================ [ FUNCTIONS ] ============================================== */
extern "C" int std_printf(const char *fmt, ...) {
  va_list args;
  int length;

  va_start(args, fmt);
  if (_stddebug) {
    length = vfprintf(_stddebug, fmt, args);
  } else {
    length = vprintf(fmt, args);
  }
  va_end(args);

  return length;
}

PYBIND11_MODULE(AsPy, m) {
  m.doc() = "pybind11 AsPy library";
  py::class_<can>(m, "can")
    .def(py::init<std::string, uint32_t, uint32_t>(), py::arg("device") = "simulator",
         py::arg("port") = 0, py::arg("baudrate") = 1000000)
    .def("is_opened", &can::is_opened)
    .def("read", &can::read, py::arg("canid"))
    .def("write", &can::write, py::arg("canid"), py::arg("data"));
  py::class_<isotp>(m, "isotp")
    .def(py::init<py::kwargs>(), "\tprotocol: str, 'CAN' or 'LIN', default 'CAN'\n"
                                 "\tLL_DL: int, Link Layer Data Length, default 8\n"
                                 "\tdevice: str, 'simulator', ..., default 'simulator'\n"
                                 "\tport: int, default 0\n"
                                 "\tbaudrate: int, default 500000\n"
                                 "\ttxid: int, default 0x731 for 'CAN', 0x3C for 'LIN'\n"
                                 "\trxid: int, default 0x732 for 'CAN', 0x3D for 'LIN'\n"
                                 "\tblock_size: int, default 8\n"
                                 "\tSTmin: int, default 0 for CAN, 100 for LIN, unit ms\n")
    .def("transmit", &isotp::transmit, py::arg("data"))
    .def("receive", &isotp::receive);
}
