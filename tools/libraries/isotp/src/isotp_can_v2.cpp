/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "isotp.h"
#include "isotp_types.hpp"
#include "canlib.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include "Std_Debug.h"
/* ================================ [ MACROS    ] ============================================== */
#ifndef CANTP_MAX_CHANNELS
#define CANTP_MAX_CHANNELS 32
#endif

#define ISOTP_DEFAULT_TIMEOUT 5000

/* see ISO 15765-2 2004 */
#define N_PCI_MASK 0xF0u
#define N_PCI_SF 0x00u
#define N_PCI_FF 0x10u
#define N_PCI_CF 0x20u
#define N_PCI_FC 0x30u
#define N_PCI_SF_DL 0x0Fu
/* Flow Control Status Mask */
#define N_PCI_FS 0x0Fu
/* Flow Control Status */
#define N_PCI_CTS 0x00u
#define N_PCI_WT 0x01u
#define N_PCI_OVFLW 0x02u

#define N_PCI_SN 0x0Fu

#define PDU_LENGTH_MAX (0xFFFFUL)
/* ================================ [ TYPES     ] ============================================== */
class IsotpCanV2 {
public:
  IsotpCanV2(IsoTp_Type *isotp, int busid) : m_isotp(isotp), m_busid(busid) {
    m_LinkLayerDataLength = m_isotp->Params.LinkLayerDataLength;
    m_N_TA = m_isotp->Params.N_TA;
  }

  ~IsotpCanV2() {
    if (m_busid >= 0) {
      CanL_Close(m_busid);
    }
  }

  int transmit(const uint8_t *txBuffer, size_t txSize) {
    int r = 0;
    size_t sfMaxLen = getSFMaxLen();

    if (sfMaxLen >= txSize) {
      r = sendSF(txBuffer, txSize);
    } else {
      m_txBuffer.resize(txSize);
      m_offset = 0;
      memcpy(&m_txBuffer[0], txBuffer, txSize);
      r = sendFF();
      m_waitFirstFC = true;
      m_SN = 1;
      r = waitFC();
      while ((0 == r) && (m_offset < txSize)) {
        r = sendCF();
      }
    }

    return r;
  }

  int receive(uint8_t *rxBuffer, size_t rxSize) {
    int r = 0;
    m_offset = 0;
    while (0 == r) {
      r = waitFrame();
      if (r > 0) {
        memcpy(&rxBuffer[0], &m_rxBuffer[0], r);
      }
    }
    return r;
  }

private:
  int sendFrame(const uint8_t *data, size_t dlc) {
    ASLOG(ISOTP,
          ("[%d]-TX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", m_isotp->Channel, dlc,
           data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));

    bool ok = CanL_Write(m_busid, m_isotp->Params.U.CAN.TxCanId, dlc, &data[0]);
    return ok ? 0 : -EIO;
  }

  int sendSF(const uint8_t *txBuffer, size_t txSize) {
    std::vector<uint8_t> data;
    size_t pos = 0;
    data.resize(m_LinkLayerDataLength, m_padding);

    if (isExtendedAddressing()) {
      data[pos] = (uint8_t)m_N_TA;
      pos++;
    }

    if ((m_LinkLayerDataLength > 8u) && ((7u - pos) < txSize)) {
      data[pos] = N_PCI_SF;
      pos++;
      data[pos] = (uint8_t)txSize;
      pos++;
    } else {
      data[pos] = N_PCI_SF | (uint8_t)(txSize & 0x7u);
      pos++;
    }

    memcpy(&data[pos], txBuffer, txSize);
    pos += txSize;

    size_t dl = getDL(pos);

    return sendFrame(&data[0], dl);
  }

  int sendFF() {
    size_t TpSduLength = m_txBuffer.size();
    std::vector<uint8_t> data;
    size_t pos = 0;
    data.resize(m_LinkLayerDataLength, m_padding);

    if (isExtendedAddressing()) {
      data[pos] = (uint8_t)m_N_TA;
      pos++;
    }

    if ((m_LinkLayerDataLength > 8u) && (TpSduLength > 4095u)) {
      data[pos] = N_PCI_FF;
      pos++;
      data[pos] = 0u;
      pos++;
      data[pos] = (uint8_t)(TpSduLength >> 24) & 0xFFu;
      pos++;
      data[pos] = (uint8_t)(TpSduLength >> 16) & 0xFFu;
      pos++;
      data[pos] = (uint8_t)(TpSduLength >> 8) & 0xFFu;
      pos++;
      data[pos] = (uint8_t)TpSduLength & 0xFFu;
      pos++;
    } else {
      data[pos] = N_PCI_FF | (uint8_t)((TpSduLength >> 8) & 0x0Fu);
      pos++;
      data[pos] = (uint8_t)TpSduLength & 0xFFu;
      pos++;
    }
    size_t copyLen = m_LinkLayerDataLength - pos;
    memcpy(&data[pos], &m_txBuffer[0], copyLen);
    pos += copyLen;
    m_offset = copyLen;
    return sendFrame(&data[0], pos);
  }

  int handleFC(uint8_t pci, const uint8_t *data, uint8_t length) {
    int r = 0;
    switch ((pci & N_PCI_FS)) {
    case N_PCI_CTS:
      if (length < 2u) {
        ASLOG(ISOTPE, ("[%d]FC invalid DLC.\n", m_isotp->Channel));
      } else {
        if (true == m_waitFirstFC) {
          m_waitFirstFC = false;
          m_cfgBS = data[0];
          m_STmin = data[1];
        }
        m_BS = m_cfgBS;
      }
      break;
    case N_PCI_WT:
      if (m_WftCounter < 0xFFu) {
        m_WftCounter++;
      }
      if (m_WftCounter > m_RxWftMax) {
        r = -ETXTBSY;
      } else {
        r = 0; /* wait more */
      }
      break;
    case N_PCI_OVFLW:
      ASLOG(ISOTPE, ("[%d]FC Overflow.\n", m_isotp->Channel));
      r = -EOVERFLOW;
      break;
    default:
      /* do nothing */
      ASLOG(ISOTPE, ("[%d]Invalid Flow Control Status.\n", m_isotp->Channel));
      r = -EINVAL;
      break;
    }
    return r;
  }

  int txIndication(const CanL_FrameType &frame) {
    int r = 0;
    uint8_t pci;
    const uint8_t *data;
    uint8_t length;

    if (isExtendedAddressing()) {
      if (m_N_TA != frame.Data[0]) {
        ASLOG(ISOTPI, ("[%d]not for me, ignore\n", m_isotp->Channel));
        r = 0;
      } else if (frame.Dlc > 2u) {
        pci = frame.Data[1];
        data = &frame.Data[2];
        length = frame.Dlc - 2u;
      } else {
        ASLOG(ISOTPE, ("[%d]invalid DLC\n", m_isotp->Channel));
        r = -EINVAL;
      }
    } else {
      pci = frame.Data[0];
      data = &frame.Data[1];
      length = frame.Dlc - 1u;
    }

    switch (pci & N_PCI_MASK) {
    case N_PCI_FC:
      r = handleFC(pci, data, length);
      break;
    default:
      ASLOG(ISOTPE, ("[%d]-RX with invalid PCI 0x%02X\n", m_isotp->Channel, pci));
      r = -EINVAL;
      break;
    }

    return r;
  }

  int waitFC() {
    int r = 0;
    uint32_t canid = m_isotp->Params.U.CAN.RxCanId;
    bool ok = CanL_WaitV2(m_busid, canid, m_timeoutMs);
    if (true == ok) {
      CanL_FrameType frame;
      frame.CanId = canid;
      ok = CanL_ReadV2(m_busid, &frame);
      if (true == ok) {
        ASLOG(ISOTP, ("[%d]-RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
                      m_isotp->Channel, frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2],
                      frame.Data[3], frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7]));
        r = txIndication(frame);
      } else {
        r = -EIO;
      }
    } else {
      r = -ETIMEDOUT;
    }
    return r;
  }

  int sendCF() {
    int r = 0;
    std::vector<uint8_t> data;
    size_t pos = 0;
    data.resize(m_LinkLayerDataLength, m_padding);

    if (isExtendedAddressing()) {
      data[pos] = (uint8_t)m_N_TA;
      pos++;
    }

    data[pos] = N_PCI_CF | m_SN;
    pos++;
    m_SN++;
    if (m_SN > 15u) {
      m_SN = 0u;
    }

    size_t bufferSize = m_LinkLayerDataLength - pos;
    if (bufferSize > (m_txBuffer.size() - m_offset)) {
      bufferSize = m_txBuffer.size() - m_offset;
    }

    memcpy(&data[pos], &m_txBuffer[m_offset], bufferSize);
    pos += bufferSize;
    m_offset += bufferSize;
    size_t dl = getDL(pos);
    r = sendFrame(&data[0], dl);
    if (0 == r) {
      if (m_offset < m_txBuffer.size()) {
        uint8_t stMin = m_STmin;
        if (m_BS > 0) {
          m_BS--;
          if (0 == m_BS) {
            r = waitFC();
            if (0 == r) {
              m_BS = m_cfgBS;
              stMin = 0;
            }
          }
        }
        if (stMin > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(stMin));
        }
      }
    } else {
      r = -EIO;
    }
    return r;
  }

  int handleSF(uint8_t pci, const uint8_t *data, uint8_t length) {
    int r = 0;
    size_t sfMaxLen = getSFMaxLen();
    size_t SduLength = pci & N_PCI_SF_DL;
    const uint8_t *SduDataPtr;
    uint8_t dataLen = length;

    if ((0u == SduLength) && (m_LinkLayerDataLength > 8u)) {
      SduLength = data[0];
      SduDataPtr = &data[1];
      dataLen -= 1u;
    } else {
      SduDataPtr = data;
    }

    if (0 != m_offset) {
      ASLOG(ISOTPE, ("[%d] SF received in wrong state, abort!\n", m_isotp->Channel));
      r = -EFAULT;
    } else if ((SduLength <= sfMaxLen) && (SduLength > 0u) && (SduLength <= dataLen)) {
      m_rxBuffer.resize(SduLength);
      memcpy(&m_rxBuffer[0], SduDataPtr, SduLength);
      r = (int)SduLength;
    } else {
      ASLOG(ISOTPE, ("[%d]SF received with invalid len: %d %d %d!\n", m_isotp->Channel, (int)length,
                     (int)sfMaxLen, (int)dataLen));
      r = -EINVAL;
    }

    return r;
  }

  int sendFC() {
    std::vector<uint8_t> data;
    size_t pos = 0;
    data.resize(m_LinkLayerDataLength, m_padding);

    if (isExtendedAddressing()) {
      data[pos] = (uint8_t)m_N_TA;
      pos++;
    }

    data[pos] = N_PCI_FC | N_PCI_CTS;
    pos++;
    data[pos] = m_isotp->Params.U.CAN.BlockSize;
    pos++;
    data[pos] = m_isotp->Params.U.CAN.STmin;
    pos++;

    size_t dl = getDL(pos);
    ASLOG(ISOTP,
          ("[%d]+TX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n", m_isotp->Channel, dl,
           data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]));
    bool ok = CanL_Write(m_busid, m_isotp->Params.U.CAN.TxCanId, dl, &data[0]);
    return ok ? 0 : -EIO;
  }

  int handleFF(uint8_t pci, const uint8_t *data, uint8_t length) {
    int r = 0;
    size_t ffLen;
    size_t sfMaxLen;
    size_t TpSduLength;
    const uint8_t *SduDataPtr;
    size_t SduLength;

    TpSduLength = (((uint32_t)pci & 0x0Fu) << 8) + data[0];
    SduDataPtr = &data[1];
    SduLength = (size_t)length - 1u;
    if (isExtendedAddressing()) {
      ffLen = m_LinkLayerDataLength - 3u;
    } else {
      ffLen = m_LinkLayerDataLength - 2u;
    }

    if ((m_LinkLayerDataLength > 8u) && (0u == TpSduLength)) {
      TpSduLength = ((uint32_t)data[2] << 24) + ((uint32_t)data[3] << 16) +
                    ((uint32_t)data[4] << 8) + ((uint32_t)data[5]);
      SduDataPtr = &data[6];
      SduLength -= 4u;
      ffLen -= 4u;
    }
    sfMaxLen = getSFMaxLen();

    if (0 != m_offset) {
      ASLOG(ISOTPE, ("[%d] FF received in wrong state, abort!\n", m_isotp->Channel));
      r = -EFAULT;
    } else if (TpSduLength > PDU_LENGTH_MAX) {
      ASLOG(ISOTPE, ("[%d]FF size too big %u!\n", m_isotp->Channel, (int)TpSduLength));
      r = -EOVERFLOW;
    } else if ((TpSduLength <= ffLen) || (TpSduLength <= sfMaxLen)) {
      ASLOG(ISOTPE, ("[%d]FF size invalid %u(<=%d,%d)!\n", m_isotp->Channel, (int)TpSduLength,
                     (int)ffLen, (int)sfMaxLen));
      r = -EINVAL;
    } else if (SduLength == ffLen) {
      m_rxBuffer.resize(TpSduLength);
      memcpy(&m_rxBuffer[0], SduDataPtr, SduLength);
      m_offset = ffLen;
      m_SN = 1;
      r = sendFC();
    } else {
      ASLOG(ISOTPE, ("[%d]FF received with invalid len %d(!=%d)!\n", m_isotp->Channel,
                     (int)SduLength, (int)ffLen));
      r = -EINVAL;
    }

    return r;
  }

  int handleCF(uint8_t pci, const uint8_t *data, uint8_t length) {
    int r = 0;
    size_t cfLen;

    if (isExtendedAddressing()) {
      cfLen = m_LinkLayerDataLength - 2u;
    } else {
      cfLen = m_LinkLayerDataLength - 1u;
    }

    if (m_offset == 0) {
      ASLOG(ISOTPE, ("[%d] CF received in wrong state, abort!\n", m_isotp->Channel));
      r = -EFAULT;
    } else if ((pci & N_PCI_SN) != m_SN) {
      ASLOG(ISOTPE, ("[%d]CF with invalid SN %d!=%d!\n", m_isotp->Channel, (int)(pci & N_PCI_SN),
                     (int)m_SN));
      r = -EINVAL;
    } else {
      size_t copyLen = cfLen;
      if (copyLen > (m_rxBuffer.size() - m_offset)) {
        copyLen = m_rxBuffer.size() - m_offset;
      }
      memcpy(&m_rxBuffer[m_offset], data, copyLen);
      m_offset += copyLen;
      m_SN++;
      if (m_SN > 15u) {
        m_SN = 0u;
      }
      if (m_offset >= m_rxBuffer.size()) {
        r = (int)m_rxBuffer.size();
      } else {
        r = 0;
      }
    }

    return r;
  }

  int rxIndication(const CanL_FrameType &frame) {
    int r = 0;
    uint8_t pci;
    const uint8_t *data;
    uint8_t length;

    if (isExtendedAddressing()) {
      if (m_N_TA != frame.Data[0]) {
        ASLOG(ISOTPI, ("[%d]not for me, ignore\n", m_isotp->Channel));
        r = 0;
      } else if (frame.Dlc > 2u) {
        pci = frame.Data[1];
        data = &frame.Data[2];
        length = frame.Dlc - 2u;
      } else {
        ASLOG(ISOTPE, ("[%d]invalid DLC\n", m_isotp->Channel));
        r = -EINVAL;
      }
    } else {
      pci = frame.Data[0];
      data = &frame.Data[1];
      length = frame.Dlc - 1u;
    }

    switch (pci & N_PCI_MASK) {
    case N_PCI_SF:
      r = handleSF(pci, data, length);
      break;
    case N_PCI_FF:
      r = handleFF(pci, data, length);
      break;
    case N_PCI_CF:
      r = handleCF(pci, data, length);
      break;
    default:
      ASLOG(ISOTPE, ("[%d]+RX with invalid PCI 0x%02X\n", m_isotp->Channel, pci));
      r = -EINVAL;
      break;
    }

    return r;
  }

  int waitFrame() {
    int r = 0;
    uint32_t canid = m_isotp->Params.U.CAN.RxCanId;
    bool ok = CanL_WaitV2(m_busid, canid, m_timeoutMs);
    if (true == ok) {
      CanL_FrameType frame;
      frame.CanId = canid;
      ok = CanL_ReadV2(m_busid, &frame);
      if (true == ok) {
        ASLOG(ISOTP, ("[%d]+RX len=%d data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
                      m_isotp->Channel, frame.Dlc, frame.Data[0], frame.Data[1], frame.Data[2],
                      frame.Data[3], frame.Data[4], frame.Data[5], frame.Data[6], frame.Data[7]));
        r = rxIndication(frame);
      } else {
        r = -EIO;
      }
    } else {
      r = -ETIMEDOUT;
    }
    return r;
  }

  bool isExtendedAddressing() const {
    return (0xFFFF != m_N_TA);
  }

  size_t getSFMaxLen() {
    size_t sfMaxLen;
    if (m_LinkLayerDataLength > 8u) {
      if (isExtendedAddressing()) {
        sfMaxLen = m_LinkLayerDataLength - 3u;
      } else {
        sfMaxLen = m_LinkLayerDataLength - 2u;
      }
    } else {
      if (isExtendedAddressing()) {
        sfMaxLen = m_LinkLayerDataLength - 2u;
      } else {
        sfMaxLen = m_LinkLayerDataLength - 1u;
      }
    }
    return sfMaxLen;
  }

  size_t getDL(size_t len) {
    size_t dl = m_LinkLayerDataLength;
    uint32_t i;
    const uint8_t lLinkLayerDataLengths[] = {8, 12, 16, 20, 24, 32, 48};

    for (i = 0; i < sizeof(lLinkLayerDataLengths); i++) {
      if (len <= lLinkLayerDataLengths[i]) {
        dl = lLinkLayerDataLengths[i];
        break;
      }
    }
    return dl;
  }

private:
  IsoTp_Type *m_isotp;
  uint8_t m_LinkLayerDataLength;
  uint16_t m_N_TA;
  std::vector<uint8_t> m_txBuffer;
  std::vector<uint8_t> m_rxBuffer;
  int m_busid;
  size_t m_offset;
  size_t m_length;
  uint8_t m_padding = 0x55;
  uint32_t m_timeoutMs = ISOTP_DEFAULT_TIMEOUT;
  uint8_t m_cfgBS = 0;
  uint8_t m_BS = 0;
  uint8_t m_STmin = 0;
  uint8_t m_WftCounter = 0;
  uint8_t m_RxWftMax = 100;
  uint8_t m_SN;
  bool m_waitFirstFC;
};
/* ================================ [ DECLARES  ] ============================================== */
int IsoTp_CanV2Receive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize);
void IsoTp_CanV2Destroy(IsoTp_Type *isotp);
/* ================================ [ DATAS     ] ============================================== */
static IsoTp_Type isotpTp[CANTP_MAX_CHANNELS];
static std::mutex isotpMutex;
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
IsoTp_Type *IsoTp_CanV2Create(IsoTp_ParameterType *params) {
  IsoTp_Type *isotp = NULL;
  int busid = -1;
  int r = 0;
  size_t i;

  std::unique_lock<std::mutex> lck(isotpMutex);
  for (i = 0; i < ARRAY_SIZE(isotpTp); i++) {
    if (TRUE == isotpTp[i].Running) {
      if ((0 == strcmp(isotpTp[i].Params.Device, params->Device)) &&
          (isotpTp[i].Params.Port == params->Port) &&
          ((isotpTp[i].Params.U.CAN.RxCanId == params->U.CAN.RxCanId) ||
           (isotpTp[i].Params.U.CAN.TxCanId == params->U.CAN.TxCanId))) {
        ASLOG(ISOTPE, ("isotp CAN %s:%d %X:%X already opened\n", params->Device, params->Port,
                       params->U.CAN.RxCanId, params->U.CAN.TxCanId));
        r = -EEXIST;
        break;
      }
    }
  }

  if (0 == r) {
    for (i = 0; i < ARRAY_SIZE(isotpTp); i++) {
      if (FALSE == isotpTp[i].Running) {
        isotp = &isotpTp[i];
        isotp->Running = TRUE;
        isotp->Channel = (uint8_t)i;
        break;
      }
    }
  }

  if (NULL != isotp) {
    busid = CanL_Open(params->Device, params->Port, params->Baudrate);
    if (busid < 0) {
      ASLOG(ISOTPE, ("isotp CAN %s:%d open failed\n", params->Device, params->Port));
      isotp->Running = FALSE;
      r = -EINVAL;
    }
  } else {
    r = -ENOENT;
  }

  if (0 == r) {
    isotp->Params = *params;
    isotp->ErrorTimeout = ISOTP_DEFAULT_TIMEOUT;
    Std_TimerStop(&isotp->TimerErrorNotify);
    isotp->Priv = new IsotpCanV2(isotp, busid);
  }

  return isotp;
}

int IsoTp_CanV2Transmit(IsoTp_Type *isotp, const uint8_t *txBuffer, size_t txSize,
                        uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  IsotpCanV2 *pWorker = (IsotpCanV2 *)isotp->Priv;

  r = pWorker->transmit(txBuffer, txSize);
  if (0 == r) {
    if (NULL != rxBuffer) {
      r = IsoTp_CanV2Receive(isotp, rxBuffer, rxSize);
    }
  }

  return r;
}

int IsoTp_CanV2Receive(IsoTp_Type *isotp, uint8_t *rxBuffer, size_t rxSize) {
  int r = 0;
  IsotpCanV2 *pWorker = (IsotpCanV2 *)isotp->Priv;

  r = pWorker->receive(rxBuffer, rxSize);

  return r;
}

int IsoTp_CanV2IoCtl(IsoTp_Type *isotp, int cmd, const void *data, size_t size) {
  int r = -__LINE__;
  std::lock_guard<std::mutex> lg(isotp->Mutex);
  switch (cmd) {
  case ISOTP_IOCTL_SET_TX_ID:
    if ((NULL != data) && (size == sizeof(uint32_t))) {
      uint32_t txId = *(uint32_t *)data;
      *(uint32_t *)data = isotp->Params.U.CAN.TxCanId;
      isotp->Params.U.CAN.TxCanId = txId;
      r = 0;
    }
    break;
  case ISOTP_IOCTL_SET_TIMEOUT:
    if ((NULL != data) && (size == sizeof(uint32_t))) {
      uint32_t timeoutUs = *(uint32_t *)data;
      *(uint32_t *)data = isotp->ErrorTimeout;
      isotp->ErrorTimeout = timeoutUs;
      r = 0;
    }
  default:
    break;
  }

  return r;
}

void IsoTp_CanV2Destroy(IsoTp_Type *isotp) {
  isotp->Running = FALSE;
  delete (IsotpCanV2 *)isotp->Priv;
}
