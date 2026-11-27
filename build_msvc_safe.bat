@echo off
setlocal EnableExtensions

pushd "%~dp0" || exit /b 1

set "SRC=src\winmm_proxy.cpp"
set "DEF=src\winmm_proxy.def"
set "OUT_DIR=build"
set "OUT_DLL=%OUT_DIR%\winmm.dll"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

if exist "scripts\gen_winmm_exports.ps1" (
    echo [BUILD] Generating WinMM exports...
    powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\gen_winmm_exports.ps1" -OutDir "src"
    if errorlevel 1 (
        echo [ERROR] Failed to generate WinMM exports.
        popd
        exit /b 1
    )
)

set "VCVARS="
for %%V in (18 2026 2022 2019) do (
    for %%E in (Community Professional Enterprise BuildTools) do (
        if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if not defined VCVARS if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
    )
)

if defined VCVARS goto :build_msvc

where g++.exe >nul 2>&1
if not errorlevel 1 goto :build_mingw

echo [ERROR] Neither Visual Studio MSVC (cl.exe) nor MinGW-w64 (g++.exe) was found.
popd
exit /b 1

:build_msvc
echo [BUILD] Loading MSVC x64 environment: "%VCVARS%"
call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment.
    popd
    exit /b 1
)

echo [BUILD] Compiling winmm.dll with cl.exe ...
cl /nologo /LD /O2 /Ob2 /Oi /Ot /std:c++20 /EHsc /utf-8 /MT ^
    /DWIN32_LEAN_AND_MEAN /DNOMINMAX ^
    "%SRC%" ^
    /Fe:"%OUT_DLL%" ^
    /Fo:"%OUT_DIR%\winmm_proxy.obj" ^
    /link /DEF:"%DEF%" /IMPLIB:"%OUT_DIR%\winmm.lib" /OPT:REF /OPT:ICF kernel32.lib user32.lib
if errorlevel 1 (
    echo [ERROR] MSVC build failed.
    popd
    exit /b 1
)

del /q "%OUT_DIR%\winmm_proxy.obj" "%OUT_DIR%\winmm.lib" "%OUT_DIR%\winmm.exp" 2>nul
echo [OK] Built: "%~dp0%OUT_DLL%"
echo [INFO] Copy "%~dp0%OUT_DLL%" to: Steam\steamapps\common\GarrysMod\bin\win64\winmm.dll
popd
exit /b 0

:build_mingw
echo [BUILD] MSVC not found; falling back to MinGW g++ ...
call "%~dp0build_mingw_gcc.bat"
set "ERR=%errorlevel%"
popd
exit /b %ERR%
