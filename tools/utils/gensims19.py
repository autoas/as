#!/usr/bin/env python3
"""Generate a multi-section S19 dummy application for host bootloader simulation.

Example (run from the repository root):
    python tools/utils/gensims19.py -n 8 -s 8192 -g 2048 -b 0x1000 -o build/AppDummy.s19.A
"""
import argparse


def make_s19_record(addr, chunk):
    addr_bytes = [(addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF]
    count = len(chunk) + 5
    body = [count] + addr_bytes + list(chunk)
    checksum = (~sum(body)) & 0xFF
    line = 'S3' + ''.join('%02X' % b for b in body) + '%02X\n' % checksum
    return line


def main():
    parser = argparse.ArgumentParser(description='Generate multi-section S19 dummy application')
    parser.add_argument('-n', '--sections', type=eval, default=8, help='Number of sections (default: 8)')
    parser.add_argument('-s', '--section-size', type=eval, default=4096, help='Size of each section in bytes (default: 4096)')
    parser.add_argument('-g', '--gap', type=eval, default=1024, help='Gap between sections in bytes (default: 1024)')
    parser.add_argument('-b', '--base', type=eval, default=0x1000, help='Base address (default: 0x1000)')
    parser.add_argument('-o', '--output', default='build/AppDummy.s19', help='Output file (default: build/AppDummy.s19)')
    args = parser.parse_args()

    data = bytes([i % 256 for i in range(args.section_size * args.sections)])

    with open(args.output, 'w') as f:
        f.write('S00600004844521B\n')
        addr = args.base
        for sec in range(args.sections):
            for offset in range(0, args.section_size, 16):
                chunk = data[sec * args.section_size + offset: sec * args.section_size + offset + 16]
                if len(chunk) > 0:
                    f.write(make_s19_record(addr, chunk))
                    addr += len(chunk)
            addr += args.gap
        f.write('S5030001FB\n')
        f.write('S9030001FB\n')

    print('Generated %s with %d sections' % (args.output, args.sections))


if __name__ == '__main__':
    main()
