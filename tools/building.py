# SSAS - Simple Smart Automotive Software
# Copyright (C) 2021 Parai Wang <parai@foxmail.com>

import os
import glob
import shutil
import SCons
from SCons.Script import *

RootDir = os.path.abspath(os.path.dirname(__file__) + '/..')

AddOption('--verbose',
          dest='verbose',
          action='store_true',
          default=False,
          help='print verbose information during build')

AddOption('--app',
          dest='application',
          type=str,
          default=None,
          help='to choose which application to be build')

AddOption('--lib',
          dest='library',
          type=str,
          default=None,
          help='to choose which library to be build')

AddOption('--os',
          dest='os',
          type=str,
          default=None,
          help='to choose which os to be used')

AddOption('--net',
          dest='net',
          type=str,
          default='none',
          help='to choose which net(lwip or none) to be used')

AddOption('--cpl',
          dest='compiler',
          type=str,
          default='GCC',
          help='to choose which compiler to be used')

AddOption('--release',
          dest='release',
          action='store_true',
          default=False,
          help='release ssas software by makefile')

AddOption('--cfg',
          dest='cfg',
          type=str,
          default=None,
          help='the directory path to config')

AddOption('--strip',
          dest='strip',
          action='store_true',
          default=False,
          help='strip symbols')

AddOption('--prebuilt',
          dest='prebuilt',
          action='store_true',
          default=True,
          help='using prebuilt libraries')

CWD = os.path.abspath('.')
appName = GetOption('application')
libName = GetOption('library')
cplName = GetOption('compiler')
paths = [CWD, 'build', os.name]
if cplName != None:
    paths.append(cplName)
if appName != None:
    paths.append(appName)
elif libName != None:
    paths.append(libName)

BUILD_DIR = os.path.join(*paths)
Export('BUILD_DIR')

TARGET_OS = GetOption('os')
Export('TARGET_OS')

_cfg_path = GetOption('cfg')
if _cfg_path != None:
    _cfg_path = os.path.abspath(_cfg_path)

__compilers__ = {}
__libraries__ = {}
__OSs__ = {}
__apps__ = {}
__cpppath__ = {}
__cfgs__ = {}

__default_compiler__ = 'GCC'


def aslog(*arg):
    if GetOption('verbose'):
        print(*arg)


class Win32Spawn:
    def spawn(self, sh, escape, cmd, args, env):
        # deal with the cmd build-in commands which cannot be used in
        # subprocess.Popen
        if cmd == 'del':
            for f in args[1:]:
                try:
                    os.remove(f)
                except Exception as e:
                    print('Error removing file: %s' % (e))
                    return -1
            return 0

        import subprocess

        newargs = ' '.join(args[1:])
        cmdline = cmd + " " + newargs

        # Make sure the env is constructed by strings
        _e = dict([(k, str(v)) for k, v in env.items()])

        # Windows(tm) CreateProcess does not use the env passed to it to find
        # the executables. So we have to modify our own PATH to make Popen
        # work.
        try:
            _e['PATH'] = os.environ['PATH']+';'+_e['PATH']
        except KeyError:
            pass

        try:
            proc = subprocess.Popen(cmdline, env=_e, shell=False)
        except Exception as e:
            print('Error in calling:\n%s' % (cmdline))
            print('Exception: %s: %s' % (e, os.strerror(e.errno)))
            return e.errno

        return proc.wait()


def IsBuildForWindows():
    if IsPlatformWindows():
        return GetOption('compiler') in ['GCC', 'MSVC', 'x86GCC']
    return False


def IsBuildForWin32():
    if IsPlatformWindows():
        return GetOption('compiler') in ['x86GCC']
    return False


def IsBuildForAndroid():
    return GetOption('compiler') in ['NDK']


def IsBuildForHost():
    return GetOption('compiler') in ['GCC', 'MSVC', 'NDK', 'x86GCC']


def IsPlatformWindows():
    bYes = False
    if(os.name == 'nt'):
        bYes = True
    if(sys.platform == 'msys'):
        bYes = True
    return bYes


def MKDir(p):
    ap = os.path.abspath(p)
    try:
        os.makedirs(ap)
    except:
        if(not os.path.exists(ap)):
            raise Exception('Fatal Error: can\'t create directory <%s>' % (ap))


def RMDir(p):
    if(os.path.exists(p)):
        shutil.rmtree(p)


def MKFile(p, c='', m='w'):
    f = open(p, m)
    f.write(c)
    f.close()


def RMFile(p):
    if(os.path.exists(p)):
        print('removing %s' % (os.path.abspath(p)))
        os.remove(os.path.abspath(p))


def RunCommand(cmd, e=True):
    aslog(' >> RunCommand "%s"' % (cmd))
    if(os.name == 'nt'):
        cmd = cmd.replace('&&', '&')
    ret = os.system(cmd)
    if(0 != ret and e):
        raise Exception('FAIL of RunCommand "%s" = %s' % (cmd, ret))
    return ret


def RunSysCmd(cmd):
    import subprocess
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    try:
        o = output.decode('utf-8')
    except UnicodeDecodeError:
        o = output
    return p_status, o


def Download(url, tgt=None):
    # curl is better than wget on msys2
    if(tgt == None):
        tgt = url.split('/')[-1]

    def IsProperType(f):
        tL = {'.zip': 'Zip archive data', '.tar.gz': 'gzip compressed data',
              '.tar.xz': 'XZ compressed data', '.tar.bz2': 'bzip2 compressed data'}
        if(not os.path.exists(f)):
            return False
        if(0 == os.path.getsize(f)):
            return False
        for t, v in tL.items():
            if(f.endswith(t)):
                err, info = RunSysCmd('file %s' % (tgt))
                if(v not in info):
                    return False
                break
        return True
    if(not os.path.exists(tgt)):
        print('Downloading from %s to %s' % (url, tgt))
        ret = RunCommand('curl %s -o %s' % (url, tgt), False)
        if((ret != 0) or (not IsProperType(tgt))):
            tf = url.split('/')[-1]
            RMFile(tf)
            print('temporarily saving to %s' % (os.path.abspath(tf)))
            RunCommand('wget %s' % (url))
            RunCommand('mv -v %s %s' % (tf, tgt))


def PkgGlob(pkg, objs):
    sc = '%s/SConscript' % (pkg)
    cstr = "objs = []\n"
    for obj in objs:
        cstr += "objs += Glob('%s')\n" % (obj)
    cstr += "Return('objs')"
    if not os.path.isfile(sc):
        with open(sc, 'w') as f:
            f.write(cstr)
    else:
        with open(sc) as f:
            cstr2 = f.read()
        if (cstr != cstr2):
            with open(sc, 'w') as f:
                f.write(cstr)
    return SConscript(sc, variant_dir='%s/%s' % (BUILD_DIR, os.path.basename(pkg)), duplicate=0)


def Package(url, ** parameters):
    if(type(url) == dict):
        parameters = url
        url = url['url']
    cwd = GetCurrentDir()
    bsw = os.path.basename(cwd)
    download = '%s/download' % (RootDir)
    if 'dir' in parameters:
        download = '%s/%s' % (download, parameters['dir'])
    MKDir(download)
    pkgBaseName = os.path.basename(url)
    if(pkgBaseName.endswith('.zip')):
        tgt = '%s/%s' % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-4]
        pkg = '%s/%s' % (download, pkgName)
        MKDir(pkg)
        flag = '%s/.unzip.done' % (pkg)
        if(not os.path.exists(flag)):
            try:
                RunCommand('cd %s && unzip ../%s' % (pkg, pkgBaseName))
            except Exception as e:
                print('WARNING:', e)
            MKFile(flag, 'url')
    elif(pkgBaseName.endswith('.rar')):
        tgt = '%s/%s' % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-4]
        pkg = '%s/%s' % (download, pkgName)
        MKDir(pkg)
        flag = '%s/.unrar.done' % (pkg)
        if(not os.path.exists(flag)):
            try:
                RunCommand('cd %s && unrar x ../%s' % (pkg, pkgBaseName))
            except Exception as e:
                print('WARNING:', e)
            MKFile(flag, 'url')
    elif(pkgBaseName.endswith('.tar.gz') or pkgBaseName.endswith('.tar.xz')):
        tgt = '%s/%s' % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-7]
        pkg = '%s/%s' % (download, pkgName)
        MKDir(pkg)
        flag = '%s/.unzip.done' % (pkg)
        if(not os.path.exists(flag)):
            RunCommand('cd %s && tar xf ../%s' % (pkg, pkgBaseName))
            MKFile(flag, 'url')
    elif(pkgBaseName.endswith('.tar.bz2')):
        tgt = '%s/%s' % (download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-8]
        pkg = '%s/%s' % (download, pkgName)
        MKDir(pkg)
        flag = '%s/.unzip.done' % (pkg)
        if(not os.path.exists(flag)):
            RunCommand('cd %s && tar xf ../%s' % (pkg, pkgBaseName))
            MKFile(flag, 'url')
    elif(pkgBaseName.endswith('.git')):
        pkgName = pkgBaseName[:-4]
        pkg = '%s/%s' % (download, pkgName)
        if(not os.path.exists(pkg)):
            RunCommand('cd %s && git clone %s' % (download, url))
        if('version' in parameters):
            flag = '%s/.%s.version.done' % (pkg, bsw)
            if(not os.path.exists(flag)):
                ver = parameters['version']
                RunCommand('cd %s && git checkout %s' % (pkg, ver))
                MKFile(flag, ver)
                # remove all cmd Done flags
                for cmdF in Glob('%s/.*.cmd.done' % (pkg)):
                    RMFile(str(cmdF))
    else:
        pkg = '%s/%s' % (download, url)
        if(not os.path.isdir(pkg)):
            print('ERROR: %s require %s but now it is missing! It maybe downloaded later, so please try build again.' % (bsw, url))
    # cmd is generally a series of 'sed' operatiron to do some simple modifications
    if('cmd' in parameters):
        flag = '%s/.%s.cmd.done' % (pkg, bsw)
        cmd = 'cd %s && ' % (pkg)
        cmd += parameters['cmd']
        if(not os.path.exists(flag)):
            RunCommand(cmd)
            MKFile(flag, cmd)
    if('pyfnc' in parameters):
        flag = '%s/.%s.pyfnc.done' % (pkg, bsw)
        if(not os.path.exists(flag)):
            parameters['pyfnc'](pkg)
            MKFile(flag)
    if 'patch' in parameters:
        flag = '%s/.%s.patch.done' % (pkg, bsw)
        cmd = 'cd %s && ' % (pkg)
        cmd += 'patch -p1 < %s' % (parameters['patch'])
        if(not os.path.exists(flag)):
            RunCommand(cmd)
            MKFile(flag, cmd)
    # post check
    verList = Glob('%s/.*.version.done' % (pkg))
    cmdList = Glob('%s/.*.cmd.done' % (pkg))
    if(len(verList) >= 2 or len(cmdList) >= 2):
        print('WARNING: BSW %s : 2 or more BSWs require package %s, '
              'please make sure version and cmd has no conflicts\n'
              '\t please check %s/SConscript' % (bsw, pkgBaseName, cwd))
    return pkg


class CustomEnv(dict):
    def __init__(self):
        self.env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
        self.CPPPATH = []
        self.objs = {}

    def Replace(self, **kwargs):
        pass

    def Append(self, **kwargs):
        self.env.Append(**kwargs)

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
        self.objs[src] = kwargs
        return [src]

    def SharedObject(self, src, **kwargs):
        aslog('SHCC', src)
        self.objs[src] = kwargs
        return [src]

    def Library(self, libName, objs, **kwargs):
        return ['lib%s.a'%(libName)]

    def SharedLibrary(self, libName, objs, **kwargs):
        self.shared = True
        self.Program(libName, objs, **kwargs)

    def w(self, l):
        self.mkf.write(l)

    def relpath(self, p, silent=False):
        try:
            p = os.path.relpath(p, RootDir)
            p = p.replace(os.sep, '/')
        except Exception as e:
            if silent:
                pass
            else:
                raise e
        return p

    def abspath(self, p):
        p = os.path.abspath(p)
        p = p.replace(os.sep, '/')
        return p

    def copy(self, src):
        rp = self.relpath(src)
        print('  release %s' % (rp))
        dst = os.path.join(self.WDIR, rp)
        d = os.path.dirname(dst)
        MKDir(d)
        shutil.copy(src, dst)
        d = os.path.dirname(src)
        extras = Glob('%s/*.h' % (d))
        if 'infras' in d:
            extras += Glob('%s/*/*.h' % (d))
            extras += Glob('%s/*/*.c' % (d))
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
                shutil.copytree(cpppath, os.path.join(
                    self.WDIR, rp), dirs_exist_ok=True)
            except:
                pass


class ReleaseEnv(CustomEnv):
    def __init__(self, env):
        super().__init__()
        self.env = env

    def Program(self, appName, objs, **kwargs):
        self.appName = appName
        self.cplName = getattr(self, 'compiler', GetOption('compiler'))
        self.WDIR = '%s/release/%s' % (RootDir, appName)
        MKDir(self.WDIR)
        self.GenerateMakefile(objs, **kwargs)
        self.CopyFiles(objs)
        print('release %s done!' % (appName))
        exit()

    def link_flags(self, LINKFLAGS):
        cstr = ''
        for flg in LINKFLAGS:
            if '-T' in flg:
                lds = flg.replace('-T', '').replace('"', '')
                self.copy(lds)
                cstr = '%s -T"%s"' % (cstr, self.relpath(lds))
            elif '-Map=' in flg:
                ss = flg.split('=')
                prefix = ss[0]
                mp = ss[-1].replace('"', '')
                cstr = '%s %s="%s"' % (cstr, prefix, self.relpath(mp))
            elif '.map' in flg:
                cstr = ' '.join([cstr, self.relpath(flg)])
            else:
                cstr = ' '.join([cstr, flg])
        return cstr

    def GenerateMakefile(self, objs, **kwargs):
        self.mkf = open('%s/Makefile.%s' % (self.WDIR, self.cplName), 'w')
        self.w('# Makefile for %s\n' % (self.appName))
        CC = os.path.basename(self.env['CC'])
        CXX = os.path.basename(self.env['CXX'])
        LD = os.path.basename(self.env['LINK'])
        if LD == '$SMARTLINK':
            LD = CC
        self.w('CC=%s\n' % (CC))
        self.w('LD=%s\n' % (LD))
        self.w('CXX=%s\n' % (CXX))
        self.w('ifeq ($V, 1)\n')
        self.w('Q=\n')
        self.w('else\n')
        self.w('Q=@\n')
        self.w('endif\n')
        self.w('LINKFLAGS=%s\n' %
               (self.link_flags(kwargs.get('LINKFLAGS', []))))
        self.w('LIBS=%s\n' % (' '.join('-l%s' % (i)
               for i in kwargs.get('LIBS', []))))
        self.w('LIBPATH=%s\n' % (' '.join('-L"%s"' % (i)
               for i in kwargs.get('LIBPATH', []))))
        BUILD_DIR = os.path.join(
            'build', os.name, self.cplName, self.appName).replace(os.sep, '/')
        objd = []
        objm = {}
        for src in objs:
            rp = self.relpath(str(src))
            if rp.endswith('.c') or rp.endswith('.s') or rp.endswith('.S'):
                obj = rp[:-2]
            elif rp.endswith('.cpp'):
                obj = rp[:-4]
            else:
                raise Exception('unkown file type %s' % (rp))
            obj = '%s/%s.o' % (BUILD_DIR, obj)
            d = os.path.dirname(obj)
            if d not in objd:
                objd.append(d)
            self.w('objs-y += %s\n' % (obj))
            if src in self.objs:
                args = self.objs[src]
            else:
                args = kwargs
            for i in args.get('CPPPATH', []):
                if i not in self.CPPPATH:
                    self.CPPPATH.append(i)
            objm[obj] = {'src': rp, 'd': d}
            objm[obj]['CPPFLAGS'] = ' '.join(args.get('CPPFLAGS', []))
            objm[obj]['CPPDEFINES'] = ' '.join(
                ['-D%s' % (i) for i in args.get('CPPDEFINES', [])])
            objm[obj]['CPPPATH'] = ' '.join(
                ['-I"%s"' % (self.relpath(i, silent=True)) for i in args.get('CPPPATH', [])])
        self.w('\nall: %s/%s.exe\n' % (BUILD_DIR, self.appName))
        self.w('%s/%s.exe: $(objs-y)\n' % (BUILD_DIR, self.appName))
        self.w('\t@echo LD $@\n')
        self.w('\t$Q $(LD) $(LINKFLAGS) $(objs-y) $(LIBS) $(LIBPATH) -o $@\n\n')
        if 'S19' in self.env:
            ss = self.env['S19'].split(' ')
            ss[0] = os.path.basename(ss[0])
            s19 = ' '.join(ss)
            cmd = s19.format('%s/%s.exe' % (BUILD_DIR, self.appName),
                             '%s/%s.s19' % (BUILD_DIR, self.appName))
            self.w('\t$Q %s\n\n' % (cmd))
        for d in objd:
            self.w('%s:\n\tmkdir -p $@\n' % (d))
        for obj, m in objm.items():
            self.w('%s: %s\n' % (m['src'], m['d']))
            self.w('%s: %s\n' % (obj, m['src']))
            CC = 'CC'
            if m['src'].endswith('.cpp'):
                CC = 'CXX'
            self.w('\t@echo %s $<\n' % (CC))
            self.w('\t$Q $(%s) %s %s %s -c $< -o $@\n' %
                   (CC, m['CPPFLAGS'], m['CPPDEFINES'], m['CPPPATH']))
        self.w('\nclean:\n\t@rm -fv $(objs-y) %s/%s.exe\n\n' %
               (BUILD_DIR, self.appName))
        self.mkf.close()


def register_compiler(compiler):
    if not compiler.__name__.startswith('Compiler'):
        raise Exception(
            'compiler name %s not starts with "Compiler"' % (compiler.__name__))
    name = compiler.__name__[8:]

    def create_compiler(**kwargs):
        env = compiler(**kwargs)
        if IsPlatformWindows():
            win32_spawn = Win32Spawn()
            env['SPAWN'] = win32_spawn.spawn
        if(not GetOption('verbose')):
            # override the default verbose command string
            env.Replace(
                ARCOMSTR='AR $TARGET',
                ASCOMSTR='AS $SOURCE',
                ASPPCOMSTR='AS $SOURCE',
                CCCOMSTR='CC $SOURCE',
                CXXCOMSTR='CXX $SOURCE',
                LINKCOMSTR='LINK $TARGET',
                SHCCCOMSTR='SHCC $SOURCE',
                SHCXXCOMSTR='SHCXX $SOURCE',
                SHLINKCOMSTR='SHLINK $TARGET')
        if GetOption('release'):
            env = ReleaseEnv(env)
        return env
    if name not in __compilers__:
        aslog('register compiler %s' % (name))
        __compilers__[name] = create_compiler
    else:
        raise KeyError('compiler %s already registered' % (name))


def register_driver(driver):
    if not driver.__name__.startswith('Driver'):
        raise Exception('driver name %s not starts with "Driver"' %
                        (driver.__name__))
    name = '%s:%s' % (driver.__name__[6:], driver.cls)
    if name not in __libraries__:
        aslog('register driver %s' % (name))
        __libraries__[name] = driver
    else:
        raise KeyError('driver %s already registered' % (name))


def register_library(library):
    if not library.__name__.startswith('Library'):
        raise Exception('library name %s not starts with "Library"' %
                        (library.__name__))
    name = library.__name__[7:]
    if name not in __libraries__:
        aslog('register library %s' % (name))
        __libraries__[name] = library
    else:
        raise KeyError('library %s already registered' % (name))


def register_os(library):
    register_library(library)
    name = library.__name__[7:]
    aslog('register OS %s' % (name))
    __OSs__[name] = library


def register_application(app):
    if not app.__name__.startswith('Application'):
        raise Exception(
            'application name %s not starts with "Application"' % (app.__name__))
    name = app.__name__[11:]
    if name not in __apps__:
        aslog('register app %s' % (name))
        __apps__[name] = app
    else:
        raise KeyError('app %s already registered' % (name))


def query_application(name):
    if name in __apps__:
        return __apps__[name]
    else:
        raise KeyError('app %s not found' % (name))


@register_compiler
def CompilerArmGCC(**kwargs):
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env.Append(CFLAGS=['-std=gnu99'])
    env.Append(CPPFLAGS=['-Wall'])
    if not GetOption('strip'):
        env.Append(CPPFLAGS=['-g'])
    if(IsPlatformWindows()):
        gccarm = 'https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-win32.zip'
    else:
        gccarm = 'https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-linux.tar.bz2'
    ARMGCC = os.getenv('ARMGCC')
    if ARMGCC != None:
        cpl = ARMGCC
    else:
        cpl = Package(gccarm)
        if(not IsPlatformWindows()):
            cpl += '/gcc-arm-none-eabi-5_4-2016q3'
    env['CC'] = '%s/bin/arm-none-eabi-gcc' % (cpl)
    env['CXX'] = '%s/bin/arm-none-eabi-g++' % (cpl)
    env['AS'] = '%s/bin/arm-none-eabi-gcc -c' % (cpl)
    env['LINK'] = '%s/bin/arm-none-eabi-ld' % (cpl)
    env['S19'] = '%s/bin/arm-none-eabi-objcopy -O srec --srec-forceS3 --srec-len 32 {0} {1}' % (
        cpl)
    env.Append(CPPFLAGS=['-ffunction-sections', '-fdata-sections'])
    env.Append(LINKFLAGS=['--gc-sections'])
    return env


@register_compiler
def CompilerArm64GCC(**kwargs):
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env.Append(CFLAGS=['-std=gnu99'])
    env.Append(CPPFLAGS=['-Wall', '-fno-stack-protector'])
    if not GetOption('strip'):
        env.Append(CPPFLAGS=['-g'])
    if(IsPlatformWindows()):
        gccarm64 = 'gcc-linaro-7.2.1-2017.11-i686-mingw32_aarch64-elf.tar.xz'
    else:
        gccarm64 = 'gcc-linaro-7.2.1-2017.11-x86_64_aarch64-elf.tar.xz'
    ARM64GCC = os.getenv('ARM64GCC')
    if ARM64GCC != None:
        cpl = ARM64GCC
    else:
        pkg = Package(
            'https://releases.linaro.org/components/toolchain/binaries/7.2-2017.11/aarch64-elf/%s' % (gccarm64))
        cpl = '%s/%s' % (pkg, gccarm64[:-7])
    env['CC'] = '%s/bin/aarch64-elf-gcc' % (cpl)
    env['CXX'] = '%s/bin/aarch64-elf-g++' % (cpl)
    env['AS'] = '%s/bin/aarch64-elf-gcc -c' % (cpl)
    env['LINK'] = '%s/bin/aarch64-elf-ld' % (cpl)
    env['S19'] = '%s/bin/aarch64-elf-objcopy -O srec --srec-forceS3 --srec-len 32 {0} {1}' % (
        cpl)
    env.Append(CPPFLAGS=['-ffunction-sections', '-fdata-sections'])
    env.Append(LINKFLAGS=['--gc-sections'])
    return env


@register_compiler
def CompilerArmCC(**kwargs):
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env.Append(CPPFLAGS=['--c99'])
    if not GetOption('strip'):
        env.Append(CPPFLAGS=['-g'])
    ARMCC = os.getenv('ARMCC')
    if ARMCC != None:
        cpl = ARMCC
    else:
        cpl = 'C:/Keil_v5/ARM/ARMCC'
    env['CC'] = '%s/bin/armcc' % (cpl)
    env['CXX'] = '%s/bin/armcc' % (cpl)
    env['AS'] = '%s/bin/armasm' % (cpl)
    env['LINK'] = '%s/bin/armlink' % (cpl)
    env['S19'] = '%s/bin/fromelf --m32 {0} --output {1}' % (cpl)
    env.Append(CPPFLAGS=['--split_sections'])
    env.Append(LINKFLAGS=['--strict', '--summary_stderr', '--info', 'summarysizes',
                          '--map', '--xref', '--callgraph', '--symbols', '--info', 'sizes', '--info', 'totals',
                          '--info', 'unused', '--info', 'veneers'])
    return env


@register_compiler
def CompilerCM0PGCC(**kwargs):
    env = CreateCompiler('ArmGCC')
    env.Append(CPPFLAGS=['-mthumb', '-mlong-calls', '-mcpu=cortex-m0plus'])
    env.Append(ASFLAGS=['-mthumb', '-mcpu=cortex-m0plus'])
    env.Append(LINKFLAGS=['-mthumb', '-mcpu=cortex-m0plus'])
    env['LINK'] = env['LINK'][:-2] + 'gcc'
    env['LINKFLAGS'].remove('--gc-sections')
    env.Append(LINKFLAGS=['-Wl,--gc-sections'])
    return env


def __CompilerGCC(**kwargs):
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env.Append(CFLAGS=['-std=gnu99'])
    env.Append(CPPFLAGS=['-Wall'])
    if not GetOption('strip'):
        env.Append(CPPFLAGS=['-g'])
    if IsBuildForWindows():
        env.Append(LINKFLAGS=['-static'])
    return env


@register_compiler
def CompilerGCC(**kwargs):
    return __CompilerGCC(**kwargs)


@register_compiler
def CompilerNDK(**kwargs):
    import glob
    HOME = os.getenv('HOME')
    if HOME is None:
        HOME = os.getenv('USERPROFILE')
    NDK = os.path.join(HOME, 'AppData/Local/Android/Sdk/ndk-bundle')
    if(not os.path.exists(NDK)):
        NDK = os.getenv('ANDROID_NDK')
    if(NDK is None or not os.path.exists(NDK)):
        print(
            '==> Please set environment ANDROID_NDK\n\tset ANDROID_NDK=/path/to/android-ndk')
        exit()
    if(IsPlatformWindows()):
        host = 'windows'
        NDK = NDK.replace(os.sep, '/')
    else:
        host = 'linux'
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env['ANDROID_NDK'] = NDK
    agcc = glob.glob(
        NDK + '/toolchains/aarch64-linux-android-*/prebuilt/%s-x86_64/bin/aarch64-linux-android-gcc*' % (host))
    if len(agcc) > 0:
        agcc = agcc[-1]
        sysroot = glob.glob(NDK + '/platforms/android-2*/arch-arm64')[-1]
        env['CC'] = agcc
        env['CC'] = agcc
        env['AS'] = agcc
        env['CXX'] = os.path.dirname(agcc) + '/aarch64-linux-android-g++'
        env['LINK'] = env['CXX']
        env.Append(CCFLAGS=['--sysroot', sysroot, '-fPIE', '-pie'])
        env.Append(LINKFLAGS=['--sysroot', sysroot, '-fPIE',  '-pie'])
        env.Append(CFLAGS=['-std=gnu99'])
        env.Append(CPPFLAGS=['-Wall'])
        if not GetOption('strip'):
            env.Append(CPPFLAGS=['-g'])
    else:
        GCC = NDK + '/toolchains/llvm/prebuilt/%s-x86_64' % (host)
        env['CC'] = GCC + '/bin/aarch64-linux-android28-clang'
        env['AS'] = GCC + '/bin/aarch64-linux-android28-clang'
        env['CXX'] = GCC + '/bin/aarch64-linux-android28-clang++'
        env['LINK'] = GCC + '/bin/aarch64-linux-android28-clang++'
    return env


def AddPythonDev(env):
    pyp = sys.executable
    if(IsPlatformWindows()):
        pyp = pyp.replace(os.sep, '/')[:-10]
        pylib = 'python'+sys.version[0]+sys.version[2]
        if(pylib in env.get('LIBS', [])):
            return
        pf = '%s/libs/lib%s.a' % (pyp, pylib)
        if(not os.path.exists(pf)):
            RunCommand(
                'cp {0}/libs/{1}.lib {0}/libs/lib{1}.a'.format(pyp, pylib))
        env.Append(CPPDEFINES=['_hypot=hypot'])
        env.Append(CPPPATH=['%s/include' % (pyp)])
        env.Append(LIBPATH=['%s/libs' % (pyp)])
        istr = 'set'
        pybind11 = '%s/Lib/site-packages/pybind11/include' % (pyp)
    else:
        pyp = os.sep.join(pyp.split(os.sep)[:-2])
        if(sys.version[0:3] == '2.7'):
            _, pyp = RunSysCmd('which python3')
            pyp = os.sep.join(pyp.split(os.sep)[:-2])
            _, version = RunSysCmd(
                'python3 -c "import sys; print(sys.version[0:3])"')
            pylib = 'python'+version+'m'
        else:
            pylib = 'python'+sys.version[0:3]+'m'
        if(pylib in env.get('LIBS', [])):
            return
        env.Append(CPPPATH=['%s/include/%s' % (pyp, pylib)])
        if(pyp == '/usr'):
            env.Append(LIBPATH=['%s/lib/x86_64-linux-gnu' % (pyp)])
            env.Append(CPPPATH=['%s/local/include/%s' % (pyp, pylib[:9])])
        else:
            env.Append(LIBPATH=['%s/lib' % (pyp)])
        istr = 'export'
        pybind11 = '%s/lib/%s/site-packages/pybind11/include' % (
            pyp, pylib[:9])
    env.Append(CPPPATH=[pybind11])
    aslog('%s PYTHONHOME=%s if see error " Py_Initialize: unable to load the file system codec"' % (istr, pyp))
    env.Append(LIBS=[pylib, 'pthread', 'stdc++', 'm'])


@register_compiler
def CompilerPYCC(**kwargs):
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env.Append(CFLAGS=['-std=gnu99'])
    env.Append(CPPFLAGS=['-Wall'])
    if not GetOption('strip'):
        env.Append(CPPFLAGS=['-g'])
    if IsPlatformWindows():
        env.Append(LINKFLAGS=['-static'])
    AddPythonDev(env)
    return env


if IsPlatformWindows():
    VCs = Glob(
        'C:/Program Files*/Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64')
    if len(VCs) > 0:
        VC = str(VCs[-1])

        @register_compiler
        def CompilerMSVC(**kwargs):
            env = Environment()
            env['CC'] = os.path.join(VC, 'cl.exe')
            env['AR'] = os.path.join(VC, 'lib.exe')
            env['LINK'] = os.path.join(VC, 'link.exe')
            return env

    @register_compiler
    def Compilerx86GCC(**kwargs):
        env = __CompilerGCC(**kwargs)
        MSYS2 = os.getenv('MSYS2')
        env['CC'] = '%s/mingw32/bin/gcc' % (MSYS2)
        env['CXX'] = '%s/mingw32/bin/g++' % (MSYS2)
        env['AR'] = '%s/mingw32/bin/ar' % (MSYS2)
        env['LINK'] = '%s/mingw32/bin/gcc' % (MSYS2)
        return env


class QMakeEnv(CustomEnv):
    def __init__(self):
        super().__init__()
        if IsPlatformWindows():
            if os.path.isdir('C:/Qt'):
                QTDIR = 'C:/Qt'
            else:
                QTDIR = os.getenv('QT_DIR')
            exe = '.exe'
        else:
            QTDIR = os.getenv('QT_DIR')
            exe = ''
        if not os.path.isdir(QTDIR):
            raise Exception('QT not found, set QT_DIR=/path/to/Qt')
        self.qmake = Glob('%s/*/*/bin/qmake%s' % (QTDIR, exe))[0].rstr()

    def Program(self, appName, objs, **kwargs):
        CPPPATH = []
        LIBPATH = []
        CPPDEFINES = []
        LIBS = []
        self.getKL(kwargs, 'LIBPATH', LIBPATH)
        self.getKL(kwargs, 'LIBS', LIBS)
        for _, kwargs in self.objs.items():
            self.getKL(kwargs, 'CPPPATH', CPPPATH)
            self.getKL(kwargs, 'LIBPATH', LIBPATH)
            self.getKL(kwargs, 'CPPDEFINES', CPPDEFINES)
        qpro = open('%s/%s.pro' % (BUILD_DIR, appName), 'w')
        if getattr(self, 'shared', False):
            qpro.write('TEMPLATE = lib\n')
        qpro.write('QT += core gui widgets\n')
        qpro.write('CONFIG += c++11\n')
        qpro.write('CONFIG += object_parallel_to_source\n')
        qpro.write('DEFINES -= UNICODE _UNICODE\n')
        qpro.write('INCLUDEPATH += %s\n\n' %
                   ('    '.join(['"%s" \\\n' % (self.abspath(p)) for p in CPPPATH])))
        qpro.write('DEFINES += %s\n\n' % (' '.join(CPPDEFINES)))
        cppstr = 'SOURCES += \\\n'
        hppstr = 'HEADERS += \\\n'
        for obj in objs:
            rp = self.abspath(obj.rstr())
            if rp.endswith('.hpp'):
                hppstr += '    %s \\\n' % (rp)
            else:
                cppstr += '    %s \\\n' % (rp)
        qpro.write(cppstr + '\n')
        qpro.write(hppstr + '\n')
        qpro.write('LIBS += %s\n\n' % (' '.join(['-l%s' % (l) for l in LIBS])))
        qpro.write('LIBPATH += %s\n\n' %
                   ('    '.join(['"%s" \\\n' % (self.abspath(p)) for p in LIBPATH])))
        qpro.close()
        cmd = '%s %s/%s.pro -spec win32-g++ "CONFIG+=debug"' % (
            self.qmake, BUILD_DIR, appName)
        RunCommand(cmd)
        cmd = 'cd %s && make' % (BUILD_DIR)
        RunCommand(cmd)


@ register_compiler
def CompilerQMake(**kwargs):
    return QMakeEnv()


def CreateCompiler(name, **kwargs):
    if name in __compilers__:
        return __compilers__[name](**kwargs)
    else:
        raise KeyError('compiler %s not found, available compilers: %s' % (
            name, [k for k in __compilers__.keys()]))


def GetCurrentDir():
    conscript = File('SConscript')
    fn = conscript.rfile()
    path = os.path.dirname(fn.abspath)
    return path


def RegisterCPPPATH(name, path, force=False):
    if not name.startswith('$'):
        raise Exception('CPPPATH name %s not starts with "$"' % (name))
    if name not in __cpppath__ or force == True:
        __cpppath__[name] = path
    else:
        raise KeyError('CPPPATH %s already registered' % (name))


def RequireCPPPATH(name):
    if not name.startswith('$'):
        raise Exception('CPPPATH name %s not starts with "$"' % (name))
    if name in __cpppath__:
        return __cpppath__[name]
    else:
        raise KeyError('CPPPATH %s not found, available CPPPATH: %s' %
                       (name, [k for k in __cpppath__.keys()]))


def RegisterConfig(name, source, force=False):
    if name not in __cfgs__ or force == True:
        if (len(source) == 0):
            raise Exception('No config provided for %s' % (name))
        path = os.path.dirname(str(source[0]))
        __cfgs__[name] = (path, source)
    else:
        raise KeyError('CFG %s already registered' % (name))


def RequireConfig(name):
    if name in __cfgs__:
        return __cfgs__[name]
    else:
        raise KeyError('CFG %s not found, available CPPPATH: %s' %
                       (name, [k for k in __cfgs__.keys()]))


def RequireLibrary(name):
    if name in __libraries__:
        return __libraries__[name]
    else:
        raise KeyError('CPPPATH %s not found, available CPPPATH: %s' %
                       (name, [k for k in __libraries__.keys()]))


class BuildBase():
    def __init__(self):
        self.__libs__ = {}
        self.__libs_order__ = []
        self.__extra_libs__ = []
        for name in getattr(self, 'LIBS', []):
            self.ensure_lib(name)

    def init_base(self):
        self.CPPPATH = []
        self.LIBPATH = []
        self.include = []
        self.LIBS = []
        self.CPPDEFINES = []


    def AddPostAction(self, action):
        if not hasattr(self, '__post_actions__'):
            self.__post_actions__ = []
        self.__post_actions__.append(action)

    def Append(self, **kwargs):
        env = self.ensure_env()
        env.Append(**kwargs)

    def Remove(self, **kwargs):
        env = self.ensure_env()
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

    def ensure_env(self):
        if getattr(self, 'user', None):
            return self.user.ensure_env()
        if self.env is None:
            cplName = getattr(self, 'compiler', GetOption('compiler'))
            if cplName not in __compilers__:
                print('avaliable compilers:', [
                      k for k in __compilers__.keys()])
                print('use option "--cpl=?" to choose a compiler to build.')
                exit(-1)
            self.env = CreateCompiler(cplName)
        return self.env

    def RegisterCPPPATH(self, name, path, force=False):
        if not name.startswith('$'):
            raise Exception('CPPPATH name %s not starts with "$"' % (name))
        if getattr(self, 'user', None):
            if getattr(self.user, 'user', None):
                self.user.RegisterCPPPATH(name, path, force)
            else:
                if name not in self.user.__cpppath__ or force == True:
                    self.user.__cpppath__[name] = path
                else:
                    raise KeyError('CPPPATH %s already registered for %s' %
                                   (name, self.user.__class__.__name__))
        else:
            RegisterCPPPATH(name, path, force)

    def RequireCPPPATH(self, name):
        if not name.startswith('$'):
            raise Exception('CPPPATH name %s not starts with "$"' % (name))
        if getattr(self, 'user', None):
            if getattr(self.user, 'user', None):
                self.user.RequireCPPPATH(name)
            else:
                if name in self.user.__cpppath__:
                    return self.user.__cpppath__[name]
        elif hasattr(self, '__cpppath__'):
            if name in self.__cpppath__:
                return self.__cpppath__[name]
        return RequireCPPPATH(name)

    def RegisterConfig(self, name, source, force=False):
        if getattr(self, 'user', None):
            if name not in self.user.__cfgs__ or force == True:
                if (len(source) == 0):
                    if GetOption('prebuilt'):
                        return
                    raise Exception('No config provided for %s' % (name))
                path = os.path.dirname(str(source[0]))
                self.user.__cfgs__[name] = (path, source)
            else:
                raise KeyError('CFG %s already registered for %s' %
                               (name, self.user.__class__.__name__))
        else:
            RegisterConfig(name, source, force)
        # register a special config path for the module
        path = os.path.dirname(str(source[0]))
        self.RegisterCPPPATH('$%s_Cfg' % (name), path, force)

    def RequireConfig(self, name):
        if getattr(self, 'user', None):
            if name in self.user.__cfgs__:
                return self.user.__cfgs__[name]
        elif hasattr(self, '__cfgs__'):
            if name in self.__cfgs__:
                return self.__cfgs__[name]
        return RequireConfig(name)

    def is_shared_library(self):
        if getattr(self, 'shared', False):
            return True
        if getattr(self, 'user', None):
            return self.user.is_shared_library()
        return False

    def ensure_lib(self, name):
        if getattr(self, 'user', None) and not getattr(self, 'shared', False):
            self.user.ensure_lib(name)
        else:
            if (name in __libraries__):
                if name not in self.__libs__:
                    self.__libs_order__.append(name)
                    self.__libs__[name] = __libraries__[name](
                        user=self, env=self.env, compiler=getattr(self, 'compiler', None))
            else:
                if name not in self.__extra_libs__:
                    self.__extra_libs__.append(name)

    def get_libs(self):
        libs = {}
        if getattr(self, 'user', None) and not getattr(self, 'shared', False):
            libs = self.user.get_libs()
        else:
            libs = self.__libs__
        return libs

    def get_includes(self, searched_libs):
        CPPPATH = []
        if getattr(self, 'include', None):
            if type(self.include) == str:
                CPPPATH.append(self.include)
            else:
                CPPPATH.extend(self.include)
        elif getattr(self, 'INCLUDE', None):
            if type(self.INCLUDE) == str:
                CPPPATH.append(self.INCLUDE)
            else:
                CPPPATH.extend(self.INCLUDE)

        libs = self.get_libs()
        for libName in getattr(self, 'LIBS', []):
            if (libName in libs and libName not in searched_libs):
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


class Library(BuildBase):
    # for a library, it provides header files(*.h) and the library(*.a)
    def __init__(self, **kwargs):
        if not hasattr(self, 'name'):
            self.name = self.__class__.__name__[7:]
        aslog('init library %s' % (self. name))
        # local cpp_path for libraries
        self.__cpppath__ = {}
        self.__cfgs__ = {}
        self.user = kwargs.get('user', None)
        self.env = kwargs.get('env', None)
        compiler = kwargs.get('compiler', getattr(self, 'compiler', None))
        if compiler != None:
            self.compiler = compiler
        if False == getattr(self, 'typed_class', False):
            self.init_base()
        self.config()
        if GetOption('prebuilt') and GetOption('library') != self.name:
            liba = os.path.abspath('%s/../prebuilt/lib%s.a' %
                                   (BUILD_DIR, self.name))
            if os.path.isfile(liba):
                self.source = [liba]
        if hasattr(self, 'include') and len(self.include):
            self.RegisterCPPPATH('$%s' % (self.name), self.include)
        super().__init__()

    def get_opt_cfg(self, CPPPATH, libName):
        cfg_path = _cfg_path
        if (cfg_path != None):
            if os.path.isfile(cfg_path):
                CPPPATH.append(os.path.dirname(cfg_path))
            elif os.path.isdir(cfg_path):
                source = Glob('%s/%s_Cfg.c' % (cfg_path, libName))
                if (len(source) > 0):
                    CPPPATH.append(cfg_path)

    def objs(self):
        libName = self.name
        aslog('build objs of %s' % (libName))
        env = self.ensure_env()
        CPPPATH = getattr(self, 'CPPPATH', [])
        CPPDEFINES = getattr(self, 'CPPDEFINES', []) + \
            env.get('CPPDEFINES', [])
        CFLAGS = env.get('CFLAGS', [])
        CPPFLAGS = getattr(self, 'CPPFLAGS', []) + env.get('CPPFLAGS', [])
        CPPPATH = [self.RequireCPPPATH(p) if p.startswith('$') else p
                   for p in CPPPATH] + env.get('CPPPATH', [])
        searched_libs = []
        CPPPATH += self.get_includes(searched_libs)
        try:
            # others has provide the config for this library
            cfg_path, source = self.RequireConfig(libName)
            CPPPATH.append(cfg_path)
            for src in source[1:]:
                cfg_path = os.path.dirname(str(src))
                CPPPATH.append(cfg_path)
            self.source += source
        except KeyError:
            self.get_opt_cfg(CPPPATH, libName)

        for name in getattr(self, 'LIBS', []):
            if (name in __libraries__):
                try:
                    libInclude = self.RequireCPPPATH('$%s' % (name))
                    CPPPATH.append(libInclude)
                except KeyError:
                    pass
        objs = []
        for c in self.source:
            if str(c).endswith('.a'):
                objs.append(c)
            elif self.is_shared_library():
                objs += env.SharedObject(c, CPPPATH=CPPPATH,
                                         CPPDEFINES=CPPDEFINES, CPPFLAGS=CPPFLAGS, CFLAGS=CFLAGS)
            else:
                objs += env.Object(c, CPPPATH=CPPPATH, CPPDEFINES=CPPDEFINES,
                                   CPPFLAGS=CPPFLAGS, CFLAGS=CFLAGS)
        return objs

    def build(self):
        libName = self.name
        if ':' in libName:
            libName = libName.split(':')[0]
        env = self.ensure_env()
        objs = self.objs()
        LIBPATH = env.get('LIBPATH', []) + getattr(self, 'LIBPATH', [])
        for libName_, lib in self.__libs__.items():
            objs += lib.objs()
            LIBPATH += getattr(lib, 'LIBPATH', [])
        if self.is_shared_library():
            LIBS = []
            LINKFLAGS = getattr(self, 'LINKFLAGS', []) + \
                env.get('LINKFLAGS', [])
            objs2 = []
            for obj in objs:
                if not str(obj).endswith('.a'):
                    objs2.append(obj)
                else:
                    name = os.path.basename(obj)[3:-2]
                    LIBPATH.append(os.path.dirname(obj))
                    LIBS.append(name)
            LIBS += env.get('LIBS', []) + self.__extra_libs__
            return env.SharedLibrary(libName, objs2, LIBPATH=LIBPATH, LIBS=LIBS, LINKFLAGS=LINKFLAGS)
        return env.Library(libName, objs)


class Driver(Library):
    def __init__(self, **kwargs):
        self.name = '%s:%s' % (self.__class__.__name__[6:], self.cls)
        super().__init__(**kwargs)


class Application(BuildBase):
    def __init__(self, **kwargs):
        self.name = self.__class__.__name__[11:]
        aslog('init application %s' % (self. name))
        # local cpp_path for libraries
        self.__cpppath__ = {}
        self.__cfgs__ = {}
        self.env = None
        self.init_base()
        self.config()
        if TARGET_OS != None:
            if TARGET_OS not in __OSs__:
                raise Exception('Invalid OS %s' % (TARGET_OS))
            self.LIBS += [TARGET_OS, 'OSAL']
            self.Append(CPPDEFINES=['USE_%s' % (TARGET_OS.upper()), 'USE_OSAL'])
        super().__init__()

    def build(self):
        env = self.ensure_env()
        appName = self.name
        aslog('build application %s' % (appName))
        LIBS = env.get('LIBS', [])
        CPPDEFINES = getattr(self, 'CPPDEFINES', []) + \
            env.get('CPPDEFINES', [])
        CPPFLAGS = getattr(self, 'CPPFLAGS', []) + env.get('CPPFLAGS', [])
        LINKFLAGS = getattr(self, 'LINKFLAGS', []) + env.get('LINKFLAGS', [])
        CPPPATH = [self.RequireCPPPATH(p) if p.startswith('$') else p
                   for p in getattr(self, 'CPPPATH', [])]
        CPPPATH += env.get('CPPPATH', [])
        objs = self.source
        LIBPATH = env.get('LIBPATH', []) + getattr(self, 'LIBPATH', [])
        searched_libs = []
        CPPPATH += self.get_includes(searched_libs)
        for name in self.__libs_order__:
            if self.__libs__[name].is_shared_library():
                aslog('build shared library %s' % (name))
                self.__libs__[name].build()
                continue
            aslog('build library %s' % (name))
            objs_ = self.__libs__[name].objs()
            tooMuch = True if (len(objs) + len(objs_)) > 200 else False
            libObjs = []
            for obj in objs_:
                if not str(obj).endswith('.a'):
                    if not tooMuch:
                        objs.append(obj)
                        if '%s_Cfg' % (name) not in str(obj):
                            libObjs.append(obj)
                    else:
                        libObjs.append(obj)
                else:
                    libName = os.path.basename(str(obj))[3:-2]
                    p = os.path.dirname(str(obj))
                    if p not in LIBPATH:
                        LIBPATH.append(p)
                    if (libName in ['Dcm', 'Com']):
                        LIBS.insert(0, name)
                    else:
                        LIBS.append(libName)
            if (len(libObjs) > 0):
                if ':' in name:
                    libName = name.split(':')[0]
                else:
                    libName = name
                lib = env.Library(libName, libObjs)[0]
                if tooMuch:
                    p = os.path.dirname(lib.get_abspath())
                    if p not in LIBPATH:
                        LIBPATH.append(p)
                    LIBS.append(name)
            LIBPATH += getattr(self.__libs__[name], 'LIBPATH', [])
        LIBS += self.__extra_libs__
        target = env.Program(appName, objs, CPPPATH=CPPPATH,
                             CPPDEFINES=CPPDEFINES, CPPFLAGS=CPPFLAGS, LIBS=LIBS,
                             LINKFLAGS=LINKFLAGS, LIBPATH=LIBPATH)
        if 'S19' in env:
            BUILD_DIR = os.path.dirname(target[0].get_abspath())
            action = env['S19'].format(
                target[0].get_abspath(), '%s/%s.s19' % (BUILD_DIR, appName))
            env.AddPostAction(target, action)
        for action in getattr(self, '__post_actions__', []):
            env.AddPostAction(target, action)


class Qemu():
    def __init__(self, arch='arm64'):
        arch_map = {'x86': 'i386', 'cortex-m': 'arm', 'arm64': 'aarch64'}
        self.arch = arch
        self.port = self.FindPort()
        self.portCAN0 = self.FindPort(self.port+1)
        self.params = '-serial tcp:127.0.0.1:%s,server -serial tcp:127.0.0.1:%s,server' % (
            self.port, self.portCAN0)
        if('gdb' in COMMAND_LINE_TARGETS):
            self.params += ' -gdb tcp::1234 -S'
        if(self.arch in arch_map.keys()):
            self.arch = arch_map[self.arch]
        self.qemu = self.FindQemu()

    def FindPort(self, port=1103):
        import socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        while(port < 2000):
            try:
                sock.bind(("127.0.0.1", port))
                break
            except:
                port += 1
        sock.close()
        return port

    def FindQemu(self):
        qemu = 'qemu-system-%s' % (self.arch)
        if(IsPlatformWindows()):
            qemu += '.exe'
        return qemu

    def Run(self, params):
        import threading
        self.is_running = True
        t1 = threading.Thread(target=self.thread_stdio, args=())
        t2 = threading.Thread(target=self.thread_can0, args=())
        t1.start()
        t2.start()
        cmd = '%s %s %s' % (self.qemu, params, self.params)
        RunCommand(cmd)
        self.is_running = False
        t1.join()
        t2.join()
        exit(0)

    def thread_stdio(self):
        import socket
        import time
        import keyboard
        import threading
        time.sleep(3)
        self.com = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.com.connect(('127.0.0.1', self.port))
        self.com.settimeout(0.001)
        print('QEMU: UART terminal online')

        def thread_key():
            while(self.is_running):
                p = keyboard.read_key()
                self.com.send(p.encode('utf-8'))
                time.sleep(1)
        t1 = threading.Thread(target=thread_key, args=())
        t1.start()
        while(self.is_running):
            try:
                d = self.com.recv(4096)
                if (len(d)):
                    print(d.decode('utf-8'), end='')
            except socket.timeout:
                pass
        t1.join()

    def thread_can0(self):
        import socket
        import time
        time.sleep(3)
        self.canbus = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.canbus.connect(('127.0.0.1', 8000))
        self.canbus.settimeout(0.001)
        self.can0 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.can0.connect(('127.0.0.1', self.portCAN0))
        self.can0.settimeout(0.001)
        while(self.is_running):
            try:
                frame = self.canbus.recv(69)
                while((len(frame) < 69) and (len(frame) > 0)):
                    frame += self.canbus.recv(69-len(frame))
                if (len(frame) == 69):
                    self.can0.send(frame)
            except socket.timeout:
                pass
            try:
                frame = self.can0.recv(69)
                while((len(frame) < 69) and (len(frame) > 0)):
                    frame += self.can0.recv(69-len(frame))
                if (len(frame) == 69):
                    self.canbus.send(frame)
            except socket.timeout:
                pass


def Building():
    appName = GetOption('application')
    libName = GetOption('library')
    if appName not in __apps__ and libName not in __libraries__:
        print('avaliable apps:', [k for k in __apps__.keys()])
        print('avaliable libraries:', [k for k in __libraries__.keys()])
        print('avaliable compilers:', [k for k in __compilers__.keys()])
        print('avaliable OS:', [k for k in __OSs__.keys()])
        print('use option "--app=?" to choose an application to build or,')
        print('use option "--lib=?" to choose a library to build.')
        print('use option "--cpl=?" to choose a compiler to build.')
        print('use option "--os=?" to choose an OS to be used.')
        exit(-1)

    if appName in __apps__:
        __apps__[appName]().build()

    if libName in __libraries__:
        __libraries__[libName]().build()


def generate(cfgs):
    from generator import Generate
    if type(cfgs) is list:
        L = [cfg.rstr() if type(cfg) is SCons.Node.FS.File else cfg for cfg in cfgs]
    elif type(cfgs) is SCons.Node.FS.File:
        L = [cfgs.rstr()]
    elif type(cfgs) is str:
        L = [cfgs]
    else:
        L = cfgs
    Generate(L)
