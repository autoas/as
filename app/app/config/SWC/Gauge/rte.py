# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>
from generator import asar
from bswcom import *

NAMESPACE = "Default"

UINT16_T = asar.platform.ImplementationTypes.uint16

VehicleSpeed_IV = CAN0_RxMsgAbsInfo_VehicleSpeed_IV
TachoSpeed_IV = CAN0_RxMsgAbsInfo_TachoSpeed_IV

gagues = ["VehicleSpeed", "TachoSpeed"]

Stmo_IVs = {}
for gg in gagues:
    IV = asar.factory.ConstantTemplate(f"{gg}_IV", NAMESPACE, 0)
    Stmo_IVs[gg] = IV

Stmo_I = asar.SenderReceiverInterfaceTemplate("Stmo", NAMESPACE, [(UINT16_T, gg) for gg in gagues])
Com_I = asar.SenderReceiverInterfaceTemplate("Com", NAMESPACE, [(UINT16_T, gg) for gg in gagues])

Gauges_P = {"VehicleSpeed": VehicleSpeed_IV, "TachoSpeed": TachoSpeed_IV}


class Gauge:
    def __init__(self, workspace):
        self.workspace = workspace
        depends = [asar.EcuM_CurrentMode_I, Stmo_I, Com_I]
        for gg, IV in Stmo_IVs.items():
            depends.extend([IV])
        for gg, IV in Gauges_P.items():
            depends.extend([IV])
        self.Component = asar.factory.GenericComponentTypeTemplate(
            "Gauge", NAMESPACE, self.create_component, depends=depends
        )

    def create_component(
        self,
        package: asar.ar_element.Package,
        workspace: asar.ar_workspace.Workspace,
        deps: dict[str, asar.ar_element.ARElement] | None,
        **_1,
    ) -> asar.ar_element.ApplicationSoftwareComponentType:
        ecu_mode_interface = deps[asar.EcuM_CurrentMode_I.ref(workspace)]
        swc_name = "Gauge"
        swc = asar.ar_element.ApplicationSoftwareComponentType(swc_name)
        package.append(swc)
        swc.create_require_port(
            "EcuM_CurrentMode", ecu_mode_interface, com_spec={"enhanced_mode_api": False, "supports_async": False}
        )
        ports = []
        ports2 = []
        interface = deps[Stmo_I.ref(workspace)]
        for gg, IV in Stmo_IVs.items():
            init = deps[IV.ref(workspace)]
            ports.append(f"VALUE:Stmo_{gg}/{gg}")
            ports2.append("Stmo_" + gg)
            swc.create_provide_port(
                "Stmo_" + gg,
                interface,
                com_spec={
                    gg: {
                        "init_value": init.ref(),
                        "uses_end_to_end_protection": False,
                    }
                },
            )
        interface = deps[Com_I.ref(workspace)]
        for gg, IV in Gauges_P.items():
            init = deps[IV.ref(workspace)]
            ports.append(f"VALUE:Com_{gg}/{gg}")
            ports2.append("Com_" + gg)
            swc.create_require_port(
                "Com_" + gg,
                interface,
                com_spec={
                    gg: {
                        "init_value": init.ref(),
                        "alive_timeout": 0,
                        "enable_update": False,
                        "uses_end_to_end_protection": False,
                        "handle_never_received": False,
                    }
                },
            )
        init_runnable_name = swc_name + "_Init"
        periodic_runnable_name = swc_name + "_Run"
        behavior = swc.create_internal_behavior()
        behavior.create_port_api_options(ports2, enable_take_address=False, indirect_api=False)
        behavior.create_runnable(init_runnable_name, can_be_invoked_concurrently=False, minimum_start_interval=0)
        runnable = behavior.create_runnable(
            periodic_runnable_name, can_be_invoked_concurrently=False, minimum_start_interval=0
        )
        runnable.create_port_access(ports)
        behavior.create_swc_mode_mode_switch_event(
            init_runnable_name, "EcuM_CurrentMode/RUN", asar.ar_enum.ModeActivationKind.ON_ENTRY
        )
        behavior.create_timing_event(periodic_runnable_name, 20.0 / 1000)
        return swc

    def create(self):
        self.workspace.apply(self.Component)


def main(dir):
    asar.Gen(Gauge, dir)
