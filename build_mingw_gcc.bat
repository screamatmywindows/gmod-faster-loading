@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "SRC=%SCRIPT_DIR%src\winmm_proxy.cpp"
set "GEN_PS1=%SCRIPT_DIR%scripts\gen_winmm_exports.ps1"
set "OUT_DIR=%SCRIPT_DIR%build"
set "DEF=%OUT_DIR%\winmm_proxy.def"
set "OUT_DLL=%OUT_DIR%\winmm.dll"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [BUILD] Extracting all exports from System32\winmm.dll ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%GEN_PS1%" -OutDir "%OUT_DIR%"
if errorlevel 1 (
    echo [ERROR] Failed to generate WinMM export list.
    exit /b 1
)

where g++ >nul 2>&1
if errorlevel 1 (
    echo [ERROR] g++ not found in PATH. Install MinGW-w64 / MSYS2 or use build_msvc_safe.bat.
    exit /b 1
)

echo [BUILD] Compiling winmm.dll with MinGW-w64 g++ ...
g++ -shared -O3 -std=c++20 -static -static-libgcc -static-libstdc++ ^
    -DWIN32_LEAN_AND_MEAN -DNOMINMAX -I"%OUT_DIR%" ^
    "%SRC%" "%DEF%" ^
    -o "%OUT_DLL%" ^
    -lkernel32 -luser32 -Wl,--enable-stdcall-fixup
if errorlevel 1 (
    echo [ERROR] MinGW build failed.
    exit /b 1
)

echo [OK] Built: %OUT_DLL%
echo [INFO] Copy %OUT_DLL% to: Steam\steamapps\common\GarrysMod\bin\win64\winmm.dll
exit /b 0
