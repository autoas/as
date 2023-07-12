# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import json
from .helper import *

__all__ = ['Gen']


def Gen_CanTp(cfg, dir):
    H = open('%s/CanTp_Cfg.h' % (dir), 'w')
    GenHeader(H)
    H.write('#ifndef CANTP_CFG_H\n')
    H.write('#define CANTP_CFG_H\n')
    H.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    H.write('#ifndef CANIF_CANTP_BASEID\n')
    H.write('#define CANIF_CANTP_BASEID 0\n')
    H.write('#endif\n\n')
    for i, chl in enumerate(cfg['channels']):
        H.write('#define CANTP_%s_RX %s\n' % (chl['name'], i))
        H.write('#define CANTP_%s_TX %s\n' % (chl['name'], i))
        H.write('#ifndef USE_CANIF\n')
        H.write('#define CANIF_%s_TX (CANIF_CANTP_BASEID+%s)\n' %
                (chl['name'], i))
        H.write('#endif\n')
    H.write('\n')
    if 'zero_cost' in cfg:
        H.write('#define PDUR_%s_CANTP_ZERO_COST\n\n' % (cfg['zero_cost'].upper()))
    H.write('#define CANTP_MAIN_FUNCTION_PERIOD 10\n')
    H.write('#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(x)  \\\n')
    H.write('  ((x + CANTP_MAIN_FUNCTION_PERIOD - 1) / CANTP_MAIN_FUNCTION_PERIOD)\n')
    H.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    H.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    H.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    H.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    H.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    H.write('#endif /* CANTP_CFG_H */\n')
    H.close()

    C = open('%s/CanTp_Cfg.c' % (dir), 'w')
    GenHeader(C)
    C.write(
        '/* ================================ [ INCLUDES  ] ============================================== */\n')
    C.write('#ifdef USE_CANIF\n')
    C.write('#include "CanIf_Cfg.h"\n')
    C.write('#endif\n')
    C.write('#include "CanTp_Cfg.h"\n')
    C.write('#include "CanTp.h"\n')
    C.write('#include "CanTp_Types.h"\n')
    C.write(
        '/* ================================ [ MACROS    ] ============================================== */\n')
    C.write('#ifndef CANTP_CFG_N_As\n')
    C.write('#define CANTP_CFG_N_As 25\n')
    C.write('#endif\n')
    C.write('#ifndef CANTP_CFG_N_Bs\n')
    C.write('#define CANTP_CFG_N_Bs 1000\n')
    C.write('#endif\n')
    C.write('#ifndef CANTP_CFG_N_Cr\n')
    C.write('#define CANTP_CFG_N_Cr 1000\n')
    C.write('#endif\n\n')
    C.write('#ifndef CANTP_CFG_STMIN\n')
    C.write('#define CANTP_CFG_STMIN 0\n')
    C.write('#endif\n\n')
    C.write('#ifndef CANTP_CFG_BS\n')
    C.write('#define CANTP_CFG_BS 8\n')
    C.write('#endif\n\n')
    C.write('#ifndef CANTP_CFG_RX_WFT_MAX\n')
    C.write('#define CANTP_CFG_RX_WFT_MAX 8\n')
    C.write('#endif\n\n')
    C.write('#ifndef CANTP_LL_DL\n')
    C.write('#define CANTP_LL_DL 8\n')
    C.write('#endif\n\n')
    C.write('#ifndef CANTP_CFG_PADDING\n')
    C.write('#define CANTP_CFG_PADDING 0x55\n')
    C.write('#endif\n\n')
    C.write('#if defined(_WIN32) || defined(linux)\n')
    C.write('#define L_CONST\n')
    C.write('#else\n')
    C.write('#define L_CONST const\n')
    C.write('#endif\n')
    C.write(
        '/* ================================ [ TYPES     ] ============================================== */\n')
    C.write(
        '/* ================================ [ DECLARES  ] ============================================== */\n')
    C.write(
        '/* ================================ [ DATAS     ] ============================================== */\n')
    for chl in cfg['channels']:
        C.write('static uint8_t u8%sData[CANTP_LL_DL];\n' % (chl['name']))
    C.write('static L_CONST CanTp_ChannelConfigType CanTpChannelConfigs[] = {\n')
    for i, chl in enumerate(cfg['channels']):
        C.write('  {\n')
        C.write('    /* %s */\n' % (chl['name']))
        C.write('    CANTP_STANDARD,\n')
        C.write('    CANIF_%s_TX,\n' % (chl['name']))
        C.write('    %s /* PduR_RxPduId */,\n' % (i))
        C.write('    %s /* PduR_TxPduId */,\n' % (i))
        C.write('    CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_As),\n')
        C.write('    CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Bs),\n')
        C.write('    CANTP_CONVERT_MS_TO_MAIN_CYCLES(CANTP_CFG_N_Cr),\n')
        C.write('    CANTP_CFG_STMIN,\n')
        C.write('    CANTP_CFG_BS,\n')
        C.write('    0 /* N_TA */,\n')
        C.write('    CANTP_CFG_RX_WFT_MAX,\n')
        C.write('    CANTP_LL_DL,\n')
        C.write('    CANTP_CFG_PADDING,\n')
        C.write('    u8%sData,\n' % (chl['name']))
        C.write('  },\n')
    C.write('};\n\n')
    C.write(
        'static CanTp_ChannelContextType CanTpChannelContexts[ARRAY_SIZE(CanTpChannelConfigs)];\n\n')
    C.write('const CanTp_ConfigType CanTp_Config = {\n')
    C.write('  CanTpChannelConfigs,\n')
    C.write('  CanTpChannelContexts,\n')
    C.write('  ARRAY_SIZE(CanTpChannelConfigs),\n')
    C.write('};\n')
    C.write(
        '/* ================================ [ LOCALS    ] ============================================== */\n')
    C.write('#if defined(_WIN32) || defined(linux)\n')
    C.write('#include <stdlib.h>\n')
    C.write('static void __attribute__((constructor)) _ll_dl_init(void) {\n')
    C.write('  int i;\n')
    C.write('  char *llDlStr = getenv("LL_DL");\n')
    C.write('  if (llDlStr != NULL) {\n')
    C.write('    for( i = 0; i < ARRAY_SIZE(CanTpChannelConfigs); i++ ) {\n')
    C.write('      CanTpChannelConfigs[i].LL_DL = atoi(llDlStr);\n')
    C.write('    }\n')
    C.write('  }\n')
    C.write('}\n')
    C.write('#endif\n')
    C.write(
        '/* ================================ [ FUNCTIONS ] ============================================== */\n')
    C.close()


def Gen(cfg):
    dir = os.path.join(os.path.dirname(cfg), 'GEN')
    os.makedirs(dir, exist_ok=True)
    with open(cfg) as f:
        cfg = json.load(f)
    Gen_CanTp(cfg, dir)
