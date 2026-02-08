import os
import jinja2
from .helper import *
from .SomeIpXf import *

__all__ = ["Gen_SomeIpProxyC"]

def Gen_SomeIpProxyC(cfg, service, dir, source):
    allStructs = GetStructs(cfg)
    service_name = service["name"]
    source["%sProxyC" % (service_name)] = ["%s/%sProxy.c" % (dir, service_name)]
    
    # Create Jinja2 environment
    template_dir = os.path.join(os.path.dirname(__file__), "templates")
    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(template_dir),
        trim_blocks=True,
        lstrip_blocks=True
    )
    
    # Add custom filters and functions
    env.filters['toMacro'] = toMacro
    env.globals['toMacro'] = toMacro
    env.globals['GetArgTypeC'] = GetArgTypeC
    env.globals['GetArgRefC'] = GetArgRefC
    env.globals['GetXfCType'] = GetXfCType
    env.globals['GetTypePayloadSize'] = GetTypePayloadSize
    env.globals['SomeIpXfEncode'] = SomeIpXfEncode
    env.globals['SomeIpXfDecode'] = SomeIpXfDecode
    env.globals['GetArgs'] = GetArgs
    
    # Prepare context data
    import datetime
    context = {
        "cfg": cfg,
        "service": service,
        "service_name": service_name,
        "allStructs": allStructs,
        "now": datetime.datetime.now()
    }
    
    # Render header template
    header_template = env.get_template("someip_proxy_h.j2")
    header_content = header_template.render(**context)
    
    # Write header file
    header_path = os.path.join(dir, f"{service_name}Proxy.h")
    with open(header_path, "w") as H:
        H.write(header_content)
    print(f"GEN {os.path.abspath(header_path)}")
    
    # Render source template
    source_template = env.get_template("someip_proxy_c.j2")
    source_content = source_template.render(**context)
    
    # Write source file
    source_path = os.path.join(dir, f"{service_name}Proxy.c")
    with open(source_path, "w") as C:
        C.write(source_content)
    print(f"GEN {os.path.abspath(source_path)}")
