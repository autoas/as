# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import PyFee
import random
import os
import pickle


def format(s):
    return str(s).replace("'", "").replace(",", "")


def resume(**kwargs):
    with open('.blocks.pkl', 'rb') as f:
        blocks = pickle.loads(f.read())
    print(blocks)
    with open('Fls.img', 'rb') as f:
        r = PyFee.img(f.read())
        #assert(r)
    NUM_BANKS = kwargs.get('NUM_BANKS', 2)
    BLOCK_SIZE = kwargs.get('BLOCK_SIZE', 0x10000)
    PyFee.config(blocks, bankSize=NUM_BANKS, blockSize=BLOCK_SIZE)
    PyFee.power_on()
    for name in blocks.keys():
        data = PyFee.read(name)
        print('read %s:%s' % (
            name, format(['%02X' % (d) for d in data])))
    PyFee.power_off()


def normal(**kwargs):
    PyFee.erase()
    NUM_BLOCKS = kwargs.get('NUM_BLOCKS', 32)
    MIN_SIZE = kwargs.get('MIN_SIZE', 1)
    MAX_SIZE = kwargs.get('MAX_SIZE', 32)
    NUM_BANKS = kwargs.get('NUM_BANKS', 2)
    BLOCK_SIZE = kwargs.get('BLOCK_SIZE', 0x10000)
    nLoops = kwargs.get('loops', 10000)
    blocks = {}
    for i in range(NUM_BLOCKS):
        blocks[str(i)] = random.randint(MIN_SIZE, MAX_SIZE)
    with open('.blocks.pkl', 'wb') as f:
        f.write(pickle.dumps(blocks))
    PyFee.config(blocks, bankSize=NUM_BANKS, blockSize=BLOCK_SIZE)
    for n in range(nLoops):
        PyFee.power_on()
        lastDatas = {}
        for i in range(NUM_BLOCKS):
            name = str(i)
            data = bytes([random.randint(0, 255)
                          for i in range(blocks[name])])
            lastDatas[name] = data
            r = PyFee.write(name, data)
            assert(r)
        PyFee.power_off()
        PyFee.power_on()
        for i in range(NUM_BLOCKS):
            name = str(i)
            data = PyFee.read(name)
            if data != lastDatas[name]:
                with open('Fls.img', 'wb') as f:
                    f.write(PyFee.img())
                raise Exception('%s loops, %s: not equal, %s %s' %
                                (n, name, data, lastDatas[name]))
        PyFee.power_off()
        print('Fee normal test %s/%s times PASS' % (n, nLoops))
    with open('Fls-normal-%s.img' % (NUM_BANKS), 'wb') as f:
        f.write(PyFee.img())


def abnormal(**kwargs):
    PyFee.erase()
    NUM_BLOCKS = kwargs.get('NUM_BLOCKS', 32)
    MIN_SIZE = kwargs.get('MIN_SIZE', 1)
    MAX_SIZE = kwargs.get('MAX_SIZE', 32)
    NUM_BANKS = kwargs.get('NUM_BANKS', 2)
    BLOCK_SIZE = kwargs.get('BLOCK_SIZE', 0x10000)
    verbose = kwargs.get('verbose', False)
    nLoops = kwargs.get('loops', 100000)
    blocks = {}
    for i in range(NUM_BLOCKS):
        blocks[str(i)] = random.randint(MIN_SIZE, MAX_SIZE)
    with open('.blocks.pkl', 'wb') as f:
        f.write(pickle.dumps(blocks))
    PyFee.config(blocks, bankSize=NUM_BANKS, blockSize=BLOCK_SIZE)

    PyFee.power_on()
    lastDatas = {}
    history = {}
    for i in range(NUM_BLOCKS):
        name = str(i)
        data = bytes([random.randint(0, 255)
                      for i in range(blocks[name])])
        lastDatas[name] = [data]
        history[name] = [data]
        if verbose:
            print('write:', i, ['%X' % (d) for d in data])
        r = PyFee.write(name, data)
        assert(r)
    PyFee.power_off()
    for n in range(nLoops):
        PyFee.power_on()
        for i in range(NUM_BLOCKS):
            name = str(i)
            data = bytes([random.randint(0, 255)
                          for i in range(blocks[name])])
            if verbose:
                print('try write:', i, ['%X' % (d) for d in data])
            PyFee.write(name, data, False)
            loops = random.randint(1, 1000)
            PyFee.schedule(loops)
            r = PyFee.result()
            if (r):
                lastDatas[name] = [data]
                history[name] = history[name][-9:] + [data]
            else:
                lastDatas[name] = [data, lastDatas[name][-1]]
                PyFee.power_off()
                PyFee.power_on()
                break

        PyFee.power_off()
        PyFee.power_on()
        for i in range(NUM_BLOCKS):
            name = str(i)
            data = PyFee.read(name)
            if data is None:
                data = []
            if verbose:
                print('read:', i, ['%X' % (d) for d in data])
            if not any(data == d for d in lastDatas[name]):
                with open('Fls.img', 'wb') as f:
                    f.write(PyFee.img())
                str1 = format(['%02X' % (d) for d in data])
                str2 = format([['%02X' % (d) for d in D]
                              for D in lastDatas[name]])
                strh = format([['%02X' % (d) for d in D]
                              for D in history[name]])
                raise Exception('%s loops, %s: not equal, %s\n\t%s\n\thistory: %s' %
                                (n, name, str1, str2, strh))
            lastDatas[name] = [data]
        PyFee.power_off()
        print('Fee abnormal test %s/%s times PASS' % (n, nLoops))
    with open('Fls-abnormal-%s.img' % (NUM_BANKS), 'wb') as f:
        f.write(PyFee.img())


normal()
normal(NUM_BANKS=5, BLOCK_SIZE=128*1024)
abnormal()
abnormal(NUM_BANKS=5, BLOCK_SIZE=128*1024)
# resume()
# resume(NUM_BANKS=5, BLOCK_SIZE=128*1024)
