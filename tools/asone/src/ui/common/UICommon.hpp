/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023-2024 Parai Wang <parai@foxmail.com>
 */
#ifndef UICOMMON_HPP
#define UICOMMON_HPP
/* ================================ [ INCLUDES  ] ============================================== */
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QTimerEvent>
#include <QScrollArea>
#include <QSplitter>
#include "MultiSelectComboBox.hpp"

#include <vector>
#include <memory>
#include <map>
#include <regex>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "aslua.hpp"
#include "Log.hpp"
#include "bitarray.hpp"

using json = nlohmann::ordered_json;
using namespace as;

class UIGroup;

namespace as {
namespace ui {
namespace common {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
class UIDataTable;
class UIData : public QWidget {
  Q_OBJECT
public:
  enum DataType {
    UINT,              /* an integer with a bit size < 32 */
    UINT_SELECT,       /* an integer with selections */
    UINT_MULTI_SELECT, /* an integer with multiple selections */
    UINT_N,
    TABLE,
  };

  enum DisplayType {
    DEC,
    HEX,
    ASC
  };

  struct Data {
    std::string Name;
    DataType DataType;
    DisplayType DisplayType;
    uint32_t BitSize;
    uint32_t BitMask;
    std::vector<uint32_t> Datas; /* use to hold current value of data */
    uint32_t Size = 1;
    std::map<std::string, uint32_t> Selects; /* selections for UINT_SELECT */
    bool Readonly = false;
    bool Optional = false; /* optional true can only be the last one for Rx */
    bool Visible = true;
    bool Newline = false;
    UIData *Ui = nullptr;
    UIDataTable *UiTable = nullptr;
    std::vector<std::shared_ptr<Data>> Elements; /* if dataType is TABLE */

    std::shared_ptr<AsLuaScript> LuaScript;
    std::string Lua; /* lua script API to create the display content */

  public:
    std::string Str();
    std::string Raw();
    void Put(BitArray &TxBits); /* put data to TxBits */
    void Get(BitArray &RxBits); /* get data from RxBits */
    std::vector<uint32_t> Eval(std::string s);

  private:
    std::string Decode();
    std::string Encode(std::string s);
  };

public:
  UIData(std::shared_ptr<Data> data);
  ~UIData();

public:
  void Put(std::string s);
  std::string Get(void);

private:
  std::shared_ptr<Data> m_Data;

  QLineEdit *m_LeData = nullptr;
  QComboBox *m_CmbxData = nullptr;
  MultiSelectComboBox *m_MscbData = nullptr;
};

class UIDataTable : public QTableWidget {
  Q_OBJECT
public:
  UIDataTable(std::shared_ptr<UIData::Data> data);
  ~UIDataTable();

public:
  void Put(BitArray &ba);
  void Get(void);

private:
  void Decode(BitArray &ba);

private:
  std::shared_ptr<UIData::Data> m_Data;
};

template <typename T> class UITreeItem : public QTreeWidgetItem {
public:
  UITreeItem(std::string name) : QTreeWidgetItem({QString(name.c_str())}) {
  }

  UITreeItem(std::string name, UIGroup *group, QWidget *widget)
    : QTreeWidgetItem({QString(name.c_str())}), m_Group(group), m_Widget(widget) {
  }

  ~UITreeItem() {
  }

  QWidget *Service(void) {
    return m_Widget;
  }

  UIGroup *Group(void) {
    return m_Group;
  }

  void SetTabIndex(int tabIndex) {
    m_TabIndex = tabIndex;
  }

  int TabIndex(void) {
    return m_TabIndex;
  }

private:
  UIGroup *m_Group = nullptr;
  QWidget *m_Widget = nullptr;
  int m_TabIndex = -1;
};

class UIServiceBase : public QGroupBox {
public:
  UIServiceBase(std::string name, std::shared_ptr<AsLuaScript> LuaScript);
  ~UIServiceBase();

  std::shared_ptr<UIData::Data> ParseData(json &js);

  QVBoxLayout *CreateUi(json &js);

protected:
  std::string m_Name;
  uint8_t m_SID;
  std::vector<std::shared_ptr<UIData::Data>> m_TxDatas;
  std::vector<std::shared_ptr<UIData::Data>> m_RxDatas;

  size_t m_RxBitSize = 0;
  size_t m_TxBitSize = 0;

  QPushButton *m_BtnEnter;

  std::shared_ptr<AsLuaScript> m_Lua;
  std::regex m_ReType;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
static inline int ToInteger(std::string s) {
  int r;
  if ((0 == strncmp(s.c_str(), "0x", 2)) || (0 == strncmp(s.c_str(), "0X", 2))) {
    r = strtoul(s.c_str(), NULL, 16);
  } else {
    r = atoi(s.c_str());
  }
  return r;
}

template <typename T> T JsonGet(json &obj, std::string key, T dft) {
  T r = dft;
  if (obj.contains(key)) {
    r = obj[key].get<T>();
  }
  return r;
};

template <typename T> T JsonGetInt(json &obj, std::string key, T dft) {
  T r = dft;
  if (obj.contains(key)) {
    auto s = obj[key].dump();
    if (s.c_str()[0] == '"') {
      s = s.substr(1, s.size() - 2);
    }
    r = (T)ToInteger(s);
  }
  return r;
};

template <typename T> std::string ToAscii(std::vector<T> data) {
  std::stringstream ss;

  for (size_t i = 0; i < data.size(); i++) {
    ss << (char)data[i];
  }

  return ss.str();
};

template <typename T> std::string ToHex(std::vector<T> data) {
  std::stringstream ss;

  for (size_t i = 0; i < data.size(); i++) {
    ss << "0x" << std::setw(2) << std::setfill('0') << std::hex << (T)data[i];
    if ((i + 1) < data.size()) {
      ss << ", ";
    }
  }

  return ss.str();
};

template <typename T> std::string ToDec(std::vector<T> data) {
  std::stringstream ss;

  for (size_t i = 0; i < data.size(); i++) {
    ss << std::dec << (T)data[i];
    if ((i + 1) < data.size()) {
      ss << ", ";
    }
  }

  return ss.str();
};
} /* namespace common */
} /* namespace ui */
} /* namespace as */
#endif /* UICOMMON_HPP */
