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

def sync_no_crc():
    r, id, data = n0.read(0x100)
    while(False == r):
        r, id, data = n0.read(0x100)
    t2r = get_timestamp()
    if data[0] not in [0x10, 0x20]:
        return
    sn = data[2]&0x0F
    st0r = (data[4]<<24) + (data[5]<<16) + (data[6]<<8) + data[7]
    r, id, data = n0.read(0x100)
    while(False == r):
        r, id, data = n0.read(0x100)
    if data[0] not in [0x18, 0x28]:
        return
    t3r = get_timestamp()
    ovs = data[3]&0x03
    t4r = (data[4]<<24) + (data[5]<<16) + (data[6]<<8) + data[7]
    reltime =  (t3r - t2r) + (st0r+ovs)*1000000000 + t4r
    print('%d: reltime = %x, now = %x, diff = %d' % (sn, reltime, t3r, t3r - reltime))

while(True):
    sync_no_crc()