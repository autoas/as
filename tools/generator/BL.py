# SSAS - Simple Smart Automotive Software
# Copyright (C) 2025 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ["Gen"]


def Gen_BL(cfg, dir):
    general = cfg.get("general", {})
    memory = cfg.get("memory", {})

    BL_USE_AB = general.get("BL_USE_AB", False)
    BL_USE_META = general.get("BL_USE_META", False)
    ROD_OFFSET = toNum(general.get("ROD_OFFSET", 0x800))

    fls_drv = memory.get("FlashDriver", {})
    fls_low = toNum(fls_drv.get("address", 0))
    fls_size = toNum(fls_drv.get("size", 2048))
    fls_high = fls_low + fls_size

    flash = memory.get("Flash", [{}])
    flash_a = memory.get("FlashA", flash)
    flash_b = memory.get("FlashB", [{}])
    fee = memory.get("Fee", [])

    app_a_base = toNum(flash_a[0].get("address", 0))
    app_a_end = toNum(flash_a[-1].get("address", 0)) + toNum(flash_a[-1].get("size", 0))
    app_a_sector_size = toNum(flash_a[-1].get("sectorSize", 512))
    app_a_finger_print_size = toNum(general.get("FINGER_PRINT_SIZE", app_a_sector_size // 4))
    app_a_meta_size = toNum(general.get("META_SIZE", app_a_sector_size // 4))
    app_a_info_size = toNum(general.get("APP_SECTION_INFO_SIZE", app_a_sector_size // 4))
    app_a_valid_flag_size = toNum(general.get("APP_VALID_FLAG_SIZE", app_a_sector_size // 4))
    app_a_meta_backup_size = app_a_finger_print_size + app_a_meta_size + app_a_info_size + app_a_valid_flag_size

    app_b_base = toNum(flash_b[0].get("address", 0))
    app_b_end = toNum(flash_b[-1].get("address", 0)) + toNum(flash_b[-1].get("size", 0))
    app_b_sector_size = toNum(flash_b[-1].get("sectorSize", 512))
    app_b_finger_print_size = toNum(general.get("FINGER_PRINT_SIZE", app_b_sector_size // 4))
    app_b_meta_size = toNum(general.get("META_SIZE", app_b_sector_size // 4))
    app_b_info_size = toNum(general.get("APP_SECTION_INFO_SIZE", app_b_sector_size // 4))
    app_b_valid_flag_size = toNum(general.get("APP_VALID_FLAG_SIZE", app_b_sector_size // 4))
    app_b_meta_backup_size = app_b_finger_print_size + app_b_meta_size + app_b_info_size + app_b_valid_flag_size
    # ===== Generate BL_Cfg.h =====
    H = open(os.path.join(dir, "BL_Cfg.h"), "w")
    GenHeader(H)
    H.write("#ifndef BL_CFG_H\n")
    H.write("#define BL_CFG_H\n\n")
    H.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    H.write("/* ================================ [ MACROS    ] ============================================== */\n")
    H.write("%s #define BL_USE_BL_IN_APP\n" % ("" if general.get("BL_USE_BL_IN_APP", False) else "//"))
    H.write("%s #define BL_USE_AB\n" % ("" if BL_USE_AB else "//"))
    H.write("%s #define BL_USE_AB_UPDATE_ACTIVE\n" % ("" if general.get("BL_USE_AB_UPDATE_ACTIVE", False) else "//"))
    H.write("%s #define BL_USE_META\n" % ("" if general.get("BL_USE_META", False) else "//"))
    H.write("%s #define BL_USE_CRC_16\n" % ("" if general.get("BL_USE_CRC_16", False) else "//"))
    H.write("%s #define BL_USE_CRC_32\n" % ("" if general.get("BL_USE_CRC_32", False) else "//"))
    H.write(
        "%s #define BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER\n"
        % ("" if general.get("BL_USE_AB_ACTIVE_BASED_ON_META_ROLLING_COUNTER", False) else "//")
    )
    H.write(
        "%s #define FL_USE_WRITE_WINDOW_BUFFER\n" % ("" if general.get("FL_USE_WRITE_WINDOW_BUFFER", False) else "//")
    )
    H.write("%s #define BL_USE_APP_INFO\n" % ("" if general.get("BL_USE_APP_INFO", False) else "//"))
    H.write("%s #define BL_USE_APP_INFO_V2\n" % ("" if general.get("BL_USE_APP_INFO_V2", False) else "//"))
    H.write(
        "%s #define DCM_DISABLE_PROGRAM_SESSION_PROTECTION\n"
        % ("" if general.get("DCM_DISABLE_PROGRAM_SESSION_PROTECTION", False) else "//")
    )
    H.write("%s #define BL_USE_BUILTIN_FLS_READ\n" % ("" if general.get("BL_USE_BUILTIN_FLS_READ", False) else "//"))
    H.write("%s #define BL_USE_FLS_READ\n\n" % ("" if general.get("BL_USE_FLS_READ", False) else "//"))

    H.write("#define FLASH_DRIVER_START_ADDRESS 0x%08X\n\n" % (fls_low))

    if "CAN_DIAG_P2P_RX" in general:
        H.write(f"#define CAN_DIAG_P2P_RX 0x{toNum(general['CAN_DIAG_P2P_RX']):X}\n")
    if "CAN_DIAG_P2P_TX" in general:
        H.write(f"#define CAN_DIAG_P2P_TX 0x{toNum(general['CAN_DIAG_P2P_TX']):X}\n")
    if "CAN_DIAG_P2A_RX" in general:
        H.write(f"#define CAN_DIAG_P2A_RX 0x{toNum(general['CAN_DIAG_P2A_RX']):X}\n")

    H.write(f"\n#define FLASH_ERASE_SIZE 0x{toNum(general.get('FLASH_ERASE_SIZE', 512)):X}\n")
    H.write(f"#define FLASH_WRITE_SIZE 0x{toNum(general.get('FLASH_WRITE_SIZE', 8)):X}\n")
    H.write(f"#define FLASH_READ_SIZE 0x{toNum(general.get('FLASH_READ_SIZE', 1)):X}\n")
    H.write(f"#define FL_ERASE_PER_CYCLE {general.get('FL_ERASE_PER_CYCLE', 1)}\n")
    H.write(f"#define FL_WRITE_PER_CYCLE {general.get('FL_WRITE_PER_CYCLE', '(4096 / FLASH_WRITE_SIZE)')}\n")
    H.write(f"#define FL_READ_PER_CYCLE {general.get('FL_READ_PER_CYCLE', '(4096 / FLASH_READ_SIZE)')}\n")
    H.write(f"#define FL_WRITE_WINDOW_SIZE {general.get('FL_WRITE_WINDOW_SIZE', '(8 * FLASH_WRITE_SIZE)')}\n")
    H.write(
        f"#define BL_APP_VALID_SAMPLE_SIZE {general.get('BL_APP_VALID_SAMPLE_SIZE', 'FLASH_ALIGNED_READ_SIZE(32)')}\n"
    )
    H.write(f"#define BL_APP_VALID_SAMPLE_STRIDE {general.get('BL_APP_VALID_SAMPLE_STRIDE', '1024')}\n")
    H.write(f"#define BL_FLS_READ_SIZE {general.get('BL_FLS_READ_SIZE', '256')}\n")

    FL_ERASE_RCRRP_CYCLE = general.get("FL_ERASE_RCRRP_CYCLE", 0)
    if FL_ERASE_RCRRP_CYCLE > 0:
        H.write(f"#define FL_ERASE_RCRRP_CYCLE {FL_ERASE_RCRRP_CYCLE}\n")
    FL_WRITE_RCRRP_CYCLE = general.get("FL_WRITE_RCRRP_CYCLE", 0)
    if FL_WRITE_RCRRP_CYCLE > 0:
        H.write(f"#define FL_WRITE_RCRRP_CYCLE {FL_WRITE_RCRRP_CYCLE}\n")
    H.write("\n")
    H.write("/* ================================ [ TYPES     ] ============================================== */\n")
    H.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    H.write("/* ================================ [ DATAS     ] ============================================== */\n")
    H.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    H.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    H.write("#endif /* BL_CFG_H */\n")
    H.close()

    # ===== Generate BL_Cfg.c =====
    C = open(os.path.join(dir, "BL_Cfg.c"), "w")
    GenHeader(C)
    C.write("/* ================================ [ INCLUDES  ] ============================================== */\n")
    C.write('#include "bl.h"\n')
    C.write('#include "RoD.h"\n\n')
    C.write("#ifdef _WIN32\n")
    C.write("#include <stdlib.h>\n")
    C.write("#endif\n")
    C.write("/* ================================ [ MACROS    ] ============================================== */\n")
    blAppMemoryHighA = app_a_end - app_a_meta_backup_size
    blFingerPrintAddrA = (
        app_a_end
        - app_a_meta_backup_size
        - app_a_finger_print_size
        - app_a_meta_size
        - app_a_info_size
        - app_a_valid_flag_size
    )
    blAppMetaAddrA = blFingerPrintAddrA + app_a_finger_print_size
    blAppInfoAddrA = blAppMetaAddrA + app_a_meta_size
    blAppValidFlagAddrA = blAppInfoAddrA + app_a_info_size
    blAppMetaBackupAddrA = blAppMemoryHighA
    blAppMemoryHighB = app_b_end - app_b_meta_backup_size
    blFingerPrintAddrB = (
        app_b_end
        - app_b_meta_backup_size
        - app_b_finger_print_size
        - app_b_meta_size
        - app_b_info_size
        - app_b_valid_flag_size
    )
    blAppMetaAddrB = blFingerPrintAddrB + app_b_finger_print_size
    blAppInfoAddrB = blAppMetaAddrB + app_b_meta_size
    blAppValidFlagAddrB = blAppInfoAddrB + app_b_info_size
    blAppMetaBackupAddrB = blAppMemoryHighB
    C.write("/*     Flash Layout\n")
    C.write("*  +---------------------+\n")
    C.write("*  |    Boot Loader      |                           FINGER + INFO\n")
    C.write("*  +---------------------+ <-- 0x%08X         +--------------------------+ <-- 0\n" % (app_a_base))
    C.write(
        "*  |                     |                        |     Finger Print         | %s\n"
        % (app_a_finger_print_size)
    )
    C.write("*  |                     |                        |         Meta             | %s\n" % (app_a_meta_size))
    C.write(
        "*  |    APP A            |                        +--------------------------+ <-- %s\n"
        % (app_a_finger_print_size + app_a_meta_size)
    )
    C.write("*  |                     |                        |      INFO                | %s\n" % (app_a_info_size))
    C.write(
        "*  +---------------------+ <-- 0x%08X         |    APP VALID FLAG        | %s\n"
        % (blFingerPrintAddrA, app_a_valid_flag_size)
    )
    C.write(
        "*  | APP A FINGER + INFO |                        +--------------------------+ <-- %s\n"
        % (app_a_finger_print_size + app_a_meta_size + app_a_info_size + app_a_valid_flag_size)
    )
    C.write("*  +---------------------+ <-- 0x%08X\n" % (blAppMemoryHighA))
    C.write("*  | APP A META BACKUP   |\n")
    C.write("*  +---------------------+ <-- 0x%08X\n" % (app_a_end))
    if BL_USE_AB:
        C.write("*  +---------------------+ <-- 0x%08X\n" % (app_b_base))
        C.write("*  |                     |\n")
        C.write("*  |                     |\n")
        C.write("*  |    APP B            |\n")
        C.write("*  |                     |\n")
        C.write("*  +---------------------+ <-- 0x%08X\n" % (blFingerPrintAddrB))
        C.write("*  | APP B FINGER + INFO |\n")
        C.write("*  +---------------------+ <-- 0x%08X\n" % (blAppMemoryHighB))
        C.write("*  | APP B META BACKUP   |\n")
        C.write("*  +---------------------+ <-- 0x%08X\n" % (app_b_end))
    C.write("*/\n\n")

    C.write("#ifndef BL_USE_AB\n")
    C.write("#define blAppMemoryLowA blAppMemoryLow\n")
    C.write("#define blAppMemoryHighA blAppMemoryHigh\n")
    C.write("#define blFingerPrintAddrA blFingerPrintAddr\n")
    C.write("#define blAppInfoAddrA blAppInfoAddr\n")
    C.write("#define blAppValidFlagAddrA blAppValidFlagAddr\n")
    C.write("#define blMemoryListA blMemoryList\n")
    C.write("#define blMemoryListASize blMemoryListSize\n")
    C.write("#ifdef BL_USE_META\n")
    C.write("#define blAppMetaAddrA blAppMetaAddr\n")
    C.write("#define blAppMetaBackupAddrA blAppMetaBackupAddr\n")
    C.write("#endif\n")
    C.write("#endif\n\n")

    C.write("#ifdef _WIN32\n")
    C.write("#define L_CONST\n")
    C.write("#else\n")
    C.write("#define L_CONST const\n")
    C.write("#endif\n\n")
    C.write("/* ================================ [ TYPES     ] ============================================== */\n")
    C.write("/* ================================ [ DECLARES  ] ============================================== */\n")
    C.write("/* ================================ [ DATAS     ] ============================================== */\n")
    C.write("#ifdef _WIN32\n")
    C.write("static const RoD_ConfigType RoD_ConfigDummy = {\n")
    C.write("  NULL, 0, 0, 0, 0,\n")
    C.write("};\n")
    C.write("#endif\n")
    C.write("const BL_MemoryInfoType blMemoryListA[] = {\n")
    C.write(f" /* FLASH DRIVER */ {{0x{fls_low:08X}, 0x{fls_high:08X}, BL_FLSDRV_IDENTIFIER}},\n")
    for idx, bank in enumerate(flash_a):
        bank_addr = toNum(bank.get("address", 0))
        bank_size = toNum(bank.get("size", 0))
        if idx == len(flash_a) - 1:
            bank_size -= app_a_meta_backup_size
        C.write(f" /* APPLICATION A */ {{0x{bank_addr:08X}, 0x{bank_addr + bank_size:08X}, BL_FLASH_IDENTIFIER}},\n")
    for bank in fee:
        bank_addr = toNum(bank.get("address", 0))
        bank_size = toNum(bank.get("size", 0))
        C.write(f" /* FLASH FEE BANK */ {{0x{bank_addr:08X}, 0x{bank_addr + bank_size:08X}, BL_FEE_IDENTIFIER}},\n")
    C.write("};\n")
    C.write("const uint32_t blMemoryListASize = sizeof(blMemoryListA)/sizeof(blMemoryListA[0]);\n\n")

    if general.get("BL_USE_AB", False):
        C.write("\n")
        C.write("const BL_MemoryInfoType blMemoryListB[] = {\n")
        C.write(f" /* FLASH DRIVER */ {{0x{fls_low:08X}, 0x{fls_high:08X}, BL_FLSDRV_IDENTIFIER}},\n")
        for bank in flash_b:
            bank_addr = toNum(bank.get("address", 0))
            bank_size = toNum(bank.get("size", 0))
            if idx == len(flash_b) - 1:
                bank_size -= app_a_meta_backup_size
            C.write(
                f" /* APPLICATION B */ {{0x{bank_addr:08X}, 0x{bank_addr + bank_size:08X}, BL_FLASH_IDENTIFIER}},\n"
            )
        for bank in fee:
            bank_addr = toNum(bank.get("address", 0))
            bank_size = toNum(bank.get("size", 0))
            C.write(f" /* FLASH FEE BANK */ {{0x{bank_addr:08X}, 0x{bank_addr + bank_size:08X}, BL_FEE_IDENTIFIER}},\n")
        C.write("};\n")
        C.write("const uint32_t blMemoryListBSize = sizeof(blMemoryListB)/sizeof(blMemoryListB[0]);\n\n")

    C.write(f"const uint32_t blFlsDriverMemoryLow = 0x{fls_low:08X};\n")
    C.write(f"L_CONST uint32_t blFlsDriverMemoryHigh = 0x{fls_high:08X};\n\n")

    C.write(f"const uint32_t blAppMemoryLowA = 0x{app_a_base:08X};\n")
    C.write(f"const uint32_t blAppMemoryHighA = 0x{blAppMemoryHighA:08X};\n")
    C.write("#ifdef _WIN32\n")
    C.write(f"const RoD_ConfigType *const RoD_AppConfigA = &RoD_ConfigDummy;\n")
    C.write("#else\n")
    C.write(f"const RoD_ConfigType *const RoD_AppConfigA = (const RoD_ConfigType *)(0x{app_a_base+ROD_OFFSET:08x});\n")
    C.write("#endif\n")
    C.write(f"const uint32_t blFingerPrintAddrA = 0x{blFingerPrintAddrA:08X};\n")
    C.write(f"const uint32_t blAppInfoAddrA = 0x{blAppInfoAddrA:08X};\n")
    C.write(f"const uint32_t blAppValidFlagAddrA = 0x{blAppValidFlagAddrA:08X};\n")
    if BL_USE_META:
        C.write(f"const uint32_t blAppMetaAddrA = 0x{blAppMetaAddrA:08X};\n")
        C.write(f"const uint32_t blAppMetaBackupAddrA = 0x{blAppMetaBackupAddrA:08X};\n")

    if general.get("BL_USE_AB", False):
        C.write(f"const uint32_t blAppMemoryLowB = 0x{app_b_base:08X};\n")
        C.write(f"const uint32_t blAppMemoryHighB = 0x{blAppMemoryHighB:08X};\n")
        C.write("#ifdef _WIN32\n")
        C.write(f"const RoD_ConfigType *const RoD_AppConfigB = &RoD_ConfigDummy;\n")
        C.write("#else\n")
        C.write(
            f"const RoD_ConfigType *const RoD_AppConfigB = (const RoD_ConfigType *)(0x{app_b_base+ROD_OFFSET:08x});\n"
        )
        C.write("#endif\n")
        C.write(f"const uint32_t blFingerPrintAddrB = 0x{blFingerPrintAddrB:08X};\n")
        C.write(f"const uint32_t blAppInfoAddrB = 0x{blAppInfoAddrB:08X};\n")
        C.write(f"const uint32_t blAppValidFlagAddrB = 0x{blAppValidFlagAddrB:08X};\n")
        if BL_USE_META:
            C.write(f"const uint32_t blAppMetaAddrB = 0x{blAppMetaAddrB:08X};\n")
            C.write(f"const uint32_t blAppMetaBackupAddrB = 0x{blAppMetaBackupAddrB:08X};\n")
    C.write("/* ================================ [ LOCALS    ] ============================================== */\n")
    C.write("#ifdef _WIN32\n")
    C.write("static void __attribute__((constructor)) _blcfg_start(void) {\n")
    C.write('  char *hiStr = getenv("BL_FLSDRV_MEMORY_HIGH");\n')
    C.write("  if (hiStr != NULL) {\n")
    C.write("    blFlsDriverMemoryHigh = strtoul(hiStr, NULL, 10);\n")
    C.write("  }\n")
    C.write("}\n")
    C.write("#endif\n")
    C.write("/* ================================ [ FUNCTIONS ] ============================================== */\n")
    C.write("\n")
    C.close()


def Gen(cfg_path):
    dir = os.path.join(os.path.dirname(cfg_path), "GEN")
    os.makedirs(dir, exist_ok=True)
    with open(cfg_path) as f:
        cfg = json.load(f)
    Gen_BL(cfg, dir)
    return [os.path.join(dir, "BL_Cfg.c")]
