import os
import sys
import json
import random
import time

CWD = os.path.dirname(__file__)
if CWD == "":
    CWD = os.path.abspath(".")

ASONE = os.path.abspath("%s/../../../asone" % (CWD))
sys.path.append(ASONE)

from one.dcm import dcm

uds = None


def GetProp(first, second, attr, dft):
    return first.get(attr, second.get(attr, dft))


def proc_cfg(cfg):
    idx = 0
    for dtc in cfg["DTCs"]:
        events = dtc.get("events", [])
        if len(events) == 0:
            dtc["idx"] = idx
            idx = idx + 1
        else:
            for event in events:
                event["idx"] = idx
                idx = idx + 1
    conditions = []
    for dtc in cfg["DTCs"]:
        for c in dtc.get("conditions", []):
            if c not in conditions:
                conditions.append(c)
    cfg["conditions"] = conditions
    cfg["snapshots"] = {}
    for env in cfg["Environments"]:
        cfg["snapshots"][eval(env["id"])] = env


def wait_nvm_done():
    MEMIF_IDLE = 1
    MEMIF_BUSY = 2
    ercd, response = uds.transmit([0x31, 0x03, 0xFE, 0xEF])
    assert True == ercd
    status = response[4]
    while MEMIF_IDLE != status:
        time.sleep(1)
        ercd, response = uds.transmit([0x31, 0x03, 0xFE, 0xEF])
        assert True == ercd
        status = response[4]


def power_off():
    # a Ecu Reset to simulate power off
    ercd, response = uds.transmit([0x11, 0x03])
    assert True == ercd
    time.sleep(2)


def power_on():
    ercd, response = uds.transmit([0x10, 0x01])
    while False == ercd:
        time.sleep(1)
        ercd, response = uds.transmit([0x10, 0x01])


def clear_all_dtc():
    ercd, response = uds.transmit([0x14, 0xFF, 0xFF, 0xFF])
    assert True == ercd
    wait_nvm_done()
    power_off()
    power_on()


DEM_OPERATION_CYCLE_POWER = 0
DEM_OPERATION_CYCLE_IGNITION = 1

DEM_OPERATION_CYCLE_STOPPED = 0
DEM_OPERATION_CYCLE_STARTED = 1


def set_operation_cycle_state(operationCycleId, cycleState):
    ercd, response = uds.transmit([0x31, 0x01, 0xDE, 0xED, 0x00, operationCycleId, cycleState])
    assert True == ercd


def set_condition(condition, enabled):
    ercd, response = uds.transmit([0x31, 0x01, 0xDE, 0xED, 0x01, condition, enabled])
    assert True == ercd


def start_all_operation_cycles():
    set_operation_cycle_state(DEM_OPERATION_CYCLE_POWER, DEM_OPERATION_CYCLE_STARTED)
    set_operation_cycle_state(DEM_OPERATION_CYCLE_IGNITION, DEM_OPERATION_CYCLE_STARTED)


def enable_all_conditions(cfg):
    for idx, condition in enumerate(cfg["conditions"]):
        set_condition(idx, 1)


def disable_all_conditions(cfg):
    for idx, condition in enumerate(cfg["conditions"]):
        set_condition(idx, 0)


def stop_all_operation_cycles():
    set_operation_cycle_state(DEM_OPERATION_CYCLE_POWER, DEM_OPERATION_CYCLE_STOPPED)
    set_operation_cycle_state(DEM_OPERATION_CYCLE_IGNITION, DEM_OPERATION_CYCLE_STOPPED)


DEM_EVENT_STATUS_PASSED = 0
DEM_EVENT_STATUS_FAILED = 1
DEM_EVENT_STATUS_PREPASSED = 2
DEM_EVENT_STATUS_PREFAILED = 3


def set_event_staus(eventId, status, nTimes=1):
    for i in range(nTimes):
        ercd, response = uds.transmit([0x31, 0x01, 0xDE, 0xED, 0x02, (eventId >> 8) & 0xFF, eventId & 0xFF, status])
        assert True == ercd


DEM_UDS_STATUS_TF = 0x01
DEM_UDS_STATUS_TFTOC = 0x02
DEM_UDS_STATUS_PDTC = 0x04
DEM_UDS_STATUS_CDTC = 0x08
DEM_UDS_STATUS_TNCSLC = 0x10
DEM_UDS_STATUS_TFSLC = 0x20
DEM_UDS_STATUS_TNCTOC = 0x40
DEM_UDS_STATUS_WIR = 0x80


def report_dtc_by_stats_mask(status):
    ercd, response = uds.transmit([0x19, 0x02, status])
    assert True == ercd
    record = response[3:]
    num = len(record) // 4
    dtcStatus = {}
    for i in range(num):
        rec = record[i * 4 : (i + 1) * 4]
        number = (rec[0] << 16) + (rec[1] << 8) + rec[2]
        status = rec[3]
        dtcStatus[number] = status
    return dtcStatus


def check_dtc_is_status(dtc, expectedStatus):
    dtcStatus = report_dtc_by_stats_mask(0xFF)
    status = dtcStatus.get(eval(dtc["number"]), 0)
    if expectedStatus == status:
        print("  PASS:", dtc["name"], "%02X == %02X OK" % (status, expectedStatus))
    else:
        print("  FAIL:", dtc["name"], "%02X != %02X NG" % (status, expectedStatus))
        raise


def decodeData(data, record, rec):
    assert data["type"] in ["uint8", "uint16", "uint32", "uint64"]
    length = int(eval(data["type"][4:]) / 8)
    value = 0
    for j in range(length):
        value = (value << 8) + record[j]
    print("\t    %s = %s (%s)" % (data["name"], value, [hex(s) for s in record[:length]]))
    rec[data["name"]] = value
    return record[length:]


def check_dtc_has_snapshot(dtc, cfg):
    number = eval(dtc["number"])
    ercd, response = uds.transmit([0x19, 0x04, (number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF, 0xFF])
    assert True == ercd
    status = response[5]
    print("    Snapshot Status = %02X" % (status))
    record = response[6:]
    recs = {}
    while len(record) > 0:
        rec = {}
        recnum = record[0]
        print("\tRecordNumber = %d" % (record[0]))
        print("\tNumber Of Datas = %d" % (record[1]))
        num = record[1]
        record = record[2:]
        for i in range(num):
            idx = (record[0] << 8) + record[1]
            record = record[2:]
            data = cfg["snapshots"][idx]
            if data["type"] in ["uint8", "uint16", "uint32", "uint64"]:
                record = decodeData(data, record, rec)
            elif data["type"] == "struct":
                for sd in data["data"]:
                    record = decodeData(sd, record, rec)
        recs[recnum] = rec
    return recs


def check_dtc_has_extend(dtc, cfg):
    ExtendedDatas = cfg["ExtendedDatas"]
    number = eval(dtc["number"])
    ercd, response = uds.transmit([0x19, 0x06, (number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF, 0xFF])
    assert True == ercd
    status = response[5]
    print("    Extended Data Status = %02X" % (status))
    record = response[6:]
    recs = {}
    while len(record) > 0:
        recnum = record[0]
        print("\tRecordNumber = %d" % (recnum))
        num = len(ExtendedDatas)
        rec = {}
        record = record[1:]
        for i in range(num):
            data = ExtendedDatas[i]
            record = decodeData(data, record, rec)
        recs[recnum] = rec
    return recs


def ASSERT_EQ(a, b):
    if a != b:
        raise Exception("%s != %s" % (a, b))

def ASSERT_NE(a, b):
    if a == b:
        raise Exception("%s != %s" % (a, b))

def test_dtc_status_case1(cfg):
    MaxFreezeFrameNum = cfg.get("MaxFreezeFrameNum", 2)
    power_on()
    clear_all_dtc()
    start_all_operation_cycles()
    enable_all_conditions(cfg)
    general = cfg.get("general", {})
    # test DTC pre-fail with debounce
    for dtc in cfg["DTCs"]:
        if "idx" not in dtc:
            continue  # skip combined DTC
        print("Status Test Fail for DTC", dtc["name"], dtc["number"])
        idx = dtc["idx"]
        DebounceCounterIncrementStepSize = GetProp(dtc, general, "DebounceCounterIncrementStepSize", 1)
        DebounceCounterFailedThreshold = GetProp(dtc, general, "DebounceCounterFailedThreshold", 10)
        DebounceCounterJumpDown = GetProp(dtc, general, "DebounceCounterJumpDown", False)
        DebounceCounterPassedThreshold = GetProp(dtc, general, "DebounceCounterPassedThreshold", -10)
        DebounceCounterDecrementStepSize = GetProp(dtc, general, "DebounceCounterDecrementStepSize", 2)
        ConfirmationThreshold = GetProp(dtc, general, "ConfirmationThreshold", 1)
        AgingCycleCounterThreshold = GetProp(dtc, general, "AgingCycleCounterThreshold", 2)
        FreezeFrameRecordTrigger = GetProp(dtc, general, "FreezeFrameRecordTrigger", "TEST_FAILED")
        ExtendedDataRecordTrigger = GetProp(dtc, general, "ExtendedDataRecordTrigger", "TEST_FAILED")
        nTimes = DebounceCounterFailedThreshold // DebounceCounterIncrementStepSize
        set_event_staus(idx, DEM_EVENT_STATUS_PREFAILED, nTimes - 1)
        check_dtc_is_status(dtc, DEM_UDS_STATUS_TNCTOC | DEM_UDS_STATUS_TNCSLC)
        set_event_staus(idx, DEM_EVENT_STATUS_PREFAILED, 1)
        if ConfirmationThreshold > 1:
            status = DEM_UDS_STATUS_TF | DEM_UDS_STATUS_TFTOC | DEM_UDS_STATUS_TFSLC | DEM_UDS_STATUS_PDTC
        else:
            status = (
                DEM_UDS_STATUS_TF
                | DEM_UDS_STATUS_TFTOC
                | DEM_UDS_STATUS_TFSLC
                | DEM_UDS_STATUS_PDTC
                | DEM_UDS_STATUS_CDTC
            )
        check_dtc_is_status(dtc, status)
        if FreezeFrameRecordTrigger == "TEST_FAILED":
            recs = check_dtc_has_snapshot(dtc, cfg)
            ASSERT_EQ(len(recs.keys()), 1)
        if ExtendedDataRecordTrigger == "TEST_FAILED":
            recs = check_dtc_has_extend(dtc, cfg)
            ASSERT_EQ(len(recs.keys()), 1)
            ASSERT_EQ(recs[1]["FaultOccuranceCounter"], 1)
        for i in range(ConfirmationThreshold - 1):
            stop_all_operation_cycles()
            start_all_operation_cycles()
            set_event_staus(idx, DEM_EVENT_STATUS_PREFAILED, nTimes)
        status = (
            DEM_UDS_STATUS_TF | DEM_UDS_STATUS_TFTOC | DEM_UDS_STATUS_TFSLC | DEM_UDS_STATUS_PDTC | DEM_UDS_STATUS_CDTC
        )
        check_dtc_is_status(dtc, status)
        if FreezeFrameRecordTrigger == "TEST_FAILED":
            if ConfirmationThreshold == 1:  # ensure 2nd capture
                stop_all_operation_cycles()
                start_all_operation_cycles()
                set_event_staus(idx, DEM_EVENT_STATUS_PREFAILED, nTimes)
            recs = check_dtc_has_snapshot(dtc, cfg)
            ASSERT_EQ(len(recs.keys()), MaxFreezeFrameNum)
        if ExtendedDataRecordTrigger == "TEST_FAILED":
            recs = check_dtc_has_extend(dtc, cfg)
            ASSERT_EQ(len(recs.keys()), 1)
            if ConfirmationThreshold == 1:
                ASSERT_EQ(recs[1]["FaultOccuranceCounter"], 2)
            else:
                ASSERT_EQ(recs[1]["FaultOccuranceCounter"], ConfirmationThreshold)
        print("Status Test Pass for DTC", dtc["name"], dtc["number"])
        if DebounceCounterJumpDown:
            nTimes = 1 + (-DebounceCounterPassedThreshold) // DebounceCounterDecrementStepSize
        else:
            nTimes = (
                DebounceCounterFailedThreshold - DebounceCounterPassedThreshold
            ) // DebounceCounterDecrementStepSize
        set_event_staus(idx, DEM_EVENT_STATUS_PREPASSED, nTimes)
        check_dtc_is_status(
            dtc, DEM_UDS_STATUS_TFSLC | DEM_UDS_STATUS_PDTC | DEM_UDS_STATUS_CDTC | DEM_UDS_STATUS_TFTOC
        )
        print("Status aging for DTC", dtc["name"], dtc["number"], "aging counter=", AgingCycleCounterThreshold)
        stop_all_operation_cycles()
        for i in range(AgingCycleCounterThreshold):
            start_all_operation_cycles()
            set_event_staus(idx, DEM_EVENT_STATUS_PREPASSED, nTimes)
            stop_all_operation_cycles()
            if i < (AgingCycleCounterThreshold - 1):
                check_dtc_is_status(dtc, DEM_UDS_STATUS_TFSLC | DEM_UDS_STATUS_CDTC)
            else:
                check_dtc_is_status(dtc, 0)
        start_all_operation_cycles()
    disable_all_conditions(cfg)
    stop_all_operation_cycles()


def test_dtc_status_case2(cfg):
    # special test those DTC with FF triggerred when TF
    MaxFreezeFrameNum = cfg.get("MaxFreezeFrameNum", 2)
    power_on()
    clear_all_dtc()

    general = cfg.get("general", {})
    # test DTC pre-fail with debounce
    for dtc in cfg["DTCs"]:
        if "idx" not in dtc:
            continue  # skip combined DTC
        print("Status Test Fail for DTC", dtc["name"], dtc["number"])
        idx = dtc["idx"]
        DebounceCounterIncrementStepSize = GetProp(dtc, general, "DebounceCounterIncrementStepSize", 1)
        DebounceCounterFailedThreshold = GetProp(dtc, general, "DebounceCounterFailedThreshold", 10)
        DebounceCounterJumpDown = GetProp(dtc, general, "DebounceCounterJumpDown", False)
        DebounceCounterPassedThreshold = GetProp(dtc, general, "DebounceCounterPassedThreshold", -10)
        DebounceCounterDecrementStepSize = GetProp(dtc, general, "DebounceCounterDecrementStepSize", 2)
        ConfirmationThreshold = GetProp(dtc, general, "ConfirmationThreshold", 1)
        AgingCycleCounterThreshold = GetProp(dtc, general, "AgingCycleCounterThreshold", 2)
        FreezeFrameRecordTrigger = GetProp(dtc, general, "FreezeFrameRecordTrigger", "TEST_FAILED")
        ExtendedDataRecordTrigger = GetProp(dtc, general, "ExtendedDataRecordTrigger", "TEST_FAILED")
        if FreezeFrameRecordTrigger != 'TEST_FAILED': continue
        nTimes = DebounceCounterFailedThreshold // DebounceCounterIncrementStepSize
        start_all_operation_cycles()
        enable_all_conditions(cfg)
        set_event_staus(idx, DEM_EVENT_STATUS_PREFAILED, nTimes) # make it failed
        rec0 = check_dtc_has_snapshot(dtc, cfg)
        stop_all_operation_cycles()
        time.sleep(1) # make time changed
        start_all_operation_cycles()
        set_event_staus(idx, DEM_EVENT_STATUS_PREFAILED, nTimes) # make it failed
        rec1 = check_dtc_has_snapshot(dtc, cfg)
        ASSERT_NE(str(rec0), str(rec1))

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="DEM Test through UDS")
    parser.add_argument("-c", "--config", type=str, help="DEM Test config json file", required=True)
    parser.add_argument("-t", "--txid", type=str, help="UDS CanTp TxId", default="0x731")
    parser.add_argument("-r", "--rxid", type=str, help="UDS CanTp RxId", default="0x732")
    parser.add_argument("-l", "--lldl", type=str, help="UDS CanTp RxId", default='8')
    args = parser.parse_args()
    uds = dcm(
        protocol="CAN",
        device="simulator_v2",
        port=0,
        txid=eval(args.txid),
        rxid=eval(args.rxid),
        LL_DL=eval(args.lldl),
        verbose=False,
    )
    with open(args.config) as f:
        cfg = json.load(f)
    proc_cfg(cfg)
    test_dtc_status_case1(cfg)
    test_dtc_status_case2(cfg)