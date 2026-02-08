# SSAS - Simple Smart Automotive Software
# Copyright (C) 2026 Parai Wang <parai@foxmail.com>

import os
import jinja2
import datetime
from .helper import *
from .SomeIpXf import *

__all__ = ["Gen_SomeIpSkeleton"]

def Gen_SomeIpSkeleton(cfg, service, dir, source):
    allStructs = GetStructs(cfg)
    service_name = service["name"]
    source["%sSkeleton" % (service_name)] = ["%s/%sSkeleton.cpp" % (dir, service_name)]
    
    # Create Jinja2 environment
    template_dir = os.path.join(os.path.dirname(__file__), "templates")
    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(template_dir),
        trim_blocks=True,
        lstrip_blocks=True
    )
    
    # Add custom filters
    env.filters['toMacro'] = toMacro
    env.filters['getXfCType'] = lambda obj, structs: GetXfCType(obj, structs)
    env.filters['getTypePayloadSize'] = lambda obj, structs: GetTypePayloadSize(obj, structs)
    env.filters['SomeIpXfEncode'] = lambda obj, structs, dest, size, src: SomeIpXfEncode(obj, structs, dest, size, src)
    env.filters['SomeIpXfDecode'] = lambda obj, structs, src, size, dest: SomeIpXfDecode(obj, structs, src, size, dest)
    
    # Prepare context data
    MaxPayloadSize = 32
    listenNum = service.get("listen", 1)
    context = {
        "cfg": cfg,
        "service": service,
        "service_name": service_name,
        "allStructs": allStructs,
        "MaxPayloadSize": MaxPayloadSize,
        "listenNum": listenNum,
        "now": datetime.datetime.now(),
        "GetArgs": GetArgs,
        "GetTypePayloadSize": GetTypePayloadSize
    }
    
    # Render header template
    header_template = env.get_template("someip_skeleton_hpp.j2")
    header_content = header_template.render(**context)
    
    # Write header file
    header_path = os.path.join(dir, f"{service_name}Skeleton.hpp")
    with open(header_path, "w") as H:
        H.write(header_content)
    print(f"GEN {os.path.abspath(header_path)}")
    
    # Render source template
    source_template = env.get_template("someip_skeleton_cpp.j2")
    source_content = source_template.render(**context)
    
    # Write source file
    source_path = os.path.join(dir, f"{service_name}Skeleton.cpp")
    with open(source_path, "w") as C:
        C.write(source_content)
    print(f"GEN {os.path.abspath(source_path)}")
