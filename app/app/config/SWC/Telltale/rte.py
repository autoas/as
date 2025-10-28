# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>
from generator import asar
from bswcom import *

NAMESPACE = "Default"

Led1Sts_IV = CAN0_RxMsgAbsInfo_Led1Sts_IV
Led2Sts_IV = CAN0_RxMsgAbsInfo_Led2Sts_IV
Led3Sts_IV = CAN0_RxMsgAbsInfo_Led3Sts_IV

LedSts_P = {
    "Led1Sts": Led1Sts_IV,
    "Led2Sts": Led2Sts_IV,
    "Led3Sts": Led3Sts_IV,
}

InactiveActive_T = asar.factory.ImplementationEnumDataTypeTemplate(
    "InactiveActive_T",
    NAMESPACE,
    asar.platform.BaseTypes.uint8,
    [
        "InactiveActive_Inactive",
        "InactiveActive_Active",
        "InactiveActive_Error",
        "InactiveActive_NotAvailable",
    ],
)

OnOff_T = asar.factory.ImplementationEnumDataTypeTemplate(
    "OnOff_T",
    NAMESPACE,
    asar.platform.BaseTypes.uint8,
    ["OnOff_Off", "OnOff_On", "OnOff_1Hz", "OnOff_2Hz", "OnOff_3Hz"],
)

UINT8_T = asar.platform.ImplementationTypes.uint8

ttList = [
    "TPMS",
    "LowOil",
    "PosLamp",
    "TurnLeft",
    "TurnRight",
    "AutoCruise",
    "HighBeam",
    "SeatbeltDriver",
    "SeatbeltPassenger",
    "Airbag",
]
# TelltaleStatus: means the COM signals which control the related Telltale status
# TelltaleState: means the state of Telltale: on, off or flash
TELLTALE_IVs = {}
for tt in ttList:
    TELLTALE_IV = asar.factory.ConstantTemplate(f"{tt}_IV", NAMESPACE, "OnOff_Off")
    TELLTALE_IVs[tt] = TELLTALE_IV

Telltale_I = asar.SenderReceiverInterfaceTemplate("Telltale", NAMESPACE, [(OnOff_T, tt) for tt in ttList])
LedSts_I = asar.SenderReceiverInterfaceTemplate(
    "Com", NAMESPACE, [(UINT8_T, LedSts) for LedSts, IV in LedSts_P.items()]
)


class Telltale:
    def __init__(self, workspace):
        self.workspace = workspace
        depends = [asar.EcuM_CurrentMode_I, Telltale_I, LedSts_I]
        for tt, IV in TELLTALE_IVs.items():
            depends.extend([IV])
        for led, IV in LedSts_P.items():
            depends.extend([IV])
        self.Component = asar.factory.GenericComponentTypeTemplate(
            "Telltale", NAMESPACE, self.create_component, depends=depends
        )

    def create_component(
        self,
        package: asar.ar_element.Package,
        workspace: asar.ar_workspace.Workspace,
        deps: dict[str, asar.ar_element.ARElement] | None,
        **_1,
    ) -> asar.ar_element.ApplicationSoftwareComponentType:
        ecu_mode_interface = deps[asar.EcuM_CurrentMode_I.ref(workspace)]
        swc_name = "Telltale"
        swc = asar.ar_element.ApplicationSoftwareComponentType(swc_name)
        package.append(swc)
        swc.create_require_port(
            "EcuM_CurrentMode", ecu_mode_interface, com_spec={"enhanced_mode_api": False, "supports_async": False}
        )
        ports = []
        ports2 = []
        interface = deps[Telltale_I.ref(workspace)]
        for tt, IV in TELLTALE_IVs.items():
            init = deps[IV.ref(workspace)]
            ports.append(f"VALUE:Telltale_{tt}/{tt}")
            ports2.append("Telltale_" + tt)
            swc.create_provide_port(
                "Telltale_" + tt,
                interface,
                com_spec={
                    tt: {
                        "init_value": init.ref(),
                        "uses_end_to_end_protection": False,
                    }
                },
            )
        interface = deps[LedSts_I.ref(workspace)]
        for LedSts, IV in LedSts_P.items():
            init = deps[IV.ref(workspace)]
            ports.append(f"VALUE:Com_{LedSts}/{LedSts}")
            ports2.append("Com_" + LedSts)
            swc.create_require_port(
                "Com_" + LedSts,
                interface,
                com_spec={
                    LedSts: {
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
    asar.Gen(Telltale, dir)
