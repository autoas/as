#!/usr/bin/env python3
"""Single-node dummy UDS responder for the DoIP-to-CAN gateway demo.

Run one instance per simulated ECU in a separate shell, e.g.::

    python dummy_ecu.py --RxId 0x731 --TxId 0x732 --LinkLayerDataLength 64 --Name ECU_A
    python dummy_ecu.py --RxId 0x741 --TxId 0x742 --LinkLayerDataLength 64 --Name ECU_B
    python dummy_ecu.py --RxId 0x757 --TxId 0x75F --LinkLayerDataLength 8  --Name ECU_C

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
    ap.add_argument("--RxId", "--rxid", dest="RxId", type=parse_int, required=True,
                    help="CAN request id the ECU listens on (gateway TX id), e.g. 0x731")
    ap.add_argument("--TxId", "--txid", dest="TxId", type=parse_int, required=True,
                    help="CAN response id the ECU answers on (gateway RX id), e.g. 0x732")
    ap.add_argument("--LinkLayerDataLength", "--ll-dl", dest="LinkLayerDataLength", type=int, default=8,
                    help="CAN frame payload size: 8 for classic CAN, 64 for CAN FD (default: 8)")
    ap.add_argument("--Name", "--name", dest="Name", default="ECU",
                    help="node name used in log output (default: ECU)")
    ap.add_argument("--Device", "--device", dest="Device", default="simulator_v2",
                    help="simulator CAN device (default: simulator_v2)")
    args = ap.parse_args()

    from one.AsPy import isotp

    node = isotp(Protocol="CAN", Device=args.Device,
                 RxId=args.RxId, TxId=args.TxId, LinkLayerDataLength=args.LinkLayerDataLength)
    print(f"[{args.Name}] listening rxid=0x{args.RxId:03X} txid=0x{args.TxId:03X} "
          f"LinkLayerDataLength={args.LinkLayerDataLength} on {args.Device} (Ctrl-C to stop)")

    while True:
        req = node.receive()
        if not req:
            continue
        print(f"[{args.Name}] request:", ["%02X" % x for x in req])
        res = [0x40 | req[0]] + list(req[1:])
        if req[0] == 0x10:
            res += [0x00, 0x32, 0x01, 0xF4]
        node.transmit(bytes(res))
        print(f"[{args.Name}] response:", ["%02X" % x for x in res])


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nstopped.")
