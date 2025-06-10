/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023-2024 Parai Wang <parai@foxmail.com>
 */
#ifndef __UI_COMMON_HPP__
#define __UI_COMMON_HPP__
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
    std::string name;
    DataType dataType;
    DisplayType displayType;
    uint32_t bitSize;
    uint32_t bitMask;
    std::vector<uint32_t> data; /* use to hold current value of data */
    uint32_t size = 1;
    std::map<std::string, uint32_t> selects; /* selections for UINT_SELECT */
    bool readonly = false;
    bool optional = false; /* optional true can only be the last one for Rx */
    bool visible = true;
    bool newline = false;
    UIData *ui = nullptr;
    UIDataTable *uiTable = nullptr;
    std::vector<std::shared_ptr<Data>> elements; /* if dataType is TABLE */

    std::shared_ptr<AsLuaScript> luaScript;
    std::string lua; /* lua script API to create the display content */

  public:
    std::string str();
    std::string raw();
    void put(BitArray &TxBits); /* put data to TxBits */
    void get(BitArray &RxBits); /* get data from RxBits */
    std::vector<uint32_t> eval(std::string s);

  private:
    std::string decode();
    std::string encode(std::string s);
  };

public:
  UIData(std::shared_ptr<Data> data);
  ~UIData();

public:
  void put(std::string s);
  std::string get(void);

private:
  std::shared_ptr<Data> m_Data;

  QLineEdit *m_leData = nullptr;
  QComboBox *m_cmbxData = nullptr;
  MultiSelectComboBox *m_mscbData = nullptr;
};

class UIDataTable : public QTableWidget {
  Q_OBJECT
public:
  UIDataTable(std::shared_ptr<UIData::Data> data);
  ~UIDataTable();

public:
  void put(BitArray &ba);
  void get(void);

private:
  void decode(BitArray &ba);

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

  QWidget *service(void) {
    return m_Widget;
  }

  UIGroup *group(void) {
    return m_Group;
  }

  void setTabIndex(int tabIndex) {
    m_TabIndex = tabIndex;
  }

  int tabIndex(void) {
    return m_TabIndex;
  }

private:
  UIGroup *m_Group = nullptr;
  QWidget *m_Widget = nullptr;
  int m_TabIndex = -1;
};

class UIServiceBase : public QGroupBox {
public:
  UIServiceBase(std::string name, std::shared_ptr<AsLuaScript> luaScript);
  ~UIServiceBase();

  std::shared_ptr<UIData::Data> parseData(json &js);

  QVBoxLayout *createUI(json &js);

protected:
  std::string m_Name;
  uint8_t m_SID;
  std::vector<std::shared_ptr<UIData::Data>> m_TxDatas;
  std::vector<std::shared_ptr<UIData::Data>> m_RxDatas;

  size_t m_RxBitSize = 0;
  size_t m_TxBitSize = 0;

  QPushButton *m_btnEnter;

  std::shared_ptr<AsLuaScript> m_Lua;
  std::regex m_reType;
};
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
static inline int toInteger(std::string s) {
  int r;
  if ((0 == strncmp(s.c_str(), "0x", 2)) || (0 == strncmp(s.c_str(), "0X", 2))) {
    r = strtoul(s.c_str(), NULL, 16);
  } else {
    r = atoi(s.c_str());
  }
  return r;
}

template <typename T> T get(json &obj, std::string key, T dft) {
  T r = dft;
  if (obj.contains(key)) {
    r = obj[key].get<T>();
  }
  return r;
};

template <typename T> T get2(json &obj, std::string key, T dft) {
  T r = dft;
  if (obj.contains(key)) {
    auto s = obj[key].dump();
    if (s.c_str()[0] == '"') {
      s = s.substr(1, s.size() - 2);
    }
    r = (T)toInteger(s);
  }
  return r;
};

template <typename T> std::string to_ascii(std::vector<T> data) {
  std::stringstream ss;

  for (size_t i = 0; i < data.size(); i++) {
    ss << (char)data[i];
  }

  return ss.str();
};

template <typename T> std::string to_hex(std::vector<T> data) {
  std::stringstream ss;

  for (size_t i = 0; i < data.size(); i++) {
    ss << "0x" << std::setw(2) << std::setfill('0') << std::hex << (T)data[i];
    if ((i + 1) < data.size()) {
      ss << ", ";
    }
  }

  return ss.str();
};

template <typename T> std::string to_dec(std::vector<T> data) {
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
#endif /* __UI_COMMON_HPP__ */
