import time
import os
import sys

CWD = os.path.dirname(__file__)
if CWD == "":
    CWD = os.path.abspath(".")

ASONE = os.path.abspath("%s/../../../../tools/asone" % (CWD))
sys.path.append(ASONE)

from one.AsPy import can

n0 = can('simulator_v2', 0, 1000000)


def get_timestamp():
    return int(time.time()*1000000000)


def sync_no_crc(sn):
    t0r = get_timestamp()
    st0r = t0r//1000000000
    # setup SYNC message
    data = [0x10, 0x00, 0x0 + sn, 0x00, (st0r >> 24) & 0xFF,
            (st0r >> 16) & 0xFF, (st0r >> 8) & 0xFF, st0r & 0xFF]
    r = n0.write(0x100, bytes(data))
    assert(True == r)
    t1r = get_timestamp()
    t4r = t1r - st0r*1000000000
    # setup FUP message
    data = [0x18, 0x00, 0x0 + sn, 0x0, (t4r >> 24) & 0xFF,
            (t4r >> 16) & 0xFF, (t4r >> 8) & 0xFF, t4r & 0xFF]
    r = n0.write(0x100, bytes(data))
    assert(True == r)
    print('t0r is 0x%x ns' % (t0r))
    print('t1r is 0x%x ns' % (t1r))
    print('st0r is 0x%x ns' % (st0r*1000000000))
    print('t4r is 0x%x ns' % (t4r))


for i in range(10):
    sync_no_crc(i % 15)
    time.sleep(5)
