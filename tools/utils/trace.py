import os
import sys
import json
import time
import signal

CWD = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.abspath('%s/../asone' % (CWD)))


def get_u32_little(raw, offset):
    return raw[offset] + (raw[offset+1] << 8) + (raw[offset+2] << 16) + (raw[offset+3] << 24)


def get_u32(raw, offset=0, endian='little'):
    if endian == 'little':
        return get_u32_little(raw, offset)
    return get_u32_big(raw, offset)


def get_elapsed(now, prev, maxT, dir='up'):
    if dir == 'up':
        if now >= prev:
            return now - prev
        else:
            return maxT - prev + now
    else:
        if now > prev:
            return maxT - now + prev
        else:
            return prev - now


def process(binOrPath, args):
    with open(args.config) as f:
        cfg = json.load(f)
    if type(binOrPath) is bytes:
        BIN = binOrPath
    else:
        with open(binOrPath, 'rb') as f:
            BIN = f.read()
    nBits = None
    for i in range(1, 13):
        if ((1 << i) > len(cfg['events'])):
            nBits = i
            break

    numOfEvents = len(BIN)//4
    trs = {'displayTimeUnit': 'ns', 'traceEvents': []}
    now = 0
    last = None
    if args.max != None:
        maxT = args.max
    else:
        maxT = 1 << (32-nBits)
    mask = (1 << (32-nBits))-1
    lastStatus = {}
    for i in range(numOfEvents):
        ev = get_u32(BIN, i*4, args.endian)
        id = ev >> (32-nBits)
        ts = ev & mask
        if last == None:
            last = ts
        now = now + get_elapsed(ts, last, maxT, args.dir)
        last = ts
        name = cfg['events'][id]
        ph = 'I'
        if name[-2:] == '_B':
            name = name[:-2]
            ph = 'B'
        if name[-2:] == '_E':
            name = name[:-2]
            ph = 'E'
        if name in lastStatus:
            last_ph, last_ts = lastStatus[name]['ph'], lastStatus[name]['ts']
            if last_ph == ph:
                # make things right is the related E and B is missing, so for any duration time is 0, there is someting wrong
                if ph == 'B':
                    rt = {'name': cfg['area'], 'ph': 'E', 'pid': '0', 'tid': name, 'ts': last_ts}
                    trs['traceEvents'].append(rt)

                elif ph == 'E':
                    rt = {'name': cfg['area'], 'ph': 'B', 'pid': '0', 'tid': name, 'ts': now}
                    trs['traceEvents'].append(rt)
        lastStatus[name] = {'ph': ph, 'ts': now}
        rt = {'name': cfg['area'], 'ph': ph, 'pid': '0', 'tid': name, 'ts': now}
        trs['traceEvents'].append(rt)
    with open(args.output, 'w') as f:
        json.dump(trs, f)
        print('saving %s done' % (args.output))


lExit = False

def main(args):
    global lExit
    if os.path.isfile(args.input):
        process(args.input, args)
    else:
        from one.AsPy import can as AsCan
        ins = args.input
        if ins == 'default':
            ins = 'simulator_v2:0:500000:0x7FD'
        device, port, baud, canid = ins.split(':')
        port = eval(port)
        baud = eval(baud)
        canid = eval(canid)
        node = AsCan(device, port, baud)
        start = time.time()
        prev = time.time()
        BIN = bytes([])
        while (False == lExit):
            ercd, _, data = node.read(canid)
            if (ercd):
                BIN += data
            elapsed = time.time() - prev
            if elapsed > 1:
                print('process %d events, duration %.2f s' % (len(BIN), time.time() - start))
                process(BIN, args)
                prev = time.time()


def safe_exit(signo, frame):
    global lExit
    if lExit:
        sys.exit(0)
    lExit = True


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input', help='the input trace bin', type=str, required=True)
    parser.add_argument('-c', '--config', help='config json', type=str, required=True)
    parser.add_argument('-o', '--output', help='the output trace json',
                        default='.trace.json', type=str, required=False)
    parser.add_argument('--endian', help='endian: big or little',
                        default='little', type=str, required=False)
    parser.add_argument('--dir', help='the timer direction: up or down',
                        default='up', type=str, required=False)
    parser.add_argument('--max', help='maximum value of the timer',
                        default=None, type=int, required=False)
    args = parser.parse_args()
    signal.signal(signal.SIGINT, safe_exit)
    main(args)
