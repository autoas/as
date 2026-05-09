@echo off

set ASPATH=%~dp0
set astmp=%ASPATH%
set ASDISK=%astmp:~0,2%
set ASPATH=%ASPATH:~,-1%

%ASDISK%
cd %ASPATH%

REM base env PATH
set MSYS2=C:\msys64
set PATH=C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0
set PATH=C:\Anaconda3;C:\Anaconda3\Scripts;%MSYS2%\mingw64\bin;%MSYS2%\usr\bin;%MSYS2%\mingw32\bin;%MSYS2%\clang64\bin;%PATH%

set PATH=%PATH%;%ASPATH%\build\nt\GCC\one

if defined AS_DOWNLOAD_DIR (
    set ConEmu=%AS_DOWNLOAD_DIR%\ConEmu\ConEmu64.exe
) else (
    set ConEmu=%ASPATH%\download\ConEmu\ConEmu64.exe
)

if EXIST %ConEmu% goto prepareEnv
if defined AS_DOWNLOAD_DIR (
    mkdir "%AS_DOWNLOAD_DIR%"
    cd "%AS_DOWNLOAD_DIR%"
) else (
    mkdir download
    cd download
)
mkdir ConEmu
cd ConEmu
wget https://github.com/Maximus5/ConEmu/releases/download/v23.07.24/ConEmuPack.230724.7z
"C:\Program Files\7-Zip\7z.exe" x ConEmuPack.230724.7z
cd %ASPATH%

:prepareEnv
set MSYS=winsymlinks:nativestrict

start %ConEmu% -title sim-app-boot-tools ^
	-runlist -new_console:d:"%ASPATH%":t:sim ^
	^|^|^| -new_console:d:"%ASPATH%":t:app ^
	^|^|^| -new_console:d:"%ASPATH%":t:boot ^
	^|^|^| -new_console:d:"%ASPATH%/tools":t:tools

