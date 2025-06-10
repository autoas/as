/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2023-2024 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "UICommon.hpp"

namespace as {
namespace ui {
namespace common {
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
std::string UIData::Data::decode() {
  std::string s;
  lua_arg_t inArgs[2];
  lua_arg_t outArgs[4];

  inArgs[0].type = LUA_ARG_TYPE_UINT32_N;
  inArgs[0].u32N = data;

  inArgs[1].type = LUA_ARG_TYPE_UINT32;
  inArgs[1].u32 = bitSize;

  outArgs[0].type = LUA_ARG_TYPE_BOOL;   /* decoding results */
  outArgs[1].type = LUA_ARG_TYPE_STRING; /* decoding string */
  auto r = luaScript->call(lua.c_str(), inArgs, 2, outArgs, 2);
  if (0 == r) {
    if (outArgs[0].b) {
      s = outArgs[1].string;
    }
  }

  return s;
}

std::string UIData::Data::encode(std::string s) {
  std::string retS = s;
  lua_arg_t inArgs[2];
  lua_arg_t outArgs[4];

  std::vector<uint32_t> d;

  if (UIData::DisplayType::ASC == displayType) {
    d = eval("text=" + s);
  } else {
    d = eval("{" + s + "}");
  }

  inArgs[0].type = LUA_ARG_TYPE_UINT32_N;
  inArgs[0].u32N = d;

  inArgs[1].type = LUA_ARG_TYPE_UINT32;
  inArgs[1].u32 = bitSize;

  outArgs[0].type = LUA_ARG_TYPE_BOOL;   /* encoding results */
  outArgs[1].type = LUA_ARG_TYPE_STRING; /* encoding string */
  auto r = luaScript->call(lua.c_str(), inArgs, 2, outArgs, 2);
  if (0 == r) {
    if (outArgs[0].b) {
      if (false == outArgs[1].string.empty()) {
        retS = outArgs[1].string;
      };
    }
  }

  return retS;
}

std::vector<uint32_t> UIData::Data::eval(std::string s) {
  std::vector<uint32_t> res;
  lua_arg_t inArgs[1];
  lua_arg_t outArgs[1];

  inArgs[0].type = LUA_ARG_TYPE_STRING;
  inArgs[0].string = (char *)s.c_str();

  outArgs[0].type = LUA_ARG_TYPE_UINT32_N;
  auto r = luaScript->call("eval", inArgs, 1, outArgs, 1);
  if (0 == r) {
    res = outArgs[0].u32N;
  } else {
    throw std::runtime_error("eval '" + s + "' failed");
  }

  return res;
}

std::string UIData::Data::str() {
  std::stringstream ss;
  std::string s;

  if (UIData::DataType::UINT_N == dataType) {
    if (UIData::DisplayType::HEX == displayType) {
      s = to_hex(data);
    } else if (UIData::DisplayType::ASC == displayType) {
      s = to_ascii(data);
    } else {
      s = to_dec(data);
    }
  } else if (UIData::DataType::UINT == dataType) {
    if (UIData::DisplayType::HEX == displayType) {
      ss << "0x" << std::setw(2) << std::setfill('0') << std::hex << data[0];
      s = ss.str();
    } else {
      ss << std::dec << data[0];
      s = ss.str();
    }
  } else if (UIData::DataType::UINT_SELECT == dataType) {
    for (auto &it : selects) {
      if (data[0] == it.second) {
        s = it.first;
        break;
      }
    }
  } else { /* UINT_MULTI_SELECT */
    std::vector<std::string> slects;
    for (auto &it : selects) {
      if ((data[0] & it.second) == it.second) {
        slects.push_back(it.first);
      }
    }
    for (size_t i = 0; i < slects.size(); i++) {
      ss << slects[i];
      if (i < (slects.size() - 1)) {
        ss << ", ";
      }
    }
    s = ss.str();
  }

  return s;
}

std::string UIData::Data::raw() {
  return to_hex(data);
}

void UIData::Data::put(BitArray &TxBits) {
  if (nullptr != ui) {
    auto s = ui->get();
    if (false == s.empty()) {
      if (false == lua.empty()) {
        s = encode(s);
      }
      if (UIData::DisplayType::ASC == displayType) {
        data = eval("text=" + s);
      } else {
        data = eval("{" + s + "}");
      }
    }
  } else if (nullptr != uiTable) {
    uiTable->get();
  } else {
    throw std::runtime_error("no ui for data " + name);
  }

  for (auto &v : data) {
    TxBits.put(v, bitSize);
  }
}

void UIData::Data::get(BitArray &RxBits) {
  if (true == optional) {
    if (0 == RxBits.left()) {
      return;
    }
  }
  if (nullptr != ui) {
    for (auto &v : data) {
      v = RxBits.get(bitSize);
    }
    auto s = str();
    if (false == lua.empty()) {
      auto s2 = decode();
      if (false == s2.empty()) {
        s = s2;
      }
    }
    ui->put(s);
  } else if (nullptr != uiTable) {
    uiTable->put(RxBits);
  } else {
    throw std::runtime_error("no ui for data " + name);
  }
}

UIData::UIData(std::shared_ptr<Data> data) : m_Data(data) {
  auto vbox = new QVBoxLayout();
  if (DataType::UINT_SELECT == m_Data->dataType) {
    m_cmbxData = new QComboBox();
    for (auto &it : m_Data->selects) {
      m_cmbxData->addItem(tr(it.first.c_str()));
    }
    m_cmbxData->setCurrentText(tr(m_Data->str().c_str()));
    m_cmbxData->setToolTip(tr(m_Data->raw().c_str()));
    vbox->addWidget(m_cmbxData);
    if (m_Data->readonly) {
      m_cmbxData->setEnabled(false);
    }
  } else if (DataType::UINT_MULTI_SELECT == m_Data->dataType) {
    m_mscbData = new MultiSelectComboBox();
    for (auto &it : m_Data->selects) {
      m_mscbData->addItem(tr(it.first.c_str()));
    }
    QString qstr = m_Data->str().c_str();
    QStringList qstrList = qstr.split(", ");
    m_mscbData->setCurrentText(qstrList);
    m_mscbData->setToolTip(tr(m_Data->raw().c_str()));
    vbox->addWidget(m_mscbData);
    if (m_Data->readonly) {
      m_mscbData->setEnabled(false);
    }
  } else {
    m_leData = new QLineEdit(tr(m_Data->str().c_str()));
    if (m_Data->readonly) {
      m_leData->setReadOnly(true);
      m_leData->setStyleSheet("background-color:rgba(128,128,128,255)");
    }
    m_leData->setToolTip(tr(m_Data->raw().c_str()));
    vbox->addWidget(m_leData);
  }
  setLayout(vbox);
}

void UIData::put(std::string s) {
  if (DataType::UINT_MULTI_SELECT == m_Data->dataType) {
    QString qstr = s.c_str();
    QStringList qstrList = qstr.split(", ");
    m_mscbData->setCurrentText(qstrList);
    m_mscbData->setToolTip(tr(m_Data->raw().c_str()));
  } else if (DataType::UINT_SELECT == m_Data->dataType) {
    m_cmbxData->setCurrentText(tr(s.c_str()));
    m_cmbxData->setToolTip(tr(m_Data->raw().c_str()));
  } else {
    m_leData->setText(tr(s.c_str()));
    m_leData->setToolTip(tr(m_Data->raw().c_str()));
  }
}

std::string UIData::get(void) {
  std::string s;
  if (DataType::UINT_SELECT == m_Data->dataType) {
    auto ts = m_cmbxData->currentText().toStdString();
    for (auto &it : m_Data->selects) {
      if (ts == it.first) {
        s = std::to_string(it.second);
        break;
      }
    }
  } else if (DataType::UINT_MULTI_SELECT == m_Data->dataType) {
    auto qstrList = m_mscbData->currentText();
    uint32_t u32 = 0;
    for (auto &it : m_Data->selects) {
      if (true == qstrList.contains(tr(it.first.c_str()))) {
        u32 |= it.second;
      }
    }
    s = std::to_string(u32);
  } else {
    s = m_leData->text().toStdString();
  }
  return s;
}

UIData::~UIData() {
}

UIDataTable::UIDataTable(std::shared_ptr<UIData::Data> data) : m_Data(data) {
  QStringList headers;
  for (auto &d : m_Data->elements) {
    headers << tr(d->name.c_str());
  }
  setColumnCount(m_Data->elements.size());
  setHorizontalHeaderLabels(headers);
  setMinimumHeight(200);
}

void UIDataTable::decode(BitArray &ba) {
  std::vector<uint32_t> res;
  lua_arg_t inArgs[1];
  lua_arg_t outArgs[4];
  auto data = ba.left_bytes();

  inArgs[0].type = LUA_ARG_TYPE_UINT8_N;
  inArgs[0].u8N = data;

  outArgs[0].type = LUA_ARG_TYPE_BOOL;     /* decoding results */
  outArgs[1].type = LUA_ARG_TYPE_STRING_N; /* decoding headers */
  outArgs[2].type = LUA_ARG_TYPE_STRING_N; /* decoding values */
  outArgs[3].type = LUA_ARG_TYPE_UINT32;   /* consumed bits */
  auto r = m_Data->luaScript->call(m_Data->lua.c_str(), inArgs, 1, outArgs, 4);
  if (0 == r) {
    if (outArgs[0].b) {
      auto numHeaders = outArgs[1].stringN.size();
      clear();
      QStringList headers;
      std::vector<int> widths;
      for (auto &h : outArgs[1].stringN) {
        headers << tr(h.c_str());
        widths.push_back(h.size() * 10);
      }
      setColumnCount(numHeaders);

      setHorizontalHeaderLabels(headers);
      size_t idx = 0;
      for (auto &v : outArgs[2].stringN) {
        int row = idx / numHeaders;
        int col = idx % numHeaders;
        setRowCount(row + 1);
        QTableWidgetItem *newItem = new QTableWidgetItem(tr(v.c_str()));
        newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
        setItem(row, col, newItem);
        if (widths[col] < (int)(v.size() * 10)) {
          widths[col] = v.size() * 10;
          if (widths[col] > 500) {
            widths[col] = 500;
          }
        }
        idx++;
      }
      for (int col = 0; col < (int)widths.size(); col++) {
        setColumnWidth(col, widths[col]);
      }
      ba.drop(outArgs[3].u32);
    } else {
      throw std::runtime_error("Failed to decode table content:" + outArgs[1].stringN[0]);
    }
  } else {
    throw std::runtime_error("Failed to decode table content from [" + to_hex(data) + "]");
  }
}

void UIDataTable::get(void) {
  lua_arg_t outArgs[3];

  outArgs[0].type = LUA_ARG_TYPE_UINT32_N; /* get new bytes data */
  outArgs[1].type = LUA_ARG_TYPE_STRING_N; /* bytes data headers */
  outArgs[2].type = LUA_ARG_TYPE_STRING_N; /* bytes data values */
  auto r = m_Data->luaScript->call(m_Data->lua.c_str(), nullptr, 0, outArgs, 3);
  if (0 == r) {
    m_Data->data = outArgs[0].u32N;
    auto numHeaders = outArgs[1].stringN.size();
    clear();
    QStringList headers;
    std::vector<int> widths;
    for (auto &h : outArgs[1].stringN) {
      headers << tr(h.c_str());
      widths.push_back(h.size() * 10);
    }
    setColumnCount(numHeaders);

    setHorizontalHeaderLabels(headers);
    size_t idx = 0;
    for (auto &v : outArgs[2].stringN) {
      int row = idx / numHeaders;
      int col = idx % numHeaders;
      setRowCount(row + 1);
      QTableWidgetItem *newItem = new QTableWidgetItem(tr(v.c_str()));
      newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
      setItem(row, col, newItem);
      if (widths[col] < (int)(v.size() * 10)) {
        widths[col] = v.size() * 10;
        if (widths[col] > 500) {
          widths[col] = 500;
        }
      }
      idx++;
    }
    for (int col = 0; col < (int)widths.size(); col++) {
      setColumnWidth(col, widths[col]);
    }
  } else {
    throw std::runtime_error("Failed to get table content: " + m_Data->lua);
  }
}

void UIDataTable::put(BitArray &ba) {
  auto left = ba.left();
  if (0 == m_Data->bitSize) { /* in this case lua script API must be provided */
    decode(ba);
  } else if (0 == (left % m_Data->bitSize)) {
    auto leftN = left / m_Data->bitSize;
    auto n = m_Data->size;
    if (0 == n) {
      n = leftN;
    }

    if (n <= leftN) {
      setRowCount(n);
      for (uint32_t row = 0; row < n; row++) {
        int col = 0;
        for (auto &data : m_Data->elements) {
          for (auto &v : data->data) {
            v = ba.get(data->bitSize);
          }
          auto s = data->str();
          QTableWidgetItem *newItem = new QTableWidgetItem(tr(s.c_str()));
          newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
          setItem(row, col, newItem);
          col++;
        }
      }
    } else {
      throw std::runtime_error("not enough for " + m_Data->name + " expect: " + std::to_string(n) +
                               " left: " + std::to_string(leftN));
    }
  } else {
    throw std::runtime_error("not bit aligned for " + m_Data->name + " expect: " +
                             std::to_string(m_Data->bitSize) + " left: " + std::to_string(left));
  }
}

UIDataTable::~UIDataTable() {
}

UIServiceBase::UIServiceBase(std::string name, std::shared_ptr<AsLuaScript> luaScript)
  : QGroupBox(name.c_str()), m_Name(name), m_Lua(luaScript) {
  m_reType = std::regex("(U)(\\d+)(Select|Array|MultiSelect|)");
}

UIServiceBase::~UIServiceBase() {
}

std::shared_ptr<UIData::Data> UIServiceBase::parseData(json &js) {
  auto data = std::make_shared<UIData::Data>();

  data->name = js["name"].get<std::string>();
  data->luaScript = m_Lua;
  data->lua = get<std::string>(js, "lua", "");

  auto display = get<std::string>(js, "display", "dec");

  if (display == "asc") {
    data->displayType = UIData::DisplayType::ASC;
  } else if (display == "hex") {
    data->displayType = UIData::DisplayType::HEX;
  } else {
    data->displayType = UIData::DisplayType::DEC;
  }
  data->readonly = get<bool>(js, "readonly", false);
  data->optional = get<bool>(js, "optional", false);
  data->visible = get<bool>(js, "visible", true);
  data->newline = get<bool>(js, "newline", false);

  auto type = js["type"].get<std::string>();
  std::smatch match;
  if (std::regex_search(type, match, m_reType)) {
#if 0
    std::cout << "matches for '" << type << "'\n";
    std::cout << "Prefix: '" << match.prefix() << "'\n";
    for (size_t i = 0; i < match.size(); ++i)
      std::cout << i << ": " << match[i] << '\n';
    std::cout << "Suffix: '" << match.suffix() << "\'\n\n";
#endif
    data->bitSize = toInteger(match[2]);
    data->bitMask = (uint32_t)((1 << data->bitSize) - 1);
    if ("Array" == match[3]) {
      data->dataType = UIData::DataType::UINT_N;
      data->size = js["size"].get<uint32_t>();
      if (js.contains("default")) {
        data->data = data->eval(js["default"].dump());
      }
      for (size_t i = data->data.size(); i < data->size; i++) {
        data->data.push_back(0);
      }
    } else if ("Select" == match[3]) {
      data->dataType = UIData::DataType::UINT_SELECT;
      for (auto &select : js["Select"]) {
        auto name = select["name"].get<std::string>();
        auto value = get2<uint32_t>(select, "value", 0);
        data->selects[name] = value;
      }
      auto v = get2<uint32_t>(js, "default", 0);
      data->data.push_back(v);
    } else if ("MultiSelect" == match[3]) {
      data->dataType = UIData::DataType::UINT_MULTI_SELECT;
      for (auto &select : js["Select"]) {
        auto name = select["name"].get<std::string>();
        auto value = get2<uint32_t>(select, "value", 0);
        data->selects[name] = value;
      }
      auto v = get2<uint32_t>(js, "default", 0);
      data->data.push_back(v);
    } else {
      data->dataType = UIData::DataType::UINT;
      auto v = get2<uint32_t>(js, "default", 0);
      data->data.push_back(v);
    }
  } else {
    throw std::runtime_error("found invalid type " + type + " for " + data->name);
  }

  LOG(INFO, "  data: %s bit size=%d type=%d display=%d default=%s\n", data->name.c_str(),
      data->bitSize, data->dataType, data->displayType, data->str().c_str());
  for (auto &it : data->selects) {
    LOG(INFO, "    select %s 0x%x\n", it.first.c_str(), it.second);
  }
  return data;
}

QVBoxLayout *UIServiceBase::createUI(json &js) {
  m_SID = get2(js, "SID", 0);
  if (0 == m_SID) {
    throw std::runtime_error("found invalid SID for " + m_Name);
  }
  LOG(INFO, "transmit datas for %s:\n", m_Name.c_str());
  if (js.contains("transmit")) {
    for (auto &djs : js["transmit"]) {
      if (djs["type"].get<std::string>() == "Table") {
        auto table = std::make_shared<UIData::Data>();
        table->name = djs["name"].get<std::string>();
        table->dataType = UIData::DataType::TABLE;
        table->bitSize = 8;
        table->readonly = false;
        table->size = get<uint32_t>(djs, "size", 0);
        table->luaScript = m_Lua;
        table->lua = get<std::string>(djs, "lua", "");
        table->visible = get<bool>(djs, "visible", true);
        m_TxDatas.push_back(table);
      } else {
        auto data = parseData(djs);
        data->optional = false;
        m_TxBitSize += data->bitSize * data->size;
        m_TxDatas.push_back(data);
      }
    }
  }

  LOG(INFO, "receive datas for %s:\n", m_Name.c_str());
  if (js.contains("receive")) {
    for (auto &djs : js["receive"]) {
      if (djs["type"].get<std::string>() == "Table") {
        auto table = std::make_shared<UIData::Data>();
        table->name = djs["name"].get<std::string>();
        table->dataType = UIData::DataType::TABLE;
        table->bitSize = 0; /* the baisc element total bit size */
        table->readonly = true;
        table->size = get<uint32_t>(djs, "size", 0);
        table->luaScript = m_Lua;
        table->lua = get<std::string>(djs, "lua", "");
        table->visible = get<bool>(djs, "visible", true);
        for (auto &tjs : djs["datas"]) {
          auto data = parseData(tjs);
          data->readonly = true;
          table->bitSize += data->bitSize * data->size;
          table->elements.push_back(data);
        }
        m_RxDatas.push_back(table);
      } else {
        auto data = parseData(djs);
        data->readonly = true;
        if (false == data->optional) {
          m_RxBitSize += data->bitSize * data->size;
        }
        m_RxDatas.push_back(data);
      }
    }
  }

  auto vbox = new QVBoxLayout();

  int row = 0;
  int col = 0;
  auto grid = new QGridLayout();
  bool hasTxTable = false;
  for (auto &data : m_TxDatas) {
    if (UIData::DataType::TABLE == data->dataType) {
      hasTxTable = true;
      continue;
    }
    data->ui = new UIData(data);
    if (data->visible) {
      auto label = data->name + ":";
      grid->addWidget(new QLabel(tr(label.c_str())), row, col);
      grid->addWidget(data->ui, row, col + 1);
      col += 2;

      if ((col >= 8) || (true == data->newline)) {
        row += 1;
        col = 0;
      }
    }
  }
  if (hasTxTable) {
    vbox->addLayout(grid);

    for (auto &data : m_TxDatas) {
      if (UIData::DataType::TABLE == data->dataType) {
        auto table = new UIDataTable(data);
        data->uiTable = table;
        if (data->visible) {
          vbox->addWidget(table);
        }
      }
    }
    grid = new QGridLayout();
    row = 0;
  }

  m_btnEnter = new QPushButton("Enter");
  m_btnEnter->setMaximumWidth(100);
  grid->addWidget(m_btnEnter, row, col);
  row += 1;
  col = 0;

  for (auto &data : m_RxDatas) {
    if (UIData::DataType::TABLE == data->dataType) {
      continue;
    }
    data->ui = new UIData(data);
    if (data->visible) {
      auto label = data->name + ":";
      grid->addWidget(new QLabel(tr(label.c_str())), row, col);
      grid->addWidget(data->ui, row, col + 1);
      col += 2;

      if ((col >= 8) || (true == data->newline)) {
        row += 1;
        col = 0;
      }
    }
  }

  vbox->addLayout(grid);

  for (auto &data : m_RxDatas) {
    if (UIData::DataType::TABLE == data->dataType) {
      auto table = new UIDataTable(data);
      data->uiTable = table;
      if (data->visible) {
        vbox->addWidget(table);
      }
    }
  }

  return vbox;
}

} /* namespace common */
} /* namespace ui */
} /* namespace as */
