/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "topic.hpp"
#include "Std_Topic.h"
#include "isotp.h"
#include "canlib.h"
namespace as {
namespace topic {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static std::map<std::string, std::shared_ptr<MessagePublisher<std::shared_ptr<com::Message>>>>
  s_PubsMap;
static std::mutex s_Lock;
/* ================================ [ LOCALS    ] ============================================== */
static std::shared_ptr<MessagePublisher<std::shared_ptr<com::Message>>>
get_publisher(std::string topic) {
  std::shared_ptr<MessagePublisher<std::shared_ptr<com::Message>>> pub;
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_PubsMap.find(topic);
  if (it != s_PubsMap.end()) {
    pub = it->second;
  } else {
    pub = std::make_shared<MessagePublisher<std::shared_ptr<com::Message>>>("com");
    pub->create(topic);
  }
  return pub;
}

static void put_everything(std::shared_ptr<com::Message> msg) {
  auto pub = get_publisher("everything");
  pub->put(msg);
}
/* ================================ [ FUNCTIONS ] ============================================== */
namespace com {
Publisher::Publisher(std::string name) : m_Pub(name) {
  (void)m_Pub.create(name);
}

Publisher::~Publisher() {
}

void Publisher::push(std::shared_ptr<com::Message> msg) {
  m_Pub.put(msg);
  put_everything(msg);
}

Subscriber::Subscriber(std::string name, uint32_t capability) : m_Sub(name) {
  (void)m_Sub.create(name, capability);
}

Subscriber::~Subscriber() {
}

std::shared_ptr<Message> Subscriber::pop(void) {
  std::shared_ptr<Message> msg = nullptr;
  (void)m_Sub.get(msg, true, 0);
  return msg;
}

std::vector<std::string> topics() {
  return MessageBroker<std::shared_ptr<Message>>::topics();
}

} /* namespace com */

std::shared_ptr<MessageQueue<figure::FigureConfig>> figure::config(void) {
  return MessageQueue<figure::FigureConfig>::add("figure/config");
}

std::shared_ptr<MessageQueue<figure::Point>> figure::line(std::string figure, std::string line) {
  std::string topic = "figure/" + figure + "/" + line;
  return MessageQueue<figure::Point>::add(topic);
}

std::shared_ptr<MessageQueue<figure::Point>> figure::find_line(std::string figure,
                                                               std::string line) {
  std::string topic = "figure/" + figure + "/" + line;
  return MessageQueue<figure::Point>::find(topic);
}

extern "C" void Std_TopicIsoTpPut(uint8_t channleId, int isRx, uint32_t id, uint32_t dlc,
                                  const uint8_t *data) {
  auto pub = get_publisher("isotp");
  auto msg = std::make_shared<com::Message>();

  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->timestamp = std::string(date);
  }

  msg->name = "isotp";
  msg->network = std::to_string(channleId);

  if (isRx) {
    msg->dir = "RX";
  } else {

    msg->dir = "TX";
  }

  {
    std::stringstream ss;
    ss << "0x" << std::hex << (id & (~CAN_ID_EXTENDED));
    msg->id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->data = ss.str();
  }

  pub->put(msg);
  put_everything(msg);
}

extern "C" void Std_TopicUdsPut(void *isotp_, int isRx, uint32_t dlc, const uint8_t *data) {
  isotp_parameter_t *param = (isotp_parameter_t *)isotp_;
  auto pub = get_publisher("uds");
  auto msg = std::make_shared<com::Message>();
  uint32_t id = (uint32_t)-1;
  switch (param->protocol) {
  case ISOTP_OVER_CAN:
    if (isRx) {
      id = param->U.CAN.RxCanId & (~CAN_ID_EXTENDED);
    } else {
      id = param->U.CAN.TxCanId & (~CAN_ID_EXTENDED);
    }
    break;
  case ISOTP_OVER_LIN:
    if (isRx) {
      id = param->U.LIN.RxId;
    } else {
      id = param->U.LIN.TxId;
    }
    break;
  default:
    break;
  }

  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->timestamp = std::string(date);
  }

  if (isRx) {
    msg->dir = "RX";
  } else {

    msg->dir = "TX";
  }

  msg->name = "uds";
  msg->network = param->device + std::string(":") + std::to_string(param->port);

  if ((uint32_t)-1 != id) {
    std::stringstream ss;
    ss << "0x" << std::hex << id;
    msg->id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->data = ss.str();
  }
  pub->put(msg);
  put_everything(msg);
}

extern "C" void Std_TopicCanPut(int busid, int isRx, uint32_t canid, uint32_t dlc,
                                const uint8_t *data) {
  auto pub = get_publisher("CAN-BUS" + std::to_string(busid));
  auto msg = std::make_shared<com::Message>();
  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->timestamp = std::string(date);
  }

  if (isRx) {
    msg->dir = "RX";
  } else {

    msg->dir = "TX";
  }

  msg->name = "";
  msg->network = std::string("CAN-BUS") + std::to_string(busid);

  {
    std::stringstream ss;
    ss << "0x" << std::hex << (canid & (~CAN_ID_EXTENDED));
    msg->id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->data = ss.str();
  }
  pub->put(msg);
  put_everything(msg);
}

extern "C" void Std_TopicLinPut(int busid, int isRx, uint32_t id, uint32_t dlc,
                                const uint8_t *data) {
  auto pub = get_publisher("LIN-BUS" + std::to_string(busid));
  auto msg = std::make_shared<com::Message>();
  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->timestamp = std::string(date);
  }

  if (isRx) {
    msg->dir = "RX";
  } else {

    msg->dir = "TX";
  }

  msg->name = "";
  msg->network = std::string("LIN-BUS") + std::to_string(busid);

  {
    std::stringstream ss;
    ss << "0x" << std::hex << id;
    msg->id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->data = ss.str();
  }
  pub->put(msg);
  put_everything(msg);
}
} /* namespace topic */
} /* namespace as */
