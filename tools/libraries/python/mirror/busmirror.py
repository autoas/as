import os
import sys
import socket
import struct
import logging
from datetime import datetime, timedelta
import re

CWD = os.path.dirname(__file__)
if CWD == "":
    CWD = os.path.abspath(".")

ASONE = os.path.abspath("%s/../../../asone" % (CWD))
sys.path.append(ASONE)

from one.AsPy import can

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s.%(msecs)03d [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[logging.StreamHandler()],
)

logger = logging.getLogger("BusMirror")

LastSequenceNumber = -1

CanBusMap = {}


def CanForward(networkId, canId, payload):
    global CanBusMap
    ctrlId = 10 + networkId
    if ctrlId not in CanBusMap:
        n0 = can("simulator_v2", ctrlId)
        CanBusMap[ctrlId] = n0
        logger.error(f"forward CAN{networkId} to VCAN{ctrlId}")
    n0 = CanBusMap[ctrlId]
    n0.write(canId, payload)


def DecodeHeaderTimestamp(HeaderTimestamp):
    seconds = (
        (HeaderTimestamp[0] << 40)
        + (HeaderTimestamp[1] << 32)
        + (HeaderTimestamp[2] << 24)
        + (HeaderTimestamp[3] << 16)
        + (HeaderTimestamp[4] << 8)
        + HeaderTimestamp[5]
    )
    nanoseconds = (
        (HeaderTimestamp[6] << 24) + (HeaderTimestamp[7] << 16) + (HeaderTimestamp[8] << 8) + HeaderTimestamp[9]
    )

    date = datetime.fromtimestamp(seconds) + timedelta(microseconds=nanoseconds // 1000)
    return date


def Decode(data):
    global LastSequenceNumber
    ProtocolVersion = data[0]
    SequenceNumber = data[1]
    HeaderTimestamp = data[2:12]
    DataLength = (data[12] << 8) + data[13]
    assert DataLength + 14 == len(data)
    if ProtocolVersion != 1:
        logger.error("wrong protocol version")
    if LastSequenceNumber != -1 and (LastSequenceNumber + 1) & 0xFF != SequenceNumber:
        logger.error("wrong sequence number or missing")
    date = DecodeHeaderTimestamp(HeaderTimestamp)
    logger.info(f"{date}")
    data = data[14:]
    frames = []
    while len(data) > 0:
        Timestamp = ((data[0] << 8) + data[1]) * 10
        tim = date + timedelta(microseconds=Timestamp)
        bHasNetworkState = (data[2] & 0x80) != 0
        bHasFrameId = (data[2] & 0x40) != 0
        bHasPayload = (data[2] & 0x20) != 0
        networkType = data[2] & 0x1F
        networkId = data[3]
        if networkType == 1:
            NT = "CAN%s" % (networkId)
        elif networkType == 2:
            NT = "LIN%s" % (networkId)
        else:
            raise
        offset = 4
        if bHasNetworkState:
            networkState = data[offset]
            offset += 1
        else:
            networkState = 0
        if bHasFrameId:
            if networkType == 1:  # CAN
                frameId = (data[offset] << 24) + (data[offset + 1] << 16) + (data[offset + 2] << 8) + data[offset + 3]
                offset += 4
            else:  # LIN
                frameId = data[offset] & 0x3F
                offset += 1
        else:
            frameId = 0
        if bHasPayload:
            length = data[offset]
            offset += 1
            payload = data[offset : offset + length]
            payloadS = " ".join(["%02x" % (p) for p in payload])
            offset += length
            if networkType == 1:
                CanForward(networkId, frameId, payload)
        else:
            payload = []
            length = 0
            payloadS = ""
        logger.info(f"{NT} NS={networkState:02x} ID={frameId:x} LEN={length} data=[{payloadS}] @ {tim}")
        frames.append([NT, networkState, frameId, length, payload, tim])
        data = data[offset:]

    LastSequenceNumber = SequenceNumber

    return SequenceNumber, date, frames


def Main(addr):
    ip, port = addr.split(":")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", eval(port)))
    mreq = struct.pack("4sL", socket.inet_aton(ip), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    while True:
        data, addr = sock.recvfrom(1400)  # Buffer size 1024 bytes
        Decode(data)


def AnalyzeAscFile(filename):
    from scapy.all import Ether, Raw, bytes_hex

    lastSequenceNumber = -1
    reETH = re.compile(r"^\s*(\d+\.\d+)\s+ETH\s+(\d+)\s+(Rx|Tx)\s+([0-9a-fA-F]+):([0-9a-fA-F]+)")
    with open(filename, "r") as fin, open(filename + ".dec", "w") as fout:
        for l in fin.readlines():
            fout.write(l)
            m = reETH.match(l)
            if m:
                timestamp, eth, dir, seq, asc = m.groups()
                raw = bytes.fromhex(asc)
                pkt = Ether(raw)
                if pkt.haslayer("UDP") and pkt.haslayer("Raw"):
                    payload = pkt["Raw"].load
                    if payload[0] == 1:  # version check
                        SequenceNumber, date, frames = Decode(payload)
                        if lastSequenceNumber != -1 and (lastSequenceNumber + 1) & 0xFF != SequenceNumber:
                            seqStatus = "NOK"
                        else:
                            seqStatus = "OK"
                        lastSequenceNumber = SequenceNumber
                        fout.write(f"SequenceNumber={SequenceNumber} {seqStatus} numFrames={len(frames)} date={date}\n")
                        for NT, networkState, frameId, length, payload, tim in frames:
                            payloadS = " ".join(["%02x" % (p) for p in payload])
                            fout.write(
                                f"{NT} NS={networkState:02x} ID={frameId&0x1FFFFFFF:X}x LEN={length} data=[{payloadS}] @ {tim}\n"
                            )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="bus mirror dump utility")
    parser.add_argument("-a", "--address", type=str, default="224.244.224.245:30511", help="The target UDP IP address")
    args = parser.parse_args()
    if args.address.endswith(".asc"):
        AnalyzeAscFile(args.address)
    else:
        Main(args.address)
