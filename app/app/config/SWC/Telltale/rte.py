# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>
from generator import asar
import autosar
from bswcom import *


class InactiveActive_T(autosar.Template):
    valueTable = ['InactiveActive_Inactive',
                  'InactiveActive_Active',
                  'InactiveActive_Error',
                  'InactiveActive_NotAvailable']

    @classmethod
    def apply(cls, ws):
        package = ws.getDataTypePackage()
        if package.find(cls.__name__) is None:
            package.createIntegerDataType(
                cls.__name__, valueTable=cls.valueTable)


class OnOff_T(autosar.Template):
    valueTable = ['OnOff_Off',
                  'OnOff_On',
                  'OnOff_1Hz',
                  'OnOff_2Hz',
                  'OnOff_3Hz']

    @classmethod
    def apply(cls, ws):
        package = ws.getDataTypePackage()
        if package.find(cls.__name__) is None:
            package.createIntegerDataType(
                cls.__name__, valueTable=cls.valueTable)


ttList = ['TPMS', 'LowOil', 'PosLamp', 'TurnLeft', 'TurnRight', 'AutoCruise', 'HighBeam',
          'SeatbeltDriver', 'SeatbeltPassenger', 'Airbag']
# TelltaleStatus: means the COM signals which control the related Telltale status
# TelltaleState: means the state of Telltale: on, off or flash
TELLTALE_D = []
for tt in ttList:
    TELLTALE_D.append(asar.createDataElementTemplate(
        '%sState' % (tt), OnOff_T))
TELLTALE_I = asar.createSenderReceiverInterfaceTemplate(
    'TELLTALE_I', TELLTALE_D)
TELLTALE_P = {}
for tt in ttList:
    C_TELLTALE_IV = asar.createConstantTemplateFromEnumerationType(
        'C_Telltale%sStatus_IV' % (tt), OnOff_T, 3)
    TELLTALE_P[tt] = asar.createSenderReceiverPortTemplate(
        'Telltale', TELLTALE_I, C_TELLTALE_IV, elemName='%sState' % (tt))


class Telltale(autosar.Template):
    @classmethod
    def apply(cls, ws):
        componentName = cls.__name__
        package = ws.getComponentTypePackage()
        if package.find(componentName) is None:
            swc = package.createApplicationSoftwareComponent(componentName)
            cls.addPorts(swc)
            cls.addBehavior(swc)

    @classmethod
    def addPorts(cls, swc):
        componentName = cls.__name__
        for name, p in TELLTALE_P.items():
            swc.apply(p.Send)
        swc.apply(Led1Sts.Receive)
        swc.apply(Led2Sts.Receive)
        swc.apply(Led3Sts.Receive)

    @classmethod
    def addBehavior(cls, swc):
        swc.behavior.createRunnable('Telltale_run', portAccess=['%s/%s' % (
            p.name, p.comspec[0].name) for p in swc.requirePorts+swc.providePorts])
        swc.behavior.createTimingEvent('Telltale_run', period=20)


def main(dir):
    asar.Gen(Telltale, dir)
