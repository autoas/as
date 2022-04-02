import os
import sys
CWD = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.abspath('%s/../..' % (CWD)))
from utils.common import crc16

__args = None


def get_u32_little(raw, offset):
    return raw[offset] + (raw[offset+1] << 8) + (raw[offset+2] << 16) + (raw[offset+3] << 24)


def get_u32(raw, offset=0):
    if __args.endian == 'little':
        return get_u32_little(raw, offset)
    return get_u32_big(raw, offset)


def get_u16_little(raw, offset):
    return raw[offset] + (raw[offset+1] << 8)


def get_u16(raw, offset=0):
    if __args.endian == 'little':
        return get_u16_little(raw, offset)
    return get_u16_big(raw, offset)


def align(size, align_size=None):
    if align_size == None:
        align_size = __args.page_size
    return ((size + align_size - 1) & (~(align_size - 1)))


def align_data(size):
    return align(align(size, 2) + 4)


def decodeData(admin, data):
    isGood = True
    r = 'OK'
    BlockNumber = get_u16(admin)
    InvBlockNumber = get_u16(admin, 2)
    if BlockNumber != ((~InvBlockNumber) & 0xFFFF):
        r = 'NOK'
        isGood = False
    Address = get_u32(admin, 4)
    NumberOfWriteCycles = get_u32(admin, 8)
    BlockSize = get_u16(admin, 12)
    Crc = get_u16(admin, 14)
    calcCrc = crc16(admin[:-2])
    if calcCrc != Crc:
        isGood = False
        r = 'Admin CRC NOK (E%X != R%X)' % (Crc, calcCrc)
    Crc = get_u16(data[-4:-2])
    InvCrc = get_u16(data[-2:])
    calcCrc = crc16(data[:-4])
    if calcCrc != Crc and Crc != ((~InvCrc) & 0xFFFF):
        isGood = False
        r += ' Data CRC NOK (E%X, ~%X R%X)' % (Crc, InvCrc, calcCrc)
    print('BlockNumber=%d BlockSize=%d Address=0x%X NumberOfWriteCycles=%d %s' % (
        BlockNumber, BlockSize, Address, NumberOfWriteCycles, r))
    if isGood:
        print('  data=[%s] Crc: %X ~%X %X' % (' '.join(['%02X' % (s)
              for s in data[:BlockSize]]), Crc, InvCrc, calcCrc))


def decodeBlock(id, block):
    offset = align(8)
    Number = get_u32(block, offset)
    InvNumber = get_u32(block, offset+4)
    if Number != ((~InvNumber) & 0xFFFFFFFF):
        raise Exception('Number %X != InvNumber %X' % (Number, InvNumber))
    offset = align(offset + 8)
    FullMagic = get_u32(block, offset)
    print('block%d: Number=%d, FullMagic=%X' % (id, Number, FullMagic))
    adminOff = align(offset + 4)
    dataOff = len(block)
    while(adminOff < dataOff):
        admin = block[adminOff:adminOff+align(16)]
        if not all(s == 0xFF for s in admin):
            BlockSize = get_u16(admin, 12)
            if BlockSize != 0xFFFF:
                dataOff -= align_data(BlockSize)
                data = block[dataOff:dataOff+align_data(BlockSize)]
                decodeData(admin, data)
            else:
                decodeData(admin, [0xFF, 0xFF])
            adminOff += align(16)
        else:
            break


def main(args):
    global __args
    __args = args
    with open(args.img, 'rb') as f:
        IMG = f.read()

    nBlocks = len(IMG)/args.block_size
    if nBlocks != int(nBlocks):
        raise Exception('Invalid Fee Img %s with size %d, not N times of %d' % (
            args.img, len(IMG), args.block_size))
    nBlocks = int(nBlocks)

    for i in range(nBlocks):
        block = IMG[i*args.block_size:(i+1)*args.block_size]
        MagicNumber = get_u32(block)
        InvMagicNumber = get_u32(block, 4)
        if MagicNumber == (~InvMagicNumber) & 0xFFFFFFFF:
            decodeBlock(i, block)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--img', help='flash image', required=True)
    parser.add_argument('--block_size', help='block size',
                        default='64*1024', type=str, required=False)
    parser.add_argument('--page_size', help='page size',
                        default=8, type=int, required=False)
    parser.add_argument('--endian', help='endian: big or little',
                        default='little', type=str, required=False)
    args = parser.parse_args()
    args.block_size = eval(args.block_size)
    main(args)
