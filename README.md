# gmod-faster-loading (`winmm.dll`)

Speeds up heavy maps and addon packs without touching game files or visuals

## Install

1. Switch Garry's Mod to the `x86-64 - Chromium + 64-bit binaries` Steam beta
2. Put `winmm.dll` into `GarrysMod\bin\win64\winmm.dll`
3. Launch the game

## Update Resilience

Most hooks use standard Win32 APIs and don't depend on GMod offsets. An ABI self-test validates `VFileSystem022` on startup and safely disables that layer if an update changes it

## Crash Reporting

Set `profiler_enabled=1` in `GarrysMod\bin\win64\gmod_load_optimizer.ini`, reproduce the crash, and open an Issue with files from `GarrysMod\gmod_load_logs\`:
* `crash_log.txt` + latest `crash_*.mdmp`
* `profiler_status.log` and `load_*.log`
* Map name and system RAM

## What It Does

* Negative Lookup Blocking - skips hundreds of thousands of missing-file disk checks across addon folders.
* MapPack Bypass - stops redundant copying of Workshop `.bsp` maps into `cache/map_pack.bsp`.
* Micro-Read Coalescing (`.vtx`, `.vhv`, `.vmt`) & BSP PakLump Index - reads assets in one pass instead of tiny chunk reads.
* VTF / VPK RAM Caches & Background Prefetch - caches reused textures/VPK blocks and pre-warms map assets on background threads.
* `VirtualFree` Quarantine & `SetCursor` Dedup - reduces Windows kernel lock and syscall overhead during loads.
* Auto Hardware Tuning - detects available RAM (`ram_profile=auto`), SSD vs. HDD, and enables RAM Guard to trim caches under memory pressure.

## Build (Windows x64)

* MSVC: run `build_msvc_safe.bat`
* MinGW-w64: run `build_mingw_gcc.bat`

Output: `build\winmm.dll`