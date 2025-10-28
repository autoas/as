import os
import sys

CWD = os.path.dirname(__file__)
if CWD == "":
    CWD = os.path.abspath(".")
ASROOT = os.path.abspath("%s/../../../" % (CWD))
AsPy = "%s/build/%s/GCC/AsPy" % (ASROOT, os.name)
if not os.path.exists("%s/AsPy.%s" % (CWD, "pyd" if os.name == "nt" else "so")):
    if os.name == "nt":
        cmd = "cd %s & scons --lib=AsPy" % (ASROOT)
        cmd += "& cp -v %s/AsPy.dll %s/AsPy.pyd" % (AsPy, CWD)
    else:
        cmd = "cd %s & scons --lib=AsPy" % (ASROOT)
        cmd += " && cp -v %s/AsPy.so %s/AsPy.so" % (AsPy, CWD)
    os.system(cmd)

if hasattr(os, "add_dll_directory"):
    # Python higher version is not supporting add PATH for windows
    PATH = os.getenv("PATH")
    for p in PATH.split(";"):
        p = p.strip()
        if p != '' and " " not in p:
            os.add_dll_directory(p)
