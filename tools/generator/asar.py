# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>

import autosar


class Workspace(autosar.Workspace):
    def __init__(self, **kwargs):
        version = kwargs.get('version', 3.0)
        patch = kwargs.get('patch', 2)
        schema = kwargs.get('schema', None)
        if schema is None and ((version == 3.0 and patch == 2) or (version == "3.0.2")):
            schema = 'autosar_302_ext.xsd'
        super().__init__(version, patch, schema)
        self.defaultPackages = {'DataType': 'DataType',
                                'CompuMethod': 'DataTypeSemantics',
                                'Unit': 'DataTypeUnits',
                                'Constant': 'Constant',
                                'PortInterface': 'PortInterface',
                                'ModeDclrGroup': 'ModeDclrGroup',
                                'ComponentType': 'ComponentType',
                                }

    def apply(self, template, **kwargs):
        """
        Applies template to this workspace
        """
        if len(kwargs) == 0:
            rv = template.apply(self)
        else:
            rv = template.apply(self, **kwargs)
        template.usageCount += 1
        return rv

    @classmethod
    def _createDefaultDataTypes(cls, package):
        package.createBooleanDataType('Boolean')
        package.createIntegerDataType('SInt8', -128, 127)
        package.createIntegerDataType('SInt16', -32768, 32767)
        package.createIntegerDataType('SInt32', -2147483648, 2147483647)
        package.createIntegerDataType('UInt8', 0, 255)
        package.createIntegerDataType('UInt16', 0, 65535)
        package.createIntegerDataType('UInt32', 0, 4294967295)
        package.createRealDataType(
            'Float', None, None, minValType='INFINITE', maxValType='INFINITE')
        package.createRealDataType('Double', None, None, minValType='INFINITE',
                                   maxValType='INFINITE', hasNaN=True, encoding='DOUBLE')

    def getDataTypePackage(self):
        """
        Returns the current data type package from the workspace. If the workspace doesn't yet have such package a default package will be created and returned.
        """
        package = self.find(self.defaultPackages["DataType"])
        if package is None:
            package = self.createPackage(
                self.defaultPackages["DataType"], role="DataType")
            package.createSubPackage(
                self.defaultPackages["CompuMethod"], role="CompuMethod")
            package.createSubPackage(self.defaultPackages["Unit"], role="Unit")
            Workspace._createDefaultDataTypes(package)
        return package

    def getPortInterfacePackage(self):
        """
        Returns the current port interface package from the workspace. If the workspace doesn't yet have such package a default package will be created and returned.
        """
        package = self.find(self.defaultPackages["PortInterface"])
        if package is None:
            package = self.createPackage(
                self.defaultPackages["PortInterface"], role="PortInterface")
        return package

    def getConstantPackage(self):
        """
        Returns the current constant package from the workspace. If the workspace doesn't yet have such package, a default package will be created and returned.
        """
        package = self.find(self.defaultPackages["Constant"])
        if package is None:
            package = self.createPackage(
                self.defaultPackages["Constant"], role="Constant")
        return package

    def getModeDclrGroupPackage(self):
        """
        Returns the current mode declaration group package from the workspace. If the workspace doesn't yet have such package, a default package will be created and returned.
        """
        package = self.find(self.defaultPackages["ModeDclrGroup"])
        if package is None:
            package = self.createPackage(
                self.defaultPackages["ModeDclrGroup"], role="ModeDclrGroup")
        return package

    def getComponentTypePackage(self):
        """
        Returns the current component type package from the workspace. If the workspace doesn't yet have such package, a default package will be created and returned.
        """
        if self.roles["ComponentType"] is not None:
            packageName = self.roles["ComponentType"]
        else:
            packageName = self.defaultPackages["ComponentType"]
        package = self.find(packageName)
        if package is None:
            package = self.createPackage(packageName, role="ComponentType")
        return package


def __createDataTypeFromTemplate(name, min, max):
    @classmethod
    def apply(cls, ws):
        package = ws.getDataTypePackage()
        if package.find(cls.__name__) is None:
            package.createIntegerDataType(
                cls.__name__, min=cls.minValue, max=cls.maxValue, offset=0, scaling=1, unit='1')
    return type(name, (autosar.Template,), dict(minValue=min, maxValue=max, apply=apply))


def __createArrayDataTypeFromTemplate(name, typeRef, arraySize=8):
    @classmethod
    def apply(cls, ws):
        package = ws.getDataTypePackage()
        if package.find(cls.__name__) is None:
            typeDef = package.find(typeRef.__name__)
            package.createArrayDataType(cls.__name__, typeDef.ref, arraySize)
    return type(name, (autosar.Template,), dict(typeRef=typeRef, apply=apply))


def createConstantTemplateFromEnumerationType(name, dataTypeTemplate, default=None):
    @classmethod
    def apply(cls, ws):
        package = ws.getConstantPackage()
        if package.find(cls.__name__) is None:
            ws.apply(cls.dataTypeTemplate)
            if(cls.default is None):
                cls.default = len(cls.dataTypeTemplate.valueTable)-1
            package.createConstant(
                cls.__name__, cls.dataTypeTemplate.__name__, cls.default)
    return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply, default=default))


def createConstantTemplateFromPhysicalType(name, dataTypeTemplate, default=None):
    @classmethod
    def apply(cls, ws):
        package = ws.getConstantPackage()
        if package.find(cls.__name__) is None:
            ws.apply(cls.dataTypeTemplate)
            if(cls.default is None):
                cls.default = cls.dataTypeTemplate.maxValue
            package.createConstant(
                cls.__name__, cls.dataTypeTemplate.__name__, cls.default)
    return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply, default=default))


def DataElement(name, typeRef, isQueued=False, softwareAddressMethodRef=None, swCalibrationAccess=None, swImplPolicy=None, parent=None, adminData=None):
    return autosar.portinterface.DataElement(name, typeRef, isQueued, softwareAddressMethodRef, swCalibrationAccess, swImplPolicy, parent, adminData)


def createDataElementTemplate(name, dataTypeTemplate, default=None):
    @classmethod
    def apply(cls, ws):
        ws.apply(cls.dataTypeTemplate)
        return DataElement(name, dataTypeTemplate.__name__)
    return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply))


def createSenderReceiverInterfaceTemplate(name, dataTypeTemplate, dataName=None):
    @classmethod
    def apply(cls, ws):
        package = ws.getPortInterfacePackage()
        if package.find(name) is None:
            if(cls.dataName is None):
                cls.dataName = name
            if(type(cls.dataTypeTemplate) == list):
                dataElements = []
                for dataTypeTemplate in cls.dataTypeTemplate:
                    dataElements.append(ws.apply(dataTypeTemplate))
                package.createSenderReceiverInterface(name, dataElements)
            else:
                ws.apply(cls.dataTypeTemplate)
                package.createSenderReceiverInterface(name, autosar.DataElement(
                    cls.dataName, cls.dataTypeTemplate.__name__))
    return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply, dataName=dataName))


def _createProvidePortHelper(swc, name, portInterfaceTemplate, initValueTemplate=None, elemName=None):
    ws = swc.rootWS()
    ws.apply(portInterfaceTemplate)
    if initValueTemplate is not None:
        ws.apply(initValueTemplate)
        swc.createProvidePort(name, portInterfaceTemplate.__name__,
                              dataElement=elemName, initValueRef=initValueTemplate.__name__)
    else:
        swc.createProvidePort(name, portInterfaceTemplate.__name__)


def _createRequirePortHelper(swc, name, portInterfaceTemplate, initValueTemplate=None, aliveTimeout=0, elemName=None):
    ws = swc.rootWS()
    ws.apply(portInterfaceTemplate)
    if initValueTemplate is not None:
        ws.apply(initValueTemplate)
        swc.createRequirePort(name, portInterfaceTemplate.__name__, dataElement=elemName,
                              initValueRef=initValueTemplate.__name__, aliveTimeout=aliveTimeout)
    else:
        swc.createRequirePort(
            name, portInterfaceTemplate.__name__, aliveTimeout=aliveTimeout)


def _createProvidePortTemplate(innerClassName, templateName, portInterfaceTemplate, initValueTemplate, elemName=None):
    @classmethod
    def apply(cls, swc):
        _createProvidePortHelper(
            swc, cls.name, cls.portInterfaceTemplate, cls.initValueTemplate, cls.elemName)
    return type(innerClassName, (autosar.Template,), dict(name=templateName, portInterfaceTemplate=portInterfaceTemplate, initValueTemplate=initValueTemplate, apply=apply, elemName=elemName))


def _createRequirePortTemplate(innerClassName, templateName, portInterfaceTemplate, initValueTemplate, aliveTimeout=0, elemName=None):
    @classmethod
    def apply(cls, swc):
        _createRequirePortHelper(swc, cls.name, cls.portInterfaceTemplate,
                                 cls.initValueTemplate, cls.aliveTimeout, cls.elemName)
    return type(innerClassName, (autosar.Template,), dict(name=templateName, portInterfaceTemplate=portInterfaceTemplate, initValueTemplate=initValueTemplate, aliveTimeout=aliveTimeout, apply=apply, elemName=elemName))

# name is the port name


def createSenderReceiverPortTemplate(name, portInterfaceTemplate, initValueTemplate=None, aliveTimeout=0, elemName=None):
    return type(name, (), dict(Provide=_createProvidePortTemplate('Provide', name, portInterfaceTemplate, initValueTemplate, elemName),
                               Send=_createProvidePortTemplate(
                                   'Send', name, portInterfaceTemplate, initValueTemplate, elemName),
                               Require=_createRequirePortTemplate(
                                   'Require', name, portInterfaceTemplate, initValueTemplate, aliveTimeout, elemName),
                               Receive=_createRequirePortTemplate('Receive', name, portInterfaceTemplate, initValueTemplate, aliveTimeout, elemName)))


SINT8_T = __createDataTypeFromTemplate('SINT8_T', -128, 127)
UINT8_T = __createDataTypeFromTemplate('UINT8_T', 0, 255)
SINT16_T = __createDataTypeFromTemplate('SINT16_T', -32768, 32767)
UINT16_T = __createDataTypeFromTemplate('UINT16_T', 0, 65535)
SINT32_T = __createDataTypeFromTemplate('SINT32_T', -2147483648, 2147483647)
UINT32_T = __createDataTypeFromTemplate('UINT32_T', 0, 4294967295)
UINT8_N_T = __createArrayDataTypeFromTemplate('UINT8_N_T', UINT8_T)
UINT16_N_T = __createArrayDataTypeFromTemplate('UINT16_N_T', UINT16_T)
UINT32_N_T = __createArrayDataTypeFromTemplate('UINT32_N_T', UINT32_T)
SINT8_N_T = __createArrayDataTypeFromTemplate('SINT8_N_T', SINT8_T)
SINT16_N_T = __createArrayDataTypeFromTemplate('SINT16_N_T', SINT16_T)
SINT32_N_T = __createArrayDataTypeFromTemplate('SINT32_N_T', SINT32_T)


def Gen(cls, dir):
    ws = Workspace()
    ws.apply(cls)
    ws.saveXML('%s/%s.arxml' % (dir, cls.__name__))
    partition = autosar.rte.Partition()
    pkg = ws.getComponentTypePackage()
    partition.addComponent(pkg[cls.__name__])
    rtegen = autosar.rte.TypeGenerator(partition)
    rtegen.generate(dir)
    rtegen = autosar.rte.ComponentHeaderGenerator(partition)
    rtegen.generate(dir)
    print('GEN RTE Interface for', cls.__name__)
