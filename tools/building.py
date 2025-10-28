# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import glob
import shutil
import SCons
from SCons.Script import *
import re

RootDir = os.path.abspath(os.path.dirname(__file__) + "/..")

if os.name == "nt" or sys.platform in ["msys", "cygwin"]:
    PATH = os.getenv("PATH")
    os.environ["PATH"] = "%s;%s/build/nt/GCC/one" % (PATH, RootDir)

AddOption(
    "--verbose", dest="verbose", action="store_true", default=False, help="print verbose information during build"
)

AddOption("--app", dest="application", type=str, default=None, help="to choose which application to be build")

AddOption("--lib", dest="library", type=str, default=None, help="to choose which library to be build")

AddOption("--os", dest="os", type=str, default=None, help="to choose which os to be used")

AddOption("--arch", dest="arch", type=str, default=None, help="to choose CPU architecture to be used")

AddOption("--net", dest="net", type=str, default="none", help="to choose which net(lwip or none) to be used")

AddOption("--cpl", dest="compiler", type=str, default="GCC", help="to choose which compiler to be used")

AddOption("--release", dest="release", type=str, default=None, help="release ssas software: cmake, make, build")

AddOption("--cfg", dest="cfg", type=str, default=None, help="the directory path to config")

AddOption("--gen", dest="gen", action="store_true", default=False, help="force to generate config")

AddOption("--strip", dest="strip", action="store_true", default=False, help="strip symbols")

AddOption("--prebuilt", dest="prebuilt", action="store_true", default=False, help="using prebuilt libraries")

AddOption("--det", dest="det", action="store_true", default=False, help="enable development error trace")

CWD = os.path.abspath(".")
appName = GetOption("application")
libName = GetOption("library")
cplName = GetOption("compiler")
paths = [CWD, "build", os.name]
if cplName != None:
    paths.append(cplName)
if appName != None:
    paths.append(appName)
elif libName != None:
    paths.append(libName)

BUILD_DIR = os.path.join(*paths)
Export("BUILD_DIR")

TARGET_OS = GetOption("os")
Export("TARGET_OS")

TARGET_ARCH = GetOption("arch")
Export("TARGET_ARCH")

_cfg_path = GetOption("cfg")
if _cfg_path != None:
    _cfg_path = os.path.abspath(_cfg_path)

__compilers__ = {}
__libraries__ = {}
__OSs__ = {}
__apps__ = {}
__cpppath__ = {}
__cfgs__ = {}

__default_compiler__ = "GCC"


def aslog(*arg):
    if GetOption("verbose"):
        print(*arg)


class Win32Spawn:
    def spawn(self, sh, escape, cmd, args, env):
        # deal with the cmd build-in commands which cannot be used in
        # subprocess.Popen
        if cmd == "del":
            for f in args[1:]:
                try:
                    os.remove(f)
                except Exception as e:
                    print("Error removing file: %s" % (e))
                    return -1
            return 0

        import subprocess

        newargs = " ".join(args[1:])
        cmdline = cmd + " " + newargs

        # Make sure the env is constructed by strings
        _e = dict([(k, str(v)) for k, v in env.items()])

        # Windows(tm) CreateProcess does not use the env passed to it to find
        # the executables. So we have to modify our own PATH to make Popen
        # work.
        try:
            _e["PATH"] = os.environ["PATH"] + ";" + _e["PATH"]
        except KeyError:
            pass

        try:
            proc = subprocess.Popen(cmdline, env=_e, shell=False)
        except Exception as e:
            print("Error in calling:\n%s" % (cmdline))
            print("Exception: %s: %s" % (e, os.strerror(e.errno)))
            return e.errno

        return proc.wait()


def IsBuildForWindows(compiler=None):
    if compiler is None:
        compiler = GetOption("compiler")
    if IsPlatformWindows():
        return compiler in ["GCC", "MSVC", "x86GCC", "PYCC", "QMake"]
    return False


def IsBuildForWin32(compiler=None):
    if compiler is None:
        compiler = GetOption("compiler")
    if IsPlatformWindows():
        return compiler in ["x86GCC"]
    return False


def IsBuildForMSVC(compiler=None):
    if compiler is None:
        compiler = GetOption("compiler")
    if IsPlatformWindows():
        return compiler in ["MSVC"]
    return False


def IsBuildForAndroid(compiler=None):
    if compiler is None:
        compiler = GetOption("compiler")
    return compiler in ["NDK"]


def IsBuildForHost(compiler=None):
    if compiler is None:
        compiler = GetOption("compiler")
    return compiler in ["GCC", "MSVC", "NDK", "x86GCC", "PYCC"]


def IsMsysPython():
    bYes = False
    if sys.platform in ["msys", "cygwin"]:
        bYes = True
    return bYes


def IsPlatformWindows():
    bYes = False
    if os.name == "nt":
        bYes = True
    if sys.platform in ["msys", "cygwin"]:
        bYes = True
    return bYes


def IsPlatformTermux():
    bYes = False
    ver = os.getenv("TERMUX_VERSION")
    if ver != None:
        bYes = True
    return bYes


def GetMsys2Root():
    rt = os.getenv("MSYS2")
    if None == rt:
        if os.path.isdir("c:/msys64"):
            rt = "c:/msys64"
    if None == rt:
        raise Exception("please specify env MSYS2 root directory")
    return rt


def MKDir(p):
    ap = os.path.abspath(p)
    try:
        os.makedirs(ap)
    except:
        if not os.path.exists(ap):
            raise Exception("Fatal Error: can't create directory <%s>" % (ap))


def RMDir(p):
    if os.path.exists(p):
        shutil.rmtree(p)


def MKFile(p, c="", m="w"):
    f = open(p, m)
    f.write(c)
    f.close()


def RMFile(p):
    if os.path.exists(p):
        print("removing %s" % (os.path.abspath(p)))
        os.remove(os.path.abspath(p))


def RunCommand(cmd, e=True):
    aslog(' >> RunCommand "%s"' % (cmd))
    if os.name == "nt":
        cmd = cmd.replace("&&", "&")
    ret = os.system(cmd)
    if 0 != ret and e:
        raise Exception('FAIL of RunCommand "%s" = %s' % (cmd, ret))
    return ret


def RunSysCmd(cmd):
    import subprocess

    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    try:
        o = output.decode("utf-8")
    except UnicodeDecodeError:
        o = output
    return p_status, o


def Download(url, tgt=None):
    # curl is better than wget on msys2
    if tgt == None:
        tgt = url.split("/")[-1]

    def IsProperType(f):
        tL = {
            ".zip": "Zip archive data",
            ".tar.gz": "gzip compressed data",
            ".tar.xz": "XZ compressed data",
            ".tar.bz2": "bzip2 compressed data",
        }
        if not os.path.exists(f):
            return False
        if 0 == os.path.getsize(f):
            return False
        for t, v in tL.items():
            if f.endswith(t):
                err, info = RunSysCmd("file %s" % (tgt))
                if v not in info:
                    return False
                break
        return True

    if not os.path.exists(tgt):
        print("Downloading from %s to %s" % (url, tgt))
        ret = RunCommand("curl %s -o %s" % (url, tgt), False)
        if (ret != 0) or (not IsProperType(tgt)):
            tf = url.split("/")[-1]
            RMFile(tf)
            print("temporarily saving to %s" % (os.path.abspath(tf)))
            RunCommand("wget %s" % (url))
            RunCommand("mv -v %s %s" % (tf, tgt))


def PkgGlob(pkg, objs):
    assert os.path.isdir(pkg)
    pkg = os.path.abspath(pkg)
    sc = "%s/SConscript" % (pkg)
    cstr = "objs = []\n"
    for obj in objs:
        cstr += "objs += Glob('%s')\n" % (obj)
    cstr += "Return('objs')"
    if not os.path.isfile(sc):
        with open(sc, "w") as f:
            f.write(cstr)
    else:
        with open(sc) as f:
            cstr2 = f.read()
        if cstr != cstr2:
            with open(sc, "w") as f:
                f.write(cstr)
    return SConscript(sc, variant_dir="%s/%s" % (BUILD_DIR, pkg.replace(RootDir, "")), duplicate=0)


def __isInRemove__(x, remove):
    if type(x) == type("str"):
        bn = os.path.basename(x)
        xn = x.replace(os.sep, "/")
    else:
        bn = os.path.basename(x.rstr())
        xn = x.rstr().replace(os.sep, "/")
    if bn in remove:
        return True
    else:
        for rm in remove:
            reRM = re.compile(r"^%s$" % (rm))
            if reRM.search(bn):
                return True
            rm = rm.replace(os.sep, "/")
            if xn.endswith(rm):
                return True
            reRM = re.compile(r"%s$" % (rm))
            if reRM.search(xn):
                return True
    return False


def SrcRemove(src, remove):
    if not src:
        return
    L = []
    for item in src:
        if type(item) == type("str"):
            if __isInRemove__(item, remove):
                L.append(item)
        else:
            if type(item) == list:
                Li = []
                for itt in item:
                    if __isInRemove__(itt, remove):
                        Li.append(itt)
                for itt in Li:
                    item.remove(itt)
            else:
                if __isInRemove__(item, remove):
                    L.append(item)
    for item in L:
        src.remove(item)


def Package(url, **parameters):
    if type(url) == dict:
        parameters = url
        url = url["url"]
    cwd = GetCurrentDir()
    bsw = os.path.basename(cwd)
    download = "%s/download" % (RootDir)
    if "dir" in parameters:
        download = "%s/%s" % (download, parameters["dir"])
    MKDir(download)
    pkgBaseName = os.path.basename(url)
    if pkgBaseName.endswith(".zip"):
        tgt = "%s/%s" % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-4]
        pkg = "%s/%s" % (download, pkgName)
        MKDir(pkg)
        flag = "%s/.unzip.done" % (pkg)
        if not os.path.exists(flag):
            try:
                RunCommand("cd %s && unzip ../%s" % (pkg, pkgBaseName))
            except Exception as e:
                print("WARNING:", e)
            MKFile(flag, "url")
    elif pkgBaseName.endswith(".rar"):
        tgt = "%s/%s" % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-4]
        pkg = "%s/%s" % (download, pkgName)
        MKDir(pkg)
        flag = "%s/.unrar.done" % (pkg)
        if not os.path.exists(flag):
            try:
                RunCommand("cd %s && unrar x ../%s" % (pkg, pkgBaseName))
            except Exception as e:
                print("WARNING:", e)
            MKFile(flag, "url")
    elif pkgBaseName.endswith(".tar.gz") or pkgBaseName.endswith(".tar.xz") or pkgBaseName.endswith(".tgz"):
        tgt = "%s/%s" % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-7]
        pkg = "%s/%s" % (download, pkgName)
        MKDir(pkg)
        flag = "%s/.unzip.done" % (pkg)
        if not os.path.exists(flag):
            RunCommand("cd %s && tar xf ../%s" % (pkg, pkgBaseName))
            MKFile(flag, "url")
    elif pkgBaseName.endswith(".tar.bz2"):
        tgt = "%s/%s" % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-8]
        pkg = "%s/%s" % (download, pkgName)
        MKDir(pkg)
        flag = "%s/.unzip.done" % (pkg)
        if not os.path.exists(flag):
            RunCommand("cd %s && tar xf ../%s" % (pkg, pkgBaseName))
            MKFile(flag, "url")
    elif pkgBaseName.endswith(".git"):
        pkgName = pkgBaseName[:-4]
        pkg = "%s/%s" % (download, pkgName)
        if not os.path.exists(pkg):
            RunCommand("cd %s && git clone %s" % (download, url))
        if "version" in parameters:
            flag = "%s/.%s.version.done" % (pkg, bsw)
            if not os.path.exists(flag):
                ver = parameters["version"]
                RunCommand("cd %s && git checkout %s" % (pkg, ver))
                MKFile(flag, ver)
                # remove all cmd Done flags
                for cmdF in Glob("%s/.*.cmd.done" % (pkg)):
                    RMFile(str(cmdF))
    elif pkgBaseName.endswith(".exe"):
        tgt = "%s/%s" % (download, pkgBaseName)
        Download(url, tgt)
        pkg = "%s/%s" % (download, pkgBaseName[:-4])
        pkg = pkg.replace("/", os.sep)
        flag = pkg + "/.installed"
        if not os.path.isfile(flag):
            print("please install with the default")
            RunCommand("cd %s && %s" % (download, pkgBaseName))
            MKDir(pkg)
            MKFile(flag, url)
    else:
        pkg = "%s/%s" % (download, url)
        if not os.path.isdir(pkg):
            print(
                "ERROR: %s require %s but now it is missing! It maybe downloaded later, so please try build again."
                % (bsw, url)
            )
    # cmd is generally a series of 'sed' operatiron to do some simple modifications
    if "cmd" in parameters:
        flag = "%s/.%s.cmd.done" % (pkg, bsw)
        cmd = "cd %s && " % (pkg)
        cmd += parameters["cmd"]
        if not os.path.exists(flag):
            RunCommand(cmd)
            MKFile(flag, cmd)
    if "pyfnc" in parameters:
        flag = "%s/.%s.pyfnc.done" % (pkg, bsw)
        if not os.path.exists(flag):
            parameters["pyfnc"](pkg)
            MKFile(flag)
    if "patch" in parameters:
        flag = "%s/.%s.patch.done" % (pkg, bsw)
        cmd = "cd %s && " % (pkg)
        cmd += "patch -p1 < %s" % (parameters["patch"])
        if not os.path.exists(flag):
            RunCommand(cmd)
            MKFile(flag, cmd)
    # post check
    verList = Glob("%s/.*.version.done" % (pkg))
    cmdList = Glob("%s/.*.cmd.done" % (pkg))
    if len(verList) >= 2 or len(cmdList) >= 2:
        print(
            "WARNING: BSW %s : 2 or more BSWs require package %s, "
            "please make sure version and cmd has no conflicts\n"
            "\t please check %s/SConscript" % (bsw, pkgBaseName, cwd)
        )
    return pkg


class CustomEnv(dict):
    def __init__(self):
        self.env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
        self.CPPPATH = []
        self.objs = {}

    def Replace(self, **kwargs):
        pass

    def Append(self, **kwargs):
        self.env.Append(**kwargs)

    def Remove(self, **kwargs):
        env = self.env
        for key, v in kwargs.items():
            if key in env:
                if type(v) is str:
                    if v in env[key]:
                        env[key].remove(v)
                else:
                    for vv in v:
                        if vv in env[key]:
                            env[key].remove(vv)

    def AddPostAction(self, target, action):
        if not hasattr(self, "__post_actions__"):
            self.__post_actions__ = []
        self.__post_actions__.append([target, action])

    def get(self, key, default=None):
        return self.env.get(key, default)

    def getKL(self, kwargs, K, L):
        for x in kwargs.get(K, []):
            if type(x) is list:
                for x1 in x:
                    if x1 not in L:
                        L.append(x1)
            else:
                if x not in L:
                    L.append(x)

    def Object(self, src, **kwargs):
        aslog("CC", src)
        self.objs[src] = kwargs
        return [src]

    def SharedObject(self, src, **kwargs):
        aslog("SHCC", src)
        self.objs[src] = kwargs
        return [src]

    def Library(self, libName, objs, **kwargs):
        return ["lib%s.a" % (libName)]

    def SharedLibrary(self, libName, objs, **kwargs):
        self.shared = True
        return self.Program(libName, objs, **kwargs)

    def Program(self, appName, objs, **kwargs):
        for x in objs:
            if x not in self.objs:
                aslog("CC", x)
                self.objs[x] = kwargs

    def w(self, l):
        self.mkf.write(l)

    def relpath(self, p, silent=False):
        try:
            p = os.path.relpath(p, RootDir)
            p = p.replace(os.sep, "/")
        except Exception as e:
            if silent:
                pass
            else:
                raise e
        return p

    def abspath(self, p):
        p = os.path.abspath(p)
        p = p.replace(os.sep, "/")
        return p

    def unixpath(self, p):
        p = self.abspath(p)
        if p[1] == ":":
            p = "/" + p[0].lower() + "/" + p[2:]
        return p

    def copy(self, src):
        rp = self.relpath(src)
        print("  release %s" % (rp))
        dst = os.path.join(self.WDIR, rp)
        d = os.path.dirname(dst)
        MKDir(d)
        shutil.copy(src, dst)
        d = os.path.dirname(src)
        extras = Glob("%s/*.h" % (d))
        if "infras" in d:
            extras += Glob("%s/*/*.h" % (d))
            extras += Glob("%s/*/*.c" % (d))
        for f in extras:
            src = str(f)
            rp = self.relpath(src)
            dst = os.path.join(self.WDIR, rp)
            MKDir(os.path.dirname(dst))
            shutil.copy(src, dst)

    def CopyFiles(self, objs):
        for src in objs:
            self.copy(str(src))
        for cpppath in self.CPPPATH:
            try:
                rp = self.relpath(cpppath)
                shutil.copytree(cpppath, os.path.join(self.WDIR, rp), dirs_exist_ok=True)
            except Exception as e:
                print("WARNING: failed to copy %s: %s" % (rp, e))


class ReleaseEnv(CustomEnv):
    def __init__(self, env, release):
        super().__init__()
        self.env = env
        self.release = release

    def Program(self, appName, objs, **kwargs):
        super().Program(appName, objs, **kwargs)
        self.appName = appName
        self.cplName = getattr(self, "compiler", GetOption("compiler"))
        if self.release == "build":
            self.WDIR = os.path.join(RootDir, "build", os.name, self.cplName, self.appName).replace(os.sep, "/")
        else:
            self.WDIR = "%s/release/%s" % (RootDir, appName)
        MKDir(self.WDIR)
        if self.release == "make":
            self.GenerateMakefile(objs, **kwargs)
        if self.release == "gmake":
            self.GenerateGMakefile(objs, **kwargs)
        if self.release in ["keil"]:
            self.GenerateKeilProject(objs, **kwargs)
        if self.release in ["cmake", "build"]:
            self.GenerateCMake(objs, **kwargs)
        if self.release in ["cmake", "make"]:
            self.CopyFiles(objs)
        print("release %s done!" % (appName))
        exit()

    def link_flags(self, LINKFLAGS):
        cstr = ""
        bIsLDS = False
        for flg in LINKFLAGS:
            if "-T" in flg:
                lds = flg.replace("-T", "").replace('"', "")
                if lds != "":
                    self.copy(lds)
                    cstr = '%s -T"%s"' % (cstr, self.relpath(lds))
                else:
                    bIsLDS = True
            elif bIsLDS:
                self.copy(flg)
                cstr = '%s -T"%s"' % (cstr, self.relpath(flg))
                bIsLDS = False
            elif "-Map=" in flg:
                ss = flg.split("=")
                prefix = ss[0]
                mp = ss[-1].replace('"', "")
                cstr = '%s %s="%s"' % (cstr, prefix, self.relpath(mp))
            elif ".map" in flg:
                cstr = " ".join([cstr, self.relpath(flg)])
            else:
                cstr = " ".join([cstr, flg])
        return cstr

    def GenerateKeilProject(self, objs, **kwargs):
        import xml.etree.ElementTree as ET

        self.mkf = "%s/%s.uvprojx" % (self.WDIR, self.appName)
        CPPPATH = []
        CPPDEFINES = []
        for _, kwargs in self.objs.items():
            self.getKL(kwargs, "CPPPATH", CPPPATH)
            self.getKL(kwargs, "CPPDEFINES", CPPDEFINES)
        root = ET.Element("ROOT")
        define = ET.Element("Define")
        define.text = " ".join(CPPDEFINES)
        root.append(define)
        incs = ET.Element("IncludePath")
        incs.text = ";".join(CPPPATH)
        root.append(incs)
        groups = ET.Element("Groups")
        grpsMap = {}
        for obj, kwargs in self.objs.items():
            filePath = str(obj)
            elements = filePath.replace(RootDir + os.path.sep, "").split(os.path.sep)
            fileName = elements[-1]
            groupName = elements[-2]
            if groupName.lower() in ["src", "source"]:
                groupName = elements[-3]
            if groupName not in grpsMap:
                group = ET.Element("Group")
                grpName = ET.Element("GroupName")
                grpName.text = groupName
                group.append(grpName)
                grpFiles = ET.Element("Files")
                group.append(grpFiles)
                groups.append(group)
                grpsMap[groupName] = [group, grpFiles]
            else:
                group, grpFiles = grpsMap[groupName]
            ef = ET.Element("File")
            efn = ET.Element("FileName")
            efn.text = fileName
            eft = ET.Element("FileType")
            eft.text = "1"
            efp = ET.Element("FilePath")
            efp.text = filePath
            ef.extend([efn, eft, efp])
            grpFiles.append(ef)
        root.append(groups)
        tree = ET.ElementTree(root)
        tree.write(self.mkf, encoding="utf-8", xml_declaration=True)

    def GenerateMakefile(self, objs, **kwargs):
        self.mkf = open("%s/Makefile.%s" % (self.WDIR, self.cplName), "w")
        self.w("# Makefile for %s\n" % (self.appName))
        CPLPATH = os.path.dirname(self.env["CC"])
        CC = os.path.basename(self.env["CC"])
        CXX = os.path.basename(self.env["CXX"])
        LD = os.path.basename(self.env["LINK"])
        if LD == "$SMARTLINK":
            LD = CC
        if CPLPATH == "":
            _, CPLPATH = RunSysCmd("which %s" % (CC))
        self.w("CPLPATH?=%s\n" % (CPLPATH))
        self.w("CC=${CPLPATH}/%s\n" % (CC))
        self.w("LD=${CPLPATH}/%s\n" % (LD))
        self.w("CXX=${CPLPATH}/%s\n" % (CXX))
        self.w("ifeq ($V, 1)\n")
        self.w("Q=\n")
        self.w("else\n")
        self.w("Q=@\n")
        self.w("endif\n")
        linkflags = self.link_flags(kwargs.get("LINKFLAGS", []))
        linkflags = linkflags.replace(os.path.dirname(CPLPATH), "${CPLPATH}/..")
        self.w("LINKFLAGS=%s\n" % (linkflags))
        self.w("LIBS=%s\n" % (" ".join("-l%s" % (i) for i in kwargs.get("LIBS", []))))
        self.w("LIBPATH=%s\n" % (" ".join('-L"%s"' % (i) for i in kwargs.get("LIBPATH", []))))
        BUILD_DIR = os.path.join("build", os.name, self.cplName, self.appName).replace(os.sep, "/")
        objd = []
        objm = {}
        for src in objs:
            rp = self.relpath(str(src))
            if rp.endswith(".c") or rp.endswith(".s") or rp.endswith(".S"):
                obj = rp[:-2]
            elif rp.endswith(".cpp"):
                obj = rp[:-4]
            else:
                raise Exception("unkown file type %s" % (rp))
            obj = "%s/%s.o" % (BUILD_DIR, obj)
            d = os.path.dirname(obj)
            if d not in objd:
                objd.append(d)
            self.w("objs-y += %s\n" % (obj))
            if src in self.objs:
                args = self.objs[src]
            else:
                args = kwargs
            for i in args.get("CPPPATH", []):
                if i not in self.CPPPATH:
                    self.CPPPATH.append(i)
            objm[obj] = {"src": rp, "d": d}
            objm[obj]["CPPFLAGS"] = " ".join(args.get("CPPFLAGS", []))
            objm[obj]["CPPFLAGS"] = objm[obj]["CPPFLAGS"].replace(os.path.dirname(CPLPATH), "${CPLPATH}/..")
            objm[obj]["CPPDEFINES"] = " ".join(["-D%s" % (i) for i in args.get("CPPDEFINES", [])])
            objm[obj]["CPPPATH"] = " ".join(
                ['-I"%s"' % (self.relpath(i, silent=True)) for i in args.get("CPPPATH", [])]
            )
        self.w("\nall: %s/%s.exe\n" % (BUILD_DIR, self.appName))
        self.w("%s/%s.exe: $(objs-y)\n" % (BUILD_DIR, self.appName))
        self.w("\t@echo LD $@\n")
        self.w("\t$Q $(LD) $(LINKFLAGS) $(objs-y) $(LIBS) $(LIBPATH) -o $@\n\n")
        if "S19" in self.env:
            ss = self.env["S19"].split(" ")
            ss[0] = os.path.basename(ss[0])
            s19 = " ".join(ss)
            cmd = s19.format("%s/%s.exe" % (BUILD_DIR, self.appName), "%s/%s.s19" % (BUILD_DIR, self.appName))
            self.w("\t$Q ${CPLPATH}/%s\n\n" % (cmd))
        for d in objd:
            self.w("%s:\n\tmkdir -p $@\n" % (d))
        for obj, m in objm.items():
            self.w("%s: %s\n" % (m["src"], m["d"]))
            self.w("%s: %s\n" % (obj, m["src"]))
            CC = "CC"
            if m["src"].endswith(".cpp"):
                CC = "CXX"
            self.w("\t@echo %s $<\n" % (CC))
            self.w("\t$Q $(%s) %s %s %s -c $< -o $@\n" % (CC, m["CPPFLAGS"], m["CPPDEFINES"], m["CPPPATH"]))
        self.w("\nclean:\n\t@rm -fv $(objs-y) %s/%s.exe\n\n" % (BUILD_DIR, self.appName))
        self.mkf.close()

    def GenerateGMakefile(self, objs, **kwargs):
        CPPPATH = []
        LIBPATH = []
        CPPDEFINES = []
        LIBS = []
        self.getKL(kwargs, "LIBPATH", LIBPATH)
        self.getKL(kwargs, "LIBS", LIBS)
        for _, kwargs in self.objs.items():
            self.getKL(kwargs, "CPPPATH", CPPPATH)
            self.getKL(kwargs, "LIBPATH", LIBPATH)
            self.getKL(kwargs, "CPPDEFINES", CPPDEFINES)
        self.mkf = open("%s/makefile" % (self.WDIR), "w")
        self.w("# gamke for %s\n" % (self.appName))
        self.w('SSAS_ROOT="%s"\n' % (self.unixpath(RootDir)))
        VPATH = []
        for obj in self.objs:
            d = os.path.dirname(self.relpath(str(obj)))
            if d not in VPATH:
                VPATH.append(d)
        self.w("%s_VPATH += \\\n" % (self.appName.upper()))
        for d in VPATH:
            self.w("  ${SSAS_ROOT}/%s \\\n" % (d))
        self.w("\n\n")
        self.w("%s_SOURCES += \\\n" % (self.appName.upper()))
        for obj in self.objs:
            self.w("  ${SSAS_ROOT}/%s \\\n" % (self.relpath(str(obj))))
        self.w("\n\n")
        self.w("%s_LIBS += \\\n" % (self.appName.upper()))
        for x in LIBS:
            self.w("  -l%s \\\n" % (x))
        self.w("\n\n")
        self.w("%s_LIBPATH += \\\n" % (self.appName.upper()))
        for x in LIBPATH:
            self.w('  -L"${SSAS_ROOT}/%s" \\\n' % (self.relpath(x, silent=True)))
        self.w("\n\n")
        self.w("%s_INCLUDE += \\\n" % (self.appName.upper()))
        for x in CPPPATH:
            self.w('  -I"${SSAS_ROOT}/%s" \\\n' % (self.relpath(x, silent=True)))
        self.w("\n\n")
        self.w("%s_DEFINES += \\\n" % (self.appName.upper()))
        for x in CPPDEFINES:
            self.w("  -D%s \\\n" % (x))
        self.w("\n\n")
        self.mkf.close()

    def GenerateCMake(self, objs, **kwargs):
        CPPPATH = []
        LIBPATH = []
        CPPDEFINES = []
        LIBS = []
        self.getKL(kwargs, "LIBPATH", LIBPATH)
        self.getKL(kwargs, "LIBS", LIBS)
        for _, kwargs in self.objs.items():
            self.getKL(kwargs, "CPPPATH", CPPPATH)
            self.getKL(kwargs, "LIBPATH", LIBPATH)
            self.getKL(kwargs, "CPPDEFINES", CPPDEFINES)
        self.mkf = open("%s/CMakeLists.txt" % (self.WDIR), "w")
        self.w("# CMake for %s\n" % (self.appName))
        self.w('#   cmake -G "Unix Makefiles" ..\n\n')
        self.w("cmake_minimum_required(VERSION 3.2)\n\n")
        self.w("PROJECT(%s)\n\n" % (self.appName))
        if self.release == "build":
            self.w('SET(ROOT_DIR "%s")\n' % (self.unixpath(RootDir)))
        else:
            self.w("SET(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})\n")
        self.w("SET(%s_SOURCES\n" % (self.appName.upper()))
        for obj in self.objs:
            self.w("  ${ROOT_DIR}/%s\n" % (self.relpath(str(obj))))
        self.w(")\n\n")
        self.w("SET(%s_LIBS\n" % (self.appName.upper()))
        for x in LIBS:
            self.w("  %s\n" % (x))
        self.w(")\n\n")
        self.w("SET(%s_LIBPATH\n" % (self.appName.upper()))
        for x in LIBPATH:
            self.w('  "${ROOT_DIR}/%s"\n' % (self.relpath(x, silent=True)))
        self.w(")\n\n")
        self.w("SET(%s_INCLUDE\n" % (self.appName.upper()))
        for x in CPPPATH:
            self.w('  "${ROOT_DIR}/%s"\n' % (self.relpath(x, silent=True)))
        self.w(")\n\n")
        self.w("SET(%s_DEFINES\n" % (self.appName.upper()))
        for x in CPPDEFINES:
            self.w("  %s\n" % (x))
        self.w(")\n\n")
        if getattr(self, "shared", False):
            self.w("add_library(%s SHARED ${%s_SOURCES})\n\n" % (self.appName, self.appName.upper()))
        else:
            self.w("add_executable(%s ${%s_SOURCES})\n\n" % (self.appName, self.appName.upper()))
        self.w("target_link_libraries(%s PUBLIC ${%s_LIBS})\n\n" % (self.appName, self.appName.upper()))
        self.w("target_link_directories(%s PUBLIC ${%s_LIBPATH})\n\n" % (self.appName, self.appName.upper()))
        self.w("target_include_directories(%s PUBLIC ${%s_INCLUDE})\n\n" % (self.appName, self.appName.upper()))
        self.w("target_compile_definitions(%s PUBLIC ${%s_DEFINES})\n\n" % (self.appName, self.appName.upper()))
        self.w("add_custom_command(TARGET %s POST_BUILD\n" % (self.appName))
        self.w("  COMMAND echo add the custom command here)\n")
        self.mkf.close()
        if self.release == "build":
            cmd = 'cd %s && cmake -G "Unix Makefiles" . && make' % (self.WDIR)
            RunCommand(cmd)


def register_compiler(compiler):
    if not compiler.__name__.startswith("Compiler"):
        raise Exception('compiler name %s not starts with "Compiler"' % (compiler.__name__))
    name = compiler.__name__[8:]

    def create_compiler(**kwargs):
        env = compiler(**kwargs)
        if IsPlatformWindows() and not IsMsysPython():
            win32_spawn = Win32Spawn()
            env["SPAWN"] = win32_spawn.spawn
        if not GetOption("verbose"):
            # override the default verbose command string
            env.Replace(
                ARCOMSTR="AR $TARGET",
                ASCOMSTR="AS $SOURCE",
                ASPPCOMSTR="AS $SOURCE",
                CCCOMSTR="CC $SOURCE",
                CXXCOMSTR="CXX $SOURCE",
                LINKCOMSTR="LINK $TARGET",
                SHCCCOMSTR="SHCC $SOURCE",
                SHCXXCOMSTR="SHCXX $SOURCE",
                SHLINKCOMSTR="SHLINK $TARGET",
            )
        release = GetOption("release")
        if release in ["build", "cmake", "make", "gmake", "keil"]:
            env = ReleaseEnv(env, release)
        elif release != None:
            raise Exception("invalid release option, choose from: build, cmake, make")
        return env

    if name not in __compilers__:
        aslog("register compiler %s" % (name))
        __compilers__[name] = create_compiler
    else:
        raise KeyError("compiler %s already registered" % (name))


def register_driver(driver):
    if not driver.__name__.startswith("Driver"):
        raise Exception('driver name %s not starts with "Driver"' % (driver.__name__))
    name = "%s:%s" % (driver.__name__[6:], driver.cls)
    if name not in __libraries__:
        aslog("register driver %s" % (name))
        __libraries__[name] = driver
    else:
        raise KeyError("driver %s already registered" % (name))


def register_library(library):
    if not library.__name__.startswith("Library"):
        raise Exception('library name %s not starts with "Library"' % (library.__name__))
    name = library.__name__[7:]
    if name not in __libraries__:
        aslog("register library %s" % (name))
        __libraries__[name] = library
    else:
        raise KeyError("library %s already registered" % (name))


def register_os(library):
    register_library(library)
    name = library.__name__[7:]
    aslog("register OS %s" % (name))
    __OSs__[name] = library


def register_application(app):
    if not app.__name__.startswith("Application"):
        raise Exception('application name %s not starts with "Application"' % (app.__name__))
    name = app.__name__[11:]
    if name not in __apps__:
        aslog("register app %s" % (name))
        __apps__[name] = app
    else:
        raise KeyError("app %s already registered" % (name))


def query_application(name):
    if name in __apps__:
        return __apps__[name]
    else:
        raise KeyError("app %s not found" % (name))


@register_compiler
def CompilerArmGCC(**kwargs):
    env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
    env.Append(CFLAGS=["-std=gnu99"])
    env.Append(CPPFLAGS=["-Wall"])
    if not GetOption("strip"):
        env.Append(CPPFLAGS=["-g"])
        env.Append(ASFLAGS=["-g"])
    version = os.getenv("ARMGCC_VERSION", kwargs.get("version", "5.4.1"))
    if IsPlatformWindows():
        if version == "5.4.1":
            gccarm = "https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-win32.zip"
        elif version == "10.3.1":
            gccarm = "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-win32.zip"
        else:
            gccarm = "https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi.zip"
    else:
        if version == "5.4.1":
            gccarm = "https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-linux.tar.bz2"
        elif version == "10.3.1":
            gccarm = "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2"
        else:
            gccarm = "https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi.tar.xz"
    ARMGCC = os.getenv("ARMGCC")
    if ARMGCC != None:
        cpl = ARMGCC
    else:
        cpl = Package(gccarm)
        if not IsPlatformWindows():
            if version == "5.4.1":
                cpl += "/gcc-arm-none-eabi-5_4-2016q3"
            else:
                cpl += "/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi"
        else:
            if version == "13.3.1":
                cpl += "/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi"
            elif version == "10.3.1":
                cpl += "/gcc-arm-none-eabi-10.3-2021.10"
    machine = kwargs.get("machine", "")
    env.Append(LIBPATH=["%s/lib/gcc/arm-none-eabi/%s/%s" % (cpl, version, machine)])
    env.Append(LIBPATH=["%s/arm-none-eabi/lib/%s" % (cpl, machine)])
    env["CC"] = "%s/bin/arm-none-eabi-gcc" % (cpl)
    env["CXX"] = "%s/bin/arm-none-eabi-g++" % (cpl)
    env["AS"] = "%s/bin/arm-none-eabi-gcc -c" % (cpl)
    env["LINK"] = "%s/bin/arm-none-eabi-ld" % (cpl)
    env["S19"] = "%s/bin/arm-none-eabi-objcopy -O srec --srec-forceS3 --srec-len 32 {0} {1}" % (cpl)
    env["ELFSIZE"] = "%s/bin/arm-none-eabi-size --format=berkeley {0}" % (cpl)
    env.Append(CPPFLAGS=["-ffunction-sections", "-fdata-sections"])
    env.Append(LINKFLAGS=["--gc-sections"])
    return env


@register_compiler
def CompilerArm64GCC(**kwargs):
    env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
    env.Append(CFLAGS=["-std=gnu99"])
    env.Append(CPPFLAGS=["-Wall", "-fno-stack-protector"])
    if not GetOption("strip"):
        env.Append(CPPFLAGS=["-g"])
        env.Append(ASFLAGS=["-g"])
    if IsPlatformWindows():
        gccarm64 = "gcc-linaro-7.2.1-2017.11-i686-mingw32_aarch64-elf.tar.xz"
    else:
        gccarm64 = "gcc-linaro-7.2.1-2017.11-x86_64_aarch64-elf.tar.xz"
    ARM64GCC = os.getenv("ARM64GCC")
    if ARM64GCC != None:
        cpl = ARM64GCC
    else:
        pkg = Package(
            "https://releases.linaro.org/components/toolchain/binaries/7.2-2017.11/aarch64-elf/%s" % (gccarm64)
        )
        cpl = "%s/%s" % (pkg, gccarm64[:-7])
    env.Append(LIBPATH=["%s/lib/gcc/aarch64-elf/7.2.1" % (cpl)])
    env.Append(LIBPATH=["%s/aarch64-elf/lib" % (cpl)])
    env.Append(LIBPATH=["%s/aarch64-elf/libc/usr/lib" % (cpl)])
    env["CC"] = "%s/bin/aarch64-elf-gcc" % (cpl)
    env["CXX"] = "%s/bin/aarch64-elf-g++" % (cpl)
    env["AS"] = "%s/bin/aarch64-elf-gcc -c" % (cpl)
    env["LINK"] = "%s/bin/aarch64-elf-ld" % (cpl)
    env["S19"] = "%s/bin/aarch64-elf-objcopy -O srec --srec-forceS3 --srec-len 32 {0} {1}" % (cpl)
    env.Append(CPPFLAGS=["-ffunction-sections", "-fdata-sections"])
    env.Append(LINKFLAGS=["--gc-sections"])
    return env


@register_compiler
def CompilerArmCC(**kwargs):
    env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
    env.Append(CPPFLAGS=["--c99"])
    if not GetOption("strip"):
        env.Append(CPPFLAGS=["-g"])
    ARMCC = os.getenv("ARMCC")
    if ARMCC != None:
        cpl = ARMCC
    else:
        cpl = "C:/Keil_v5/ARM/ARMCC"
    env["CC"] = "%s/bin/armcc" % (cpl)
    env["CXX"] = "%s/bin/armcc" % (cpl)
    env["AS"] = "%s/bin/armasm" % (cpl)
    env["LINK"] = "%s/bin/armlink" % (cpl)
    env["S19"] = "%s/bin/fromelf --m32 --m32combined {0} --output {1}" % (cpl)
    env.Append(CPPFLAGS=["--split_sections"])
    env.Append(
        LINKFLAGS=[
            "--strict",
            "--summary_stderr",
            "--info",
            "summarysizes",
            "--map",
            "--xref",
            "--callgraph",
            "--symbols",
            "--info",
            "sizes",
            "--info",
            "totals",
            "--info",
            "unused",
            "--info",
            "veneers",
        ]
    )
    env["LIBDIRPREFIX"] = "--userlibpath="
    env["LIBLINKPREFIX"] = "--library="
    return env


@register_compiler
def CompilerArmClang(**kwargs):
    env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
    if not GetOption("strip"):
        env.Append(CPPFLAGS=["-g"])
    ArmClang = os.getenv("ArmClang")
    if ArmClang != None:
        cpl = ArmClang
    else:
        cpl = "C:/Keil_v5/ARM/ARMCLANG"
    env["CC"] = "%s/bin/armclang" % (cpl)
    env["CXX"] = "%s/bin/armclang" % (cpl)
    env["AS"] = "%s/bin/armclang" % (cpl)
    env["LINK"] = "%s/bin/armlink" % (cpl)
    env["S19"] = "%s/bin/fromelf --m32 --m32combined {0} --output {1}" % (cpl)
    env.Append(
        CPPFLAGS=[
            "-xc",
            "-std=c99",
            "-fno-rtti",
            "-funsigned-char",
            "-fshort-enums",
            "-fshort-wchar",
            "-gdwarf-3",
            "-ffunction-sections",
            "-fdata-sections",
            "-Wno-packed",
            "-Wno-missing-variable-declarations",
            "-Wno-missing-prototypes",
            "-Wno-missing-noreturn",
            "-Wno-sign-conversion",
            "-Wno-nonportable-include-path",
            "-Wno-reserved-id-macro",
            "-Wno-unused-macros",
            "-Wno-documentation-unknown-command",
            "-Wno-documentation",
            "-Wno-license-management",
            "-Wno-parentheses-equality",
        ]
    )
    env.Append(
        ASFLAGS=[
            "-c",
            "-gdwarf-3",
            "-x",
            "assembler-with-cpp",
            "-masm=auto",
            "-Wa,armasm,--pd,__MICROLIB SETA 1",
            "-Wa,armasm,--pd,__UVISION_VERSION SETA 533",
        ]
    )
    env.Append(
        LINKFLAGS=[
            "--summary_stderr",
            "--info",
            "common",
            "--info",
            "debug",
            "--info",
            "summarysizes",
            "--info",
            "sizes",
            "--info",
            "totals",
            "--info",
            "unused",
            "--info",
            "veneers",
            "--map",
            "--load_addr_map_info",
            "--xref",
            "--callgraph",
            "--symbols",
        ]
    )
    return env


@register_compiler
def CompilerCM0PGCC(**kwargs):
    env = CreateCompiler("ArmGCC", machine="armv6-m")
    env.Append(CPPFLAGS=["-mthumb", "-mlong-calls", "-mcpu=cortex-m0plus"])
    env.Append(ASFLAGS=["-mthumb", "-mcpu=cortex-m0plus"])
    env.Append(LINKFLAGS=["-mthumb", "-mcpu=cortex-m0plus"])
    env["LINK"] = env["LINK"][:-2] + "gcc"
    env["LINKFLAGS"].remove("--gc-sections")
    env.Append(LINKFLAGS=["-Wl,--gc-sections"])
    return env


@register_compiler
def CompilerCM3GCC(**kwargs):
    env = CreateCompiler("ArmGCC", machine="armv7-m")
    env.Append(CPPFLAGS=["-mthumb", "-mlong-calls", "-mcpu=cortex-m3"])
    env.Append(ASFLAGS=["-mthumb", "-mcpu=cortex-m3"])
    env.Append(LINKFLAGS=["-mthumb", "-mcpu=cortex-m3"])
    env["LINK"] = env["LINK"][:-2] + "gcc"
    env["LINKFLAGS"].remove("--gc-sections")
    env.Append(LINKFLAGS=["-Wl,--gc-sections"])
    return env


@register_compiler
def CompilerCM4GCC(**kwargs):
    env = CreateCompiler("ArmGCC", machine="armv7-m")
    env.Append(CPPFLAGS=["-mthumb", "-mlong-calls", "-mcpu=cortex-m4"])
    env.Append(ASFLAGS=["-mthumb", "-mcpu=cortex-m4"])
    env.Append(LINKFLAGS=["-mthumb", "-mcpu=cortex-m4"])
    env["LINK"] = env["LINK"][:-2] + "gcc"
    env["LINKFLAGS"].remove("--gc-sections")
    env.Append(LINKFLAGS=["-Wl,--gc-sections"])
    return env


def __CompilerGCC(**kwargs):
    env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
    env.Append(CFLAGS=["-std=gnu99"])
    env.Append(CPPFLAGS=["-Wall"])
    if not GetOption("strip"):
        env.Append(CPPFLAGS=["-g"])
    if IsPlatformTermux():
        prefix = os.getenv("PREFIX")
        env["CC"] = "%s/bin/clang" % (prefix)
        env["CXX"] = "%s/bin/clang++" % (prefix)
        env.Append(LIBS=["stdc++"])
    return env


@register_compiler
def CompilerGCC(**kwargs):
    return __CompilerGCC(**kwargs)


@register_compiler
def CompilerI686GCC(**kwargs):
    env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
    env.Append(CFLAGS=["-std=gnu99"])
    env.Append(CPPFLAGS=["-Wall", "-fno-stack-protector"])
    if not GetOption("strip"):
        env.Append(CPPFLAGS=["-g"])
        env.Append(ASFLAGS=["-g"])
    if IsPlatformWindows():
        gccx86 = "i686-elf-tools-windows.zip"
    else:
        gccx86 = "i686-elf-tools-linux.zip"
    cpl = Package("https://github.com/lordmilko/i686-elf-tools/releases/download/7.1.0/%s" % (gccx86))
    env["CC"] = "%s/bin/i686-elf-gcc -m32" % (cpl)
    env["AS"] = "%s/bin/i686-elf-gcc -m32 -c" % (cpl)
    env["CXX"] = "%s/bin/i686-elf-g++ -m32" % (cpl)
    env["LINK"] = "%s/bin/i686-elf-ld -m32 -melf_i386" % (cpl)
    env.Append(CPPPATH=["%s/lib/gcc/i686-elf/7.1.0/include" % (cpl)])
    env.Append(CPPFLAGS=["-ffunction-sections", "-fdata-sections"])
    env.Append(LINKFLAGS=["--gc-sections"])
    return env


@register_compiler
def CompilerNDK(**kwargs):
    import glob

    HOME = os.getenv("HOME")
    if HOME is None:
        HOME = os.getenv("USERPROFILE")
    NDK = os.path.join(HOME, "AppData/Local/Android/Sdk/ndk-bundle")
    if not os.path.exists(NDK):
        NDK = os.getenv("ANDROID_NDK")
    if NDK is None or not os.path.exists(NDK):
        print("==> Please set environment ANDROID_NDK\n\tset ANDROID_NDK=/path/to/android-ndk")
        exit()
    if IsPlatformWindows():
        host = "windows"
        NDK = NDK.replace(os.sep, "/")
    else:
        host = "linux"
    env = Environment(TOOLS=["ar", "as", "gcc", "g++", "gnulink"])
    env["ANDROID_NDK"] = NDK
    agcc = glob.glob(
        NDK + "/toolchains/aarch64-linux-android-*/prebuilt/%s-x86_64/bin/aarch64-linux-android-gcc*" % (host)
    )
    if len(agcc) > 0:
        agcc = agcc[-1]
        sysroot = glob.glob(NDK + "/platforms/android-2*/arch-arm64")[-1]
        env["CC"] = agcc
        env["CC"] = agcc
        env["AS"] = agcc
        env["CXX"] = os.path.dirname(agcc) + "/aarch64-linux-android-g++"
        env["LINK"] = env["CXX"]
        env.Append(CCFLAGS=["--sysroot", sysroot, "-fPIE", "-pie"])
        env.Append(LINKFLAGS=["--sysroot", sysroot, "-fPIE", "-pie"])
        env.Append(CFLAGS=["-std=gnu99"])
        env.Append(CPPFLAGS=["-Wall"])
        if not GetOption("strip"):
            env.Append(CPPFLAGS=["-g"])
    else:
        GCC = NDK + "/toolchains/llvm/prebuilt/%s-x86_64" % (host)
        env["CC"] = GCC + "/bin/aarch64-linux-android28-clang"
        env["AS"] = GCC + "/bin/aarch64-linux-android28-clang"
        env["CXX"] = GCC + "/bin/aarch64-linux-android28-clang++"
        env["LINK"] = GCC + "/bin/aarch64-linux-android28-clang++"
    return env


def AddPythonDev(env):
    pyp = sys.executable
    if IsPlatformWindows():
        pyp = pyp.replace(os.sep, "/")[:-10]
        major, minor = sys.version.split(".")[:2]
        pylib = "python" + major + minor
        if pylib in env.get("LIBS", []):
            return
        pf = "%s/libs/lib%s.a" % (pyp, pylib)
        if IsMsysPython():
            env.Append(CPPPATH=["/usr/include", "/usr/include/python%s.%s" % (major, minor)])
            env.Append(LIBPATH=["/usr/lib"])
            pylib = "python%s.%s.dll" % (major, minor)
        elif not os.path.exists(pf):
            RunCommand("cp {0}/libs/{1}.lib {0}/libs/lib{1}.a".format(pyp, pylib))
        env.Append(CPPDEFINES=["_hypot=hypot"])
        env.Append(CPPPATH=["%s/include" % (pyp)])
        env.Append(LIBPATH=["%s/libs" % (pyp)])
        istr = "set"
        pybind11_inc = "%s/Lib/site-packages/pybind11/include" % (pyp)
        if not os.path.isdir(pybind11_inc):
            import pybind11

            pybind11_inc = os.path.dirname(pybind11.__file__) + "/include"
    else:
        pyp = os.sep.join(pyp.split(os.sep)[:-2])
        if sys.version[0:3] == "2.7":
            _, pyp = RunSysCmd("which python3")
            pyp = os.sep.join(pyp.split(os.sep)[:-2])
            _, version = RunSysCmd('python3 -c "import sys; print(sys.version[0:3])"')
            pylib = "python" + version + "m"
        else:
            pylib = "python" + sys.version[0:3] + "m"
        pydir = "%s/include/%s" % (pyp, pylib)
        if not os.path.isdir(pydir):
            pylib = pylib[:-1]
        if pylib in env.get("LIBS", []):
            return
        env.Append(CPPPATH=[pydir])
        if pyp == "/usr":
            env.Append(LIBPATH=["%s/lib/x86_64-linux-gnu" % (pyp)])
            env.Append(CPPPATH=["%s/local/include/%s" % (pyp, pylib[:9])])
        else:
            env.Append(LIBPATH=["%s/lib" % (pyp)])
        istr = "export"
        pybind11_inc = "%s/lib/%s/site-packages/pybind11/include" % (pyp, pylib[:9])
    env.Append(CPPPATH=[pybind11_inc])
    aslog('%s PYTHONHOME=%s if see error " Py_Initialize: unable to load the file system codec"' % (istr, pyp))
    if not IsBuildForMSVC():
        env.Append(LIBS=[pylib, "pthread", "stdc++", "m"])


@register_compiler
def CompilerPYCC(**kwargs):
    pycpl = "GCC"
    if cplName != None:
        pycpl = cplName
    env = CreateCompiler(pycpl)
    AddPythonDev(env)
    return env


if IsPlatformWindows():
    VCs = Glob("C:/Program Files*/Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64")
    if len(VCs) > 0:
        VC = str(VCs[-1])

        @register_compiler
        def CompilerMSVC(**kwargs):
            env = Environment(TOOLS=["msvc", "mslib", "mslink", "mssdk", "msvs"])
            env["CC"] = os.path.join(VC, "cl.exe")
            env["AR"] = os.path.join(VC, "lib.exe")
            env["LINK"] = os.path.join(VC, "link.exe")
            env["MAXLINELENGTH"] = 10000000
            env.Append(LINKFLAGS=["/debug"])
            env.Append(CXXFLAGS=["/EHsc", "/std:c++20"])
            return env

    @register_compiler
    def Compilerx86GCC(**kwargs):
        env = __CompilerGCC(**kwargs)
        MSYS2 = os.getenv("MSYS2")
        env["CC"] = "%s/mingw32/bin/gcc" % (MSYS2)
        env["CXX"] = "%s/mingw32/bin/g++" % (MSYS2)
        env["AR"] = "%s/mingw32/bin/ar" % (MSYS2)
        env["LINK"] = "%s/mingw32/bin/gcc" % (MSYS2)
        return env


class QMakeEnv(CustomEnv):
    def __init__(self):
        super().__init__()
        if IsPlatformWindows():
            if os.path.isdir("C:/Qt"):
                QTDIR = "C:/Qt"
            else:
                QTDIR = os.getenv("QT_DIR")
            exe = ".exe"
        else:
            QTDIR = os.getenv("QT_DIR")
            exe = ""
        if not os.path.isdir(QTDIR):
            raise Exception("QT not found, set QT_DIR=/path/to/Qt")
        self.qmake = Glob("%s/*/*/bin/qmake%s" % (QTDIR, exe))[0].rstr()
        try:
            self.winqtdeploy = Glob("%s/*/*/bin/windeployqt%s" % (QTDIR, exe))[0].rstr()
        except:
            pass
        self.__installs__ = []
        self.target = None

    def Install(self, idir, objs):
        idir = os.path.abspath("%s/%s" % (BUILD_DIR, idir))
        MKDir(idir)
        for obj in objs:
            if type(obj) is str:
                tgt = obj
            else:
                tgt = obj.get_abspath()
            cmd = "cp -v %s %s" % (tgt, idir)
            os.system(cmd)
            if self.target == obj:
                cmd = "%s %s/%s" % (self.winqtdeploy, idir, os.path.basename(tgt))
                os.system(cmd)

    def Program(self, appName, objs, **kwargs):
        super().Program(appName, objs, **kwargs)
        CPPPATH = []
        LIBPATH = []
        CPPDEFINES = []
        LIBS = []
        self.getKL(kwargs, "CPPPATH", CPPPATH)
        self.getKL(kwargs, "LIBPATH", LIBPATH)
        self.getKL(kwargs, "LIBS", LIBS)
        self.getKL(kwargs, "CPPDEFINES", CPPDEFINES)
        for _, kwargs in self.objs.items():
            self.getKL(kwargs, "CPPPATH", CPPPATH)
            self.getKL(kwargs, "LIBPATH", LIBPATH)
            self.getKL(kwargs, "CPPDEFINES", CPPDEFINES)
        qpro = open("%s/%s.pro" % (BUILD_DIR, appName), "w")
        if getattr(self, "shared", False):
            qpro.write("TEMPLATE = lib\n")
        qpro.write("QT += core gui widgets charts\n")
        qpro.write("CONFIG += c++11\n")
        qpro.write("CONFIG += console\n")
        qpro.write("CONFIG += object_parallel_to_source\n")
        qpro.write("DEFINES -= UNICODE _UNICODE\n")
        qpro.write("INCLUDEPATH += %s\n\n" % ("    ".join(['"%s" \\\n' % (self.abspath(p)) for p in CPPPATH])))
        qpro.write("DEFINES += %s\n\n" % (" ".join(CPPDEFINES)))
        cppstr = "SOURCES += \\\n"
        hppstr = "HEADERS += \\\n"
        DLLS = []
        for obj in objs:
            if type(obj) is str and (obj.endswith(".dll") or obj.endswith(".so")):
                DLLS.append(obj)
        for dll in DLLS:
            objs.remove(dll)
            if dll.endswith(".dll"):
                lib = os.path.basename(dll)[:-4]
            if dll.endswith(".so"):
                lib = os.path.basename(dll)[3:-3]
            if lib not in LIBS:
                LIBS.append(lib)
                LIBPATH.append(os.path.dirname(dll))
        for obj in objs:
            rp = self.abspath(obj.rstr())
            if rp.endswith(".hpp"):
                hppstr += "    %s \\\n" % (rp)
            else:
                cppstr += "    %s \\\n" % (rp)
        qpro.write(cppstr + "\n")
        qpro.write(hppstr + "\n")
        qpro.write("LIBS += %s\n\n" % (" ".join(["-l%s" % (l) for l in LIBS])))
        qpro.write("LIBPATH += %s\n\n" % ("    ".join(['"%s" \\\n' % (self.abspath(p)) for p in LIBPATH])))
        qpro.close()
        cmd = '%s %s/%s.pro -spec win32-g++ "CONFIG+=debug"' % (self.qmake, BUILD_DIR, appName)
        RunCommand(cmd)
        cmd = "cd %s && make" % (BUILD_DIR)
        RunCommand(cmd)
        if getattr(self, "shared", False):
            if IsPlatformWindows():
                target = "%s/debug/%s.dll" % (BUILD_DIR, appName)
            else:
                target = "%s/debug/lib%s.so" % (BUILD_DIR, appName)
        else:
            target = "%s/debug/%s.exe" % (BUILD_DIR, appName)
            self.target = target
        return [target]


@register_compiler
def CompilerQMake(**kwargs):
    return QMakeEnv()


def GetCWS12DIR():
    CWS12DIR = os.getenv("CWS12DIR")
    if CWS12DIR is None:
        for D in ["c", "d", "e", "f"]:
            ds = Glob("%s:Program*/Freescale/CWS12*" % (D))
            if len(ds) > 0:
                CWS12DIR = ds[0].rstr()
                break
    if not os.path.isdir(CWS12DIR):
        raise Exception("CWS12 Compiler not found, set CWS12DIR=/path/to/Freescale/CWS12v5.1")
    return CWS12DIR


class CWS12MakeEnv(CustomEnv):
    def __init__(self):
        super().__init__()
        CWS12DIR = GetCWS12DIR()
        self.CWS12DIR = CWS12DIR
        self.target = None

    def IsNeedBuild(self, obj, target):
        robj = "%s/%s.o" % (BUILD_DIR, os.path.basename(obj)[:-2])
        if os.path.exists(robj):
            rtm = os.path.getmtime(robj)
            stm = os.path.getmtime(obj)
            if stm < rtm:
                return False
        return True

    def AddPostAction(self, target, action):
        RunCommand(action)

    def Program(self, appName, objs, **kwargs):
        if GetOption("clean"):
            RunCommand("rm -frv %s/*" % (BUILD_DIR))
            return []
        super().Program(appName, objs, **kwargs)
        target = appName
        CPPPATH = []
        LIBPATH = []
        CPPDEFINES = []
        LIBS = []
        LINKFLAGS = []
        CPPFLAGS = []
        self.getKL(kwargs, "CPPPATH", CPPPATH)
        self.getKL(kwargs, "LIBPATH", LIBPATH)
        self.getKL(kwargs, "LIBS", LIBS)
        self.getKL(kwargs, "CPPDEFINES", CPPDEFINES)
        self.getKL(kwargs, "LINKFLAGS", LINKFLAGS)
        self.getKL(kwargs, "CPPFLAGS", CPPFLAGS)
        link_script = None
        for i, flg in enumerate(LINKFLAGS):
            if flg == "-T":
                link_script = LINKFLAGS[i + 1]
                break
        for _, kwargs in self.objs.items():
            self.getKL(kwargs, "CPPPATH", CPPPATH)
            self.getKL(kwargs, "LIBPATH", LIBPATH)
            self.getKL(kwargs, "CPPDEFINES", CPPDEFINES)
        CC = self.CWS12DIR + "/Prog/chc12.exe"
        AS = self.CWS12DIR + "/Prog/ahc12.exe"
        LINK = self.CWS12DIR + "/Prog/linker.exe"
        S19 = self.CWS12DIR + "/Prog/burner.exe"
        MAKE = '"{0}/Prog/piper.exe" "{0}/Prog/maker.exe"'.format(self.CWS12DIR)
        with open("%s/makefile-%s.9s12" % (BUILD_DIR, target), "w") as fp:
            fp.write('CC = "%s"\n' % (CC))
            fp.write('AS = "%s"\n' % (AS))
            fp.write('LD = "%s"\n' % (LINK))
            fp.write("COMMON_FLAGS = -WErrFileOff -WOutFileOff -EnvOBJPATH=%s\n" % (BUILD_DIR))
            fp.write('C_FLAGS   = -Cc -I"%s/lib/hc12c/include" -Mb -CpuHCS12X\n' % (self.CWS12DIR))
            fp.write('ASM_FLAGS = -I"%s/lib/hc12c/include" -Mb -CpuHCS12X\n' % (self.CWS12DIR))
            fp.write("LD_FLAGS  = -M -WmsgNu=abcet\n")
            fp.write('LIBS = "%s/lib/hc12c/lib/ansixbi.lib"\n' % (self.CWS12DIR))
            fp.write("INC = ")
            for p in CPPPATH:
                fp.write("-I%s " % (p))
            fp.write("\nC_FLAGS += ")
            for d in CPPDEFINES:
                fp.write("-D%s " % (d))
            fp.write("\nC_FLAGS += ")
            for d in CPPFLAGS:
                fp.write("%s " % (d))
            fp.write("\n\nOBJS = ")
            for obj in objs:
                obj = str(obj)
                if obj.endswith(".c") or obj.endswith(".C"):
                    if self.IsNeedBuild(obj, target):
                        fp.write(obj[:-2] + ".o ")
            fp.write("\n\nOBJS_LINK = ")
            for obj in objs:
                obj = str(obj)
                if obj.endswith(".c") or obj.endswith(".C"):
                    fp.write("%s/%s.o " % (BUILD_DIR, os.path.basename(obj)[:-2]))
            fp.write(
                """\n
.asm.o:
    $(ASM) $*.asm $(COMMON_FLAGS) $(ASM_FLAGS)

.c.o:
    $(CC) $*.c $(INC) $(COMMON_FLAGS) $(C_FLAGS)

all:$(OBJS) {0}.abs
    {2} OPENFILE "{0}.s19" format=motorola busWidth=1 origin=0 len=0x80000000 destination=0 SRECORD=Sx SENDBYTE 1 "{0}.abs" CLOSE

{0}.abs :
    $(LD) {1} $(COMMON_FLAGS) $(LD_FLAGS) -Add($(OBJS_LINK)) -Add($(LIBS)) -M -O$*.abs""".format(
                    "%s/%s" % (BUILD_DIR, target), link_script, S19
                )
            )
        cmd = "%s %s/makefile-%s.9s12" % (MAKE, BUILD_DIR, target)
        with open("%s/%s.bat" % (BUILD_DIR, target), "w") as fp:
            fp.write("@echo off\n%s\n" % (cmd))
        RunCommand("%s/%s.bat" % (BUILD_DIR, target))
        return []


@register_compiler
def CompilerCWS12(**kwargs):
    return CWS12MakeEnv()


def CreateCompiler(name, **kwargs):
    if name in __compilers__:
        return __compilers__[name](**kwargs)
    else:
        raise KeyError("compiler %s not found, available compilers: %s" % (name, [k for k in __compilers__.keys()]))


def GetCurrentDir():
    conscript = File("SConscript")
    fn = conscript.rfile()
    path = os.path.dirname(fn.abspath)
    return path


def RegisterCPPPATH(name, path, force=False):
    if not name.startswith("$"):
        raise Exception('CPPPATH name %s not starts with "$"' % (name))
    if name not in __cpppath__ or force == True:
        aslog("register global CPPPATH: %s=%s" % (name, path))
        __cpppath__[name] = path
    else:
        if __cpppath__[name] != path:
            raise KeyError("CPPPATH %s already registered" % (name))


def RequireCPPPATH(name):
    if not name.startswith("$"):
        raise Exception('CPPPATH name %s not starts with "$"' % (name))
    if name in __cpppath__:
        return __cpppath__[name]
    elif name.endswith("_Cfg"):
        print("WARNING: CPPPATH %s not found" % (name))
        return []
    else:
        raise KeyError("CPPPATH %s not found, available CPPPATH: %s" % (name, [k for k in __cpppath__.keys()]))


def RegisterConfig(name, source, force=False, CPPPATH=None):
    if name not in __cfgs__ or force == True:
        if len(source) == 0:
            raise Exception("No config provided for %s" % (name))
        if None != CPPPATH:
            path = CPPPATH
        else:
            path = os.path.dirname(str(source[0]))
        __cfgs__[name] = (path, source)
    else:
        if len(source) == 0:
            raise Exception("No config provided for %s" % (name))
        if None != CPPPATH:
            path = CPPPATH
        else:
            path = os.path.dirname(str(source[0]))
        if __cfgs__[name] != (path, source):
            raise KeyError("CFG %s already registered" % (name))


def RequireConfig(name):
    if name in __cfgs__:
        return __cfgs__[name]
    else:
        raise KeyError("CFG %s not found, available CPPPATH: %s" % (name, [k for k in __cfgs__.keys()]))


def RequireLibrary(name):
    if name in __libraries__:
        return __libraries__[name]
    else:
        raise KeyError("CPPPATH %s not found, available CPPPATH: %s" % (name, [k for k in __libraries__.keys()]))


class BuildBase:
    def __init__(self):
        self.__libs__ = {}
        self.__libs_order__ = []
        self.__extra_libs__ = []
        aslog("init base %s" % (self.name))
        if getattr(self, "prebuilt_shared", False):
            CPPPATH = []
            searched_libs = []
            for name in getattr(self, "LIBS", []):
                if name not in __libraries__:
                    if name not in self.__extra_libs__:
                        self.__extra_libs__.append(name)
                else:
                    lib = __libraries__[name]()
                    CPPPATH += lib.get_includes(searched_libs)
            include = getattr(self, "include", [])
            if type(include) is str:
                self.include = [include] + CPPPATH
            else:
                self.include = include + CPPPATH
            aslog("init base %s: include = %s" % (self.name, self.include))
        else:
            for name in getattr(self, "LIBS", []):
                self.ensure_lib(name)

    def init_base(self):
        self.CPPPATH = []
        self.LIBPATH = []
        self.include = []
        self.LIBS = []
        self.CPPDEFINES = []
        self.CPPFLAGS = []
        self.CXXFLAGS = []

    def AddPostAction(self, action):
        if not hasattr(self, "__post_actions__"):
            self.__post_actions__ = []
        self.__post_actions__.append(action)

    def Install(self, idir, files=None):
        if files != None:
            idir = os.path.join(BUILD_DIR, idir)
            for f in files:
                if os.path.isdir(f):
                    dst = os.path.join(idir, os.path.basename(f))
                    if os.path.isdir(dst):
                        print("rm -r %s" % (dst))
                        shutil.rmtree(dst)
                    print("cp -r %s %s" % (f, dst))
                    shutil.copytree(f, dst)
                else:
                    print("cp %s %s" % (f, idir))
                    shutil.copy(f, idir)
            return
        if getattr(self, "__install__dirs__", None) is None:
            self.__install__dirs__ = []
        self.__install__dirs__.append(idir)

    def do_install(self, target):
        env = self.ensure_env()
        idirs = getattr(self, "__install__dirs__", [])
        for idir in idirs:
            env.Install(idir, target)

    def Append(self, **kwargs):
        env = self.ensure_env()
        env.Append(**kwargs)

    def Remove(self, **kwargs):
        env = self.ensure_env()
        if isinstance(env, CustomEnv):
            env.Remove(**kwargs)
            return
        for key, v in kwargs.items():
            if key in env:
                if type(v) is str:
                    if v in env[key]:
                        env[key].remove(v)
                else:
                    for vv in v:
                        if vv in env[key]:
                            env[key].remove(vv)

    def ParseConfig(self, cmd):
        env = self.ensure_env()
        env.ParseConfig(cmd)

    def SelectOS(self, **kwargs):
        if getattr(self, "user", None):
            return self.user.SelectOS(**kwargs)
        self.os = kwargs.get("name", "OS")
        if "arch" in kwargs:
            self.arch = kwargs["arch"]
        if "variant" in kwargs:
            self.os_variant = kwargs["variant"]
        if "config" in kwargs:
            config = kwargs["config"]
            self.RegisterConfig(self.os, config)
        self.LIBS += [self.os]
        self.Append(CPPDEFINES=["USE_%s" % (self.os.upper())])
        if "CPPDEFINES" in kwargs:
            self.Append(CPPDEFINES=kwargs["CPPDEFINES"])
        if "CPPPATH" in kwargs:
            self.Append(CPPPATH=kwargs["CPPPATH"])

    def GetOS(self):
        if getattr(self, "user", None):
            return self.user.GetOS()
        if hasattr(self, "os"):
            return self.os
        if TARGET_OS:
            return TARGET_OS
        return None

    def GetOSVariant(self):
        if getattr(self, "user", None):
            return self.user.GetOSVariant()
        if hasattr(self, "os_variant"):
            return self.os_variant
        return None

    def GetArch(self):
        if getattr(self, "user", None):
            return self.user.GetArch()
        if hasattr(self, "arch"):
            return self.arch
        if TARGET_ARCH:
            return TARGET_ARCH
        raise Exception("arch is not specified, add --arch")

    def GetCompiler(self):
        if getattr(self, "user", None):
            return self.user.GetCompiler()
        if getattr(self, "compiler", None):
            return self.compiler
        return GetOption("compiler")

    def ensure_env(self):
        if getattr(self, "user", None):
            return self.user.ensure_env()
        if self.env is None:
            cplName = getattr(self, "compiler", GetOption("compiler"))
            if cplName not in __compilers__:
                print("avaliable compilers:", [k for k in __compilers__.keys()])
                print('use option "--cpl=?" to choose a compiler to build.')
                exit(-1)
            self.env = CreateCompiler(cplName)
        return self.env

    def RegisterCPPPATH(self, name, path, force=False):
        if not name.startswith("$"):
            raise Exception('CPPPATH name %s not starts with "$"' % (name))
        if getattr(self, "user", None):
            if getattr(self.user, "user", None):
                self.user.RegisterCPPPATH(name, path, force)
            else:
                if name not in self.user.__cpppath__ or force == True:
                    aslog("register local CPPPATH: %s=%s" % (name, path))
                    self.user.__cpppath__[name] = path
                else:
                    raise KeyError("CPPPATH %s already registered for %s" % (name, self.user.__class__.__name__))
        else:
            RegisterCPPPATH(name, path, force)

    def RequireCPPPATH(self, name):
        if not name.startswith("$"):
            raise Exception('CPPPATH name %s not starts with "$"' % (name))
        if getattr(self, "user", None):
            if getattr(self.user, "user", None):
                self.user.RequireCPPPATH(name)
            else:
                if name in self.user.__cpppath__:
                    return self.user.__cpppath__[name]
        elif hasattr(self, "__cpppath__"):
            if name in self.__cpppath__:
                return self.__cpppath__[name]
        return RequireCPPPATH(name)

    def Generate(self, js):
        source = {}
        ret = generate(js, GetOption("gen"))
        for name, src in ret.items():
            source[name] = []
            for x in src:
                source[name] += PkgGlob(os.path.dirname(x), [os.path.basename(x)])
        return source

    def RegisterConfig(self, name, source, force=False, CPPPATH=None):
        srcs = []
        js = []

        for s in source:
            if (type(s) is SCons.Node.FS.File and s.rstr().endswith(".json")) or (
                type(s) is str and s.endswith(".json")
            ):
                ret = self.Generate(s)
                for libName, src in ret.items():
                    if libName == name:
                        srcs += src
                    else:
                        self.Append(CPPDEFINES=["USE_%s" % (libName.split(":")[0].upper())])
                        self.__RegisterConfig(libName, src, force, CPPPATH)
                    if libName not in self.LIBS:
                        self.LIBS += [libName]
            else:
                srcs.append(s)
        self.__RegisterConfig(name, srcs, force, CPPPATH)
        if "$%s_Cfg" % (name) not in self.CPPPATH:
            self.CPPPATH.append("$%s_Cfg" % (name))

    def __RegisterConfig(self, name, source, force=False, CPPPATH=None):
        if getattr(self, "user", None):
            if name not in self.user.__cfgs__ or force == True:
                if len(source) == 0:
                    if GetOption("prebuilt"):
                        return
                    raise Exception("No config provided for %s" % (name))
                if None != CPPPATH:
                    path = CPPPATH
                else:
                    path = os.path.dirname(str(source[0]))
                aslog("register config %s: %s" % (name, [str(x) for x in source]))
                self.user.__cfgs__[name] = (path, source)
            else:
                raise KeyError("CFG %s already registered for %s" % (name, self.user.__class__.__name__))
        else:
            RegisterConfig(name, source, force, CPPPATH)
        # register a special config path for the module
        if None != CPPPATH:
            path = CPPPATH
        else:
            path = os.path.dirname(str(source[0]))
        self.RegisterCPPPATH("$%s_Cfg" % (name), path, force)

    def RequireConfig(self, name):
        if getattr(self, "user", None):
            if name in self.user.__cfgs__:
                return self.user.__cfgs__[name]
        elif hasattr(self, "__cfgs__"):
            if name in self.__cfgs__:
                return self.__cfgs__[name]
        return RequireConfig(name)

    def is_shared_library(self):
        if getattr(self, "shared", False):
            return True
        if getattr(self, "user", None):
            return self.user.is_shared_library()
        return False

    def is_object_library(self):
        ret = getattr(self, "object", False)
        return ret

    def libName(self):
        ln = self.name
        if ":" in ln:
            ln = ln.replace(":", "_").replace(".", "_")
        return ln

    def ensure_lib(self, name):
        if getattr(self, "user", None) and not getattr(self, "shared", False):
            self.user.ensure_lib(name)
        else:
            if name in __libraries__:
                if name not in self.__libs__:
                    self.__libs_order__.append(name)
                    self.__libs__[name] = __libraries__[name](
                        user=self, env=self.env, compiler=getattr(self, "compiler", None)
                    )
            else:
                if name not in self.__extra_libs__:
                    self.__extra_libs__.append(name)

    def get_libs(self):
        libs = {}
        if getattr(self, "user", None):
            libs = self.user.get_libs()
        else:
            libs = self.__libs__
        return libs

    def get_includes(self, searched_libs):
        CPPPATH = []
        if getattr(self, "include", None):
            if type(self.include) == str:
                CPPPATH.append(self.include)
            else:
                CPPPATH.extend(self.include)
        elif getattr(self, "INCLUDE", None):
            if type(self.INCLUDE) == str:
                CPPPATH.append(self.INCLUDE)
            else:
                CPPPATH.extend(self.INCLUDE)

        libs = self.get_libs()
        for libName in getattr(self, "LIBS", []):
            if libName in libs and libName not in searched_libs:
                searched_libs.append(libName)
                lib = libs[libName]
                if type(lib) is type:
                    CPPPATH.extend(lib.get_includes(lib, searched_libs))
                else:
                    CPPPATH.extend(lib.get_includes(searched_libs))
        CPPPATH2 = []
        for x in CPPPATH:
            if x not in CPPPATH2:
                CPPPATH2.append(x)
        return CPPPATH2

    def GetInclude(self, libName):
        CPPPATH = []
        searched_libs = []
        if libName in __libraries__:
            lib = __libraries__[libName]()
            CPPPATH += lib.get_includes(searched_libs)
        return CPPPATH

    def ProcessCPPPATH(self, CPPPATH):
        CPPPATH2 = []
        for p in CPPPATH:
            if p.startswith("$"):
                CPPPATH2.append(self.RequireCPPPATH(p))
            elif p.startswith("#"):
                CPPPATH2.append(self.GetInclude(p[1:]))
            else:
                CPPPATH2.append(p)
        return CPPPATH2

    def sortL(self, L):
        newL = []
        for x in L:
            if type(x) is list:
                for x1 in x:
                    if x1 not in newL:
                        newL.append(x1)
            else:
                if x not in newL:
                    newL.append(x)
        return newL


class Library(BuildBase):
    # for a library, it provides header files(*.h) and the library(*.a)
    def __init__(self, **kwargs):
        if not hasattr(self, "name"):
            self.name = self.__class__.__name__[7:]
        aslog("init library %s" % (self.name))
        # local cpp_path for libraries
        self.__cpppath__ = {}
        self.__cfgs__ = {}
        self.user = kwargs.get("user", None)
        self.env = kwargs.get("env", None)
        self.top = kwargs.get("top", False)
        compiler = kwargs.get("compiler", getattr(self, "compiler", None))
        if compiler != None:
            self.compiler = compiler
        if False == getattr(self, "typed_class", False):
            self.init_base()
        self.config()
        if (GetOption("prebuilt") or getattr(self, "shared", False)) and (GetOption("library") != self.name):
            if getattr(self, "shared", False):
                if IsBuildForWindows():
                    dll = os.path.abspath("%s/../prebuilt/%s.dll" % (BUILD_DIR, self.name))
                else:
                    dll = os.path.abspath("%s/../prebuilt/lib%s.so" % (BUILD_DIR, self.name))
                if os.path.isfile(dll):
                    self.source = [dll]
                    self.prebuilt_shared = True
                else:
                    raise Exception("The prebuilt shared library not found: %s" % (dll))
            else:
                liba = os.path.abspath("%s/../prebuilt/lib%s.a" % (BUILD_DIR, self.name))
                if os.path.isfile(liba):
                    self.source = [liba]
        if hasattr(self, "include") and len(self.include):
            self.RegisterCPPPATH("$%s" % (self.name), self.include, force=True)
        super().__init__()

    def get_opt_cfg(self, CPPPATH, libName):
        cfg_path = _cfg_path
        if cfg_path != None:
            if os.path.isfile(cfg_path):
                CPPPATH.append(os.path.dirname(cfg_path))
            elif os.path.isdir(cfg_path):
                source = Glob("%s/%s_Cfg.c" % (cfg_path, libName))
                if len(source) > 0:
                    CPPPATH.append(cfg_path)

    def objs(self):
        libName = self.name
        if getattr(self, "prebuilt_shared", False):
            aslog("prebuilt shared of %s" % (libName))
            return self.source
        aslog("build objs of %s" % (libName))
        env = self.ensure_env()
        CPPPATH = getattr(self, "CPPPATH", [])
        CPPDEFINES = getattr(self, "CPPDEFINES", []) + list(env.get("CPPDEFINES", []))
        CFLAGS = list(env.get("CFLAGS", []))
        ASFLAGS = list(env.get("ASFLAGS", []))
        CPPFLAGS = getattr(self, "CPPFLAGS", []) + list(env.get("CPPFLAGS", []))
        CXXFLAGS = getattr(self, "CXXFLAGS", []) + list(env.get("CXXFLAGS", []))
        CPPPATH = self.ProcessCPPPATH(CPPPATH) + list(env.get("CPPPATH", []))
        searched_libs = []
        CPPPATH += self.get_includes(searched_libs)
        try:
            # others has provide the config for this library
            cfg_path, source = self.RequireConfig(libName)
            CPPPATH.insert(0, cfg_path)
            for src in source[1:]:
                cfg_path = os.path.dirname(str(src))
                CPPPATH.insert(0, cfg_path)
            self.source += source
        except KeyError:
            self.get_opt_cfg(CPPPATH, libName)

        for name in getattr(self, "LIBS", []):
            if name in __libraries__:
                try:
                    libInclude = self.RequireCPPPATH("$%s" % (name))
                    CPPPATH.append(libInclude)
                except KeyError:
                    pass
        CPPPATH = self.sortL(CPPPATH)
        CPPDEFINES = self.sortL(CPPDEFINES)
        objs = []
        source = []
        for c in self.source:
            # because of "#libName" in CPPPATH, so possible that source has some duplicate files
            if c not in source:
                source.append(c)
            else:
                continue
            if str(c).endswith(".a") or str(c).endswith(".dll") or str(c).endswith(".so"):
                objs.append(c)
            elif self.is_shared_library():
                objs += env.SharedObject(
                    c, CPPPATH=CPPPATH, CPPDEFINES=CPPDEFINES, CPPFLAGS=CPPFLAGS, CFLAGS=CFLAGS, CXXFLAGS=CXXFLAGS
                )
            else:
                if "AS" in env and "gcc" in env["AS"]:
                    if str(c).endswith(".S"):
                        ASFLAGS += ["-I%s" % (x) for x in CPPPATH]
                        ASFLAGS += ["-D%s" % (x) for x in CPPDEFINES]
                        objs += env.Object(c, ASFLAGS=ASFLAGS)
                        continue
                objs += env.Object(
                    c, CPPPATH=CPPPATH, CPPDEFINES=CPPDEFINES, CPPFLAGS=CPPFLAGS, CFLAGS=CFLAGS, CXXFLAGS=CXXFLAGS
                )
        return objs

    def build(self):
        if getattr(self, "prebuilt_shared", False):
            return self.source
        libName = self.libName()
        env = self.ensure_env()
        LIBS = []
        objs = []
        LIBPATH = list(env.get("LIBPATH", [])) + getattr(self, "LIBPATH", [])
        for libName_ in self.__libs_order__:
            lib = self.__libs__[libName_]
            objs_ = lib.objs()
            thresh = 200
            # intension here that check on 'len(objs_)', not 'len(objs + objs_)'
            tooMuch = True if len(objs_) > thresh else False
            if lib.is_object_library():
                tooMuch = False  # object library not allowed to be a static library
            if tooMuch:
                lib_ = env.Library(lib.libName(), objs_)[0]
                p = os.path.dirname(lib_.get_abspath())
                if p not in LIBPATH:
                    LIBPATH.append(p)
                LIBS.append(lib.libName())
            else:
                objs += objs_
            LIBPATH += getattr(lib, "LIBPATH", [])
        objs += self.objs()
        if self.is_shared_library():
            LINKFLAGS = getattr(self, "LINKFLAGS", []) + list(env.get("LINKFLAGS", []))
            objs2 = []
            for obj in objs:
                if type(obj) is SCons.Node.FS.File:
                    if obj.rstr().endswith(".a"):
                        name = os.path.basename(obj.rstr())[3:-2]
                        LIBPATH.append(os.path.dirname(obj.rstr()))
                        LIBS.append(name)
                    else:
                        objs2.append(obj)
                elif str(obj).endswith(".a"):
                    name = os.path.basename(obj)[3:-2]
                    LIBPATH.append(os.path.dirname(obj))
                    LIBS.append(name)
                elif str(obj).endswith(".dll"):
                    name = os.path.basename(obj)[:-4]
                    LIBPATH.append(os.path.dirname(obj))
                    LIBS.append(name)
                else:
                    objs2.append(obj)
            LIBS += list(env.get("LIBS", [])) + self.__extra_libs__
            aslog("build shared library %s" % (libName))
            target = env.SharedLibrary(libName, objs2, LIBPATH=LIBPATH, LIBS=LIBS, LINKFLAGS=LINKFLAGS)
        else:
            aslog("build static library %s" % (libName))
            target = env.Library(libName, objs)
        for action in getattr(self, "__post_actions__", []):
            env.AddPostAction(target, action)
        self.do_install(target)
        return target


class Driver(Library):
    def __init__(self, **kwargs):
        self.name = "%s:%s" % (self.__class__.__name__[6:], self.cls)
        super().__init__(**kwargs)


class Application(BuildBase):
    def __init__(self, **kwargs):
        self.name = self.__class__.__name__[11:]
        aslog("init application %s" % (self.name))
        # local cpp_path for libraries
        self.__cpppath__ = {}
        self.__cfgs__ = {}
        self.env = None
        self.init_base()
        self.config()
        if TARGET_OS != None:
            if TARGET_OS not in __OSs__:
                raise Exception("Invalid OS %s" % (TARGET_OS))
            self.LIBS += [TARGET_OS]
            self.Append(CPPDEFINES=["USE_%s" % (TARGET_OS.upper())])
            if TARGET_OS != "OSAL":
                self.LIBS += ["OSAL"]
                self.Append(CPPDEFINES=["USE_OSAL"])
        if True == GetOption("det") or IsBuildForHost(self.GetCompiler()):
            self.LIBS += ["Det"]
            self.Append(CPPDEFINES=["USE_DET"])
        super().__init__()

    def build(self):
        env = self.ensure_env()
        appName = self.name
        aslog("build application %s" % (appName))
        LIBS = env.get("LIBS", [])
        CPPDEFINES = getattr(self, "CPPDEFINES", []) + list(env.get("CPPDEFINES", []))
        CPPFLAGS = getattr(self, "CPPFLAGS", []) + list(env.get("CPPFLAGS", []))
        CXXFLAGS = getattr(self, "CXXFLAGS", []) + list(env.get("CXXFLAGS", []))
        LINKFLAGS = getattr(self, "LINKFLAGS", []) + list(env.get("LINKFLAGS", []))
        CPPPATH = self.ProcessCPPPATH(getattr(self, "CPPPATH", []))
        CPPPATH += list(env.get("CPPPATH", []))
        objs = self.source
        LIBPATH = list(env.get("LIBPATH", [])) + getattr(self, "LIBPATH", [])
        searched_libs = []
        CPPPATH += self.get_includes(searched_libs)
        for name in self.__libs_order__:
            lib = self.__libs__[name]
            if lib.is_shared_library():
                aslog("build shared library %s" % (name))
                objs_ = lib.build()
                objs += objs_
                continue
            aslog("build library %s" % (name))
            objs_ = lib.objs()
            if isinstance(env, CustomEnv):
                thresh = 1000000
            else:
                thresh = 200
            tooMuch = True if (len(objs) + len(objs_)) > thresh else False
            if lib.is_object_library():
                tooMuch = False  # object library not allowed to be a static library
            libObjs = []
            for obj in objs_:
                if str(obj).endswith(".a"):
                    libName = os.path.basename(str(obj))[3:-2]
                    p = os.path.dirname(str(obj))
                    if p not in LIBPATH:
                        LIBPATH.append(p)
                    LIBS.append(libName)
                elif str(obj).endswith(".dll"):
                    libName = os.path.basename(str(obj))[:-4]
                    p = os.path.dirname(str(obj))
                    if p not in LIBPATH:
                        LIBPATH.append(p)
                    LIBS.append(libName)
                else:
                    if not tooMuch:
                        objs.append(obj)
                    else:
                        libObjs.append(obj)
            if len(libObjs) > 0:
                libName = lib.libName()
                libT = env.Library(libName, libObjs)
                libO = libT[0]
                p = os.path.dirname(libO.get_abspath())
                if p not in LIBPATH:
                    LIBPATH.append(p)
                if "armcc" in env["CC"]:
                    objs.append(libT)
                else:
                    LIBS.append(libName)
            LIBPATH += getattr(lib, "LIBPATH", [])
        LIBS += self.__extra_libs__
        target = env.Program(
            appName,
            objs,
            CPPPATH=CPPPATH,
            CPPDEFINES=CPPDEFINES,
            CPPFLAGS=CPPFLAGS,
            CXXFLAGS=CXXFLAGS,
            LIBS=LIBS,
            LINKFLAGS=LINKFLAGS,
            LIBPATH=LIBPATH,
        )
        if "S19" in env:
            BUILD_DIR = os.path.dirname(target[0].get_abspath())
            action = env["S19"].format(target[0].get_abspath(), "%s/%s.s19" % (BUILD_DIR, appName))
            env.AddPostAction(target, action)
        if "BIN" in env:
            BUILD_DIR = os.path.dirname(target[0].get_abspath())
            action = env["BIN"].format(target[0].get_abspath(), "%s/%s.bin" % (BUILD_DIR, appName))
            env.AddPostAction(target, action)
        if "ELFSIZE" in env:
            BUILD_DIR = os.path.dirname(target[0].get_abspath())
            action = env["ELFSIZE"].format(target[0].get_abspath())
            env.AddPostAction(target, action)

            def readelf(target, source, env):
                cmd = "%s %s/tools/utils/readelf.py -i %s -o %s/%s.json" % (
                    sys.executable,
                    RootDir,
                    target[0].get_abspath(),
                    BUILD_DIR,
                    appName,
                )
                print(cmd)
                # RunCommand(cmd)

            # env.AddPostAction(target, readelf)
        for action in getattr(self, "__post_actions__", []):
            env.AddPostAction(target, action)
        self.do_install(target)


class Qemu:
    def __init__(self, **kwargs):
        arch_map = {"x86": "i386", "cortex-m": "arm", "arm64": "aarch64", "armmcu": "gnuarmeclipse"}
        self.arch = kwargs["arch"]
        self.portCAN0 = self.FindPort()
        self.params = "-serial stdio"
        if kwargs.get("CAN0", False):
            self.params += " -serial tcp:127.0.0.1:%s,server" % (self.portCAN0)
        if "gdb" in COMMAND_LINE_TARGETS:
            self.params += " -gdb tcp::1234 -S"
        if self.arch in arch_map.keys():
            self.arch = arch_map[self.arch]
        self.qemu = self.FindQemu()

    def GetArmMcu(self):
        url = "https://github.com/ilg-archived/qemu/releases/download/gae-2.8.0-20161227/"
        if IsPlatformWindows():
            url += "gnuarmeclipse-qemu-win64-2.8.0-201612271623-dev-setup.exe"
        else:
            url += "gnuarmeclipse-qemu-debian64-2.8.0-201612271623-dev.tgz"
        pkg = Package(url)
        if IsPlatformWindows():
            pkg = Glob("C:/Program*/GNU*/QEMU/2.8.0-201612271623-dev/bin")[0].rstr()
        return "%s/qemu-system-gnuarmeclipse" % (pkg)

    def GetQemu(self):
        if IsPlatformWindows():
            url = "https://qemu.weilnetz.de/w64/2022/qemu-w64-setup-20221208.exe"
            Package(url)
            pkg = Glob("C:/Program*/qemu")[0].rstr()
            return "%s/qemu-system-%s" % (pkg, self.arch)
        else:
            return "qemu-system-%s" % (self.arch)

    def FindPort(self, port=9000):
        import socket

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        portMAX = port + 1000
        while port < portMAX:
            try:
                sock.bind(("127.0.0.1", port))
                break
            except:
                port += 1
        sock.close()
        return port

    def CreateDiskImg(self, file, **kwargs):
        size = kwargs.get("size", 1024 * 1024 * 32)
        type = kwargs.get("type", "raw")
        if os.path.exists(file):
            print('DiskImg "%s" already exist!' % (file))
            return
        print('Create a New DiskImg "%s"!' % (file))
        qemu = self.FindQemu()
        qemuimg = "%s/%s" % (os.path.dirname(qemu), "qemu-img")
        if IsPlatformWindows():
            qemuimg += ".exe"
            qemuimg = '%s"' % (qemuimg)
        RunCommand("%s create -f raw %s %s" % (qemuimg, file, size))

        if type.startswith("ext"):
            if IsPlatformWindows():
                pass  # TODO
            else:
                RunCommand("sudo mkfs.%s -b 4096 %s" % (type, file))
        elif type.startswith("vfat"):
            if IsPlatformWindows():
                pass  # TODO
            else:
                RunCommand("sudo mkfs.fat %s" % (file))

    def FindQemu(self):
        if self.arch == "gnuarmeclipse":
            qemu = self.GetArmMcu()
        else:
            qemu = self.GetQemu()
        if IsPlatformWindows():
            qemu += ".exe"
            qemu = '"%s"' % (qemu)
        return qemu

    def Run(self, params):
        cmd = "%s %s %s" % (self.qemu, params, self.params)
        print(cmd)
        RunCommand(cmd)


def Building():
    appName = GetOption("application")
    libName = GetOption("library")
    if appName not in __apps__ and libName not in __libraries__:
        print("avaliable apps:", [k for k in __apps__.keys()])
        print("avaliable libraries:", [k for k in __libraries__.keys()])
        print("avaliable compilers:", [k for k in __compilers__.keys()])
        print("avaliable OS:", [k for k in __OSs__.keys()])
        print('use option "--app=?" to choose an application to build or,')
        print('use option "--lib=?" to choose a library to build.')
        print('use option "--cpl=?" to choose a compiler to build.')
        print('use option "--os=?" to choose an OS to be used.')
        exit(-1)

    if appName in __apps__:
        __apps__[appName]().build()

    if libName in __libraries__:
        __libraries__[libName]().build()


def generate(cfgs, force=False):
    from generator import Generate

    if type(cfgs) is list:
        L = [cfg.rstr() if type(cfg) is SCons.Node.FS.File else cfg for cfg in cfgs]
    elif type(cfgs) is SCons.Node.FS.File:
        L = [cfgs.rstr()]
    elif type(cfgs) is str:
        L = [cfgs]
    else:
        L = cfgs
    return Generate(L, force)
