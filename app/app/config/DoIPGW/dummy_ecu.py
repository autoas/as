#!/usr/bin/env python3
"""Single-node dummy UDS responder for the DoIP-to-CAN gateway demo.

Run one instance per simulated ECU in a separate shell, e.g.::

    python dummy_ecu.py --rxid 0x731 --txid 0x732 --ll-dl 64 --name ECU_A
    python dummy_ecu.py --rxid 0x741 --txid 0x742 --ll-dl 64 --name ECU_B
    python dummy_ecu.py --rxid 0x757 --txid 0x75F --ll-dl 8  --name ECU_C

The responder answers every request with a positive response (SID | 0x40) echoing
the rest of the request. For SessionControl (0x10) it appends the standard
P2/P2* timing bytes (P2=50ms, P2*=5s).
"""

import argparse
import os
import sys

CWD = os.path.dirname(__file__)
if "" == CWD:
    CWD = "."
ROOT = os.path.abspath(CWD + "/../../../..")
sys.path.append(f"{ROOT}/tools/asone")


def parse_int(value: str) -> int:
    """Parse a hex (0x..) or decimal integer literal."""
    return int(value, 0)


def main() -> int:
    ap = argparse.ArgumentParser(description="Single-node dummy UDS responder (CAN TP).")
    ap.add_argument("--rxid", type=parse_int, required=True,
                    help="CAN request id the ECU listens on (gateway TX id), e.g. 0x731")
    ap.add_argument("--txid", type=parse_int, required=True,
                    help="CAN response id the ECU answers on (gateway RX id), e.g. 0x732")
    ap.add_argument("--ll-dl", type=int, default=8,
                    help="CAN frame payload size: 8 for classic CAN, 64 for CAN FD (default: 8)")
    ap.add_argument("--name", default="ECU",
                    help="node name used in log output (default: ECU)")
    ap.add_argument("--device", default="simulator_v2",
                    help="simulator CAN device (default: simulator_v2)")
    args = ap.parse_args()

    from one.AsPy import isotp

    node = isotp(protocol="CAN", device=args.device,
                 rxid=args.rxid, txid=args.txid, LL_DL=args.ll_dl)
    print(f"[{args.name}] listening rxid=0x{args.rxid:03X} txid=0x{args.txid:03X} "
          f"LL_DL={args.ll_dl} on {args.device} (Ctrl-C to stop)")

    while True:
        req = node.receive()
        if not req:
            continue
        print(f"[{args.name}] request:", ["%02X" % x for x in req])
        res = [0x40 | req[0]] + list(req[1:])
        if req[0] == 0x10:
            res += [0x00, 0x32, 0x01, 0xF4]
        node.transmit(bytes(res))
        print(f"[{args.name}] response:", ["%02X" % x for x in res])


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nstopped.")
