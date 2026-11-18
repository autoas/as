/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2022 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include "odx.h"
#include "tinyxml2.h"
#include "string.h"

using namespace tinyxml2;
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
template <typename T> static XMLElement *Odx_Find(T *root, std::vector<std::string> where) {
  XMLElement *next = root->FirstChildElement(where[0].c_str());

  for (size_t i = 1; (i < where.size()) && (nullptr != next); i++) {
    next = next->FirstChildElement(where[i].c_str());
  }

  if (nullptr == next) {
    std::string path = "";
    for (auto &s : where) {
      path += "/" + s;
    }
    printf("ERROR: has no %s\n", path.c_str());
  }

  return next;
}

/* ================================ [ FUNCTIONS ] ============================================== */
extern "C" {
Odx_Type *Odx_Open(const char *path) {
  Odx_Type *odx = nullptr;
  XMLDocument doc;
  std::vector<Odx_MemType *> mems;
  XMLElement *ecuMem = nullptr;
  auto ret = doc.LoadFile(path);
  if (XML_SUCCESS != ret) {
    printf("ERROR: failed to load odx %s, error is %d\n", path, ret);
  } else {
    ecuMem = Odx_Find(&doc, {"ODX", "FLASH", "ECU-MEMS", "ECU-MEM"});
  }

  while (nullptr != ecuMem) {
    const char *sid = nullptr;
    ret = ecuMem->QueryStringAttribute("ID", &sid);
    if ((XML_SUCCESS == ret) && (nullptr != sid)) {
      std::vector<Odx_BlockType *> blocks;
      XMLElement *flashData = Odx_Find(ecuMem, {"MEM", "FLASHDATAS", "FLASHDATA"});
      while (nullptr != flashData) {
        auto itsName = flashData->FirstChildElement("SHORT-NAME");
        auto itsData = flashData->FirstChildElement("DATA");
        if ((nullptr != itsName) && (nullptr != itsData)) {
          auto name = itsName->GetText();
          auto data = itsData->GetText();
          Odx_BlockType *block = new Odx_BlockType;
          block->Name = strdup(name);
          size_t len = strlen(data) / 2;
          block->Data = new uint8_t[len];
          block->Size = len;
          char txt[3] = {0, 0, 0};
          for (size_t i = 0; i < len; i++) {
            txt[0] = data[2 * i];
            txt[1] = data[2 * i + 1];
            block->Data[i] = strtoul(txt, nullptr, 16);
          }
          blocks.push_back(block);
        } else {
          printf("ERROR: %s found FLASHDATA either has no SHORT-NAME or DATA\n", path);
        }
        flashData = flashData->NextSiblingElement("FLASHDATA");
      }
      if (blocks.size() > 0) {
        Odx_MemType *mem = new Odx_MemType;
        mem->Name = strdup(sid);
        mem->Blocks = new Odx_BlockType *[blocks.size()];
        for (size_t i = 0; i < blocks.size(); i++) {
          mem->Blocks[i] = blocks[i];
        }
        mem->NumOfBlocks = blocks.size();
        mems.push_back(mem);
      } else {
        printf("ERROR: %s empty FLASHDATAS\n", path);
      }
    } else {
      printf("ERROR: %s found ECU-MEM has no ID\n", path);
    }
    ecuMem = ecuMem->NextSiblingElement("ECU-MEM");
  }

  if (mems.size() > 0) {
    odx = new Odx_Type;
    odx->Mems = new Odx_MemType *[mems.size()];
    for (size_t i = 0; i < mems.size(); i++) {
      odx->Mems[i] = mems[i];
    }
    odx->NumOfMems = mems.size();
  }

  return odx;
}

void Odx_Close(Odx_Type *odx) {
  for (size_t i = 0; i < odx->NumOfMems; i++) {
    auto mem = odx->Mems[i];
    for (size_t j = 0; j < mem->NumOfBlocks; j++) {
      auto block = mem->Blocks[j];
      delete[] block->Data;
      free(block->Name);
      delete block;
    }
    delete[] mem->Blocks;
    free(mem->Name);
    delete mem;
  }
  delete odx;
}
} /* extern "C" */
