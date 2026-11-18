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
Topic_GetPublisher(std::string topic) {
  std::shared_ptr<MessagePublisher<std::shared_ptr<com::Message>>> pub;
  std::unique_lock<std::mutex> lck(s_Lock);
  auto it = s_PubsMap.find(topic);
  if (it != s_PubsMap.end()) {
    pub = it->second;
  } else {
    pub = std::make_shared<MessagePublisher<std::shared_ptr<com::Message>>>("com");
    pub->Create(topic);
  }
  return pub;
}

static void Topic_PutEverything(std::shared_ptr<com::Message> msg) {
  auto pub = Topic_GetPublisher("everything");
  pub->Put(msg);
}
/* ================================ [ FUNCTIONS ] ============================================== */
namespace com {
Publisher::Publisher(std::string name) : m_Pub(name) {
  (void)m_Pub.Create(name);
}

Publisher::~Publisher() {
}

void Publisher::Push(std::shared_ptr<com::Message> msg) {
  m_Pub.Put(msg);
  Topic_PutEverything(msg);
}

Subscriber::Subscriber(std::string name, uint32_t capability) : m_Sub(name) {
  (void)m_Sub.Create(name, capability);
}

Subscriber::~Subscriber() {
}

std::shared_ptr<Message> Subscriber::Pop(void) {
  std::shared_ptr<Message> msg = nullptr;
  (void)m_Sub.Get(msg, true, 0);
  return msg;
}

std::vector<std::string> Topics() {
  return MessageBroker<std::shared_ptr<Message>>::Topics();
}

} /* namespace com */

std::shared_ptr<MessageQueue<figure::FigureConfig>> figure::Config(void) {
  return MessageQueue<figure::FigureConfig>::Add("figure/config");
}

std::shared_ptr<MessageQueue<figure::Point>> figure::Line(std::string figure, std::string line) {
  std::string topic = "figure/" + figure + "/" + line;
  return MessageQueue<figure::Point>::Add(topic);
}

std::shared_ptr<MessageQueue<figure::Point>> figure::FindLine(std::string figure,
                                                              std::string line) {
  std::string topic = "figure/" + figure + "/" + line;
  return MessageQueue<figure::Point>::Find(topic);
}

extern "C" void Std_TopicIsoTpPut(uint8_t channleId, int isRx, uint32_t id, uint32_t dlc,
                                  const uint8_t *data) {
  auto pub = Topic_GetPublisher("isotp");
  auto msg = std::make_shared<com::Message>();

  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->Timestamp = std::string(date);
  }

  msg->Name = "isotp";
  msg->Network = std::to_string(channleId);

  if (isRx) {
    msg->Dir = "RX";
  } else {

    msg->Dir = "TX";
  }

  {
    std::stringstream ss;
    ss << "0x" << std::hex << (id & (~CANL_ID_EXTENDED));
    msg->Id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->Dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->Data = ss.str();
  }

  pub->Put(msg);
  Topic_PutEverything(msg);
}

extern "C" void Std_TopicUdsPut(void *isotp_, int isRx, uint32_t dlc, const uint8_t *data) {
  IsoTp_ParameterType *param = (IsoTp_ParameterType *)isotp_;
  auto pub = Topic_GetPublisher("uds");
  auto msg = std::make_shared<com::Message>();
  uint32_t id = (uint32_t)-1;
  switch (param->Protocol) {
  case ISOTP_OVER_CAN:
    if (isRx) {
      id = param->U.CAN.RxCanId & (~CANL_ID_EXTENDED);
    } else {
      id = param->U.CAN.TxCanId & (~CANL_ID_EXTENDED);
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
    msg->Timestamp = std::string(date);
  }

  if (isRx) {
    msg->Dir = "RX";
  } else {

    msg->Dir = "TX";
  }

  msg->Name = "uds";
  msg->Network = param->Device + std::string(":") + std::to_string(param->Port);

  if ((uint32_t)-1 != id) {
    std::stringstream ss;
    ss << "0x" << std::hex << id;
    msg->Id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->Dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->Data = ss.str();
  }
  pub->Put(msg);
  Topic_PutEverything(msg);
}

extern "C" void Std_TopicCanPut(int busid, int isRx, uint32_t canid, uint32_t dlc,
                                const uint8_t *data) {
  auto pub = Topic_GetPublisher("CAN-BUS" + std::to_string(busid));
  auto msg = std::make_shared<com::Message>();
  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->Timestamp = std::string(date);
  }

  if (isRx) {
    msg->Dir = "RX";
  } else {

    msg->Dir = "TX";
  }

  msg->Name = "";
  msg->Network = std::string("CAN-BUS") + std::to_string(busid);

  {
    std::stringstream ss;
    ss << "0x" << std::hex << (canid & (~CANL_ID_EXTENDED));
    msg->Id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->Dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->Data = ss.str();
  }
  pub->Put(msg);
  Topic_PutEverything(msg);
}

extern "C" void Std_TopicLinPut(int busid, int isRx, uint32_t id, uint32_t dlc,
                                const uint8_t *data) {
  auto pub = Topic_GetPublisher("LIN-BUS" + std::to_string(busid));
  auto msg = std::make_shared<com::Message>();
  {
    char date[64];
    Std_GetDateTime(date, sizeof(date));
    msg->Timestamp = std::string(date);
  }

  if (isRx) {
    msg->Dir = "RX";
  } else {

    msg->Dir = "TX";
  }

  msg->Name = "";
  msg->Network = std::string("LIN-BUS") + std::to_string(busid);

  {
    std::stringstream ss;
    ss << "0x" << std::hex << id;
    msg->Id = ss.str();
  }

  {
    std::stringstream ss;
    ss << dlc;
    msg->Dlc = ss.str();
  }

  {
    std::stringstream ss;
    for (uint32_t i = 0; i < dlc; i++) {
      ss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i];
      if ((i + 1) < dlc) {
        ss << " ";
      }
    }
    msg->Data = ss.str();
  }
  pub->Put(msg);
  Topic_PutEverything(msg);
}
} /* namespace topic */
} /* namespace as */
