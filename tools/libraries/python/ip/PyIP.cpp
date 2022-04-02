/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 *
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <map>
#include <iostream>
#include <thread>
#include <pthread.h>

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <unistd.h>

#define WEAK_ALIAS_PRINTF
#include "Std_Debug.h"
#include "TcpIp.h"

namespace py = pybind11;
/* ================================ [ MACROS    ] ============================================== */
#define LOG_FILE_MAX_SIZE (10 * 1024 * 1024)
#define LOG_FILE_MAX_NUMBER 2
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
FILE *_stddebug = NULL;
int lLogIndex = 0;

static const int PYIP_TCP = (int)TCPIP_IPPROTO_TCP;
static const int PYIP_UDP = (int)TCPIP_IPPROTO_UDP;

static pthread_once_t lInitOnce = PTHREAD_ONCE_INIT;

static py::module_ *lPyMod = nullptr;
/* ================================ [ LOCALS    ] ============================================== */
static void AsPyExit(void) {
  if (_stddebug) {
    fclose(_stddebug);
  }
}

static void __attribute__((constructor)) AsPyInit(void) {
  _stddebug = fopen(".ip0.log", "w");
  atexit(AsPyExit);
}

static void init_tcpip_once(void) {
  TcpIp_Init(NULL);
}
/* ================================ [ FUNCTIONS ] ============================================== */
extern "C" int std_printf(const char *fmt, ...) {
  va_list args;
  int length;
  char logP[128];

  if (_stddebug) {
    va_start(args, fmt);
    length = vfprintf(_stddebug, fmt, args);
    fflush(_stddebug);
    // vprintf(fmt, args);
    va_end(args);

    if (ftell(_stddebug) > LOG_FILE_MAX_SIZE) {
      lLogIndex++;
      if (lLogIndex >= LOG_FILE_MAX_NUMBER) {
        lLogIndex = 0;
      }
      snprintf(logP, sizeof(logP), ".ip%d.log", lLogIndex);
      fclose(_stddebug);
      _stddebug = fopen(logP, "w");
    }
  }

  return length;
}

class PySocket {
public:
  PySocket(int protocol = PYIP_TCP) {
    pthread_once(&lInitOnce, init_tcpip_once);
    m_SockId = TcpIp_Create((TcpIp_ProtocolType)protocol);
    if (m_SockId < 0) {
      throw std::runtime_error("failed to create socket");
    }
  }

  bool bind(std::string addr, int port) {
    Std_ReturnType ret;
    const char *LocalAddr = addr.c_str();
    if (addr == "") {
      LocalAddr = NULL;
    }
    ret = TcpIp_Bind(m_SockId, LocalAddr, (uint16_t)port);
    if (E_OK != ret) {
      throw std::runtime_error("failed to bind " + addr + ":" + std::to_string(port));
    }
    return (ret == E_OK);
  }

  bool listen(int backlog) {
    Std_ReturnType ret;
    ret = TcpIp_TcpListen(m_SockId, (uint16_t)backlog);
    if (E_OK != ret) {
      throw std::runtime_error("failed to listen " + std::to_string(backlog));
    }
    return (ret == E_OK);
  }

  bool connect(std::string addr, int port) {
    Std_ReturnType ret;
    TcpIp_SockAddrType RemoteAddr;
    const char *ip = addr.c_str();

    if (addr == "") {
      ip = NULL;
    }
    TcpIp_SetupAddrFrom(&RemoteAddr, ip, (uint16_t)port);
    ret = TcpIp_TcpConnect(m_SockId, &RemoteAddr);
    if (E_OK != ret) {
      throw std::runtime_error("failed to connect to " + addr + ":" + std::to_string(port));
    }
    return (ret == E_OK);
  }

  py::object accept() {
    Std_ReturnType ret = E_NOT_OK;
    TcpIp_SockAddrType RemoteAddr;
    TcpIp_SocketIdType acceptSockId;
    while (E_OK != ret) {
      ret = TcpIp_TcpAccept(m_SockId, &acceptSockId, &RemoteAddr);
      usleep(1000);
    }

    py::list L;
    L.append(py::int_(acceptSockId));
    std::string ip = std::to_string(RemoteAddr.addr[0]) + "." + std::to_string(RemoteAddr.addr[1]) +
                     "." + std::to_string(RemoteAddr.addr[2]) + "." +
                     std::to_string(RemoteAddr.addr[3]);
    L.append(py::str(ip));
    L.append(py::int_(RemoteAddr.port));
    return L;
  }

  bool send(py::bytes b, int sock = -1) {
    Std_ReturnType ret;
    TcpIp_SocketIdType SockId = m_SockId;
    std::string str = b;

    if (sock != -1) {
      SockId = sock;
    }

    ret = TcpIp_Send(SockId, (uint8_t *)str.data(), (uint16_t)str.size());
    if (E_OK != ret) {
      throw std::runtime_error("failed to send");
    }
    return (ret == E_OK);
  }

  bool sendto(std::string addr, int port, py::bytes b, int sock = -1) {
    Std_ReturnType ret;
    TcpIp_SocketIdType SockId = m_SockId;
    std::string str = b;
    TcpIp_SockAddrType RemoteAddr;
    const char *ip = addr.c_str();

    if (addr == "") {
      ip = NULL;
    }
    TcpIp_SetupAddrFrom(&RemoteAddr, ip, (uint16_t)port);
    if (sock != -1) {
      SockId = sock;
    }

    ret = TcpIp_SendTo(SockId, &RemoteAddr, (uint8_t *)str.data(), (uint16_t)str.size());
    if (E_OK != ret) {
      throw std::runtime_error("failed to send");
    }
    return (ret == E_OK);
  }

  py::object recv(int sock = -1) {
    Std_ReturnType ret = E_OK;
    TcpIp_SocketIdType SockId = m_SockId;
    uint8_t *data;
    uint16_t Length = 0;
    uint16_t size = 4096;

    if (sock != -1) {
      SockId = sock;
    }

    data = new uint8_t[size];
    while ((E_OK == ret) && (0 == Length)) {
      ret = TcpIp_IsTcpStatusOK(SockId);
      if (E_OK != ret) {
        throw std::runtime_error("connection broken, closed");
      }
      Length = size;
      ret = TcpIp_Recv(SockId, data, &Length);
    }

    if (E_OK != ret) {
      throw std::runtime_error("failed to recv");
    }

    auto rst = py::bytes((char *)data, Length);
    delete[] data;
    return rst;
  }

  py::object recvfrom(int sock = -1) {
    Std_ReturnType ret = E_OK;
    TcpIp_SocketIdType SockId = m_SockId;
    TcpIp_SockAddrType RemoteAddr;
    uint8_t *data;
    uint16_t Length = 0;
    uint16_t size = 4096;

    if (sock != -1) {
      SockId = sock;
    }

    data = new uint8_t[size];
    while ((E_OK == ret) && (0 == Length)) {
      ret = TcpIp_IsTcpStatusOK(SockId);
      if (E_OK != ret) {
        throw std::runtime_error("connection broken, closed");
      }
      Length = size;
      ret = TcpIp_RecvFrom(SockId, &RemoteAddr, data, &Length);
    }

    if (E_OK != ret) {
      throw std::runtime_error("failed to recvfrom");
    }
    py::list L;
    std::string ip = std::to_string(RemoteAddr.addr[0]) + "." + std::to_string(RemoteAddr.addr[1]) +
                     "." + std::to_string(RemoteAddr.addr[2]) + "." +
                     std::to_string(RemoteAddr.addr[3]);
    L.append(py::str(ip));
    L.append(py::int_(RemoteAddr.port));
    L.append(py::bytes((char *)data, Length));
    delete[] data;
    return L;
  }

  ~PySocket() {
    TcpIp_Close(m_SockId, TRUE);
  }

private:
  TcpIp_SocketIdType m_SockId = -1;
};

PYBIND11_MODULE(PyIP, m) {
  m.doc() = "pybind11 PyIP library";

  lPyMod = &m;

  py::class_<PySocket>(m, "socket")
    .def_readonly_static("TCP", &PYIP_TCP)
    .def_readonly_static("UDP", &PYIP_UDP)
    .def(py::init<int>())
    .def("bind", &PySocket::bind, py::arg("addr") = "", py::arg("port"))
    .def("listen", &PySocket::listen, py::arg("backlog"))
    .def("connect", &PySocket::connect, py::arg("addr") = "", py::arg("port"))
    .def("accept", &PySocket::accept)
    .def("send", &PySocket::send, py::arg("data"), py::arg("sock") = -1)
    .def("sendto", &PySocket::sendto, py::arg("addr") = "", py::arg("port"), py::arg("data"),
         py::arg("sock") = -1)
    .def("recv", &PySocket::recv, py::arg("sock") = -1)
    .def("recvfrom", &PySocket::recvfrom, py::arg("sock") = -1);
}