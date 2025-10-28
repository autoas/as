# SSAS - Simple Smart Automotive Software
# Copyright (C) 2025 Parai Wang <parai@foxmail.com>

import os
import sys
import autosar.xml
import autosar.xml.template as ar_template
from autosar.xml.template import ElementTemplate as Template
import autosar.xml.element as ar_element
import autosar.xml.workspace as ar_workspace
from demo_system import platform, factory, portinterface
import autosar.xml.enumeration as ar_enum
from autosar.model import ImplementationModel
import autosar.generator
from .helper import *
import logging

logger = logging.getLogger("ARXML")
logger.setLevel(logging.DEBUG)
handler = logging.StreamHandler(sys.stdout)
formatter = logging.Formatter("%(asctime)s.%(msecs)03d - %(name)s - %(levelname)s - %(message)s")
handler.setLevel(logging.DEBUG)
handler.setFormatter(formatter)
logger.addHandler(handler)

NAMESPACE = "AUTOSAR_Platform"

EcuM_CurrentMode_I = portinterface.EcuM_CurrentMode_I


class SenderReceiverInterfaceTemplate(ar_template.ElementTemplate):
    """
    Sender receiver port interface template wit a list of data elements
    It is assumed that the element name ends with "_I"
    """

    def __init__(
        self,
        element_name: str,
        namespace_name: str,
        data_elements: list[(ar_template.ElementTemplate, str)] | None = None,
    ) -> None:
        super().__init__(
            element_name,
            namespace_name,
            ar_enum.PackageRole.PORT_INTERFACE,
            [template for template, _ in data_elements],
        )
        self.data_elements = data_elements

    def create(
        self,
        package: ar_element.Package,
        workspace: ar_workspace.Workspace,
        dependencies: dict[str, ar_element.ARElement] | None,
        **kwargs,
    ) -> ar_element.SenderReceiverInterface:
        """
        Create method
        """
        port_interface = ar_element.SenderReceiverInterface(self.element_name)
        for data_element_template, data_element_name in self.data_elements:
            elem_type = dependencies[data_element_template.ref(workspace)]
            elem_name = data_element_name
            port_interface.create_data_element(elem_name, type_ref=elem_type.ref())
        return port_interface


def apply_platform_types(workspace: ar_workspace.Workspace):
    """
    Applies platform templates
    """
    workspace.apply(platform.ImplementationTypes.boolean)
    workspace.apply(platform.ImplementationTypes.uint8)
    workspace.apply(platform.ImplementationTypes.uint16)
    workspace.apply(platform.ImplementationTypes.uint32)
    workspace.apply(platform.ImplementationTypes.uint64)


def GetArrayInfo(workspace, dataElemType):
    arrayInfo = {}
    for subElem in dataElemType.sub_elements:
        arrayInfo["size"] = subElem.array_size
        for variant in subElem.sw_data_def_props:
            if isinstance(variant, ar_element.SwDataDefPropsConditional):
                if variant.base_type_ref is not None:
                    base_type = workspace.find(variant.base_type_ref)
                    arrayInfo["type"] = base_type
                elif variant.impl_data_type_ref is not None:
                    impl_data_type = workspace.find(variant.impl_data_type_ref)
                    arrayInfo["type"] = impl_data_type
                else:
                    raise
    return arrayInfo


def GenRteTypeDef(workspace, elem, H, typeDefs, typeSkip):
    basic_types = ["boolean", "uint8", "uint16", "uint32", "uint64", "sint8", "sint16", "sint32", "sint64"]
    if elem.name in basic_types:
        return
    if elem.sw_data_def_props is not None:
        for variant in elem.sw_data_def_props:
            if isinstance(variant, ar_element.SwDataDefPropsConditional):
                if variant.base_type_ref is not None:
                    base_type = workspace.find(variant.base_type_ref)
                    H.write("\n#define Rte_TypeDef_%s\n" % (elem.name))
                    H.write("typedef %s %s;\n" % (base_type.name, elem.name))
                else:
                    raise
                if variant.data_constraint_ref is not None:
                    data_constraint = workspace.find(variant.data_constraint_ref)
                    for rule in data_constraint.rules:
                        if rule.internal is not None:
                            H.write(f"#define {elem.name}_LowerLimit (({elem.name}){rule.internal.lower_limit})\n")
                            H.write(f"#define {elem.name}_UpperLimit (({elem.name}){rule.internal.upper_limit})\n")
                        else:
                            raise
                if variant.compu_method_ref is not None:
                    compu_method = workspace.find(variant.compu_method_ref)
                    if compu_method.int_to_phys is not None:
                        for scale in compu_method.int_to_phys.compu_scales:
                            H.write(f"#define {scale.label} (({elem.name}){scale.lower_limit}u)\n")
            else:
                raise
    elif elem.category == "ARRAY":
        arrayInfo = GetArrayInfo(workspace, elem)
        base_type = arrayInfo["type"]
        array_size = arrayInfo["size"]
        if base_type.name not in typeDefs:
            typeSkip.append(elem)
            return
        H.write("\n#define Rte_TypeDef_%s\n" % (elem.name))
        H.write(f"typedef {base_type.name} {elem.name}[{array_size}];\n")
    else:
        raise
    typeDefs.append(elem.name)


def GetPortDataElement(workspace, port):
    port_interface_ref = port.port_interface_ref
    port_interface = workspace.find(port_interface_ref)
    if isinstance(port_interface, ar_element.ModeSwitchInterface):
        logger.warning(f"Port ModeSwitchInterface {port_interface_ref} not supported for port {port.name}")
        return None
    if port_interface is None:
        logger.warning(f"Port interface {port_interface_ref} not found for port {port.name}")
        return None
    if len(port_interface.data_elements) == 1:
        return port_interface.data_elements[0]
    for com_spec in port.com_spec:
        if com_spec.data_element_ref is not None:
            dataElemName = com_spec.data_element_ref.value.split("/")[-1]
            for data_elem in port_interface.data_elements:
                if data_elem.name == dataElemName:
                    return data_elem
    raise Exception(f"Port {port.name} has no data element defined in interface {port_interface.name}")


def GetPortDataElement_T(workspace, port, ImplementationDataTypes):
    dataElem = GetPortDataElement(workspace, port)
    if dataElem != None:
        dataElemType = workspace.find(dataElem.type_ref)
        if isinstance(dataElemType, ar_element.ImplementationDataType):
            if dataElemType not in ImplementationDataTypes:
                ImplementationDataTypes.append(dataElemType)
            if dataElemType.category == "ARRAY":
                arrayInfo = GetArrayInfo(workspace, dataElemType)
                dtype = arrayInfo["type"]
                if isinstance(dtype, ar_element.ImplementationDataType):
                    if dtype not in ImplementationDataTypes:
                        ImplementationDataTypes.append(dtype)
    return dataElem


def AnalyzeSwc(workspace, component):
    results = {"IB-DATA-READ": {}, "IB-PORT-READ": {}, "IB-DATA-WRITE": {}, "IB-PORT-WRITE": {}}
    IB = component.internal_behavior
    if IB is not None:
        for RTE in IB.runnables:
            if RTE.data_read_access is not None:
                for data_read in RTE.data_read_access:
                    accessed_variable = data_read.accessed_variable
                    if accessed_variable is not None:
                        ar_variable_iref = accessed_variable.ar_variable_iref
                        if ar_variable_iref is not None:
                            port_prototype_ref = ar_variable_iref.port_prototype_ref
                            target_data_prototype_ref = ar_variable_iref.target_data_prototype_ref
                            logger.debug(
                                f"  Read {data_read.name} from {port_prototype_ref} -> {target_data_prototype_ref}"
                            )
                            if target_data_prototype_ref is not None:
                                results["IB-DATA-READ"][target_data_prototype_ref.value] = RTE.name
                            if port_prototype_ref is not None:
                                results["IB-PORT-READ"][port_prototype_ref.value] = RTE.name
            if RTE.data_write_access is not None:
                for data_write in RTE.data_write_access:
                    accessed_variable = data_write.accessed_variable
                    if accessed_variable is not None:
                        ar_variable_iref = accessed_variable.ar_variable_iref
                        if ar_variable_iref is not None:
                            port_prototype_ref = ar_variable_iref.port_prototype_ref
                            target_data_prototype_ref = ar_variable_iref.target_data_prototype_ref
                            logger.debug(
                                f"  Read {data_read.name} from {port_prototype_ref} -> {target_data_prototype_ref}"
                            )
                            if target_data_prototype_ref is not None:
                                results["IB-DATA-WRITE"][target_data_prototype_ref.value] = RTE.name
                            if port_prototype_ref is not None:
                                results["IB-PORT-WRITE"][port_prototype_ref.value] = RTE.name
    return results


def GenRteSwc(workspace, component, dir, ImplementationDataTypes):
    results = AnalyzeSwc(workspace, component)
    H = open(f"{dir}/Rte_{component.name}.h", "w")
    GenHeader(H)
    H.write(f"#ifndef RTE_{component.name.upper()}_H\n")
    H.write(f"#define RTE_{component.name.upper()}_H\n")
    H.write('#include "Rte_Type.h"\n\n')
    for port in component.ports:
        logger.debug(f"port: {port.name} {type(port)}")
        if isinstance(port, ar_element.ProvidePortPrototype) or isinstance(port, ar_element.RequirePortPrototype):
            port_type = ""
            dataElem = GetPortDataElement(workspace, port)
            if dataElem != None:
                dataElemType = workspace.find(dataElem.type_ref)
                port_type = f"({dataElemType.name})"
            for com_spec in port.com_spec:
                if isinstance(com_spec, ar_element.ModeSwitchReceiverComSpec):
                    pass
                elif com_spec.init_value is None:
                    pass
                elif isinstance(com_spec.init_value, ar_element.ConstantReference):
                    constant_ref = com_spec.init_value.constant_ref
                    constant = workspace.find(constant_ref)
                    value = constant.value.value
                    if type(value) is str:
                        H.write(f"#define Rte_InitValue_{port.name}_{dataElem.name} {value}\n")
                    else:
                        H.write(f"#define Rte_InitValue_{port.name}_{dataElem.name} ({port_type}{value}u)\n")
        else:
            raise Exception(f"{port.name}: {type(port)} not support")
    H.write("\n/* Rte_Read_<PortName>_<DataElementName>(<DataType>* value); */\n")
    for port in component.ports:
        if isinstance(port, ar_element.RequirePortPrototype):
            dataElem = GetPortDataElement_T(workspace, port, ImplementationDataTypes)
            if dataElem != None:
                dataElemType = workspace.find(dataElem.type_ref)
                if dataElem.ref().value in results["IB-DATA-READ"]:
                    rte = results["IB-DATA-READ"][dataElem.ref().value]
                    dtype = dataElemType.name
                    if dataElemType.category == "ARRAY":
                        dtype = f"{dataElemType.name}*"
                    H.write(f"{dtype} Rte_IRead_{rte}_{port.name}_{dataElem.name}(void);\n")
                else:
                    H.write(f"Std_ReturnType Rte_Read_{port.name}_{dataElem.name}({dataElemType.name} *data);\n")
    H.write("\n/* Rte_Write_<PortName>_<DataElementName>(<DataType> value); */\n")
    for port in component.ports:
        if isinstance(port, ar_element.ProvidePortPrototype):
            dataElem = GetPortDataElement_T(workspace, port, ImplementationDataTypes)
            if dataElem != None:
                dataElemType = workspace.find(dataElem.type_ref)
                if dataElem.ref().value in results["IB-DATA-WRITE"]:
                    rte = results["IB-DATA-WRITE"][dataElem.ref().value]
                    H.write(f"void Rte_IWrite_{rte}_{port.name}_{dataElem.name}({dataElemType.name} data);\n")
                else:
                    H.write(f"Std_ReturnType Rte_Write_{port.name}_{dataElem.name}({dataElemType.name} data);\n")
    H.write("\n/* Runnable */\n")
    internal_behavior = component.internal_behavior
    if internal_behavior is not None:
        for runable in internal_behavior.runnables:
            H.write(f"void {runable.symbol}(void);\n")
    H.write(f"#endif /* RTE_{component.name.upper()}_H */\n\n")


def GenRteTypes(workspace, dir):
    ImplementationDataTypes = []
    Components = []
    for nname, namespace in workspace.namespaces.items():
        for role_name, rel_path in namespace.package_map.items():
            ref = workspace.get_package_ref_by_role(nname, role_name)
            pkg = workspace.find(rel_path)
            if pkg is not None:
                logger.debug(f"Processing package: {pkg.name} in namespace {nname} with role {role_name}")
                for element in pkg.elements:
                    if isinstance(element, ar_element.ImplementationDataType):
                        ImplementationDataTypes.append(element)
                    elif isinstance(element, ar_element.ApplicationSoftwareComponentType):
                        Components.append(element)

    for component in Components:
        GenRteSwc(workspace, component, dir, ImplementationDataTypes)

    H = open("%s/Rte_Type.h" % (dir), "w")
    GenHeader(H)
    H.write("#ifndef RTE_TYPE_H\n")
    H.write("#define RTE_TYPE_H\n")
    H.write('#include "Std_Types.h"\n\n')
    typeDefs = []
    typeSkip = ImplementationDataTypes
    loopCnt = 0
    while len(typeSkip) > 0:
        ImplementationDataTypes = typeSkip
        typeSkip = []
        for element in ImplementationDataTypes:
            GenRteTypeDef(workspace, element, H, typeDefs, typeSkip)
        loopCnt += 1
        if loopCnt > 100:
            raise Exception("Infinite loop detected in GenRteTypeDef")
    H.write("#endif /* RTE_TYPE_H */\n\n")


def Gen(cls, dir):
    config_file_path = os.path.join(os.path.dirname(factory.__file__), "../config.toml")
    document_root = dir
    workspace = ar_workspace.Workspace(config_file_path, document_root=document_root)
    apply_platform_types(workspace)
    cls(workspace).create()
    workspace.write_documents()
    GenRteTypes(workspace, dir)
    # implementation = ImplementationModel(workspace)
    # type_generator = autosar.generator.TypeGenerator(implementation)
    print("GEN RTE Interface for", cls.__name__, ":", dir)
    # print(type_generator.write_type_header(dir))


def GenFromArxml(arxmls, dir):
    logger.debug(f"GenFromArxml: {arxmls} -> {dir}")
    config_file_path = os.path.join(os.path.dirname(factory.__file__), "../config.toml")
    document_root = dir
    workspace = ar_workspace.Workspace(config_file_path, document_root=document_root)
    elementMaps = {
        ar_element.SwBaseType: ("Default", ar_enum.PackageRole.BASE_TYPE),
        ar_element.ImplementationDataType: ("Default", ar_enum.PackageRole.IMPLEMENTATION_DATA_TYPE),
        ar_element.ApplicationSoftwareComponentType: ("Default", ar_enum.PackageRole.COMPONENT_TYPE),
        ar_element.ConstantSpecification: ("Default", ar_enum.PackageRole.CONSTANT),
        ar_element.ModeDeclarationGroup: ("Default", ar_enum.PackageRole.MODE_DECLARATION),
        ar_element.ModeSwitchInterface: ("Default", ar_enum.PackageRole.PORT_INTERFACE),
        ar_element.SenderReceiverInterface: ("Default", ar_enum.PackageRole.PORT_INTERFACE),
    }
    for arxml in arxmls:
        logger.debug(f"Reading ARXML: {arxml}")
        reader = autosar.xml.Reader()
        document = reader.read_file(arxml)
        for pkg in document.packages:
            logger.debug(f"Processing package: {pkg.name}")
            for element in pkg.elements:
                logger.debug(f"Processing element: {element.name} ({type(element)})")
                if type(element) in elementMaps:
                    NAMESPACE, role = elementMaps[type(element)]
                    package_ref = workspace.get_package_ref_by_role(NAMESPACE, role)
                    package: ar_element.Package = workspace.make_packages(package_ref)
                    package.append(element)
                else:
                    raise Exception(f"Unsupported element type: {type(element)}")
            for subPkg in pkg.packages:
                package: ar_element.Package = workspace.make_packages(f"/{pkg.name}")
                package.append(subPkg)
    workspace.write_documents()
    GenRteTypes(workspace, dir)
