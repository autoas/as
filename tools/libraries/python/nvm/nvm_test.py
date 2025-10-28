import os
import sys
import json
import random
import time
import logging
from logging.handlers import RotatingFileHandler

logger = logging.getLogger("NVM")
logger.setLevel(logging.INFO)
handler = RotatingFileHandler(filename="log/nvm_test.txt", maxBytes=2 * 1024 * 1024, backupCount=10)
formatter = logging.Formatter("%(asctime)s.%(msecs)03d - %(name)s - %(levelname)s - %(message)s")
handler.setLevel(logging.INFO)
handler.setFormatter(formatter)
logger.addHandler(handler)

CWD = os.path.dirname(__file__)
if CWD == "":
    CWD = os.path.abspath(".")

ASONE = os.path.abspath("%s/../../../asone" % (CWD))
sys.path.append(ASONE)

from one.dcm import dcm

uds = None

MEMIF_IDLE = 1
MEMIF_BUSY = 2


def ASSERT_EQ(a, b):
    if a != b:
        raise Exception("%s != %s" % (a, b))


def formatd(data):
    return ",".join(["%02X" % (s) for s in data])


def power_off():
    # a Ecu Reset to simulate power off
    logger.info("power off")
    counter = 10
    ercd, response = uds.transmit([0x11, 0x03])
    while False == ercd:
        time.sleep(1)
        if counter > 0:
            counter -= 1
        else:
            raise Exception("power off failed")
        ercd, response = uds.transmit([0x11, 0x03])
    time.sleep(2)


def power_on():
    logger.info("power on")
    counter = 10
    ercd, response = uds.transmit([0x10, 0x01])
    while False == ercd:
        time.sleep(1)
        if counter > 0:
            counter -= 1
        else:
            raise Exception("power on failed")
        ercd, response = uds.transmit([0x10, 0x01])


def nvm_request(req, msg=""):
    counter = 10
    ercd, response = uds.transmit(req)
    while True != ercd:
        print("%s with error:" % (msg), response, ", retry @%.3f" % (time.time()))
        time.sleep(1)
        if counter > 0:
            counter -= 1
        else:
            # OK, the last time, try reboot the device to recover
            time.sleep(5)
            power_off()
            power_on()
            # raise Exception("nvm write failed, cantp error!")
        uds.reset()
        ercd, response = uds.transmit(req)
    return ercd, response


def nvm_write(blockId, data):
    logger.info(f"write blockId {blockId} data {formatd(data)}")
    req = [0x31, 0x01, 0xFE, 0xEF, 0, (blockId >> 8) & 0xFF, blockId & 0xFF] + data
    ercd, response = nvm_request(req, "write")
    ASSERT_EQ(req[4:], [x for x in response[4:]])


def nvm_read(blockId):
    req = [0x31, 0x01, 0xFE, 0xEF, 1, (blockId >> 8) & 0xFF, blockId & 0xFF]
    ercd, response = nvm_request(req, "read")
    logger.info(f"read blockId {blockId} data {formatd(response[4:])}")
    return [x for x in response[4:]]


def get_fee_admin():
    req = [0x31, 0x01, 0xFE, 0xEF, 2]
    ercd, response = nvm_request(req, "read fee admin")
    record = [x for x in response[4:]]
    curBank = record[0]
    adminFreeAddr = (record[1] << 24) + (record[2] << 16) + (record[3] << 8) + record[4]
    dataFreeAddr = (record[5] << 24) + (record[6] << 16) + (record[7] << 8) + record[8]
    eraseNumber = (record[9] << 24) + (record[10] << 16) + (record[11] << 8) + record[12]
    return curBank, adminFreeAddr, dataFreeAddr, eraseNumber


def nvm_write_abnormal(blockId, data, nLoops):
    logger.info(f"write nLoops {nLoops} blockId {blockId} data {formatd(data)}")
    req = (
        [0x31, 0x01, 0xFE, 0xEF, 4, (blockId >> 8) & 0xFF, blockId & 0xFF]
        + [(nLoops >> 24) & 0xFF, (nLoops >> 16) & 0xFF, (nLoops >> 8) & 0xFF, nLoops & 0xFF]
        + data
    )
    ercd, response = uds.transmit(req, timeoutMs=200)
    if ercd == True and response is not None:
        ASSERT_EQ(req[4:], [x for x in response[4:]])
        return True
    else:
        time.sleep(0.1)
        power_on()
        return False


def normal(blocks, nLoops=1000000):
    history = {}
    for n in range(nLoops):
        power_on()
        for block in blocks:
            name = block["name"]
            blockId = block["number"]
            size = block["size"]
            data = [random.randint(0, 255) for i in range(size)]
            if name not in history:
                history[name] = []
            history[name].append(data)
            history[name] = history[name][-10:]
            nvm_write(blockId, data)
        power_off()
        power_on()
        for block in blocks:
            name = block["name"]
            blockId = block["number"]
            data = nvm_read(blockId)
            if data != history[name][-1]:
                msg = "%s loops, %s(%s): %s != last of history: %s" % (
                    n,
                    name,
                    blockId,
                    formatd(data),
                    formatd(history[name]),
                )
                logger.error(msg)
                raise Exception(msg)
            # print(name, blockId, data, history[name][-1])
        curBank, adminFreeAddr, dataFreeAddr, eraseNumber = get_fee_admin()
        power_off()
        msg = (
            "NvM normal test %s/%s times PASS: curBank=%s eraseNumber=%d adminFreeAddr=0x%08x dataFreeAddr=0x%08x free=%s"
            % (n, nLoops, curBank, eraseNumber, adminFreeAddr, dataFreeAddr, dataFreeAddr - adminFreeAddr)
        )
        logger.info(msg)
        print(msg)


def abnormal(blocks, nMaxSched, nLoops=1000000):
    history = {}
    power_on()
    for block in blocks:
        name = block["name"]
        blockId = block["number"]
        size = block["size"]
        data = [random.randint(0, 255) for i in range(size)]
        if name not in history:
            history[name] = []
        history[name].append(data)
        history[name] = history[name][-5:]
        nvm_write(blockId, data)
    power_off()
    power_on()
    for n in range(nLoops):
        for block in blocks:
            name = block["name"]
            blockId = block["number"]
            size = block["size"]
            data = [random.randint(0, 255) for i in range(size)]
            loops = random.randint(1, nMaxSched)
            r = nvm_write_abnormal(blockId, data, loops)
            if True == r:
                history[name].append(data)
                history[name] = history[name][-10:]
            else:
                continue
            dataR = nvm_read(blockId)
            if dataR != history[name][-1]:
                msg = "%s loops, %s(%s): %s != last of history: %s" % (
                    n,
                    name,
                    blockId,
                    formatd(dataR),
                    formatd(history[name]),
                )
                logger.error(msg)
                raise Exception(msg)
            # print(name, blockId, data, history[name][-1])
        curBank, adminFreeAddr, dataFreeAddr, eraseNumber = get_fee_admin()
        msg = (
            "NvM abnormal test %s/%s times PASS: curBank=%s eraseNumber=%d adminFreeAddr=0x%08x dataFreeAddr=0x%08x free=%s"
            % (n, nLoops, curBank, eraseNumber, adminFreeAddr, dataFreeAddr, dataFreeAddr - adminFreeAddr)
        )
        logger.info(msg)
        print(msg)


def read_one_block(addr, size):
    counter = 10
    req = [
        0x31,
        0x01,
        0xFE,
        0xEF,
        3,
        (addr >> 24) & 0xFF,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF,
        (size >> 24) & 0xFF,
        (size >> 16) & 0xFF,
        (size >> 8) & 0xFF,
        size & 0xFF,
    ]
    ercd, response = nvm_request(req, "dump")
    return response[4:]


def dump_nvm(address, size):
    power_on()
    offset = 0
    raw = bytes()
    print("dump at address 0x%08X size 0x%08X\n" % (address, size))
    while offset < size:
        sz = size - offset
        if sz > 256:
            sz = 256
        ob = read_one_block(address + offset, sz)
        raw = raw + ob
        offset = offset + sz
        print("\033[1A%.2f%%" % (offset * 100.0 / size))
    with open("NvM.img", "wb") as f:
        f.write(raw)
    print("dump nvm as NvM.img done")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="NvM Test through UDS")
    parser.add_argument("-c", "--config", type=str, help="NVM Test config json file")
    parser.add_argument("-t", "--txid", type=str, help="UDS CanTp TxId", default="0x731")
    parser.add_argument("-r", "--rxid", type=str, help="UDS CanTp RxId", default="0x732")
    parser.add_argument("-a", "--address", type=str, help="NvM bank start address", default="0")
    parser.add_argument("-s", "--size", type=str, help="NvM bank size", default="64*1024")
    parser.add_argument("-d", "--dump", action="store_true", help="dump NvM memory", default=False)
    parser.add_argument("--debug", action="store_true", help="verbose uds message", default=False)
    parser.add_argument(
        "--max_sched",
        type=str,
        help="The maximum number of NvM task schedule loops for abnormal test, if specified, run abnormal test",
        default="0",
    )
    args = parser.parse_args()
    uds = dcm(
        protocol="CAN",
        device="simulator_v2",
        port=0,
        txid=eval(args.txid),
        rxid=eval(args.rxid),
        ll_dl=8,
        verbose=args.debug,
        log=logger,
        maxlen=1024,
    )
    if args.dump:
        dump_nvm(eval(args.address), eval(args.size))
    else:
        with open(args.config) as f:
            cfg = json.load(f)
        blocks = cfg["blocks"]
        nMaxSched = eval(args.max_sched)
        if nMaxSched > 0:
            abnormal(blocks, nMaxSched)
        else:
            normal(blocks)
