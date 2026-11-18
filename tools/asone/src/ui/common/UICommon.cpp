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
std::string UIData::Data::Decode() {
  std::string s;
  Lua_ArgType inArgs[2];
  Lua_ArgType outArgs[4];

  inArgs[0].Type = LUA_ARG_TYPE_UINT32_N;
  inArgs[0].U32N = Datas;

  inArgs[1].Type = LUA_ARG_TYPE_UINT32;
  inArgs[1].U32 = BitSize;

  outArgs[0].Type = LUA_ARG_TYPE_BOOL;   /* decoding results */
  outArgs[1].Type = LUA_ARG_TYPE_STRING; /* decoding string */
  auto r = LuaScript->Call(Lua.c_str(), inArgs, 2, outArgs, 2);
  if (0 == r) {
    if (outArgs[0].Bool) {
      s = outArgs[1].String;
    }
  }

  return s;
}

std::string UIData::Data::Encode(std::string s) {
  std::string retS = s;
  Lua_ArgType inArgs[2];
  Lua_ArgType outArgs[4];

  std::vector<uint32_t> d;

  if (UIData::DisplayType::ASC == DisplayType) {
    d = Eval("text=" + s);
  } else {
    d = Eval("{" + s + "}");
  }

  inArgs[0].Type = LUA_ARG_TYPE_UINT32_N;
  inArgs[0].U32N = d;

  inArgs[1].Type = LUA_ARG_TYPE_UINT32;
  inArgs[1].U32 = BitSize;

  outArgs[0].Type = LUA_ARG_TYPE_BOOL;   /* encoding results */
  outArgs[1].Type = LUA_ARG_TYPE_STRING; /* encoding string */
  auto r = LuaScript->Call(Lua.c_str(), inArgs, 2, outArgs, 2);
  if (0 == r) {
    if (outArgs[0].Bool) {
      if (false == outArgs[1].String.empty()) {
        retS = outArgs[1].String;
      };
    }
  }

  return retS;
}

std::vector<uint32_t> UIData::Data::Eval(std::string s) {
  std::vector<uint32_t> res;
  Lua_ArgType inArgs[1];
  Lua_ArgType outArgs[1];

  inArgs[0].Type = LUA_ARG_TYPE_STRING;
  inArgs[0].String = (char *)s.c_str();

  outArgs[0].Type = LUA_ARG_TYPE_UINT32_N;
  auto r = LuaScript->Call("eval", inArgs, 1, outArgs, 1);
  if (0 == r) {
    res = outArgs[0].U32N;
  } else {
    throw std::runtime_error("eval '" + s + "' failed");
  }

  return res;
}

std::string UIData::Data::Str() {
  std::stringstream ss;
  std::string s;

  if (UIData::DataType::UINT_N == DataType) {
    if (UIData::DisplayType::HEX == DisplayType) {
      s = ToHex(Datas);
    } else if (UIData::DisplayType::ASC == DisplayType) {
      s = ToAscii(Datas);
    } else {
      s = ToDec(Datas);
    }
  } else if (UIData::DataType::UINT == DataType) {
    if (UIData::DisplayType::HEX == DisplayType) {
      ss << "0x" << std::setw(2) << std::setfill('0') << std::hex << Datas[0];
      s = ss.str();
    } else {
      ss << std::dec << Datas[0];
      s = ss.str();
    }
  } else if (UIData::DataType::UINT_SELECT == DataType) {
    for (auto &it : Selects) {
      if (Datas[0] == it.second) {
        s = it.first;
        break;
      }
    }
  } else { /* UINT_MULTI_SELECT */
    std::vector<std::string> slects;
    for (auto &it : Selects) {
      if ((Datas[0] & it.second) == it.second) {
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

std::string UIData::Data::Raw() {
  return ToHex(Datas);
}

void UIData::Data::Put(BitArray &TxBits) {
  if (nullptr != Ui) {
    auto s = Ui->Get();
    if (false == s.empty()) {
      if (false == Lua.empty()) {
        s = Encode(s);
      }
      if (UIData::DisplayType::ASC == DisplayType) {
        Datas = Eval("text=" + s);
      } else {
        Datas = Eval("{" + s + "}");
      }
    }
  } else if (nullptr != UiTable) {
    UiTable->Get();
  } else {
    throw std::runtime_error("no ui for data " + Name);
  }

  for (auto &v : Datas) {
    TxBits.Put(v, BitSize);
  }
}

void UIData::Data::Get(BitArray &RxBits) {
  if (true == Optional) {
    if (0 == RxBits.Left()) {
      return;
    }
  }
  if (nullptr != Ui) {
    for (auto &v : Datas) {
      v = RxBits.Get(BitSize);
    }
    auto s = Str();
    if (false == Lua.empty()) {
      auto s2 = Decode();
      if (false == s2.empty()) {
        s = s2;
      }
    }
    Ui->Put(s);
  } else if (nullptr != UiTable) {
    UiTable->Put(RxBits);
  } else {
    throw std::runtime_error("no ui for data " + Name);
  }
}

UIData::UIData(std::shared_ptr<Data> Datas) : m_Data(Datas) {
  auto vbox = new QVBoxLayout();
  if (DataType::UINT_SELECT == m_Data->DataType) {
    m_CmbxData = new QComboBox();
    for (auto &it : m_Data->Selects) {
      m_CmbxData->addItem(tr(it.first.c_str()));
    }
    m_CmbxData->setCurrentText(tr(m_Data->Str().c_str()));
    m_CmbxData->setToolTip(tr(m_Data->Raw().c_str()));
    vbox->addWidget(m_CmbxData);
    if (m_Data->Readonly) {
      m_CmbxData->setEnabled(false);
    }
  } else if (DataType::UINT_MULTI_SELECT == m_Data->DataType) {
    m_MscbData = new MultiSelectComboBox();
    for (auto &it : m_Data->Selects) {
      m_MscbData->addItem(tr(it.first.c_str()));
    }
    QString qstr = m_Data->Str().c_str();
    QStringList qstrList = qstr.split(", ");
    m_MscbData->setCurrentText(qstrList);
    m_MscbData->setToolTip(tr(m_Data->Raw().c_str()));
    vbox->addWidget(m_MscbData);
    if (m_Data->Readonly) {
      m_MscbData->setEnabled(false);
    }
  } else {
    m_LeData = new QLineEdit(tr(m_Data->Str().c_str()));
    if (m_Data->Readonly) {
      m_LeData->setReadOnly(true);
      m_LeData->setStyleSheet("background-color:rgba(128,128,128,255)");
    }
    m_LeData->setToolTip(tr(m_Data->Raw().c_str()));
    vbox->addWidget(m_LeData);
  }
  setLayout(vbox);
}

void UIData::Put(std::string s) {
  if (DataType::UINT_MULTI_SELECT == m_Data->DataType) {
    QString qstr = s.c_str();
    QStringList qstrList = qstr.split(", ");
    m_MscbData->setCurrentText(qstrList);
    m_MscbData->setToolTip(tr(m_Data->Raw().c_str()));
  } else if (DataType::UINT_SELECT == m_Data->DataType) {
    m_CmbxData->setCurrentText(tr(s.c_str()));
    m_CmbxData->setToolTip(tr(m_Data->Raw().c_str()));
  } else {
    m_LeData->setText(tr(s.c_str()));
    m_LeData->setToolTip(tr(m_Data->Raw().c_str()));
  }
}

std::string UIData::Get(void) {
  std::string s;
  if (DataType::UINT_SELECT == m_Data->DataType) {
    auto ts = m_CmbxData->currentText().toStdString();
    for (auto &it : m_Data->Selects) {
      if (ts == it.first) {
        s = std::to_string(it.second);
        break;
      }
    }
  } else if (DataType::UINT_MULTI_SELECT == m_Data->DataType) {
    auto qstrList = m_MscbData->currentText();
    uint32_t u32 = 0;
    for (auto &it : m_Data->Selects) {
      if (true == qstrList.contains(tr(it.first.c_str()))) {
        u32 |= it.second;
      }
    }
    s = std::to_string(u32);
  } else {
    s = m_LeData->text().toStdString();
  }
  return s;
}

UIData::~UIData() {
}

UIDataTable::UIDataTable(std::shared_ptr<UIData::Data> Datas) : m_Data(Datas) {
  QStringList headers;
  for (auto &d : m_Data->Elements) {
    headers << tr(d->Name.c_str());
  }
  setColumnCount(m_Data->Elements.size());
  setHorizontalHeaderLabels(headers);
  setMinimumHeight(200);
}

void UIDataTable::Decode(BitArray &ba) {
  std::vector<uint32_t> res;
  Lua_ArgType inArgs[1];
  Lua_ArgType outArgs[4];
  auto Datas = ba.LeftBytes();

  inArgs[0].Type = LUA_ARG_TYPE_UINT8_N;
  inArgs[0].U8N = Datas;

  outArgs[0].Type = LUA_ARG_TYPE_BOOL;     /* decoding results */
  outArgs[1].Type = LUA_ARG_TYPE_STRING_N; /* decoding headers */
  outArgs[2].Type = LUA_ARG_TYPE_STRING_N; /* decoding values */
  outArgs[3].Type = LUA_ARG_TYPE_UINT32;   /* consumed bits */
  auto r = m_Data->LuaScript->Call(m_Data->Lua.c_str(), inArgs, 1, outArgs, 4);
  if (0 == r) {
    if (outArgs[0].Bool) {
      auto numHeaders = outArgs[1].StringN.size();
      clear();
      QStringList headers;
      std::vector<int> widths;
      for (auto &h : outArgs[1].StringN) {
        headers << tr(h.c_str());
        widths.push_back(h.size() * 10);
      }
      setColumnCount(numHeaders);

      setHorizontalHeaderLabels(headers);
      size_t idx = 0;
      for (auto &v : outArgs[2].StringN) {
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
      ba.Drop(outArgs[3].U32);
    } else {
      throw std::runtime_error("Failed to decode table content:" + outArgs[1].StringN[0]);
    }
  } else {
    throw std::runtime_error("Failed to decode table content from [" + ToHex(Datas) + "]");
  }
}

void UIDataTable::Get(void) {
  Lua_ArgType outArgs[3];

  outArgs[0].Type = LUA_ARG_TYPE_UINT32_N; /* get new bytes data */
  outArgs[1].Type = LUA_ARG_TYPE_STRING_N; /* bytes data headers */
  outArgs[2].Type = LUA_ARG_TYPE_STRING_N; /* bytes data values */
  auto r = m_Data->LuaScript->Call(m_Data->Lua.c_str(), nullptr, 0, outArgs, 3);
  if (0 == r) {
    m_Data->Datas = outArgs[0].U32N;
    auto numHeaders = outArgs[1].StringN.size();
    clear();
    QStringList headers;
    std::vector<int> widths;
    for (auto &h : outArgs[1].StringN) {
      headers << tr(h.c_str());
      widths.push_back(h.size() * 10);
    }
    setColumnCount(numHeaders);

    setHorizontalHeaderLabels(headers);
    size_t idx = 0;
    for (auto &v : outArgs[2].StringN) {
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
    throw std::runtime_error("Failed to get table content: " + m_Data->Lua);
  }
}

void UIDataTable::Put(BitArray &ba) {
  auto left = ba.Left();
  if (0 == m_Data->BitSize) { /* in this case lua script API must be provided */
    Decode(ba);
  } else if (0 == (left % m_Data->BitSize)) {
    auto leftN = left / m_Data->BitSize;
    auto n = m_Data->Size;
    if (0 == n) {
      n = leftN;
    }

    if (n <= leftN) {
      setRowCount(n);
      for (uint32_t row = 0; row < n; row++) {
        int col = 0;
        for (auto &Datas : m_Data->Elements) {
          for (auto &v : Datas->Datas) {
            v = ba.Get(Datas->BitSize);
          }
          auto s = Datas->Str();
          QTableWidgetItem *newItem = new QTableWidgetItem(tr(s.c_str()));
          newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
          setItem(row, col, newItem);
          col++;
        }
      }
    } else {
      throw std::runtime_error("not enough for " + m_Data->Name + " expect: " + std::to_string(n) +
                               " left: " + std::to_string(leftN));
    }
  } else {
    throw std::runtime_error("not bit aligned for " + m_Data->Name + " expect: " +
                             std::to_string(m_Data->BitSize) + " left: " + std::to_string(left));
  }
}

UIDataTable::~UIDataTable() {
}

UIServiceBase::UIServiceBase(std::string Name, std::shared_ptr<AsLuaScript> LuaScript)
  : QGroupBox(Name.c_str()), m_Name(Name), m_Lua(LuaScript) {
  m_ReType = std::regex("(U)(\\d+)(Select|Array|MultiSelect|)");
}

UIServiceBase::~UIServiceBase() {
}

std::shared_ptr<UIData::Data> UIServiceBase::ParseData(json &js) {
  auto Datas = std::make_shared<UIData::Data>();

  Datas->Name = js["Name"].get<std::string>();
  Datas->LuaScript = m_Lua;
  Datas->Lua = JsonGet<std::string>(js, "Lua", "");

  auto display = JsonGet<std::string>(js, "Display", "dec");

  if (display == "asc") {
    Datas->DisplayType = UIData::DisplayType::ASC;
  } else if (display == "hex") {
    Datas->DisplayType = UIData::DisplayType::HEX;
  } else {
    Datas->DisplayType = UIData::DisplayType::DEC;
  }
  Datas->Readonly = JsonGet<bool>(js, "Readonly", false);
  Datas->Optional = JsonGet<bool>(js, "Optional", false);
  Datas->Visible = JsonGet<bool>(js, "Visible", true);
  Datas->Newline = JsonGet<bool>(js, "Newline", false);

  auto type = js["Type"].get<std::string>();
  std::smatch match;
  if (std::regex_search(type, match, m_ReType)) {
#if 0
    std::cout << "matches for '" << type << "'\n";
    std::cout << "Prefix: '" << match.prefix() << "'\n";
    for (size_t i = 0; i < match.size(); ++i)
      std::cout << i << ": " << match[i] << '\n';
    std::cout << "Suffix: '" << match.suffix() << "\'\n\n";
#endif
    Datas->BitSize = ToInteger(match[2]);
    Datas->BitMask = (uint32_t)((1 << Datas->BitSize) - 1);
    if ("Array" == match[3]) {
      Datas->DataType = UIData::DataType::UINT_N;
      Datas->Size = js["Size"].get<uint32_t>();
      if (js.contains("Default")) {
        Datas->Datas = Datas->Eval(js["Default"].dump());
      }
      for (size_t i = Datas->Datas.size(); i < Datas->Size; i++) {
        Datas->Datas.push_back(0);
      }
    } else if ("Select" == match[3]) {
      Datas->DataType = UIData::DataType::UINT_SELECT;
      for (auto &select : js["Select"]) {
        auto Name = select["Name"].get<std::string>();
        auto value = JsonGetInt<uint32_t>(select, "Value", 0);
        Datas->Selects[Name] = value;
      }
      auto v = JsonGetInt<uint32_t>(js, "Default", 0);
      Datas->Datas.push_back(v);
    } else if ("MultiSelect" == match[3]) {
      Datas->DataType = UIData::DataType::UINT_MULTI_SELECT;
      for (auto &select : js["Select"]) {
        auto Name = select["Name"].get<std::string>();
        auto value = JsonGetInt<uint32_t>(select, "Value", 0);
        Datas->Selects[Name] = value;
      }
      auto v = JsonGetInt<uint32_t>(js, "Default", 0);
      Datas->Datas.push_back(v);
    } else {
      Datas->DataType = UIData::DataType::UINT;
      auto v = JsonGetInt<uint32_t>(js, "Default", 0);
      Datas->Datas.push_back(v);
    }
  } else {
    throw std::runtime_error("found invalid type " + type + " for " + Datas->Name);
  }

  LOG(INFO, "  data: %s bit size=%d type=%d display=%d default=%s\n", Datas->Name.c_str(),
      Datas->BitSize, Datas->DataType, Datas->DisplayType, Datas->Str().c_str());
  for (auto &it : Datas->Selects) {
    LOG(INFO, "    select %s 0x%x\n", it.first.c_str(), it.second);
  }
  return Datas;
}

QVBoxLayout *UIServiceBase::CreateUi(json &js) {
  m_SID = JsonGetInt(js, "SID", 0);
  if (0 == m_SID) {
    throw std::runtime_error("found invalid SID for " + m_Name);
  }
  LOG(INFO, "transmit datas for %s:\n", m_Name.c_str());
  if (js.contains("Transmit")) {
    for (auto &djs : js["Transmit"]) {
      if (djs["Type"].get<std::string>() == "Table") {
        auto table = std::make_shared<UIData::Data>();
        table->Name = djs["Name"].get<std::string>();
        table->DataType = UIData::DataType::TABLE;
        table->BitSize = 8;
        table->Readonly = false;
        table->Size = JsonGet<uint32_t>(djs, "Size", 0);
        table->LuaScript = m_Lua;
        table->Lua = JsonGet<std::string>(djs, "Lua", "");
        table->Visible = JsonGet<bool>(djs, "Visible", true);
        m_TxDatas.push_back(table);
      } else {
        auto Datas = ParseData(djs);
        Datas->Optional = false;
        m_TxBitSize += Datas->BitSize * Datas->Size;
        m_TxDatas.push_back(Datas);
      }
    }
  }

  LOG(INFO, "receive datas for %s:\n", m_Name.c_str());
  if (js.contains("Receive")) {
    for (auto &djs : js["Receive"]) {
      if (djs["Type"].get<std::string>() == "Table") {
        auto table = std::make_shared<UIData::Data>();
        table->Name = djs["Name"].get<std::string>();
        table->DataType = UIData::DataType::TABLE;
        table->BitSize = 0; /* the baisc element total bit size */
        table->Readonly = true;
        table->Size = JsonGet<uint32_t>(djs, "Size", 0);
        table->LuaScript = m_Lua;
        table->Lua = JsonGet<std::string>(djs, "Lua", "");
        table->Visible = JsonGet<bool>(djs, "Visible", true);
        for (auto &tjs : djs["Datas"]) {
          auto Datas = ParseData(tjs);
          Datas->Readonly = true;
          table->BitSize += Datas->BitSize * Datas->Size;
          table->Elements.push_back(Datas);
        }
        m_RxDatas.push_back(table);
      } else {
        auto Datas = ParseData(djs);
        Datas->Readonly = true;
        if (false == Datas->Optional) {
          m_RxBitSize += Datas->BitSize * Datas->Size;
        }
        m_RxDatas.push_back(Datas);
      }
    }
  }

  auto vbox = new QVBoxLayout();

  int row = 0;
  int col = 0;
  auto grid = new QGridLayout();
  bool hasTxTable = false;
  for (auto &Datas : m_TxDatas) {
    if (UIData::DataType::TABLE == Datas->DataType) {
      hasTxTable = true;
      continue;
    }
    Datas->Ui = new UIData(Datas);
    if (Datas->Visible) {
      auto label = Datas->Name + ":";
      grid->addWidget(new QLabel(tr(label.c_str())), row, col);
      grid->addWidget(Datas->Ui, row, col + 1);
      col += 2;

      if ((col >= 8) || (true == Datas->Newline)) {
        row += 1;
        col = 0;
      }
    }
  }
  if (hasTxTable) {
    vbox->addLayout(grid);

    for (auto &Datas : m_TxDatas) {
      if (UIData::DataType::TABLE == Datas->DataType) {
        auto table = new UIDataTable(Datas);
        Datas->UiTable = table;
        if (Datas->Visible) {
          vbox->addWidget(table);
        }
      }
    }
    grid = new QGridLayout();
    row = 0;
  }

  m_BtnEnter = new QPushButton("Enter");
  m_BtnEnter->setMaximumWidth(100);
  grid->addWidget(m_BtnEnter, row, col);
  row += 1;
  col = 0;

  for (auto &Datas : m_RxDatas) {
    if (UIData::DataType::TABLE == Datas->DataType) {
      continue;
    }
    Datas->Ui = new UIData(Datas);
    if (Datas->Visible) {
      auto label = Datas->Name + ":";
      grid->addWidget(new QLabel(tr(label.c_str())), row, col);
      grid->addWidget(Datas->Ui, row, col + 1);
      col += 2;

      if ((col >= 8) || (true == Datas->Newline)) {
        row += 1;
        col = 0;
      }
    }
  }

  vbox->addLayout(grid);

  for (auto &Datas : m_RxDatas) {
    if (UIData::DataType::TABLE == Datas->DataType) {
      auto table = new UIDataTable(Datas);
      Datas->UiTable = table;
      if (Datas->Visible) {
        vbox->addWidget(table);
      }
    }
  }

  return vbox;
}

} /* namespace common */
} /* namespace ui */
} /* namespace as */
