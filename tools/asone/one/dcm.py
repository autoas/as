# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 Parai Wang <parai@foxmail.com>

import time
import os
import sys
from .AsPy import isotp


class dcm():
    __service__ = {0x10: "diagnostic session control", 0x11: "ecu reset", 0x14: "clear diagnostic information",
                   0x19: "read dtc information", 0x22: "read data by identifier", 0x23: "read memory by address",
                   0x24: "read scaling data by identifier", 0x27: "security access", 0x28: "communication control",
                   0x2A: "read data by periodic identifier", 0x2C: "dynamically define data identifier", 0x2E: "write data by identifier",
                   0x2F: "input output control by identifier", 0x31: "routine control", 0x34: "request download",
                   0x35: "request upload", 0x36: "transfer data", 0x37: "request transfer exit",
                   0x3D: "write memory by address", 0x3E: "tester present", 0x7F: "negative response",
                   0x85: "control dtc setting", 0x01: "request current powertrain diagnostic data", 0x02: "request powertrain freeze frame data",
                   0x04: "clear emission related diagnostic information", 0x03: "request emission related diagnostic trouble codes", 0x07: "request emission related diagnostic trouble codes detected during current or last completed driving cycle",
                   0x09: "request vehicle information"}

    __nrc__ = {0x10: "general reject", 0x21: "busy repeat request", 0x22: "conditions not correct",
               0x24: "request sequence error", 0x31: "request out of range", 0x33: "secutity access denied",
               0x35: "invalid key", 0x36: "exceed number of attempts", 0x37: "required time delay not expired", 0x72: "general programming failure", 0x73: "wrong block sequence counter",
               0x7E: "sub function not supported in active session", 0x81: "rpm too high", 0x82: "rpm to low",
               0x83: "engine is running", 0x84: "engine is not running", 0x85: "engine run time too low",
               0x86: "temperature too high", 0x87: "temperature too low", 0x88: "vehicle speed too high",
               0x89: "vehicle speed too low", 0x8A: "throttle pedal too high", 0x8B: "throttle pedal too low",
               0x8C: "transmission range not in neutral", 0x8D: "transmission range not in gear", 0x8F: "brake switch not closed",
               0x90: "shifter lever not in park", 0x91: "torque converter clutch locked", 0x92: "voltage too high",
               0x93: "voltage too low", 0x00: "positive response", 0x11: "service not supported",
               0x12: "sub function not supported", 0x13: "incorrect message length or invalid format", 0x78: "response pending",
               0x7F: "service not supported in active session"}
    # service list which support sub function
    __sbr__ = [0x3E]

    def __init__(self, **kwargs):
        self.tp = isotp(**kwargs)
        self.last_error = None
        self.last_reponse = None

    def __get_service_name__(self, serviceid):
        try:
            service = self.__service__[serviceid]
        except KeyError:
            service = "unknown(%X)" % (serviceid)

        return service

    def __get_nrc_name__(self, nrc):
        try:
            name = self.__nrc__[nrc]
        except KeyError:
            name = "unknown(%X)" % (nrc)

        return name

    def __show_negative_response__(self, res):
        if ((res[0] == 0x7f) and (len(res) == 3)):
            service = self.__get_service_name__(res[1])
            nrc = self.__get_nrc_name__(res[2])
            self.last_error = "  >> service '%s' negative response '%s' " % (
                service, nrc)
            print(self.last_error)
        else:
            print("unknown response")

    def __show_request__(self, req):
        ss = "  >> dcm request %s = [" % (self.__get_service_name__(req[0]))
        ss += ','.join(['%02X' % (d) for d in req[:32]])
        if len(req) > 32:
            ss += ",..."
        ss += ']'
        ss += " len = %d" % (len(req))
        print(ss)

    def __show_response__(self, res):
        ss = "  >> dcm response = ["
        ss += ','.join(['%02X' % (d) for d in res[:32]])
        if len(res) > 32:
            ss += ",..."
        ss += ']'
        ss += " len = %d" % (len(res))
        self.last_reponse = ss
        print(ss)

    def transmit(self, req):
        self.last_error = None
        self.last_reponse = None
        response = None
        self.__show_request__(req)
        ercd = self.tp.transmit(bytes(req))
        if ((len(req) >= 2) and (req[0] in self.__sbr__) and ((req[1] & 0x80) != 0)):
            # suppress positive response
            return True, [req[0] | 0x40]
        while (ercd == True):
            res = self.tp.receive()
            if (res != None):
                self.__show_response__(res)
                if (req[0] | 0x40 == res[0]):
                    # positive response
                    response = res
                    break
                elif ((len(res) == 3) and (res[0] == 0x7f) and (res[1] == req[0]) and (res[2] == 0x78)):
                    # response is pending as server is busy
                    continue
                else:
                    self.__show_negative_response__(res)
                    response = res
                    ercd = False
            else:
                ercd = False
        return ercd, response
