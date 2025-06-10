/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include <gtest/gtest.h>

#include "E2E.h"
#include "E2E_Cfg.h"
#include "Std_Debug.h"
#include <string.h>
/* ================================ [ MACROS    ] ============================================== */
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* 6.12.5 E2E Profile 11 Protocol Examples */
TEST(E2E, P11_CASE1) {
  Std_ReturnType ret;
  uint16_t i = 0;
  uint8_t lData[64];
  (void)memset(lData, 0, 8);
  E2E_Init(NULL);
  /* Table 6.86 */
  ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_BOTH, lData, 8);
  ASSERT_EQ(E_OK, ret);
  EXPECT_EQ(0xcc, lData[0]);
  EXPECT_EQ(0x00, lData[1]);
  for (i = 0; i < 6; i++) {
    EXPECT_EQ(0u, lData[i + 2]);
  }
  ret = E2E_P11Check(E2E_CHECK_P11_P11_TST_BOTH, lData, 8);
  ASSERT_EQ(E_OK, ret);

  ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_BOTH, lData, 8);
  ASSERT_EQ(E_OK, ret);
  EXPECT_EQ(0x91, lData[0]);
  EXPECT_EQ(0x01, lData[1]);
  for (i = 0; i < 6; i++) {
    EXPECT_EQ(0u, lData[i + 2]);
  }
  for (i = 0; i < 32; i++) {
    ret = E2E_P11Check(E2E_CHECK_P11_P11_TST_BOTH, lData, 8);
    ASSERT_EQ(E_OK, ret);
    ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_BOTH, lData, 8);
    ASSERT_EQ(E_OK, ret);
  }
}

TEST(E2E, P11_CASE2) {
  Std_ReturnType ret;
  uint16_t i = 0;
  uint8_t lData[64];
  (void)memset(lData, 0, 8);
  E2E_Init(NULL);
  /* Table 6.86 */
  ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_NIBBLE, lData, 8);
  ASSERT_EQ(E_OK, ret);
  EXPECT_EQ(0x2A, lData[0]);
  EXPECT_EQ(0x10, lData[1]);
  for (i = 0; i < 6; i++) {
    EXPECT_EQ(0u, lData[i + 2]);
  }
  ret = E2E_P11Check(E2E_CHECK_P11_P11_TST_NIBBLE, lData, 8);
  ASSERT_EQ(E_OK, ret);

  ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_NIBBLE, lData, 8);
  ASSERT_EQ(E_OK, ret);
  EXPECT_EQ(0x77, lData[0]);
  EXPECT_EQ(0x11, lData[1]);
  for (i = 0; i < 6; i++) {
    EXPECT_EQ(0u, lData[i + 2]);
  }

  for (i = 0; i < 32; i++) {
    ret = E2E_P11Check(E2E_CHECK_P11_P11_TST_NIBBLE, lData, 8);
    ASSERT_EQ(E_OK, ret);
    std::generate(lData, &lData[8], []() {
      return rand();
    });
    ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_NIBBLE, lData, 8);
    ASSERT_EQ(E_OK, ret);
  }
}

TEST(E2E, P11_CASE3) {
  Std_ReturnType ret;
  uint16_t i = 0;
  uint8_t lData[64];
  (void)memset(lData, 0, 16);
  E2E_Init(NULL);
  /* Table 6.86 */
  ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_NIBBLE_OF64, lData, 16);
  ASSERT_EQ(E_OK, ret);
  EXPECT_EQ(0x7D, lData[8]);
  EXPECT_EQ(0x10, lData[9]);
  for (i = 0; i < 16; i++) {
    if ((i != 8) && (i != 9)) {
      EXPECT_EQ(0u, lData[i]);
    }
  }
  for (i = 0; i < 32; i++) {
    ret = E2E_P11Check(E2E_CHECK_P11_P11_TST_NIBBLE_OF64, lData, 16);
    ASSERT_EQ(E_OK, ret);
    std::generate(lData, &lData[16], []() {
      return rand();
    });
    ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_NIBBLE_OF64, lData, 16);
    ASSERT_EQ(E_OK, ret);
  }

  ret = E2E_P11Protect(E2E_PROTECT_P11_P11_TST_NIBBLE_OF64, lData, 16);
  ASSERT_EQ(E_OK, ret);
  ret = E2E_P11Check(E2E_CHECK_P11_P11_TST_NIBBLE_OF64, lData, 16);
  ASSERT_EQ(E2E_E_WRONG_SEQUENCE, ret);
  ret = E2E_P11Check(E2E_CHECK_P11_P11_TST_NIBBLE_OF64, lData, 16);
  ASSERT_EQ(E2E_E_REPEATED, ret);
}

TEST(E2E, P22_CASE1) {
  /* Table 6.99 */
  Std_ReturnType ret;
  uint16_t i, j = 0;
  uint8_t lData[64];
  /* Table 6.101 */
  const uint8_t crcG[] = {0x1b, 0x98, 0x31, 0x0d, 0x18, 0x9b, 0x65, 0x08,
                          0x1d, 0x9e, 0x37, 0x0b, 0x1e, 0x9d, 0xcd, 0x0e};
  E2E_Init(NULL);
  for (j = 0; j < sizeof(crcG); j++) {
    (void)memset(lData, 0, 8);
    ret = E2E_P22Protect(E2E_PROTECT_P22_P22_EX0, lData, 8);
    ASSERT_EQ(E_OK, ret);
    EXPECT_EQ(crcG[j], lData[0]);
    EXPECT_EQ(((j + 1) & 0xFu), lData[1]);
    for (i = 0; i < 6; i++) {
      EXPECT_EQ(0u, lData[i + 2]);
    }
    ret = E2E_P22Check(E2E_CHECK_P22_P22_EX0, lData, 8);
    ASSERT_EQ(E_OK, ret);
  }
  for (i = 0; i < 32; i++) {
    std::generate(lData, &lData[8], []() {
      return rand();
    });
    ret = E2E_P22Protect(E2E_PROTECT_P22_P22_EX0, lData, 8);
    ASSERT_EQ(E_OK, ret);
    ret = E2E_P22Check(E2E_CHECK_P22_P22_EX0, lData, 8);
    ASSERT_EQ(E_OK, ret);
  }
}

TEST(E2E, P22_CASE2) {
  /* Table 6.102 */
  Std_ReturnType ret;
  uint16_t i = 0;
  uint8_t lData[64];
  E2E_Init(NULL);

  (void)memset(lData, 0, 16);
  ret = E2E_P22Protect(E2E_PROTECT_P22_P22_EX1, lData, 16);
  ASSERT_EQ(E_OK, ret);
  for (i = 0; i < 16; i++) {
    if (8 == i) {
      EXPECT_EQ(0x14, lData[i]);
    } else if (9 == i) {
      EXPECT_EQ(0x1, lData[i]);
    } else {
      EXPECT_EQ(0u, lData[i]);
    }
  }
  ret = E2E_P22Check(E2E_CHECK_P22_P22_EX1, lData, 16);
  ASSERT_EQ(E_OK, ret);

  for (i = 0; i < 32; i++) {
    std::generate(lData, &lData[16], []() {
      return rand();
    });
    ret = E2E_P22Protect(E2E_PROTECT_P22_P22_EX1, lData, 16);
    ASSERT_EQ(E_OK, ret);
    ret = E2E_P22Check(E2E_CHECK_P22_P22_EX1, lData, 16);
    ASSERT_EQ(E_OK, ret);
  }
  ret = E2E_P22Protect(E2E_PROTECT_P22_P22_EX1, lData, 16);
  ASSERT_EQ(E_OK, ret);
  ret = E2E_P22Protect(E2E_PROTECT_P22_P22_EX1, lData, 16);
  ASSERT_EQ(E_OK, ret);
  ret = E2E_P22Check(E2E_CHECK_P22_P22_EX1, lData, 16);
  ASSERT_EQ(E2E_E_WRONG_SEQUENCE, ret);
  ret = E2E_P22Check(E2E_CHECK_P22_P22_EX1, lData, 16);
  ASSERT_EQ(E2E_E_REPEATED, ret);
}

TEST(E2E, P44_CASE1) {
  /* Table 6.42 */
  Std_ReturnType ret;
  uint32_t i = 0;
  uint8_t lData[64];
  E2E_Init(NULL);
  const uint8_t golden[] = {0x00, 0x10, 0x00, 0x00, 0x0a, 0x0b, 0x0c, 0x0d,
                            0x86, 0x2b, 0x05, 0x56, 0x00, 0x00, 0x00, 0x00};

  (void)memset(lData, 0, 16);
  ret = E2E_P44Protect(E2E_PROTECT_P44_P44_EX0, lData, 16);
  ASSERT_EQ(E_OK, ret);
  for (i = 0; i < 16; i++) {
    ASSERT_EQ(golden[i], lData[i]);
  }
  for (i = 0; i < 0xFFFF + 5; i++) {
    ret = E2E_P44Check(E2E_CHECK_P44_P44_EX0, lData, 16);
    ASSERT_EQ(E_OK, ret);
    std::generate(lData, &lData[16], []() {
      return rand();
    });
    ret = E2E_P44Protect(E2E_PROTECT_P44_P44_EX0, lData, 16);
    ASSERT_EQ(E_OK, ret);
  }
}

TEST(E2E, P05_CASE1) {
  /* Table 6.51 */
  Std_ReturnType ret;
  uint32_t i = 0;
  uint8_t lData[64];
  E2E_Init(NULL);

  (void)memset(lData, 0, 8);
  ret = E2E_P05Protect(E2E_PROTECT_P05_P05_EX0, lData, 8);
  ASSERT_EQ(E_OK, ret);
  ASSERT_EQ(0x1c, lData[1]);
  ASSERT_EQ(0xca, lData[0]);
  for (i = 0; i < 6; i++) {
    ASSERT_EQ(0, lData[i + 2]);
  }
  for (i = 0; i < 0x1FF; i++) {
    ret = E2E_P05Check(E2E_CHECK_P05_P05_EX0, lData, 8);
    ASSERT_EQ(E_OK, ret);
    std::generate(lData, &lData[16], []() {
      return rand();
    });
    ret = E2E_P05Protect(E2E_PROTECT_P05_P05_EX0, lData, 8);
    ASSERT_EQ(E_OK, ret);
  }
}

TEST(E2E, P05_CASE2) {
  /* Table 6.51 */
  Std_ReturnType ret;
  uint32_t i = 0;
  uint8_t lData[64];
  E2E_Init(NULL);

  (void)memset(lData, 0, 16);
  ret = E2E_P05Protect(E2E_PROTECT_P05_P05_EX1, lData, 16);
  ASSERT_EQ(E_OK, ret);
  ASSERT_EQ(0x91, lData[8]);
  ASSERT_EQ(0x28, lData[9]);
  for (i = 0; i < 16; i++) {
    if ((i != 8) && (i != 9)) {
      ASSERT_EQ(0, lData[i]);
    }
  }
  for (i = 0; i < 0x1FF; i++) {
    ret = E2E_P05Check(E2E_CHECK_P05_P05_EX1, lData, 16);
    ASSERT_EQ(E_OK, ret);
    std::generate(lData, &lData[16], []() {
      return rand();
    });
    ret = E2E_P05Protect(E2E_PROTECT_P05_P05_EX1, lData, 16);
    ASSERT_EQ(E_OK, ret);
  }
}
/* ================================ [ FUNCTIONS ] ============================================== */
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
