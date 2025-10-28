import os
import sys
import json
import random
import time
import logging

CWD = os.path.dirname(__file__)
if CWD == "":
    CWD = os.path.abspath(".")

ASONE = os.path.abspath("%s/../../../asone" % (CWD))
sys.path.append(ASONE)

from one.AsPy import can


logging.basicConfig(
    level=logging.DEBUG, format="%(asctime)s - %(filename)s[line:%(lineno)d] - %(levelname)s: %(message)s"
)


def can_write(n0, idx, data):
    idx = eval(idx)
    r = n0.write(idx, bytes(data))
    assert r == True
    logging.debug("  canid=%x data=%s" % (idx, ",".join([hex(x) for x in data])))


def can_read(n0, idx, timeoutMs=50):
    idx = eval(idx)
    r, canid, data = n0.read(idx, timeoutMs)
    assert r == True
    logging.debug("  canid=%x data=%s" % (canid, ",".join([hex(x) for x in data])))


def IncorrectFlowStatus(args):
    n0 = can("simulator_v2", 0)
    logging.info("1. Correct FC status:")
    can_write(n0, args.txid, [0x07, 0x31, 0x01, 0xFE, 0xEF, 0x01, 0x00, 0x10])
    can_read(n0, args.rxid)
    can_write(n0, args.txid, [0x30, 0x08, 0x0A, 0x55, 0x55, 0x55, 0x55, 0x55])
    left = 4
    while left > 0:
        can_read(n0, args.rxid)
        left = left - 1
    for fs in [3, 4, 5, 6, 7]:
        logging.info("2. Incorrect FC status %s:" % (fs))
        can_write(n0, args.txid, [0x07, 0x31, 0x01, 0xFE, 0xEF, 0x01, 0x00, 0x10])
        can_read(n0, args.rxid)
        can_write(n0, args.txid, [0x30 | fs, 0x08, 0x0A, 0x55, 0x55, 0x55, 0x55, 0x55])


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="NvM Test through UDS")
    parser.add_argument("-t", "--txid", type=str, help="UDS CanTp TxId", default="0x731")
    parser.add_argument("-r", "--rxid", type=str, help="UDS CanTp RxId", default="0x732")
    args = parser.parse_args()
    IncorrectFlowStatus(args)
