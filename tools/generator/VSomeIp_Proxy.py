# SSAS - Simple Smart Automotive Software
# Copyright (C) 2026 Parai Wang <parai@foxmail.com>
import os
import jinja2
import datetime
from .helper import *
from .SomeIpXf import *


__all__ = ["Gen_VSomeIpProxy"]


def GetArgsList(method, cfg):
    if "args" in method:
        args = GetArgs(cfg, method["args"])
        arg_list = []
        for arg in args:
            arg_list.append(f"const {toMethodTypeName(arg['type'], method)} &{arg['name']}")
        return ", ".join(arg_list)
    return ""


def GetMethodPayloadSize(method, cfg, allStructs):
    payloadSize = 0
    if "args" in method:
        args = GetArgs(cfg, method["args"])
        for arg in args:
            payloadSize += GetTypePayloadSize(arg, allStructs)
    return payloadSize


def Gen_VSomeIpProxy(cfg, service, dir, source):
    service_name = service["name"]

    source[f"{service_name}ProxyV"] = [os.path.join(dir, f"{service_name}ProxyV.cpp")]

    allStructs = GetStructs(cfg)

    context = {
        "cfg": cfg,
        "service": service,
        "service_name": service_name,
        "allStructs": allStructs,
        "now": datetime.datetime.now(),
        "GetArgs": GetArgs,
        "GetArgsList": GetArgsList,
        "GetMethodPayloadSize": GetMethodPayloadSize,
        "GetTypePayloadSize": GetTypePayloadSize,
        "GetXfCType": GetXfCType,
        "SomeIpXfEncode": SomeIpXfEncode,
        "SomeIpXfDecode": SomeIpXfDecode,
        "GetE2EOverhead": GetE2EOverhead,
        "GetE2EOffset": GetE2EOffset,
        "toMacro": toMacro,
        "toMethodTypeName": toMethodTypeName,
    }

    template_dir = os.path.join(os.path.dirname(__file__), "templates")
    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(template_dir), trim_blocks=True, lstrip_blocks=True, extensions=["jinja2.ext.do"]
    )
    env.filters['toMacro'] = toMacro

    # Render header
    header_template = env.get_template("vsomeip_proxy_hpp.j2")
    header_content = header_template.render(**context)
    header_path = os.path.join(dir, f"{service_name}ProxyV.hpp")
    with open(header_path, "w") as H:
        H.write(header_content)
    print(f"GEN {os.path.abspath(header_path)}")

    # Render source
    source_template = env.get_template("vsomeip_proxy_cpp.j2")
    source_content = source_template.render(**context)
    source_path = os.path.join(dir, f"{service_name}ProxyV.cpp")
    with open(source_path, "w") as C:
        C.write(source_content)
    print(f"GEN {os.path.abspath(source_path)}")


def toMethodTypeName(type_name, method):
    """Strip method name prefix if present: e.g., 'AdjustOutput' -> 'Output' for method 'Adjust'."""
    method_name = method["name"]
    if type_name.startswith(method_name):
        return type_name[len(method_name) :]
    return type_name
