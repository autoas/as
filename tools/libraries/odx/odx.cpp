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
template <typename T> static XMLElement *odx_find(T *root, std::vector<std::string> where) {
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
odx_t *odx_open(const char *path) {
  odx_t *odx = nullptr;
  XMLDocument doc;
  std::vector<odx_mem_t *> mems;
  XMLElement *ecu_mem_ = nullptr;
  auto ret = doc.LoadFile(path);
  if (XML_SUCCESS != ret) {
    printf("ERROR: failed to load odx %s, error is %d\n", path, ret);
  } else {
    ecu_mem_ = odx_find(&doc, {"ODX", "FLASH", "ECU-MEMS", "ECU-MEM"});
  }

  while (nullptr != ecu_mem_) {
    const char *sid = nullptr;
    ret = ecu_mem_->QueryStringAttribute("ID", &sid);
    if ((XML_SUCCESS == ret) && (nullptr != sid)) {
      std::vector<odx_block_t *> blocks;
      XMLElement *flash_data_ = odx_find(ecu_mem_, {"MEM", "FLASHDATAS", "FLASHDATA"});
      while (nullptr != flash_data_) {
        auto its_name = flash_data_->FirstChildElement("SHORT-NAME");
        auto its_data = flash_data_->FirstChildElement("DATA");
        if ((nullptr != its_name) && (nullptr != its_data)) {
          auto name = its_name->GetText();
          auto data = its_data->GetText();
          odx_block_t *block = new odx_block_t;
          block->name = strdup(name);
          size_t len = strlen(data) / 2;
          block->data = new uint8_t[len];
          block->size = len;
          char txt[3] = {0, 0, 0};
          for (size_t i = 0; i < len; i++) {
            txt[0] = data[2 * i];
            txt[1] = data[2 * i + 1];
            block->data[i] = strtoul(txt, nullptr, 16);
          }
          blocks.push_back(block);
        } else {
          printf("ERROR: %s found FLASHDATA either has no SHORT-NAME or DATA\n", path);
        }
        flash_data_ = flash_data_->NextSiblingElement("FLASHDATA");
      }
      if (blocks.size() > 0) {
        odx_mem_t *mem = new odx_mem_t;
        mem->name = strdup(sid);
        mem->blocks = new odx_block_t *[blocks.size()];
        for (size_t i = 0; i < blocks.size(); i++) {
          mem->blocks[i] = blocks[i];
        }
        mem->numOfBlocks = blocks.size();
        mems.push_back(mem);
      } else {
        printf("ERROR: %s empty FLASHDATAS\n", path);
      }
    } else {
      printf("ERROR: %s found ECU-MEM has no ID\n", path);
    }
    ecu_mem_ = ecu_mem_->NextSiblingElement("ECU-MEM");
  }

  if (mems.size() > 0) {
    odx = new odx_t;
    odx->mems = new odx_mem_t *[mems.size()];
    for (size_t i = 0; i < mems.size(); i++) {
      odx->mems[i] = mems[i];
    }
    odx->numOfMems = mems.size();
  }

  return odx;
}

void odx_close(odx_t *odx) {
  for (size_t i = 0; i < odx->numOfMems; i++) {
    auto mem = odx->mems[i];
    for (size_t j = 0; j < mem->numOfBlocks; j++) {
      auto block = mem->blocks[j];
      delete[] block->data;
      free(block->name);
      delete block;
    }
    delete[] mem->blocks;
    free(mem->name);
    delete mem;
  }
  delete odx;
}
} /* extern "C" */
