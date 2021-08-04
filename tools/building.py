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

_cfg_path = GetOption('cfg')
if _cfg_path != None:
    _cfg_path = os.path.abspath(_cfg_path)

__compilers__ = {}
__libraries__ = {}
__apps__ = {}
__cpppath__ = {}
__cfgs__ = {}

__default_compiler__ = 'GCC'


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
    if(GetOption('verbose')):
        print(' >> RunCommand "%s"' % (cmd))
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
    return p_status, output.decode('utf-8')


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


def Package(url, ** parameters):
    if(type(url) == dict):
        parameters = url
        url = url['url']
    cwd = GetCurrentDir()
    bsw = os.path.basename(cwd)
    download = '%s/download' % (RootDir)
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
    # post check
    verList = Glob('%s/.*.version.done' % (pkg))
    cmdList = Glob('%s/.*.cmd.done' % (pkg))
    if(len(verList) >= 2 or len(cmdList) >= 2):
        print('WARNING: BSW %s : 2 or more BSWs require package %s, '
              'please make sure version and cmd has no conflicts\n'
              '\t please check %s/SConscript' % (bsw, pkgBaseName, cwd))
    return pkg


class ReleaseEnv():
    def __init__(self, env):
        self.env = env
        self.CPPPATH = []
        self.objs = {}

    def Append(self, **kwargs):
        self.env.Append(**kwargs)

    def get(self, key, default=None):
        return self.env.get(key, default)

    def Object(self, src, **kwargs):
        self.objs[src] = kwargs
        return src

    def w(self, l):
        self.mkf.write(l)

    def Program(self, appName, objs, **kwargs):
        self.appName = appName
        self.cplName = getattr(self, 'compiler', GetOption('compiler'))
        self.WDIR = '%s/release/%s' % (RootDir, appName)
        MKDir(self.WDIR)
        self.GenerateMakefile(objs, **kwargs)
        self.CopyFiles(objs)
        print('release %s done!' % (appName))
        exit()

    def relpath(self, p):
        p = os.path.relpath(p, RootDir)
        p = p.replace(os.sep, '/')
        return p

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
        self.w('CC=%s\n' % (os.path.basename(self.env['CC'])))
        self.w('LD=%s\n' % (os.path.basename(self.env['LINK'])))
        self.w('ifeq ($V, 1)\n')
        self.w('Q=\n')
        self.w('else\n')
        self.w('Q=@\n')
        self.w('endif\n')
        self.w('LINKFLAGS=%s\n' %
               (self.link_flags(kwargs.get('LINKFLAGS', []))))
        self.w('LIBS=%s\n' % (' '.join('-l%s' % (i)
               for i in kwargs.get('LIBS', []))))
        BUILD_DIR = os.path.join(
            'build', os.name, self.cplName, self.appName).replace(os.sep, '/')
        objd = []
        objm = {}
        for src in objs:
            rp = self.relpath(str(src))
            if rp.endswith('.c') or rp.endswith('.s') or rp.endswith('.S'):
                obj = rp[:-2]
            else:
                raise
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
                ['-I%s' % (self.relpath(i)) for i in args.get('CPPPATH', [])])
        self.w('\nall: %s/%s.exe\n' % (BUILD_DIR, self.appName))
        self.w('%s/%s.exe: $(objs-y)\n' % (BUILD_DIR, self.appName))
        self.w('\t@echo LD $@\n')
        self.w('\t$Q $(LD) $(LINKFLAGS) $(objs-y) $(LIBS) -o $@\n\n')
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
            self.w('\t@echo CC $<\n')
            self.w('\t$Q $(CC) %s %s %s -c $< -o $@\n' %
                   (m['CPPFLAGS'], m['CPPDEFINES'], m['CPPPATH']))
        self.w('\nclean:\n\t@rm -fv $(objs-y) %s/%s.exe\n\n' %
               (BUILD_DIR, self.appName))
        self.mkf.close()

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
            rp = self.relpath(cpppath)
            shutil.copytree(cpppath, os.path.join(
                self.WDIR, rp), dirs_exist_ok=True)


def register_compiler(compiler):
    if not compiler.__name__.startswith('Compiler'):
        raise Exception(
            'compiler name %s not starts with "Compiler"' % (compiler.__name__))
    name = compiler.__name__[8:]

    def create_compiler(**kwargs):
        env = compiler(**kwargs)
        if isinstance(env, ReleaseEnv):
            env = env.env
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
        #print('register compiler %s'%(name))
        __compilers__[name] = create_compiler
    else:
        raise KeyError('compiler %s already registered' % (name))


def register_driver(driver):
    if not driver.__name__.startswith('Driver'):
        raise Exception('driver name %s not starts with "Driver"' %
                        (driver.__name__))
    name = '%s:%s' % (driver.__name__[6:], driver.cls)
    if name not in __libraries__:
        #print('register driver %s'%(name))
        __libraries__[name] = driver
    else:
        raise KeyError('driver %s already registered' % (name))


def register_library(library):
    if not library.__name__.startswith('Library'):
        raise Exception('library name %s not starts with "Library"' %
                        (library.__name__))
    name = library.__name__[7:]
    if name not in __libraries__:
        #print('register library %s'%(name))
        __libraries__[name] = library
    else:
        raise KeyError('library %s already registered' % (name))


def register_application(app):
    if not app.__name__.startswith('Application'):
        raise Exception(
            'application name %s not starts with "Application"' % (app.__name__))
    name = app.__name__[11:]
    if name not in __apps__:
        #print('register app %s'%(name))
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
    env.Append(CPPFLAGS=['-Wall', '-std=gnu99'])
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
    env['LINK'] = '%s/bin/arm-none-eabi-gcc' % (cpl)
    env['S19'] = '%s/bin/arm-none-eabi-objcopy -O srec --srec-forceS3 --srec-len 32 {0} {1}' % (
        cpl)
    env.Append(CPPFLAGS=['-ffunction-sections', '-fdata-sections'])
    env.Append(LINKFLAGS=['-Wl,--gc-sections'])
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
    return env


@register_compiler
def CompilerGCC(**kwargs):
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env.Append(CPPFLAGS=['-Wall', '-std=gnu99'])
    if not GetOption('strip'):
        env.Append(CPPFLAGS=['-g'])
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
    #print('%s PYTHONHOME=%s if see error " Py_Initialize: unable to load the file system codec"'%(istr, pyp))
    env.Append(LIBS=[pylib, 'pthread', 'stdc++', 'm'])


@register_compiler
def CompilerPYCC(**kwargs):
    env = Environment(TOOLS=['ar', 'as', 'gcc', 'g++', 'gnulink'])
    env.Append(CPPFLAGS=['-Wall', '-std=gnu99'])
    if not GetOption('strip'):
        env.Append(CPPFLAGS=['-g'])
    AddPythonDev(env)
    return env


if IsPlatformWindows():
    VCs = Glob(
        'C:/Program Files*/Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64')
    if len(VCs) >= 0:
        VC = str(VCs[-1])

        @register_compiler
        def CompilerMSVC(**kwargs):
            env = Environment()
            env['CC'] = os.path.join(VC, 'cl.exe')
            env['AR'] = os.path.join(VC, 'lib.exe')
            env['LINK'] = os.path.join(VC, 'link.exe')
            return env


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


def RegisterCPPPATH(name, path):
    if not name.startswith('$'):
        raise Exception('CPPPATH name %s not starts with "$"' % (name))
    if name not in __cpppath__:
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

    def Append(self, **kwargs):
        env = self.ensure_env()
        env.Append(**kwargs)

    def Remove(self, **kwargs):
        env = self.ensure_env()
        for key, v in kwargs.items():
            if key in env:
                if type(v) is str:
                    env[key].remove(v)
                else:
                    for vv in v:
                        env[key].remove(vv)

    def ensure_env(self):
        if self.env is None:
            cplName = getattr(self, 'compiler', GetOption('compiler'))
            if cplName not in __compilers__:
                print('avaliable compilers:', [
                      k for k in __compilers__.keys()])
                print('use option "--cpl=?" to choose a compiler to build.')
                exit(-1)
            self.env = CreateCompiler(cplName)
        return self.env

    def RegisterCPPPATH(self, name, path):
        if not name.startswith('$'):
            raise Exception('CPPPATH name %s not starts with "$"' % (name))
        if hasattr(self, 'user') and self.user:
            if name not in self.user.__cpppath__:
                self.user.__cpppath__[name] = path
            else:
                raise KeyError('CPPPATH %s already registered for %s' %
                               (name, self.user.__class__.__name__))
        else:
            RegisterCPPPATH(name, path)

    def RequireCPPPATH(self, name):
        if not name.startswith('$'):
            raise Exception('CPPPATH name %s not starts with "$"' % (name))
        if hasattr(self, 'user') and self.user:
            if name in self.user.__cpppath__:
                return self.user.__cpppath__[name]
        elif hasattr(self, '__cpppath__'):
            if name in self.__cpppath__:
                return self.__cpppath__[name]
        return RequireCPPPATH(name)

    def RegisterConfig(self, name, source, force=False):
        if hasattr(self, 'user') and self.user:
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

    def RequireConfig(self, name):
        if hasattr(self, 'user') and self.user:
            if name in self.user.__cfgs__:
                return self.user.__cfgs__[name]
        elif hasattr(self, '__cfgs__'):
            if name in self.__cfgs__:
                return self.__cfgs__[name]
        return RequireConfig(name)

    def is_shared_library(self):
        if hasattr(self, 'user') and self.user:
            return self.user.is_shared_library()
        return getattr(self, 'shared', False)

    def ensure_lib(self, name):
        if hasattr(self, 'user') and self.user:
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


class Library(BuildBase):
    # for a library, it provides header files(*.h) and the library(*.a)
    def __init__(self, **kwargs):
        if not hasattr(self, 'name'):
            self.name = self.__class__.__name__[7:]
        # local cpp_path for libraries
        self.__cpppath__ = {}
        self.__cfgs__ = {}
        self.user = kwargs.get('user', None)
        self.env = kwargs.get('env', None)
        compiler = kwargs.get('compiler', getattr(self, 'compiler', None))
        if compiler != None:
            self.compiler = compiler
        self.include = None
        self.config()
        if GetOption('prebuilt') and GetOption('library') != self.name:
            liba = os.path.abspath('%s/../%s/lib%s.a' %
                                   (BUILD_DIR, self.name, self.name))
            if os.path.isfile(liba):
                self.source = [liba]
        if self.include != None:
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
        env = self.ensure_env()
        CPPPATH = getattr(self, 'CPPPATH', [])
        CPPDEFINES = getattr(self, 'CPPDEFINES', []) + \
            env.get('CPPDEFINES', [])
        CPPFLAGS = getattr(self, 'CPPFLAGS', []) + env.get('CPPFLAGS', [])
        CPPPATH = [self.RequireCPPPATH(p) if p.startswith(
            '$') else p for p in CPPPATH] + env.get('CPPPATH', [])
        if self.include != None:
            CPPPATH.append(self.include)
        try:
            # others has provide the config for this library
            cfg_path, source = self.RequireConfig(libName)
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
        if self.is_shared_library():
            return [env.SharedObject(c, CPPPATH=CPPPATH, CPPDEFINES=CPPDEFINES, CPPFLAGS=CPPFLAGS) for c in self.source]
        return [c if str(c).endswith('.a') else env.Object(c, CPPPATH=CPPPATH, CPPDEFINES=CPPDEFINES, CPPFLAGS=CPPFLAGS) for c in self.source]

    def build(self):
        libName = self.name
        if ':' in libName:
            libName = libName.split(':')[0]
        env = self.ensure_env()
        objs = self.objs()
        LIBPATH = env.get('LIBPATH', []) + getattr(self, 'LIBPATH', [])
        for _, lib in self.__libs__.items():
            objs += lib.objs()
            LIBPATH += getattr(lib, 'LIBPATH', [])
        if self.is_shared_library():
            LIBS = env.get('LIBS', []) + self.__extra_libs__
            LINKFLAGS = getattr(self, 'LINKFLAGS', []) + \
                env.get('LINKFLAGS', [])
        return env.Library(libName, [obj for obj in objs if isinstance(obj, SCons.Node.NodeList)])


class Driver(Library):
    def __init__(self, **kwargs):
        self.name = '%s:%s' % (self.__class__.__name__[6:], self.cls)
        super().__init__(**kwargs)


class Application(BuildBase):
    def __init__(self, **kwargs):
        self.name = self.__class__.__name__[11:]
        # local cpp_path for libraries
        self.__cpppath__ = {}
        self.__cfgs__ = {}
        self.env = None
        self.config()
        super().__init__()

    def build(self):
        env = self.ensure_env()
        appName = self.name
        LIBS = []
        CPPDEFINES = getattr(self, 'CPPDEFINES', []) + \
            env.get('CPPDEFINES', [])
        CPPFLAGS = getattr(self, 'CPPFLAGS', []) + env.get('CPPFLAGS', [])
        LINKFLAGS = getattr(self, 'LINKFLAGS', []) + env.get('LINKFLAGS', [])
        CPPPATH = [self.RequireCPPPATH(p) if p.startswith(
            '$') else p for p in getattr(self, 'CPPPATH', [])]
        CPPPATH += env.get('CPPPATH', [])
        objs = self.source
        LIBPATH = env.get('LIBPATH', []) + getattr(self, 'LIBPATH', [])
        for name in self.__libs_order__:
            for obj in self.__libs__[name].objs():
                if not str(obj).endswith('.a'):
                    objs.append(obj)
                else:
                    libName = os.path.basename(obj)[3:-2]
                    LIBPATH.append(os.path.dirname(obj))
                    LIBS.append(libName)
            try:
                libInclude = self.RequireCPPPATH('$%s' % (name))
                CPPPATH.append(libInclude)
            except KeyError:
                pass
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


def Building():
    appName = GetOption('application')
    libName = GetOption('library')
    if appName not in __apps__ and libName not in __libraries__:
        print('avaliable apps:', [k for k in __apps__.keys()])
        print('avaliable libraries:', [k for k in __libraries__.keys()])
        print('avaliable compilers:', [k for k in __compilers__.keys()])
        print('use option "--app=?" to choose an application to build or,')
        print('use option "--lib=?" to choose a library to build.')
        print('use option "--cpl=?" to choose a compiler to build.')
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
