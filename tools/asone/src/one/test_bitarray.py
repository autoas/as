from AsPy import bitarray as BitArray
from bitarray import bitarray
import random

Idx = 0
while True:
    print('Test for loop', Idx)
    n = random.randint(1, 1024)
    B = BitArray()
    b = bitarray()
    for i in range(n):
        nBits = random.randint(1, 32)
        mask = (1 << nBits) - 1
        v = random.randint(0, 0xFFFFFFFF) & mask
        bstr = bin(v)[2:]
        if len(bstr) < nBits:
            bstr = '0'*(nBits-len(bstr)) + bstr
        b += bitarray(bstr)
        B.put(v, nBits)
        #print('  append %d bits: %s' % (nBits, bstr))
    if b.tobytes() != B.tobytes():
        print('  Fail')
        print('  Golden:', [format(x, '08b') for x in b.tobytes()])
        print('  Real  :', [format(x, '08b') for x in B.tobytes()])
        break
    else:
        print(' Pass')
    Idx += 1
