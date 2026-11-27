// GMod x64 Map Load Optimizer + Profiler v16 (winmm.dll)
// v12 fixes & breakthroughs:
//   - Skybox Bugfix (Layer 4c + Layer 8):
//       * Never return *ppszResolvedFilename = nullptr on a VTF cache hit when cachedResolved is empty;
//       * Exclude materials/skybox/ from Layer 4c virtual handle interception so standard skyboxes load 100% natively;
//       * Stamp g_vtfLruMap with g_vfsGeneration; unify NormalizePath (strip leading '/' / './', collapse '//');
//       * Restrict Layer 8 pak_neg strictly to static-prop light files (sp_*.vhv/.ppl, texelslighting_*.ppl);
//       * Clear BSP PakLump index on StartNewSession() and gate IndexBspPakLump on g_sessionActive (kills post-spawn re-index).
//   - Strip FILE_FLAG_NO_BUFFERING on pure-read CreateFileW/A (fixes 6.9s FS_fread unbuffered stall in filesystem_stdio.dll).
//   - Layer 9 Upgrade: Cold-start disk verification without .optmeta + multi-map cache (cache/opt_mappack/, up to 3 maps).
//   - Layer 4c Upgrade: True LRU (splice on hit) + 768 MB default + flags-agnostic key.
//   - VFS RAM Cache Upgrade: Unlock .vmt for ppszResolvedFilename != nullptr via AllocTier0String;
//     preload all uncompressed BSP .vmt files; cache small .lua (<=256 KB); persistent .vcs cache (shaders/fxc/*.vcs).
//   - Self-Overhead & VirtualFree Reduction: Cache-line-padded OptStatsAtomic, batched virtual/RA stats on Close,
//     deferred QPC in ReadAhead, pooled .vtx slurp buffers, cached HasTier0MemAlloc(), .pdata-aware ResolveExportSymbol.
// GMod x64 Map Load Optimizer + Profiler v11 (winmm.dll)
// v11 breakthroughs (targeting gm_cloak 18.5s -> ~10s):
//   - Layer 9: Deterministic cache/map_pack.bsp Zero-Copy Extraction Bypass (opt_map_pack_cache=1):
//       * Verified in filesystem_stdio.dll disassembly: Workshop .gma map loads copy 1.37 GB
//         from workshop/maps/<map>.bsp -> cache/map_pack.bsp in 4 KB chunks (~7.88s!).
//       * Verifies existing cache/map_pack.bsp via sidecar metadata + exact size + LastWriteTime +
//         live 64 KB head & 64 KB tail FNV1a-64 hash match against the source .bsp (<2 ms).
//       * On 100% match, returns a virtual write-sink handle + instant EOF on the copy read handle,
//         bypassing the entire 1.37 GB re-extraction safely!
//   - Layer 8 Upgrade: BSP Lump 40 now also preloads texelslighting_*.ppl + materials/maps/...vmt
//       * Fixes bsp_pak_neg_hits=0 by handling pathID=="BSP" O(1) negative hits (4,349 .ppl misses)
//         and zero-copy serves map-embedded .vmt files (eliminating 4-10 ms OpenEx spikes).
//   - Layer 4c Fix: Unlocks Load-Time .vtf LRU RAM Cache for ppszResolvedFilename != nullptr
//       * Uses tier0.dll!g_pMemAlloc->Alloc (vtable[1], +0x08) to populate *ppszResolvedFilename on
//         cache hits, eliminating duplicate reads of 75-149 MB skybox/lightning .vtf textures.
//   - Layer 5 Hygiene: Skips cache/map_pack.bsp and replaces polluted prefetch manifests cleanly.
//   - Auto-upgrade of v3..v10 gmod_load_optimizer.ini to config_version=11.
// GMod x64 Map Load Optimizer + Profiler v10 (External winmm.dll Proxy)
// v10 Upgrades:
//   - Layer 7: 4 MB Sequential Write-Coalescing Buffer (opt_write_buffer=1):
//       * Coalesces 350,939 x 4 KB WriteFile syscalls on cache/map_pack.bsp into ~343 x 4 MB writes
//         (eliminates ~889 ms of ntdll!ZwWriteFile kernel transitions + NTFS allocation churn).
//       * Flushes cleanly on CloseHandle, FlushFileBuffers, SetEndOfFile, SetFilePointer/Ex,
//         ReadFile, GetFileSize/Ex, and CreateFileMappingA (zero data loss, 100% deterministic).
//   - Layer 8: BSP Lump 40 (LUMP_PAKFILE) Direct ZIP Pre-Indexer & Zero-Copy Slab (opt_bsp_pak_fastpath=1):
//       * Parses VBSP header + ZIP Central Directory on cache/map_pack.bsp (on CloseHandle) and
//         loose maps/*.bsp (on CreateFileW/A) in <20 ms from hot OS page cache.
//       * Bulk-loads all uncompressed sp_*.vhv and sp_*.ppl entries into a single zero-copy RAM slab
//         and serves instant O(1) positive & negative hits in HookedOpenEx (eliminates ~969 ms .vhv
//         opens and 4,332 .ppl misses on gm_cloak; auto-freed after spawn -> 0 MB gameplay RAM).
//   - Layer 4c: Bounded Load-Time .vtf LRU RAM Cache (opt_vtf_ram_cache=1, 384 MB cap, 96 MB max file):
//       * Eliminates duplicate reads of giant .vtf textures during load (e.g. 7 x 40-75 MB HDR
//         skyboxes + 552 smaller duplicate VTFs = ~450-700 ms saved on gm_cloak).
//       * Auto-freed completely on Session_Finalized (0 MB RAM retained during gameplay).
//   - Layer 4b Upgrade: 8 MB VFS Read-Ahead + Thread-Local 1-slot handle fast-path in HookedReadEx
//     (halves .bsp refills from 394 -> ~197 and removes 350k+ g_handleMapLock acquisitions).
//   - Profiler v10: Lock-free stack caller attribution (ReadProcessMemory on ctx.Rsp) for OS wait
//     hotspots (ntdll!ZwWaitForSingleObject), reported in sampler_wait_callers.
//   - Auto-upgrade of v3..v9 gmod_load_optimizer.ini to config_version=10 (logging kept 100%).
// v9 Upgrades:
//   - CRITICAL FIX: Eliminated SRWLOCK self-deadlock on Workshop .bsp/.gma handles (gm_cloak >5min hang!):
//       * In v8, HookedReadEx held info->lock exclusively while calling g_origReadEx().
//         On Workshop .gma virtual files (like workshop/maps/gm_cloak.bsp), g_origReadEx()
//         re-entrantly calls Seek()/Tell()/EndOfFile() through the IFileSystem vtable.
//         Because HookedSeek/HookedTell/HookedEndOfFile/HookedReadLine lacked a tl_inHook
//         re-entrancy guard, the same thread tried to re-acquire the non-recursive SRWLOCK
//         and deadlocked permanently!
//       * Added tl_inHook pass-through guards to HookedEndOfFile, HookedReadLine, HookedSeek,
//         and HookedTell, plus tl_inHook protection around g_origSeek inside HookedReadLine.
//   - Added vfs_ra_refills telemetry counter to verify 4 MB read-ahead efficiency on .bsp extraction.
//   - Auto-upgrade of v3..v8 gmod_load_optimizer.ini to config_version=9 (logging kept 100%).
// v8 Upgrades:
//   - Fix v7 StartupGmaWarmup SSD Contention Regression (-480 ms on rp_joi_005se2!):
//       * Restores 10 MB per-.gma startup cap on 2 threads (finishes in <1s during menu)
//       * Immediately yields/aborts StartupGmaWarmup the instant a map load starts
//         (g_sessionActive==true) so it NEVER competes with Workshop_Mounted on SSD!
//       * Restores map prefetch default to 4 threads (2 files-first + 2 dirs-first)
//   - Layer 4b: 4 MB VFS Streaming Read-Ahead Buffer (opt_vfs_readahead=1):
//       * Eliminates CAsyncIOManager / ZwWaitForSingleObject micro-read storms on large
//         .bsp files and repeated-small-read binary handles (e.g. gm_cloak.bsp:
//         350,939 x 4KB reads = 112.4 SECONDS -> ~343 x 4MB reads = ~1s!)
//       * Lazy Seek / cached Tell / cached EndOfFile on buffered handles
//   - Layer 1 Turbo v2: Single-Pass Fused Path Normalization + 128-bit Hashing:
//       * Computes parent & leaf 128-bit hashes inline inside SplitNormalizedWinPathFast
//         in a single pass (eliminates 2 redundant string scans on ~490k calls!)
//   - Auto-upgrade of v3..v7 gmod_load_optimizer.ini to config_version=8 (logging kept 100%)
// v7 Upgrades:
//   - Layer 1 Turbo (targets ~839 ms winmm.dll self-overhead & 1066 ms .vmt OpenEx):
//       * Lock-free Thread-Local L1 128-bit Negative Cache (2048 dirs + 4096 files):
//         caches ancestor-pruned dirs & file misses with ZERO locks / ZERO heap allocs
//         (eliminates 308k+ repeated ancestor tree walks and 440k+ SRWLOCK lookups)
//       * Single-pass path normalization + dual 64-bit hashing inside SplitNormalizedWinPathFast
//       * Disables deadweight Layer 2 64KB read_buf by default (was 17 hits vs 25k+ allocs/locks)
//       * Lock-free TLS hash filter in RecordPrefetchFile/RecordPrefetchDir
//   - Layer 6: Loading-Screen USER32!SetCursor Deduplicator (targets ~178 ms NtUserSetCursor):
//       * IAT-hooks USER32.dll!SetCursor on vguimatsurface/vgui2/inputsystem/menusystem/SDL2/client
//       * Skips redundant identical SetCursor kernel syscalls ONLY while map load is active
//   - Layer 4 VFS Preservation Fix + .png Support:
//       * Stops runtime data/log writes (e.g. data/ulx_logs/*.txt) from nuking g_vfsGeneration
//       * Adds .png to shared RAM cache (eliminates 51x gmoddefault.png & UI .png re-opens)
//   - Layer 5 Prefetcher v3:
//       * Startup GMA warm-up raised to 32 MB cap on 4 threads (warms .vtf inside larger .gma)
//       * Map prefetch default raised to 6 threads (4 files-first + 2 dirs-first)
//   - Auto-upgrade of v3..v6 gmod_load_optimizer.ini to config_version=7 (logging kept 100%)
// v6 Upgrades:
//   - Smart Layer 4 Slurp Policy:
//       * .vtx per-handle slurp up to 16 MB with full read-loop (eliminates 434k+ micro-reads
//         on large .vtx like gravitia.vtx without storing >2MB files in shared RAM cache)
//       * Shared RAM cache focused on high-reuse extensions (.vmt/.vhv/.ani/.vcs/.txt/.cfg/.res/.pcf)
//       * Stops wasting allocs/memcpy on 0%-reuse single-read files (.vvd/.mdl/.lua/.phy)
//       * Zero-alloc C++20 string_view VFS cache lookups + flags-aware VFS keys
//   - 64-Shard DirCache (eliminates ~233 ms ZwWaitForAlertByThreadId SRWLOCK contention
//     between background prefetchers and main-thread lookups)
//   - Layer 5 Prefetcher v2:
//       * Early startup .gma OS Page Cache warm-up (first 10 MB of each .gma during menu boot)
//       * Skips sparse 100-200 MB .vpk archives that previously starved .vtf prefetching
//       * Split parallel pipelines (dirs + files run concurrently instead of blocking .vtf behind 6k dirs)
//       * Manifest merge (never overwrites/shrinks previously discovered dirs/files)
//   - Auto-upgrade of v3/v4/v5 gmod_load_optimizer.ini to v6 defaults (config_version=6)
// v4 Upgrades:
//   - Zero-alloc C++20 transparent wstring_view dir cache + stack-buffer path fast-path
//     (eliminates ~519k GetFullPathNameW PEB locks & ~3M heap allocs)
//   - Layer 4: IFileSystem In-Memory Small-File RAM Cache (VirtualHandlePool) for
//     repeated/micro-read .vmt/.vhv/.vtx/.mdl/.phy/.ani/.vcs/.lua files
//   - Extra DLL Win32 dir-cache IAT hooks on engine.dll & resources.dll
//   - Smart VFS generation invalidation + IBaseFileSystem::Size(path) cache
//   - Runtime PE Export Symbol Resolver in RIP sampler (e.g. ntdll.dll!NtReadFile+0x14)
// Target: Steam\steamapps\common\GarrysMod\bin\win64\winmm.dll
// Zero external dependencies; builds cleanly with MSVC (cl.exe) or MinGW-w64 (g++) in C++20.
//
// Optimizations (all configurable in gmod_load_optimizer.ini, 100% deterministic):
//   1. opt_dir_cache=1           : In-memory directory snapshot + negative/attribute cache
//                                  on filesystem_stdio.dll IAT (CreateFileA/W, GetFileAttributesExA/W).
//                                  Eliminates hundreds of thousands of NTFS + Defender syscalls.
//   2. opt_read_buffer=1         : 64 KB lazy read-ahead buffer + RAM seek/size on read-only
//                                  synchronous handles in filesystem_stdio.dll IAT (ReadFile,
//                                  SetFilePointer/Ex, GetFileSize/Ex). Eliminates ~124k+ tiny
//                                  .vtx/.vmt/.vhv ReadFile syscalls.
//   3. opt_vfs_meta_cache=1      : Generation-stamped IFileSystem cache for FileExists,
//                                  GetFileTime, and read-only OpenEx misses (skips 71-GMA/VPK
//                                  traversals on repeated probes). Auto-invalidated on writes,
//                                  addon remounts, and .bsp/.gma/.vpk opens.
//   4. opt_boost_load_priority=1 : Temporarily raises gmod.exe to ABOVE_NORMAL_PRIORITY_CLASS
//                                  during map load and restores original priority after spawn.
//
// Built-in Profiler (profiler_enabled=1 by default for A/B verification; set 0 for daily play):
//   - tier0.dll LoggingSystem listener + IAT persistence hooks
//   - IFileSystem vtable hooks (OpenEx, ReadEx, Close, FileExists, FindFirstEx, GetFileTime)
//   - Main-thread 5ms RIP sampler + determinism hashes (FNV-1a 64-bit) + optimizer counters.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <winioctl.h>
#include <tlhelp32.h>

#if defined(_MSC_VER)
#include <intrin.h>
#define PROFILER_RETURN_ADDRESS() _ReturnAddress()
#else
#define PROFILER_RETURN_ADDRESS() __builtin_return_address(0)
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// ============================================================================
// 1. Real System32\winmm.dll loader (lazy & loader-lock safe)
// ============================================================================
INIT_ONCE g_winmmInitOnce = INIT_ONCE_STATIC_INIT;
HMODULE g_realWinmm = nullptr;

BOOL CALLBACK InitRealWinmmCallback(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t sysDir[MAX_PATH] = {};
    UINT len = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::wstring fullPath(sysDir, len);
        fullPath += L"\\winmm.dll";
        g_realWinmm = LoadLibraryW(fullPath.c_str());
    }
    return TRUE;
}

inline HMODULE EnsureRealWinmm() {
    InitOnceExecuteOnce(&g_winmmInitOnce, InitRealWinmmCallback, nullptr, nullptr);
    return g_realWinmm;
}

template <typename Fn>
inline Fn ResolveWinmm(const char* name, std::atomic<void*>& cache) {
    void* p = cache.load(std::memory_order_acquire);
    if (!p) {
        HMODULE h = EnsureRealWinmm();
        if (h) {
            p = reinterpret_cast<void*>(GetProcAddress(h, name));
            cache.store(p, std::memory_order_release);
        }
    }
    return reinterpret_cast<Fn>(p);
}

// ============================================================================
// 2. High-resolution timer & FNV-1a 64-bit hashing
// ============================================================================
struct TimerState {
    double ticksToUs = 1.0;
    int64_t processStartTick = 0;

    void Init() {
        LARGE_INTEGER freq{}, now{};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&now);
        if (freq.QuadPart > 0) {
            ticksToUs = 1000000.0 / static_cast<double>(freq.QuadPart);
        }
        processStartTick = now.QuadPart;
    }

    inline int64_t NowTick() const {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        return now.QuadPart;
    }

    inline double DeltaUs(int64_t startTick, int64_t endTick) const {
        return static_cast<double>(endTick - startTick) * ticksToUs;
    }
};

TimerState g_timer;

inline uint64_t Fnv1a64Init() { return 14695981039346656037ULL; }
inline uint64_t Fnv1a64Update(uint64_t h, const char* s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        h ^= static_cast<uint8_t>(s[i]);
        h *= 1099511628211ULL;
    }
    return h;
}
inline uint64_t Fnv1a64UpdateStr(uint64_t h, const std::string& s) {
    h = Fnv1a64Update(h, s.data(), s.size());
    const char sep = '\n';
    return Fnv1a64Update(h, &sep, 1);
}

// ============================================================================
// 3. Fast Module Range Cache (maps return addresses / RIP -> module.dll + RVA)
// ============================================================================
struct ModuleRange {
    uintptr_t base = 0;
    uintptr_t end = 0;
    std::string name;
};

SRWLOCK g_modCacheLock = SRWLOCK_INIT;
std::vector<ModuleRange> g_modRanges;
int64_t g_lastModRefreshTick = 0;
std::atomic<uint32_t> g_modCacheGen{1};

std::string WideToAsciiLower(const wchar_t* ws) {
    if (!ws) return "";
    std::string out;
    while (*ws) {
        wchar_t c = *ws++;
        if (c >= L'A' && c <= L'Z') out.push_back(static_cast<char>(c - L'A' + L'a'));
        else if (c < 128) out.push_back(static_cast<char>(c));
        else out.push_back('_');
    }
    return out;
}

void RefreshModuleCacheLocked() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;

    std::vector<ModuleRange> fresh;
    fresh.reserve(128);
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
        do {
            ModuleRange mr;
            mr.base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
            mr.end = mr.base + static_cast<uintptr_t>(me.modBaseSize);
            mr.name = WideToAsciiLower(me.szModule);
            fresh.push_back(std::move(mr));
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);

    std::sort(fresh.begin(), fresh.end(), [](const ModuleRange& a, const ModuleRange& b) {
        return a.base < b.base;
    });
    g_modRanges = std::move(fresh);
    g_lastModRefreshTick = g_timer.NowTick();
    g_modCacheGen.fetch_add(1, std::memory_order_release);
}

bool ResolveAddressInCacheLocked(uintptr_t addr, std::string& outMod, uintptr_t& outRva) {
    if (g_modRanges.empty()) return false;
    auto it = std::upper_bound(g_modRanges.begin(), g_modRanges.end(), addr,
                               [](uintptr_t a, const ModuleRange& mr) { return a < mr.base; });
    if (it != g_modRanges.begin()) {
        --it;
        if (addr >= it->base && addr < it->end) {
            outMod = it->name;
            outRva = addr - it->base;
            return true;
        }
    }
    return false;
}

struct ExportEntry {
    uint32_t rva = 0;
    std::string name;
};

struct ModuleExportTable {
    std::vector<ExportEntry> exports;
};

void BuildModuleExportTable(uintptr_t base, uintptr_t end, ModuleExportTable& out) {
    if (!base || end <= base + sizeof(IMAGE_DOS_HEADER)) return;
    const size_t imgSize = static_cast<size_t>(end - base);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    if (dos->e_lfanew <= 0 || static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > imgSize) return;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    const auto& expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!expDir.VirtualAddress || expDir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) return;
    if (static_cast<size_t>(expDir.VirtualAddress) + sizeof(IMAGE_EXPORT_DIRECTORY) > imgSize) return;

    auto ed = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + expDir.VirtualAddress);
    if (!ed->NumberOfNames || !ed->AddressOfFunctions || !ed->AddressOfNames || !ed->AddressOfNameOrdinals) return;
    if (ed->AddressOfFunctions >= imgSize || ed->AddressOfNames >= imgSize || ed->AddressOfNameOrdinals >= imgSize) return;

    auto funcs = reinterpret_cast<const uint32_t*>(base + ed->AddressOfFunctions);
    auto names = reinterpret_cast<const uint32_t*>(base + ed->AddressOfNames);
    auto ords  = reinterpret_cast<const uint16_t*>(base + ed->AddressOfNameOrdinals);

    out.exports.reserve(ed->NumberOfNames);
    for (uint32_t i = 0; i < ed->NumberOfNames; ++i) {
        if (static_cast<size_t>(ed->AddressOfNames) + (i + 1) * sizeof(uint32_t) > imgSize) break;
        if (static_cast<size_t>(ed->AddressOfNameOrdinals) + (i + 1) * sizeof(uint16_t) > imgSize) break;
        uint16_t ord = ords[i];
        if (ord >= ed->NumberOfFunctions) continue;
        if (static_cast<size_t>(ed->AddressOfFunctions) + (static_cast<size_t>(ord) + 1) * sizeof(uint32_t) > imgSize) continue;
        uint32_t fnRva = funcs[ord];
        // Skip forwarded exports (pointing inside export directory)
        if (fnRva >= expDir.VirtualAddress && fnRva < expDir.VirtualAddress + expDir.Size) continue;
        uint32_t nameRva = names[i];
        if (!nameRva || nameRva >= imgSize) continue;
        const char* nm = reinterpret_cast<const char*>(base + nameRva);
        size_t maxLen = std::min<size_t>(128, imgSize - nameRva);
        size_t len = 0;
        while (len < maxLen && nm[len] != '\0') ++len;
        if (len > 0) {
            out.exports.push_back({fnRva, std::string(nm, len)});
        }
    }
    std::sort(out.exports.begin(), out.exports.end(), [](const ExportEntry& a, const ExportEntry& b) {
        if (a.rva != b.rva) return a.rva < b.rva;
        // Prefer Nt* over Zw* when both alias the same syscall stub
        bool aNt = (a.name.size() >= 2 && a.name[0] == 'N' && a.name[1] == 't');
        bool bNt = (b.name.size() >= 2 && b.name[0] == 'N' && b.name[1] == 't');
        if (aNt != bNt) return aNt;
        return a.name < b.name;
    });
}

bool LookupRuntimeFunctionBounds(uintptr_t base, uintptr_t end, uint32_t rva, uint32_t& outBegin, uint32_t& outEnd) {
    if (!base || end <= base + sizeof(IMAGE_DOS_HEADER)) return false;
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    if (base + dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > end) return false;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto& exc = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (exc.VirtualAddress == 0 || exc.Size < sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY)) return false;
    if (base + exc.VirtualAddress + exc.Size > end) return false;
    const auto* funcs = reinterpret_cast<const IMAGE_RUNTIME_FUNCTION_ENTRY*>(base + exc.VirtualAddress);
    const size_t count = exc.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);
    auto ub = std::upper_bound(funcs, funcs + count, rva,
                               [](uint32_t target, const IMAGE_RUNTIME_FUNCTION_ENTRY& rf) {
                                   return target < rf.BeginAddress;
                               });
    if (ub == funcs) return false;
    --ub;
    if (rva >= ub->BeginAddress && rva < ub->EndAddress) {
        outBegin = ub->BeginAddress;
        outEnd = ub->EndAddress;
        return true;
    }
    return false;
}

std::string ResolveExportSymbol(const std::string& modName, uintptr_t base, uintptr_t end, uintptr_t rva,
                                std::unordered_map<std::string, ModuleExportTable>& cache) {
    auto it = cache.find(modName);
    if (it == cache.end()) {
        ModuleExportTable tbl;
        BuildModuleExportTable(base, end, tbl);
        it = cache.emplace(modName, std::move(tbl)).first;
    }
    const auto& exps = it->second.exports;
    if (exps.empty()) return "";
    const uint32_t rva32 = static_cast<uint32_t>(rva);
    auto ub = std::upper_bound(exps.begin(), exps.end(), rva32,
                               [](uint32_t target, const ExportEntry& e) { return target < e.rva; });
    if (ub == exps.begin()) return "";
    --ub;
    uint32_t delta = rva32 - ub->rva;
    if (delta > 0x2000U) return "";
    uint32_t fnBegin = 0, fnEnd = 0;
    if (LookupRuntimeFunctionBounds(base, end, rva32, fnBegin, fnEnd)) {
        if (ub->rva < fnBegin || ub->rva >= fnEnd) {
            return "";
        }
    }
    char buf[192];
    std::snprintf(buf, sizeof(buf), "%s!%s+0x%x", modName.c_str(), ub->name.c_str(), delta);
    return std::string(buf);
}

std::string ResolveCallerModule(void* retAddr, uintptr_t* outRva = nullptr, uintptr_t* outBase = nullptr, uintptr_t* outEnd = nullptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(retAddr);
    std::string mod;
    uintptr_t rva = 0, base = 0, end = 0;

    auto findLocked = [&]() -> bool {
        if (g_modRanges.empty()) return false;
        auto it = std::upper_bound(g_modRanges.begin(), g_modRanges.end(), addr,
                                   [](uintptr_t a, const ModuleRange& mr) { return a < mr.base; });
        if (it != g_modRanges.begin()) {
            --it;
            if (addr >= it->base && addr < it->end) {
                mod = it->name;
                rva = addr - it->base;
                base = it->base;
                end = it->end;
                return true;
            }
        }
        return false;
    };

    AcquireSRWLockShared(&g_modCacheLock);
    bool found = findLocked();
    int64_t lastRefresh = g_lastModRefreshTick;
    ReleaseSRWLockShared(&g_modCacheLock);

    if (!found) {
        int64_t now = g_timer.NowTick();
        if (g_timer.DeltaUs(lastRefresh, now) > 250000.0) {
            AcquireSRWLockExclusive(&g_modCacheLock);
            if (g_timer.DeltaUs(g_lastModRefreshTick, now) > 250000.0) {
                RefreshModuleCacheLocked();
            }
            found = findLocked();
            ReleaseSRWLockExclusive(&g_modCacheLock);
        }
    }

    if (outRva) *outRva = found ? rva : addr;
    if (outBase) *outBase = found ? base : 0;
    if (outEnd) *outEnd = found ? end : 0;
    return found ? mod : "(unknown)";
}

// Lock-free TLS direct-mapped cache for hot-path caller module resolution
const std::string& ResolveCallerModuleFast(void* retAddr) {
    struct TlsCallerSlot {
        uintptr_t addr = 0;
        uint32_t gen = 0;
        std::string mod;
    };
    thread_local TlsCallerSlot tlsCache[32];
    const uintptr_t addr = reinterpret_cast<uintptr_t>(retAddr);
    const uint32_t curGen = g_modCacheGen.load(std::memory_order_acquire);
    const size_t idx = ((addr >> 4) ^ (addr >> 12)) & 31U;
    TlsCallerSlot& slot = tlsCache[idx];
    if (slot.gen == curGen && slot.addr == addr && !slot.mod.empty()) {
        return slot.mod;
    }
    slot.mod = ResolveCallerModule(retAddr);
    slot.addr = addr;
    slot.gen = curGen;
    return slot.mod;
}

// ============================================================================
// 4. Configuration & Paths
// ============================================================================
struct OptimizerConfig {
    // RAM auto-scaling profile: "auto", "low", "medium", "high", "manual"
    std::string ramProfile = "auto";

    // Optimizer switches
    bool enableOptimizer = true;
    bool optDirCache = true;
    bool optReadBuffer = false;
    bool optVfsMetaCache = true;
    bool optVfsRamCache = true;
    bool optExtraDllDirCache = true;
    bool optPrefetch = true;
    bool optStartupGmaWarm = true;
    bool optBoostLoadPriority = true;
    bool optDedupSetCursor = true;
    bool optVfsReadAhead = true;
    bool optWriteBuffer = true;
    bool optBspPakFastPath = true;
    bool optVtfRamCache = true;
    bool optMapPackCache = true;
    bool optStripNoBuffering = true;
    bool optVpkBlockCache = true;
    bool optVirtualFreeQuarantine = true;
    bool optBspLumpPrewarm = true;
    bool optRamGuard = true;
    bool optHddAutoTune = true;
    bool clearDirCacheOnNewMap = true;
    uint32_t ramGuardPct = 86;
    uint32_t readBufferSizeKb = 64;
    uint32_t vfsRamCacheMaxFileKb = 2048;
    uint32_t vfsVtxSlurpMaxKb = 16384;
    uint32_t vfsRamCacheMaxMb = 256;
    uint32_t vtfRamCacheMaxFileMb = 128;
    uint32_t vtfRamCacheMaxMb = 1536;
    uint32_t vpkBlockCacheMb = 512;
    uint32_t vfreeQuarantineMb = 384;
    uint32_t bspPakSlabMb = 192;
    uint32_t startupGmaMaxFileMb = 64;
    uint32_t startupGmaMaxTotalMb = 1024;
    uint32_t prefetchThreads = 2;
    uint32_t prefetchMaxMb = 2048;

    // Profiler switches (OFF by default in release build for max speed)
    bool profilerEnabled = false;
    bool hookFilesystem = true;
    bool logEveryFileEvent = true;
    bool logConsoleLines = true;
    int postSpawnTailMs = 4000;
    int chromeTraceMinUs = 25;
    int readTraceMinUs = 100;
    int sampleIntervalMs = 5;
};

OptimizerConfig g_cfg;
std::filesystem::path g_logDir;
std::filesystem::path g_dllDir;
HMODULE g_selfModule = nullptr;
std::string g_processExeName = "unknown.exe";
DWORD g_processPid = 0;

// v16: Detected system RAM, effective profile, and storage seek-penalty state
uint64_t g_sysTotalRamMb = 0;
uint64_t g_sysAvailRamMbAtStart = 0;
std::string g_effectiveRamProfile = "medium";
bool g_storageIsHdd = false;

void ApplyAdaptiveRamProfile() {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (::GlobalMemoryStatusEx(&ms)) {
        g_sysTotalRamMb = ms.ullTotalPhys / (1024ULL * 1024ULL);
        g_sysAvailRamMbAtStart = ms.ullAvailPhys / (1024ULL * 1024ULL);
    }

    std::string mode = g_cfg.ramProfile;
    for (auto& ch : mode) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (mode != "auto" && mode != "low" && mode != "medium" && mode != "high" && mode != "manual") {
        mode = "auto";
    }
    g_cfg.ramProfile = mode;

    if (mode == "manual") {
        g_effectiveRamProfile = "manual";
        return;
    }

    std::string tier = mode;
    if (mode == "auto") {
        if (g_sysTotalRamMb == 0) {
            tier = "medium";
        } else if (g_sysTotalRamMb <= 8192ULL || g_sysAvailRamMbAtStart < 3584ULL) {
            tier = "low";
        } else if (g_sysTotalRamMb <= 20480ULL || g_sysAvailRamMbAtStart < 8192ULL) {
            tier = "medium";
        } else {
            tier = "high";
        }
    }

    g_effectiveRamProfile = tier;

    if (tier == "low") {
        g_cfg.vtfRamCacheMaxMb = 256;
        g_cfg.vpkBlockCacheMb = 128;
        g_cfg.vfsRamCacheMaxMb = 96;
        g_cfg.vfreeQuarantineMb = 64;
        g_cfg.bspPakSlabMb = 96;
        g_cfg.prefetchMaxMb = 512;
        g_cfg.startupGmaMaxTotalMb = 256;
        if (g_sysTotalRamMb > 0 && g_sysTotalRamMb <= 8192ULL) {
            g_cfg.optStartupGmaWarm = false;
        }
    } else if (tier == "medium") {
        g_cfg.vtfRamCacheMaxMb = (g_sysAvailRamMbAtStart >= 6144ULL) ? 1536U : 1280U;
        g_cfg.vpkBlockCacheMb = 512;
        g_cfg.vfsRamCacheMaxMb = 256;
        g_cfg.vfreeQuarantineMb = (g_sysAvailRamMbAtStart > 0 && g_sysAvailRamMbAtStart < 5120ULL) ? 256U : 384U;
        g_cfg.bspPakSlabMb = 192;
        g_cfg.prefetchMaxMb = 2048;
        g_cfg.startupGmaMaxTotalMb = 1024;
    } else { // "high"
        g_cfg.vtfRamCacheMaxMb = 2048;
        g_cfg.vpkBlockCacheMb = 768;
        g_cfg.vfsRamCacheMaxMb = 384;
        g_cfg.vfreeQuarantineMb = 512;
        g_cfg.bspPakSlabMb = 256;
        g_cfg.prefetchMaxMb = 3072;
        g_cfg.startupGmaMaxTotalMb = 2048;
    }
}

// Zero-admin SSD vs HDD detector using IOCTL_STORAGE_QUERY_PROPERTY (dwDesiredAccess = 0).
bool DetectStorageSeekPenalty(const std::filesystem::path& path) {
    std::wstring p = path.wstring();
    if (p.empty()) return false;

    wchar_t driveLetter = 0;
    if (p.size() >= 6 && p[0] == L'\\' && p[1] == L'\\' &&
        (p[2] == L'?' || p[2] == L'.') && p[3] == L'\\' && p[5] == L':') {
        driveLetter = p[4];
    } else if (p.size() >= 2 && p[1] == L':') {
        driveLetter = p[0];
    } else {
        return false;
    }

    if (driveLetter >= L'a' && driveLetter <= L'z') {
        driveLetter = static_cast<wchar_t>(driveLetter - L'a' + L'A');
    }
    if (driveLetter < L'A' || driveLetter > L'Z') return false;

    wchar_t volPath[8] = { L'\\', L'\\', L'.', L'\\', driveLetter, L':', L'\0' };
    HANDLE hVol = ::CreateFileW(
        volPath,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (hVol == INVALID_HANDLE_VALUE) {
        return false;
    }

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType  = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR desc{};
    DWORD bytesReturned = 0;
    BOOL ok = ::DeviceIoControl(
        hVol,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query),
        &desc, sizeof(desc),
        &bytesReturned,
        nullptr);
    ::CloseHandle(hVol);

    if (!ok || bytesReturned < sizeof(desc)) {
        return false;
    }
    return desc.IncursSeekPenalty != FALSE;
}

void InitPathsAndConfig() {
    g_processPid = GetCurrentProcessId();
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
        g_processExeName = WideToAsciiLower(std::filesystem::path(exePath).filename().c_str());
    }

    wchar_t modPath[MAX_PATH] = {};
    if (g_selfModule && GetModuleFileNameW(g_selfModule, modPath, MAX_PATH) > 0) {
        g_dllDir = std::filesystem::path(modPath).parent_path();
    } else {
        g_dllDir = std::filesystem::current_path();
    }

    std::filesystem::path candidate = g_dllDir;
    if (candidate.has_parent_path() && candidate.parent_path().has_parent_path()) {
        std::wstring leaf = candidate.filename().wstring();
        std::wstring parentLeaf = candidate.parent_path().filename().wstring();
        for (auto& ch : leaf) ch = std::towlower(ch);
        for (auto& ch : parentLeaf) ch = std::towlower(ch);
        if (leaf == L"win64" && parentLeaf == L"bin") {
            g_logDir = candidate.parent_path().parent_path() / L"gmod_load_logs";
        }
    }
    if (g_logDir.empty()) {
        g_logDir = g_dllDir / L"gmod_load_logs";
    }

    std::error_code ec;
    std::filesystem::create_directories(g_logDir, ec);

    auto writeIni = [](const std::filesystem::path& p) {
        std::ofstream out(p);
        if (out) {
            out << "; GMod x64 Map Load Optimizer + Profiler v16 configuration\n"
                << "config_version=17\n"
                << "; RAM profile: auto | low | medium | high | manual\n"
                << "ram_profile=" << g_cfg.ramProfile << "\n"
                << "; --- Optimizer settings ---\n"
                << "enable_optimizer=" << (g_cfg.enableOptimizer ? 1 : 0) << "\n"
                << "opt_dir_cache=" << (g_cfg.optDirCache ? 1 : 0) << "\n"
                << "opt_read_buffer=" << (g_cfg.optReadBuffer ? 1 : 0) << "\n"
                << "opt_vfs_meta_cache=" << (g_cfg.optVfsMetaCache ? 1 : 0) << "\n"
                << "opt_vfs_ram_cache=" << (g_cfg.optVfsRamCache ? 1 : 0) << "\n"
                << "opt_extra_dll_dir_cache=" << (g_cfg.optExtraDllDirCache ? 1 : 0) << "\n"
                << "opt_prefetch=" << (g_cfg.optPrefetch ? 1 : 0) << "\n"
                << "opt_startup_gma_warm=" << (g_cfg.optStartupGmaWarm ? 1 : 0) << "\n"
                << "opt_boost_load_priority=" << (g_cfg.optBoostLoadPriority ? 1 : 0) << "\n"
                << "opt_dedup_setcursor=" << (g_cfg.optDedupSetCursor ? 1 : 0) << "\n"
                << "opt_vfs_readahead=" << (g_cfg.optVfsReadAhead ? 1 : 0) << "\n"
                << "opt_write_buffer=" << (g_cfg.optWriteBuffer ? 1 : 0) << "\n"
                << "opt_bsp_pak_fastpath=" << (g_cfg.optBspPakFastPath ? 1 : 0) << "\n"
                << "opt_vtf_ram_cache=" << (g_cfg.optVtfRamCache ? 1 : 0) << "\n"
                << "opt_map_pack_cache=" << (g_cfg.optMapPackCache ? 1 : 0) << "\n"
                << "opt_strip_no_buffering=" << (g_cfg.optStripNoBuffering ? 1 : 0) << "\n"
                << "opt_vpk_block_cache=" << (g_cfg.optVpkBlockCache ? 1 : 0) << "\n"
                << "opt_virtualfree_quarantine=" << (g_cfg.optVirtualFreeQuarantine ? 1 : 0) << "\n"
                << "opt_bsp_lump_prewarm=" << (g_cfg.optBspLumpPrewarm ? 1 : 0) << "\n"
                << "opt_ram_guard=" << (g_cfg.optRamGuard ? 1 : 0) << "\n"
                << "ram_guard_pct=" << g_cfg.ramGuardPct << "\n"
                << "opt_hdd_auto_tune=" << (g_cfg.optHddAutoTune ? 1 : 0) << "\n"
                << "clear_dir_cache_on_new_map=" << (g_cfg.clearDirCacheOnNewMap ? 1 : 0) << "\n"
                << "read_buffer_size_kb=" << g_cfg.readBufferSizeKb << "\n"
                << "vfs_ram_cache_max_file_kb=" << g_cfg.vfsRamCacheMaxFileKb << "\n"
                << "vfs_vtx_slurp_max_kb=" << g_cfg.vfsVtxSlurpMaxKb << "\n"
                << "vfs_ram_cache_max_mb=" << g_cfg.vfsRamCacheMaxMb << "\n"
                << "vtf_ram_cache_max_file_mb=" << g_cfg.vtfRamCacheMaxFileMb << "\n"
                << "vtf_ram_cache_max_mb=" << g_cfg.vtfRamCacheMaxMb << "\n"
                << "vpk_block_cache_mb=" << g_cfg.vpkBlockCacheMb << "\n"
                << "vfree_quarantine_mb=" << g_cfg.vfreeQuarantineMb << "\n"
                << "bsp_pak_slab_mb=" << g_cfg.bspPakSlabMb << "\n"
                << "startup_gma_max_file_mb=" << g_cfg.startupGmaMaxFileMb << "\n"
                << "startup_gma_max_total_mb=" << g_cfg.startupGmaMaxTotalMb << "\n"
                << "prefetch_threads=" << g_cfg.prefetchThreads << "\n"
                << "prefetch_max_mb=" << g_cfg.prefetchMaxMb << "\n"
                << "\n"
                << "; --- Profiler settings (set profiler_enabled=0 for zero-overhead daily play) ---\n"
                << "profiler_enabled=" << (g_cfg.profilerEnabled ? 1 : 0) << "\n"
                << "hook_filesystem=" << (g_cfg.hookFilesystem ? 1 : 0) << "\n"
                << "log_every_file_event=" << (g_cfg.logEveryFileEvent ? 1 : 0) << "\n"
                << "log_console_lines=" << (g_cfg.logConsoleLines ? 1 : 0) << "\n"
                << "post_spawn_tail_ms=" << g_cfg.postSpawnTailMs << "\n"
                << "chrome_trace_min_us=" << g_cfg.chromeTraceMinUs << "\n"
                << "read_trace_min_us=" << g_cfg.readTraceMinUs << "\n"
                << "sample_interval_ms=" << g_cfg.sampleIntervalMs << "\n";
        }
    };

    std::filesystem::path iniPath = g_dllDir / L"gmod_load_optimizer.ini";
    if (!std::filesystem::exists(iniPath, ec)) {
        ApplyAdaptiveRamProfile();
        writeIni(iniPath);
    } else {
        int cfgVersion = 0;
        {
            std::ifstream in(iniPath);
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                auto trim = [](std::string& s) {
                    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
                    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
                };
                trim(k);
                trim(v);
                if (k == "config_version") cfgVersion = std::atoi(v.c_str());
                else if (k == "ram_profile") g_cfg.ramProfile = v;
                else if (k == "enable_optimizer") g_cfg.enableOptimizer = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_dir_cache") g_cfg.optDirCache = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_read_buffer") g_cfg.optReadBuffer = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_vfs_meta_cache") g_cfg.optVfsMetaCache = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_vfs_ram_cache") g_cfg.optVfsRamCache = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_extra_dll_dir_cache") g_cfg.optExtraDllDirCache = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_prefetch") g_cfg.optPrefetch = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_startup_gma_warm") g_cfg.optStartupGmaWarm = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_boost_load_priority") g_cfg.optBoostLoadPriority = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_dedup_setcursor") g_cfg.optDedupSetCursor = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_vfs_readahead") g_cfg.optVfsReadAhead = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_write_buffer") g_cfg.optWriteBuffer = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_bsp_pak_fastpath") g_cfg.optBspPakFastPath = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_vtf_ram_cache") g_cfg.optVtfRamCache = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_map_pack_cache") g_cfg.optMapPackCache = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_strip_no_buffering") g_cfg.optStripNoBuffering = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_vpk_block_cache") g_cfg.optVpkBlockCache = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_virtualfree_quarantine") g_cfg.optVirtualFreeQuarantine = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_bsp_lump_prewarm") g_cfg.optBspLumpPrewarm = (std::atoi(v.c_str()) != 0);
                else if (k == "opt_ram_guard") g_cfg.optRamGuard = (std::atoi(v.c_str()) != 0);
                else if (k == "ram_guard_pct") {
                    int pct = std::atoi(v.c_str());
                    g_cfg.ramGuardPct = static_cast<uint32_t>(std::clamp(pct, 70, 96));
                }
                else if (k == "opt_hdd_auto_tune") g_cfg.optHddAutoTune = (std::atoi(v.c_str()) != 0);
                else if (k == "clear_dir_cache_on_new_map") g_cfg.clearDirCacheOnNewMap = (std::atoi(v.c_str()) != 0);
                else if (k == "read_buffer_size_kb") {
                    int kb = std::atoi(v.c_str());
                    g_cfg.readBufferSizeKb = static_cast<uint32_t>(std::clamp(kb, 16, 512));
                }
                else if (k == "vfs_ram_cache_max_file_kb") {
                    int kb = std::atoi(v.c_str());
                    g_cfg.vfsRamCacheMaxFileKb = static_cast<uint32_t>(std::clamp(kb, 64, 8192));
                }
                else if (k == "vfs_vtx_slurp_max_kb") {
                    int kb = std::atoi(v.c_str());
                    g_cfg.vfsVtxSlurpMaxKb = static_cast<uint32_t>(std::clamp(kb, 1024, 65536));
                }
                else if (k == "vfs_ram_cache_max_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.vfsRamCacheMaxMb = static_cast<uint32_t>(std::clamp(mb, 32, 1024));
                }
                else if (k == "vtf_ram_cache_max_file_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.vtfRamCacheMaxFileMb = static_cast<uint32_t>(std::clamp(mb, 4, 256));
                }
                else if (k == "vtf_ram_cache_max_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.vtfRamCacheMaxMb = static_cast<uint32_t>(std::clamp(mb, 64, 4096));
                }
                else if (k == "vpk_block_cache_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.vpkBlockCacheMb = static_cast<uint32_t>(std::clamp(mb, 16, 1024));
                }
                else if (k == "vfree_quarantine_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.vfreeQuarantineMb = static_cast<uint32_t>(std::clamp(mb, 0, 1024));
                }
                else if (k == "bsp_pak_slab_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.bspPakSlabMb = static_cast<uint32_t>(std::clamp(mb, 16, 512));
                }
                else if (k == "startup_gma_max_file_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.startupGmaMaxFileMb = static_cast<uint32_t>(std::clamp(mb, 4, 256));
                }
                else if (k == "startup_gma_max_total_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.startupGmaMaxTotalMb = static_cast<uint32_t>(std::clamp(mb, 64, 4096));
                }
                else if (k == "prefetch_threads") {
                    int t = std::atoi(v.c_str());
                    g_cfg.prefetchThreads = static_cast<uint32_t>(std::clamp(t, 1, 8));
                }
                else if (k == "prefetch_max_mb") {
                    int mb = std::atoi(v.c_str());
                    g_cfg.prefetchMaxMb = static_cast<uint32_t>(std::clamp(mb, 64, 4096));
                }
                else if (k == "profiler_enabled") g_cfg.profilerEnabled = (std::atoi(v.c_str()) != 0);
                else if (k == "hook_filesystem") g_cfg.hookFilesystem = (std::atoi(v.c_str()) != 0);
                else if (k == "log_every_file_event") g_cfg.logEveryFileEvent = (std::atoi(v.c_str()) != 0);
                else if (k == "log_console_lines") g_cfg.logConsoleLines = (std::atoi(v.c_str()) != 0);
                else if (k == "post_spawn_tail_ms") g_cfg.postSpawnTailMs = std::max(500, std::atoi(v.c_str()));
                else if (k == "chrome_trace_min_us") g_cfg.chromeTraceMinUs = std::max(0, std::atoi(v.c_str()));
                else if (k == "read_trace_min_us") g_cfg.readTraceMinUs = std::max(0, std::atoi(v.c_str()));
                else if (k == "sample_interval_ms") g_cfg.sampleIntervalMs = std::max(0, std::atoi(v.c_str()));
            }
        }
        // Auto-upgrade v3..v15 INI so v16 adaptive RAM + guard defaults activate immediately (keeping profiler_enabled intact)
        if (cfgVersion < 17) {
            g_cfg.profilerEnabled = false;
            g_cfg.ramProfile = "auto";
            g_cfg.vfsRamCacheMaxFileKb = std::max(g_cfg.vfsRamCacheMaxFileKb, 2048U);
            g_cfg.vfsVtxSlurpMaxKb = std::max(g_cfg.vfsVtxSlurpMaxKb, 16384U);
            g_cfg.vtfRamCacheMaxFileMb = std::max(g_cfg.vtfRamCacheMaxFileMb, 128U);
            g_cfg.startupGmaMaxFileMb = std::max(g_cfg.startupGmaMaxFileMb, 64U);
            g_cfg.prefetchThreads = 2U;
            g_cfg.optReadBuffer = false;
            g_cfg.optPrefetch = true;
            g_cfg.optStartupGmaWarm = true;
            g_cfg.optDedupSetCursor = true;
            g_cfg.optVfsReadAhead = true;
            g_cfg.optWriteBuffer = true;
            g_cfg.optBspPakFastPath = true;
            g_cfg.optVtfRamCache = true;
            g_cfg.optMapPackCache = true;
            g_cfg.optStripNoBuffering = true;
            g_cfg.optVpkBlockCache = true;
            g_cfg.optVirtualFreeQuarantine = true;
            g_cfg.optBspLumpPrewarm = true;
            g_cfg.optRamGuard = true;
            g_cfg.ramGuardPct = std::clamp(g_cfg.ramGuardPct, 70U, 96U);
            g_cfg.optHddAutoTune = true;
            ApplyAdaptiveRamProfile();
            writeIni(iniPath);
        } else {
            ApplyAdaptiveRamProfile();
        }
    }
}

SRWLOCK g_statusLock = SRWLOCK_INIT;
void WriteStatusLog(const std::string& msg) {
    AcquireSRWLockExclusive(&g_statusLock);
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::ofstream f(g_logDir / L"profiler_status.log", std::ios::app);
    if (f) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [PID:%u %s] ",
                      st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                      static_cast<unsigned>(g_processPid), g_processExeName.c_str());
        f << buf << msg << "\n";
    }
    ReleaseSRWLockExclusive(&g_statusLock);
}

// ============================================================================
// 5. Optimizer Telemetry & Priority Boost
// ============================================================================
struct OptStatsSnapshot {
    uint64_t dirSnapshotsIndexed = 0;
    uint64_t dirAncestorPruned = 0;
    uint64_t dirFastPathHits = 0;
    uint64_t createFileNegBlocked = 0;
    uint64_t attrCacheHits = 0;
    uint64_t attrCacheNegBlocked = 0;
    uint64_t readBufHits = 0;
    uint64_t readBufRefills = 0;
    uint64_t seekSyscallsSaved = 0;
    uint64_t sizeSyscallsSaved = 0;
    uint64_t vfsExistsHits = 0;
    uint64_t vfsTimeHits = 0;
    uint64_t vfsSizeHits = 0;
    uint64_t vfsOpenMissHits = 0;
    uint64_t vfsRamCachedFiles = 0;
    uint64_t vfsRamOpenHits = 0;
    uint64_t vfsRamReadHits = 0;
    uint64_t vfsRamReadBytes = 0;
    uint64_t vfsRamSlurpedHandles = 0;
    uint64_t vfsRamSlurpSkipSize = 0;
    uint64_t vfsRamSlurpFail = 0;
    uint64_t prefetchDirsIndexed = 0;
    uint64_t prefetchFilesWarmed = 0;
    uint64_t prefetchBytesWarmed = 0;
    uint64_t startupGmaFilesWarmed = 0;
    uint64_t startupGmaBytesWarmed = 0;
    uint64_t dirTlL1Hits = 0;
    uint64_t setCursorDeduped = 0;
    uint64_t vfsRaHits = 0;
    uint64_t vfsRaRefills = 0;
    uint64_t vfsRaBytes = 0;
    uint64_t writeBufHits = 0;
    uint64_t writeBufFlushes = 0;
    uint64_t writeBufBytes = 0;
    uint64_t bspPakIndexed = 0;
    uint64_t bspPakVhvHits = 0;
    uint64_t bspPakNegHits = 0;
    uint64_t bspPakSlabBytes = 0;
    uint64_t vtfRamCached = 0;
    uint64_t vtfRamOpenHits = 0;
    uint64_t vtfRamSavedBytes = 0;
    uint64_t mapPackBypassHits = 0;
    uint64_t mapPackSavedBytes = 0;
    // v13 Layer 10 VPK 1 MiB Block Cache
    uint64_t vpkBlockHits = 0;
    uint64_t vpkBlockMisses = 0;
    uint64_t vpkBlockSavedBytes = 0;
    uint64_t vpkSeekSaved = 0;
    // v13 Open sub-timing (microseconds)
    uint64_t openNativeUs = 0;
    uint64_t openVfsSlurpUs = 0;
    uint64_t openVtfSlurpUs = 0;
    // v13 Cache rejection & generation diagnostics
    uint64_t vfsGenBumps = 0;
    uint64_t vtfLookupMiss = 0;
    uint64_t vtfRejUnknownResolved = 0;
    uint64_t vtfLruEvictions = 0;
    uint64_t vtfGenClears = 0;
    uint64_t vtfSlurpSkipSize = 0;
    uint64_t vfsRejUnknownResolved = 0;
    uint64_t vfsSlurpSkipGen = 0;
    uint64_t vfsSlurpSkipBudget = 0;
    uint64_t mpRejNoRecentBsp = 0;
    uint64_t mpRejDiskHashFail = 0;
    uint64_t mpRejSizeMismatch = 0;
    uint64_t mpRejMetaMismatch = 0;
    uint64_t mpRejLiveHashMismatch = 0;
    // v14 Layer 11 VirtualFree Quarantine & Layer 12 BSP Lump Prewarm
    uint64_t vfreeQuarantined = 0;
    uint64_t vfreeQuarantinedBytes = 0;
    uint64_t bspLumpPrewarmBytes = 0;
    // v16 Layer 13 Runtime Memory Pressure Guard
    uint64_t ramGuardTriggers = 0;
    uint64_t ramGuardBytesFreed = 0;
};

struct OptStatsAtomic {
    alignas(64) std::atomic<uint64_t> dirSnapshotsIndexed{0};
    alignas(64) std::atomic<uint64_t> dirAncestorPruned{0};
    std::atomic<uint64_t> dirFastPathHits{0};
    std::atomic<uint64_t> createFileNegBlocked{0};
    std::atomic<uint64_t> attrCacheHits{0};
    std::atomic<uint64_t> attrCacheNegBlocked{0};
    std::atomic<uint64_t> readBufHits{0};
    std::atomic<uint64_t> readBufRefills{0};
    std::atomic<uint64_t> seekSyscallsSaved{0};
    std::atomic<uint64_t> sizeSyscallsSaved{0};
    std::atomic<uint64_t> vfsExistsHits{0};
    std::atomic<uint64_t> vfsTimeHits{0};
    std::atomic<uint64_t> vfsSizeHits{0};
    std::atomic<uint64_t> vfsOpenMissHits{0};
    std::atomic<uint64_t> vfsRamCachedFiles{0};
    std::atomic<uint64_t> vfsRamOpenHits{0};
    std::atomic<uint64_t> vfsRamReadHits{0};
    std::atomic<uint64_t> vfsRamReadBytes{0};
    std::atomic<uint64_t> vfsRamSlurpedHandles{0};
    std::atomic<uint64_t> vfsRamSlurpSkipSize{0};
    std::atomic<uint64_t> vfsRamSlurpFail{0};
    alignas(64) std::atomic<uint64_t> prefetchDirsIndexed{0};
    std::atomic<uint64_t> prefetchFilesWarmed{0};
    std::atomic<uint64_t> prefetchBytesWarmed{0};
    std::atomic<uint64_t> startupGmaFilesWarmed{0};
    std::atomic<uint64_t> startupGmaBytesWarmed{0};
    alignas(64) std::atomic<uint64_t> dirTlL1Hits{0};
    std::atomic<uint64_t> setCursorDeduped{0};
    std::atomic<uint64_t> vfsRaHits{0};
    std::atomic<uint64_t> vfsRaRefills{0};
    std::atomic<uint64_t> vfsRaBytes{0};
    std::atomic<uint64_t> writeBufHits{0};
    std::atomic<uint64_t> writeBufFlushes{0};
    std::atomic<uint64_t> writeBufBytes{0};
    std::atomic<uint64_t> bspPakIndexed{0};
    std::atomic<uint64_t> bspPakVhvHits{0};
    std::atomic<uint64_t> bspPakNegHits{0};
    std::atomic<uint64_t> bspPakSlabBytes{0};
    std::atomic<uint64_t> vtfRamCached{0};
    std::atomic<uint64_t> vtfRamOpenHits{0};
    std::atomic<uint64_t> vtfRamSavedBytes{0};
    std::atomic<uint64_t> mapPackBypassHits{0};
    std::atomic<uint64_t> mapPackSavedBytes{0};
    alignas(64) std::atomic<uint64_t> vpkBlockHits{0};
    std::atomic<uint64_t> vpkBlockMisses{0};
    std::atomic<uint64_t> vpkBlockSavedBytes{0};
    std::atomic<uint64_t> vpkSeekSaved{0};
    std::atomic<uint64_t> openNativeUs{0};
    std::atomic<uint64_t> openVfsSlurpUs{0};
    std::atomic<uint64_t> openVtfSlurpUs{0};
    std::atomic<uint64_t> vfsGenBumps{0};
    std::atomic<uint64_t> vtfLookupMiss{0};
    std::atomic<uint64_t> vtfRejUnknownResolved{0};
    std::atomic<uint64_t> vtfLruEvictions{0};
    std::atomic<uint64_t> vtfGenClears{0};
    std::atomic<uint64_t> vtfSlurpSkipSize{0};
    std::atomic<uint64_t> vfsRejUnknownResolved{0};
    std::atomic<uint64_t> vfsSlurpSkipGen{0};
    std::atomic<uint64_t> vfsSlurpSkipBudget{0};
    std::atomic<uint64_t> mpRejNoRecentBsp{0};
    std::atomic<uint64_t> mpRejDiskHashFail{0};
    std::atomic<uint64_t> mpRejSizeMismatch{0};
    std::atomic<uint64_t> mpRejMetaMismatch{0};
    std::atomic<uint64_t> mpRejLiveHashMismatch{0};
    std::atomic<uint64_t> vfreeQuarantined{0};
    std::atomic<uint64_t> vfreeQuarantinedBytes{0};
    std::atomic<uint64_t> bspLumpPrewarmBytes{0};
    std::atomic<uint64_t> ramGuardTriggers{0};
    std::atomic<uint64_t> ramGuardBytesFreed{0};

    OptStatsSnapshot Snapshot() const {
        OptStatsSnapshot s;
        s.dirSnapshotsIndexed = dirSnapshotsIndexed.load(std::memory_order_relaxed);
        s.dirAncestorPruned   = dirAncestorPruned.load(std::memory_order_relaxed);
        s.dirFastPathHits     = dirFastPathHits.load(std::memory_order_relaxed);
        s.createFileNegBlocked = createFileNegBlocked.load(std::memory_order_relaxed);
        s.attrCacheHits       = attrCacheHits.load(std::memory_order_relaxed);
        s.attrCacheNegBlocked = attrCacheNegBlocked.load(std::memory_order_relaxed);
        s.readBufHits         = readBufHits.load(std::memory_order_relaxed);
        s.readBufRefills      = readBufRefills.load(std::memory_order_relaxed);
        s.seekSyscallsSaved   = seekSyscallsSaved.load(std::memory_order_relaxed);
        s.sizeSyscallsSaved   = sizeSyscallsSaved.load(std::memory_order_relaxed);
        s.vfsExistsHits       = vfsExistsHits.load(std::memory_order_relaxed);
        s.vfsTimeHits         = vfsTimeHits.load(std::memory_order_relaxed);
        s.vfsSizeHits         = vfsSizeHits.load(std::memory_order_relaxed);
        s.vfsOpenMissHits     = vfsOpenMissHits.load(std::memory_order_relaxed);
        s.vfsRamCachedFiles   = vfsRamCachedFiles.load(std::memory_order_relaxed);
        s.vfsRamOpenHits      = vfsRamOpenHits.load(std::memory_order_relaxed);
        s.vfsRamReadHits       = vfsRamReadHits.load(std::memory_order_relaxed);
        s.vfsRamReadBytes      = vfsRamReadBytes.load(std::memory_order_relaxed);
        s.vfsRamSlurpedHandles  = vfsRamSlurpedHandles.load(std::memory_order_relaxed);
        s.vfsRamSlurpSkipSize   = vfsRamSlurpSkipSize.load(std::memory_order_relaxed);
        s.vfsRamSlurpFail       = vfsRamSlurpFail.load(std::memory_order_relaxed);
        s.prefetchDirsIndexed   = prefetchDirsIndexed.load(std::memory_order_relaxed);
        s.prefetchFilesWarmed   = prefetchFilesWarmed.load(std::memory_order_relaxed);
        s.prefetchBytesWarmed   = prefetchBytesWarmed.load(std::memory_order_relaxed);
        s.startupGmaFilesWarmed = startupGmaFilesWarmed.load(std::memory_order_relaxed);
        s.startupGmaBytesWarmed = startupGmaBytesWarmed.load(std::memory_order_relaxed);
        s.dirTlL1Hits           = dirTlL1Hits.load(std::memory_order_relaxed);
        s.setCursorDeduped      = setCursorDeduped.load(std::memory_order_relaxed);
        s.vfsRaHits             = vfsRaHits.load(std::memory_order_relaxed);
        s.vfsRaRefills          = vfsRaRefills.load(std::memory_order_relaxed);
        s.vfsRaBytes            = vfsRaBytes.load(std::memory_order_relaxed);
        s.writeBufHits          = writeBufHits.load(std::memory_order_relaxed);
        s.writeBufFlushes       = writeBufFlushes.load(std::memory_order_relaxed);
        s.writeBufBytes         = writeBufBytes.load(std::memory_order_relaxed);
        s.bspPakIndexed         = bspPakIndexed.load(std::memory_order_relaxed);
        s.bspPakVhvHits         = bspPakVhvHits.load(std::memory_order_relaxed);
        s.bspPakNegHits         = bspPakNegHits.load(std::memory_order_relaxed);
        s.bspPakSlabBytes       = bspPakSlabBytes.load(std::memory_order_relaxed);
        s.vtfRamCached          = vtfRamCached.load(std::memory_order_relaxed);
        s.vtfRamOpenHits        = vtfRamOpenHits.load(std::memory_order_relaxed);
        s.vtfRamSavedBytes      = vtfRamSavedBytes.load(std::memory_order_relaxed);
        s.mapPackBypassHits     = mapPackBypassHits.load(std::memory_order_relaxed);
        s.mapPackSavedBytes     = mapPackSavedBytes.load(std::memory_order_relaxed);
        s.vpkBlockHits          = vpkBlockHits.load(std::memory_order_relaxed);
        s.vpkBlockMisses        = vpkBlockMisses.load(std::memory_order_relaxed);
        s.vpkBlockSavedBytes    = vpkBlockSavedBytes.load(std::memory_order_relaxed);
        s.vpkSeekSaved          = vpkSeekSaved.load(std::memory_order_relaxed);
        s.openNativeUs          = openNativeUs.load(std::memory_order_relaxed);
        s.openVfsSlurpUs        = openVfsSlurpUs.load(std::memory_order_relaxed);
        s.openVtfSlurpUs        = openVtfSlurpUs.load(std::memory_order_relaxed);
        s.vfsGenBumps           = vfsGenBumps.load(std::memory_order_relaxed);
        s.vtfLookupMiss         = vtfLookupMiss.load(std::memory_order_relaxed);
        s.vtfRejUnknownResolved = vtfRejUnknownResolved.load(std::memory_order_relaxed);
        s.vtfLruEvictions       = vtfLruEvictions.load(std::memory_order_relaxed);
        s.vtfGenClears          = vtfGenClears.load(std::memory_order_relaxed);
        s.vtfSlurpSkipSize      = vtfSlurpSkipSize.load(std::memory_order_relaxed);
        s.vfsRejUnknownResolved = vfsRejUnknownResolved.load(std::memory_order_relaxed);
        s.vfsSlurpSkipGen       = vfsSlurpSkipGen.load(std::memory_order_relaxed);
        s.vfsSlurpSkipBudget    = vfsSlurpSkipBudget.load(std::memory_order_relaxed);
        s.mpRejNoRecentBsp      = mpRejNoRecentBsp.load(std::memory_order_relaxed);
        s.mpRejDiskHashFail     = mpRejDiskHashFail.load(std::memory_order_relaxed);
        s.mpRejSizeMismatch     = mpRejSizeMismatch.load(std::memory_order_relaxed);
        s.mpRejMetaMismatch     = mpRejMetaMismatch.load(std::memory_order_relaxed);
        s.mpRejLiveHashMismatch = mpRejLiveHashMismatch.load(std::memory_order_relaxed);
        s.vfreeQuarantined      = vfreeQuarantined.load(std::memory_order_relaxed);
        s.vfreeQuarantinedBytes = vfreeQuarantinedBytes.load(std::memory_order_relaxed);
        s.bspLumpPrewarmBytes   = bspLumpPrewarmBytes.load(std::memory_order_relaxed);
        s.ramGuardTriggers      = ramGuardTriggers.load(std::memory_order_relaxed);
        s.ramGuardBytesFreed    = ramGuardBytesFreed.load(std::memory_order_relaxed);
        return s;
    }
};

OptStatsSnapshot SubOptStats(const OptStatsSnapshot& endS, const OptStatsSnapshot& startS) {
    OptStatsSnapshot d;
    d.dirSnapshotsIndexed  = endS.dirSnapshotsIndexed  - startS.dirSnapshotsIndexed;
    d.dirAncestorPruned    = endS.dirAncestorPruned    - startS.dirAncestorPruned;
    d.dirFastPathHits      = endS.dirFastPathHits      - startS.dirFastPathHits;
    d.createFileNegBlocked = endS.createFileNegBlocked - startS.createFileNegBlocked;
    d.attrCacheHits        = endS.attrCacheHits        - startS.attrCacheHits;
    d.attrCacheNegBlocked  = endS.attrCacheNegBlocked  - startS.attrCacheNegBlocked;
    d.readBufHits          = endS.readBufHits          - startS.readBufHits;
    d.readBufRefills       = endS.readBufRefills       - startS.readBufRefills;
    d.seekSyscallsSaved    = endS.seekSyscallsSaved    - startS.seekSyscallsSaved;
    d.sizeSyscallsSaved    = endS.sizeSyscallsSaved    - startS.sizeSyscallsSaved;
    d.vfsExistsHits        = endS.vfsExistsHits        - startS.vfsExistsHits;
    d.vfsTimeHits          = endS.vfsTimeHits          - startS.vfsTimeHits;
    d.vfsSizeHits          = endS.vfsSizeHits          - startS.vfsSizeHits;
    d.vfsOpenMissHits      = endS.vfsOpenMissHits      - startS.vfsOpenMissHits;
    d.vfsRamCachedFiles    = endS.vfsRamCachedFiles    - startS.vfsRamCachedFiles;
    d.vfsRamOpenHits       = endS.vfsRamOpenHits       - startS.vfsRamOpenHits;
    d.vfsRamReadHits       = endS.vfsRamReadHits       - startS.vfsRamReadHits;
    d.vfsRamReadBytes      = endS.vfsRamReadBytes      - startS.vfsRamReadBytes;
    d.vfsRamSlurpedHandles  = endS.vfsRamSlurpedHandles  - startS.vfsRamSlurpedHandles;
    d.vfsRamSlurpSkipSize   = endS.vfsRamSlurpSkipSize   - startS.vfsRamSlurpSkipSize;
    d.vfsRamSlurpFail       = endS.vfsRamSlurpFail       - startS.vfsRamSlurpFail;
    d.prefetchDirsIndexed   = endS.prefetchDirsIndexed   - startS.prefetchDirsIndexed;
    d.prefetchFilesWarmed   = endS.prefetchFilesWarmed   - startS.prefetchFilesWarmed;
    d.prefetchBytesWarmed   = endS.prefetchBytesWarmed   - startS.prefetchBytesWarmed;
    // startupGma counters are cumulative process-level before/during load; report total end value
    d.startupGmaFilesWarmed = endS.startupGmaFilesWarmed;
    d.startupGmaBytesWarmed = endS.startupGmaBytesWarmed;
    d.dirTlL1Hits           = endS.dirTlL1Hits - startS.dirTlL1Hits;
    d.setCursorDeduped      = endS.setCursorDeduped - startS.setCursorDeduped;
    d.vfsRaHits             = endS.vfsRaHits - startS.vfsRaHits;
    d.vfsRaRefills          = endS.vfsRaRefills - startS.vfsRaRefills;
    d.vfsRaBytes            = endS.vfsRaBytes - startS.vfsRaBytes;
    d.writeBufHits          = endS.writeBufHits - startS.writeBufHits;
    d.writeBufFlushes       = endS.writeBufFlushes - startS.writeBufFlushes;
    d.writeBufBytes         = endS.writeBufBytes - startS.writeBufBytes;
    d.bspPakIndexed         = endS.bspPakIndexed - startS.bspPakIndexed;
    d.bspPakVhvHits         = endS.bspPakVhvHits - startS.bspPakVhvHits;
    d.bspPakNegHits         = endS.bspPakNegHits - startS.bspPakNegHits;
    d.bspPakSlabBytes       = endS.bspPakSlabBytes - startS.bspPakSlabBytes;
    d.vtfRamCached          = endS.vtfRamCached - startS.vtfRamCached;
    d.vtfRamOpenHits        = endS.vtfRamOpenHits - startS.vtfRamOpenHits;
    d.vtfRamSavedBytes      = endS.vtfRamSavedBytes - startS.vtfRamSavedBytes;
    d.mapPackBypassHits     = endS.mapPackBypassHits - startS.mapPackBypassHits;
    d.mapPackSavedBytes     = endS.mapPackSavedBytes - startS.mapPackSavedBytes;
    d.vpkBlockHits          = endS.vpkBlockHits - startS.vpkBlockHits;
    d.vpkBlockMisses        = endS.vpkBlockMisses - startS.vpkBlockMisses;
    d.vpkBlockSavedBytes    = endS.vpkBlockSavedBytes - startS.vpkBlockSavedBytes;
    d.vpkSeekSaved          = endS.vpkSeekSaved - startS.vpkSeekSaved;
    d.openNativeUs          = endS.openNativeUs - startS.openNativeUs;
    d.openVfsSlurpUs        = endS.openVfsSlurpUs - startS.openVfsSlurpUs;
    d.openVtfSlurpUs        = endS.openVtfSlurpUs - startS.openVtfSlurpUs;
    d.vfsGenBumps           = endS.vfsGenBumps - startS.vfsGenBumps;
    d.vtfLookupMiss         = endS.vtfLookupMiss - startS.vtfLookupMiss;
    d.vtfRejUnknownResolved = endS.vtfRejUnknownResolved - startS.vtfRejUnknownResolved;
    d.vtfLruEvictions       = endS.vtfLruEvictions - startS.vtfLruEvictions;
    d.vtfGenClears          = endS.vtfGenClears - startS.vtfGenClears;
    d.vtfSlurpSkipSize      = endS.vtfSlurpSkipSize - startS.vtfSlurpSkipSize;
    d.vfsRejUnknownResolved = endS.vfsRejUnknownResolved - startS.vfsRejUnknownResolved;
    d.vfsSlurpSkipGen       = endS.vfsSlurpSkipGen - startS.vfsSlurpSkipGen;
    d.vfsSlurpSkipBudget    = endS.vfsSlurpSkipBudget - startS.vfsSlurpSkipBudget;
    d.mpRejNoRecentBsp      = endS.mpRejNoRecentBsp - startS.mpRejNoRecentBsp;
    d.mpRejDiskHashFail     = endS.mpRejDiskHashFail - startS.mpRejDiskHashFail;
    d.mpRejSizeMismatch     = endS.mpRejSizeMismatch - startS.mpRejSizeMismatch;
    d.mpRejMetaMismatch     = endS.mpRejMetaMismatch - startS.mpRejMetaMismatch;
    d.mpRejLiveHashMismatch = endS.mpRejLiveHashMismatch - startS.mpRejLiveHashMismatch;
    d.vfreeQuarantined      = endS.vfreeQuarantined - startS.vfreeQuarantined;
    d.vfreeQuarantinedBytes = endS.vfreeQuarantinedBytes - startS.vfreeQuarantinedBytes;
    d.bspLumpPrewarmBytes   = endS.bspLumpPrewarmBytes - startS.bspLumpPrewarmBytes;
    d.ramGuardTriggers      = endS.ramGuardTriggers - startS.ramGuardTriggers;
    d.ramGuardBytesFreed    = endS.ramGuardBytesFreed - startS.ramGuardBytesFreed;
    return d;
}

OptStatsAtomic g_optStats;
std::atomic<bool> g_shuttingDown{false};
std::atomic<bool> g_sessionActive{false};
std::atomic<uint64_t> g_vfsGeneration{1};
std::atomic<uint64_t> g_dirCacheGen{1};

std::atomic<DWORD> g_savedPriorityClass{0};
std::atomic<bool> g_priorityBoosted{false};
// v16 Layer 13: Runtime Memory Pressure Guard (0=normal, 1=soft pause, 2=hard trim)
std::atomic<uint8_t> g_ramGuardLevel{0};
std::atomic<bool> g_ramGuardActive{false};

void ApplyLoadPriorityBoost(bool enable) {
    if (!g_cfg.enableOptimizer || !g_cfg.optBoostLoadPriority) return;
    if (enable) {
        bool expected = false;
        if (g_priorityBoosted.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            DWORD cur = GetPriorityClass(GetCurrentProcess());
            g_savedPriorityClass.store(cur ? cur : NORMAL_PRIORITY_CLASS, std::memory_order_release);
            if (cur == NORMAL_PRIORITY_CLASS || cur == BELOW_NORMAL_PRIORITY_CLASS) {
                SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
            }
        }
    } else {
        bool expected = true;
        if (g_priorityBoosted.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
            DWORD prev = g_savedPriorityClass.load(std::memory_order_acquire);
            if (prev) SetPriorityClass(GetCurrentProcess(), prev);
        }
    }
}

// ============================================================================
// 6. Optimizer Layer 1 & 2: filesystem_stdio.dll Win32 IAT Acceleration
//    (Directory Snapshot + Negative/Attr Cache + 64KB Synchronous Read Buffer)
// ============================================================================
struct WideViewHash {
    using is_transparent = void;
    size_t operator()(std::wstring_view sv) const noexcept {
        uint64_t h = 14695981039346656037ULL;
        for (wchar_t c : sv) {
            h ^= static_cast<uint16_t>(c);
            h *= 1099511628211ULL;
        }
        return static_cast<size_t>(h);
    }
};

struct WideViewEqual {
    using is_transparent = void;
    bool operator()(std::wstring_view a, std::wstring_view b) const noexcept {
        return a == b;
    }
};

struct DirSnapshot {
    bool exists = false;
    std::unordered_map<std::wstring, WIN32_FILE_ATTRIBUTE_DATA, WideViewHash, WideViewEqual> entries;
};

constexpr size_t kDirCacheShards = 64;
constexpr size_t kMaxCachedDirsPerShard = 512; // 32,768 total dirs across 64 shards

struct DirCacheShard {
    SRWLOCK lock = SRWLOCK_INIT;
    std::unordered_map<std::wstring, DirSnapshot, WideViewHash, WideViewEqual> map;
};

DirCacheShard g_dirShards[kDirCacheShards];

inline size_t DirShardIndex(std::wstring_view dir) noexcept {
    return WideViewHash{}(dir) & (kDirCacheShards - 1);
}

void ReserveDirShardsOnce() {
    for (size_t i = 0; i < kDirCacheShards; ++i) {
        AcquireSRWLockExclusive(&g_dirShards[i].lock);
        if (g_dirShards[i].map.bucket_count() < 256) {
            g_dirShards[i].map.reserve(256);
        }
        ReleaseSRWLockExclusive(&g_dirShards[i].lock);
    }
}

struct BufferedHandle {
    SRWLOCK lock = SRWLOCK_INIT;
    uint64_t pos = 0;
    uint64_t realPos = 0;
    uint64_t fileSize = 0;
    uint64_t bufStart = 0;
    uint32_t bufLen = 0;
    bool disabled = false;
    std::vector<uint8_t> buf;
};

SRWLOCK g_readBufMapLock = SRWLOCK_INIT;
std::atomic<uint32_t> g_readBufCount{0};
std::unordered_map<HANDLE, std::shared_ptr<BufferedHandle>> g_readBufMap;

// Saved original KERNEL32 pointers from filesystem_stdio.dll IAT
using FnCreateFileA           = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using FnCreateFileW           = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using FnGetFileAttributesExA  = BOOL(WINAPI*)(LPCSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
using FnGetFileAttributesExW  = BOOL(WINAPI*)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
using FnReadFile              = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
using FnSetFilePointer        = DWORD(WINAPI*)(HANDLE, LONG, PLONG, DWORD);
using FnSetFilePointerEx      = BOOL(WINAPI*)(HANDLE, LARGE_INTEGER, PLARGE_INTEGER, DWORD);
using FnGetFileSize           = DWORD(WINAPI*)(HANDLE, LPDWORD);
using FnGetFileSizeEx         = BOOL(WINAPI*)(HANDLE, PLARGE_INTEGER);
using FnCloseHandle           = BOOL(WINAPI*)(HANDLE);
using FnCreateFileMappingA    = HANDLE(WINAPI*)(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCSTR);
using FnWriteFile             = BOOL(WINAPI*)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
using FnSetEndOfFile          = BOOL(WINAPI*)(HANDLE);
using FnFlushFileBuffers      = BOOL(WINAPI*)(HANDLE);
using FnDeleteFileA           = BOOL(WINAPI*)(LPCSTR);
using FnDeleteFileW           = BOOL(WINAPI*)(LPCWSTR);
using FnCreateDirectoryW      = BOOL(WINAPI*)(LPCWSTR, LPSECURITY_ATTRIBUTES);
using FnRemoveDirectoryW      = BOOL(WINAPI*)(LPCWSTR);
using FnMoveFileExW           = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, DWORD);

FnCreateFileA          g_origCreateFileA          = nullptr;
FnCreateFileW          g_origCreateFileW          = nullptr;
FnGetFileAttributesExA g_origGetFileAttributesExA = nullptr;
FnGetFileAttributesExW g_origGetFileAttributesExW = nullptr;
FnReadFile             g_origReadFile             = nullptr;
FnSetFilePointer       g_origSetFilePointer       = nullptr;
FnSetFilePointerEx     g_origSetFilePointerEx     = nullptr;
FnGetFileSize          g_origGetFileSize          = nullptr;
FnGetFileSizeEx        g_origGetFileSizeEx        = nullptr;
FnCloseHandle          g_origCloseHandle          = nullptr;
FnCreateFileMappingA   g_origCreateFileMappingA   = nullptr;
FnWriteFile            g_origWriteFile            = nullptr;
FnSetEndOfFile         g_origSetEndOfFile         = nullptr;
FnFlushFileBuffers     g_origFlushFileBuffers     = nullptr;
FnDeleteFileA          g_origDeleteFileA          = nullptr;
FnDeleteFileW          g_origDeleteFileW          = nullptr;
FnCreateDirectoryW     g_origCreateDirectoryW     = nullptr;
FnRemoveDirectoryW     g_origRemoveDirectoryW     = nullptr;
FnMoveFileExW          g_origMoveFileExW          = nullptr;

HMODULE g_fsModuleHandle = nullptr;
bool g_fsIatHooked = false;

inline void LowerWideInPlace(std::wstring& s) {
    for (wchar_t& c : s) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        else if (c >= 128) c = static_cast<wchar_t>(std::towlower(c));
    }
}

bool AnsiToWide(const char* src, std::wstring& out) {
    if (!src || !*src) return false;
    int needed = MultiByteToWideChar(CP_ACP, 0, src, -1, nullptr, 0);
    if (needed <= 1) return false;
    out.resize(static_cast<size_t>(needed - 1));
    int written = MultiByteToWideChar(CP_ACP, 0, src, -1, out.data(), needed);
    return written > 0;
}

struct DualHash128 {
    uint64_t h1 = 0;
    uint64_t h2 = 0;
};

constexpr uint64_t kFnv1aOffset = 14695981039346656037ULL;
constexpr uint64_t kFnv1aPrime  = 1099511628211ULL;
constexpr uint64_t kHash2Seed   = 0x9e3779b97f4a7c15ULL;
constexpr uint64_t kHash2Prime  = 1315423911ULL;

inline DualHash128 HashWide128(std::wstring_view sv) noexcept {
    uint64_t h1 = kFnv1aOffset;
    uint64_t h2 = kHash2Seed;
    for (wchar_t c : sv) {
        uint64_t v = static_cast<uint16_t>(c);
        h1 = (h1 ^ v) * kFnv1aPrime;
        h2 = (h2 * kHash2Prime) ^ (v + 0x9e3779b97f4a7c15ULL);
    }
    h2 ^= static_cast<uint64_t>(sv.size()) * 0xbf58476d1ce4e5b9ULL;
    return {h1, h2};
}

struct SplitPathStorage {
    wchar_t stackBuf[512];
    std::wstring heapFallback;
    std::wstring_view parent;
    std::wstring_view leaf;
    DualHash128 parentHash{};
    DualHash128 leafHash{};
    bool hasHashes = false;
};

// Zero-alloc single-pass fast path for already-absolute Windows paths ("C:\foo\bar\baz.vmt"):
// normalizes, splits, AND computes 128-bit parent/leaf hashes in ONE pass!
bool SplitNormalizedWinPathFast(const wchar_t* raw, SplitPathStorage& st) {
    if (!raw || !*raw) return false;
    st.hasHashes = false;

    // Fast path: drive-letter absolute path without wildcards, "..", or "\.\"
    const wchar_t c0 = raw[0];
    if (((c0 >= L'A' && c0 <= L'Z') || (c0 >= L'a' && c0 <= L'z')) &&
        raw[1] == L':' && (raw[2] == L'\\' || raw[2] == L'/')) {
        size_t outLen = 0;
        size_t lastSlash = 0;
        bool needSlow = false;
        wchar_t prev = 0;
        uint64_t curH1 = kFnv1aOffset, curH2 = kHash2Seed;
        uint64_t pH1 = kFnv1aOffset, pH2 = kHash2Seed;
        uint64_t lH1 = kFnv1aOffset, lH2 = kHash2Seed;
        for (const wchar_t* p = raw; *p; ++p) {
            wchar_t c = *p;
            if (c == L'*' || c == L'?') return false;
            if (c >= 128 || outLen >= 510) { needSlow = true; break; }
            if (c == L'/') c = L'\\';
            else if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
            if (c == L'\\' && prev == L'\\') {
                continue;
            }
            if (c == L'.' && prev == L'\\') {
                wchar_t next = p[1];
                if (next == L'\\' || next == L'/' || next == L'\0' || next == L'.') {
                    needSlow = true;
                    break;
                }
            }
            const uint64_t v = static_cast<uint16_t>(c);
            if (c == L'\\') {
                lastSlash = outLen;
                pH1 = curH1;
                pH2 = curH2;
                lH1 = kFnv1aOffset;
                lH2 = kHash2Seed;
            } else {
                lH1 = (lH1 ^ v) * kFnv1aPrime;
                lH2 = (lH2 * kHash2Prime) ^ (v + 0x9e3779b97f4a7c15ULL);
            }
            curH1 = (curH1 ^ v) * kFnv1aPrime;
            curH2 = (curH2 * kHash2Prime) ^ (v + 0x9e3779b97f4a7c15ULL);
            st.stackBuf[outLen++] = c;
            prev = c;
        }
        if (!needSlow) {
            bool trimmed = false;
            while (outLen > 3 && st.stackBuf[outLen - 1] == L'\\') { --outLen; trimmed = true; }
            if (outLen <= 3) return false;
            st.stackBuf[outLen] = L'\0';
            std::wstring_view full(st.stackBuf, outLen);
            size_t slash = trimmed ? full.find_last_of(L'\\') : lastSlash;
            if (slash == std::wstring_view::npos || slash == 0 || slash + 1 >= full.size()) return false;
            st.parent = (slash == 2 && full[1] == L':') ? full.substr(0, 3) : full.substr(0, slash);
            st.leaf = full.substr(slash + 1);
            if (!st.leaf.empty()) {
                if (!trimmed && slash > 2) {
                    st.parentHash = {pH1, pH2 ^ (static_cast<uint64_t>(st.parent.size()) * 0xbf58476d1ce4e5b9ULL)};
                    st.leafHash = {lH1, lH2 ^ (static_cast<uint64_t>(st.leaf.size()) * 0xbf58476d1ce4e5b9ULL)};
                    st.hasHashes = true;
                }
                g_optStats.dirFastPathHits.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            return false;
        }
    }

    // Slow fallback: device/pipe/UNC/relative/long/".." paths
    if (raw[0] == L'\\' && raw[1] == L'\\' && (raw[2] == L'.' || raw[2] == L'?')) {
        if (raw[2] == L'.') return false;
        if (wcsstr(raw, L"Pipe\\") || wcsstr(raw, L"pipe\\")) return false;
    }
    for (const wchar_t* p = raw; *p; ++p) {
        if (*p == L'*' || *p == L'?') return false;
    }

    DWORD len = GetFullPathNameW(raw, 512, st.stackBuf, nullptr);
    if (len == 0) return false;

    std::wstring_view full;
    if (len < 512) {
        for (DWORD i = 0; i < len; ++i) {
            wchar_t& c = st.stackBuf[i];
            if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
            else if (c >= 128) c = static_cast<wchar_t>(std::towlower(c));
        }
        while (len > 3 && (st.stackBuf[len - 1] == L'\\' || st.stackBuf[len - 1] == L'/')) --len;
        if (len <= 3) return false;
        st.stackBuf[len] = L'\0';
        full = std::wstring_view(st.stackBuf, len);
    } else {
        st.heapFallback.resize(len);
        DWORD len2 = GetFullPathNameW(raw, len, st.heapFallback.data(), nullptr);
        if (len2 == 0 || len2 >= len) return false;
        st.heapFallback.resize(len2);
        while (st.heapFallback.size() > 3 && (st.heapFallback.back() == L'\\' || st.heapFallback.back() == L'/')) {
            st.heapFallback.pop_back();
        }
        if (st.heapFallback.size() <= 3) return false;
        LowerWideInPlace(st.heapFallback);
        full = st.heapFallback;
    }

    auto slash = full.find_last_of(L'\\');
    if (slash == std::wstring_view::npos || slash == 0 || slash + 1 >= full.size()) return false;
    if (full.size() >= 2 && full[0] == L'\\' && full[1] == L'\\') {
        auto second = full.find(L'\\', 2);
        if (second == std::wstring_view::npos || second == slash) return false;
    }
    st.parent = (slash == 2 && full[1] == L':') ? full.substr(0, 3) : full.substr(0, slash);
    st.leaf = full.substr(slash + 1);
    return !st.leaf.empty();
}

// Fast zero-alloc ASCII-to-wide conversion for CreateFileA / GetFileAttributesExA
bool SplitNormalizedAnsiPathFast(const char* raw, SplitPathStorage& st) {
    if (!raw || !*raw) return false;
    wchar_t tmp[512];
    size_t len = 0;
    bool pureAscii = true;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(raw); *p; ++p) {
        if (*p >= 128 || len >= 510) { pureAscii = false; break; }
        tmp[len++] = static_cast<wchar_t>(*p);
    }
    if (pureAscii) {
        tmp[len] = L'\0';
        return SplitNormalizedWinPathFast(tmp, st);
    }
    std::wstring w;
    if (!AnsiToWide(raw, w)) return false;
    return SplitNormalizedWinPathFast(w.c_str(), st);
}

// v13: Dedicated VTF generation counter so unrelated non-VTF writes NEVER wipe the 768 MB VTF LRU cache!
std::atomic<uint64_t> g_vtfGeneration{1};

// v13: Atomic load phase for zero-overhead phase-attributed telemetry
enum LoadPhase : uint8_t {
    PHASE_PRE_LUA = 0,
    PHASE_LUA_TO_SPAWN = 1,
    PHASE_POST_SPAWN = 2,
    PHASE_COUNT = 3
};
std::atomic<uint8_t> g_currentPhase{PHASE_PRE_LUA};

// ============================================================================
// Layer 11 (v14): Bounded VirtualFree(MEM_RELEASE) Quarantine During Map Load
//                 Eliminates 137-317 ms of main-thread ntdll!ZwFreeVirtualMemory
//                 stalls from tier0.dll dlmalloc & lua_shared.dll during loading!
// ============================================================================
using FnVirtualFree = BOOL(WINAPI*)(LPVOID, SIZE_T, DWORD);
FnVirtualFree g_origTier0VirtualFree = nullptr;
FnVirtualFree g_origLuaVirtualFree = nullptr;

SRWLOCK g_vfreeQuarLock = SRWLOCK_INIT;
std::vector<LPVOID> g_vfreeQuarList;
uint64_t g_vfreeQuarBytes = 0;
constexpr uint64_t kVfreeQuarMaxSingle = 16ULL * 1024ULL * 1024ULL;

uint64_t DrainVirtualFreeQuarantine(bool async) {
    std::vector<LPVOID> batch;
    uint64_t freedBytes = 0;
    AcquireSRWLockExclusive(&g_vfreeQuarLock);
    if (!g_vfreeQuarList.empty()) {
        batch.swap(g_vfreeQuarList);
        freedBytes = g_vfreeQuarBytes;
        g_vfreeQuarBytes = 0;
    }
    ReleaseSRWLockExclusive(&g_vfreeQuarLock);
    if (batch.empty()) return 0;

    auto doFree = [](std::vector<LPVOID> ptrs) {
        for (LPVOID p : ptrs) {
            if (p) ::VirtualFree(p, 0, MEM_RELEASE);
        }
    };
    if (!async || g_shuttingDown.load(std::memory_order_relaxed)) {
        doFree(std::move(batch));
        return freedBytes;
    }
    auto* heapBatch = new std::vector<LPVOID>(std::move(batch));
    HANDLE h = ::CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
        std::unique_ptr<std::vector<LPVOID>> b(static_cast<std::vector<LPVOID>*>(param));
        ::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
        for (LPVOID p : *b) {
            if (p) ::VirtualFree(p, 0, MEM_RELEASE);
        }
        ::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
        return 0;
    }, heapBatch, 0, nullptr);
    if (h) ::CloseHandle(h);
    else {
        doFree(std::move(*heapBatch));
        delete heapBatch;
    }
    return freedBytes;
}

BOOL WINAPI HookedVirtualFreeImpl(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType, FnVirtualFree origFn) {
    if (g_cfg.enableOptimizer && g_cfg.optVirtualFreeQuarantine &&
        dwFreeType == MEM_RELEASE && dwSize == 0 && lpAddress != nullptr &&
        !g_shuttingDown.load(std::memory_order_relaxed) &&
        g_sessionActive.load(std::memory_order_relaxed) &&
        g_currentPhase.load(std::memory_order_relaxed) != PHASE_POST_SPAWN &&
        g_ramGuardLevel.load(std::memory_order_relaxed) == 0) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (::VirtualQuery(lpAddress, &mbi, sizeof(mbi)) == sizeof(mbi) &&
            mbi.State == MEM_COMMIT && mbi.AllocationBase == lpAddress &&
            mbi.RegionSize > 0 && mbi.RegionSize <= kVfreeQuarMaxSingle) {
            const uint64_t maxQuarBytes = static_cast<uint64_t>(g_cfg.vfreeQuarantineMb) * 1024ULL * 1024ULL;
            bool queued = false;
            if (maxQuarBytes > 0) {
                AcquireSRWLockExclusive(&g_vfreeQuarLock);
                if (g_vfreeQuarBytes + mbi.RegionSize <= maxQuarBytes && g_vfreeQuarList.size() < 4096) {
                    g_vfreeQuarList.push_back(lpAddress);
                    g_vfreeQuarBytes += mbi.RegionSize;
                    queued = true;
                }
                ReleaseSRWLockExclusive(&g_vfreeQuarLock);
            }
            if (queued) {
                g_optStats.vfreeQuarantined.fetch_add(1, std::memory_order_relaxed);
                g_optStats.vfreeQuarantinedBytes.fetch_add(mbi.RegionSize, std::memory_order_relaxed);
                return TRUE;
            }
        }
    }
    return origFn ? origFn(lpAddress, dwSize, dwFreeType) : ::VirtualFree(lpAddress, dwSize, dwFreeType);
}

BOOL WINAPI HookedTier0VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    return HookedVirtualFreeImpl(lpAddress, dwSize, dwFreeType, g_origTier0VirtualFree);
}

BOOL WINAPI HookedLuaVirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    return HookedVirtualFreeImpl(lpAddress, dwSize, dwFreeType, g_origLuaVirtualFree);
}

inline bool AffectsVtfCacheWide(std::wstring_view leaf, std::wstring_view parent = {}) noexcept {
    auto endsWith = [](std::wstring_view s, std::wstring_view suf) {
        return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
    };
    if (leaf == L"map_pack.bsp" || endsWith(leaf, L".optmeta")) return false;
    if (!parent.empty() && parent.find(L"\\opt_mappack") != std::wstring_view::npos) return false;
    return endsWith(leaf, L".vtf") || endsWith(leaf, L".vpk") || endsWith(leaf, L".gma") || endsWith(leaf, L".bsp");
}

inline bool AffectsVtfCacheNarrow(std::string_view norm) noexcept {
    if (norm.empty() || norm == "cache/map_pack.bsp") return false;
    auto endsWith = [](std::string_view s, std::string_view suf) {
        return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
    };
    return endsWith(norm, ".vtf") || endsWith(norm, ".vpk") || endsWith(norm, ".gma") || endsWith(norm, ".bsp");
}

inline bool IsNonAssetWriteLeaf(std::wstring_view leaf, std::wstring_view parent = {}) {
    auto endsWith = [](std::wstring_view s, std::wstring_view suf) {
        return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
    };
    if (leaf == L"map_pack.bsp" ||
        endsWith(leaf, L".log") || endsWith(leaf, L".json") ||
        endsWith(leaf, L".db") || endsWith(leaf, L".db-journal") ||
        endsWith(leaf, L".db-wal") || endsWith(leaf, L".db-shm") ||
        endsWith(leaf, L".ini") || endsWith(leaf, L".tmp") ||
        endsWith(leaf, L".optmeta") || endsWith(leaf, L".cache") ||
        endsWith(leaf, L".cfg") || endsWith(leaf, L".mdmp") || endsWith(leaf, L".dmp")) {
        return true;
    }
    // Runtime logs/data/cache writes should NOT nuke VFS RAM cache!
    if (!parent.empty()) {
        if (parent.find(L"\\data\\") != std::wstring_view::npos ||
            parent.find(L"\\logs\\") != std::wstring_view::npos ||
            parent.find(L"\\gmod_load_logs") != std::wstring_view::npos ||
            parent.find(L"\\cache\\lua") != std::wstring_view::npos ||
            parent.find(L"\\cache\\opt_mappack") != std::wstring_view::npos ||
            parent.find(L"\\cfg") != std::wstring_view::npos ||
            endsWith(parent, L"\\data") || endsWith(parent, L"\\logs") ||
            endsWith(parent, L"\\cache")) {
            return true;
        }
    }
    return false;
}

inline bool IsNonAssetNarrowWritePath(std::string_view norm) noexcept {
    auto endsWith = [](std::string_view s, std::string_view suf) {
        return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
    };
    if (endsWith(norm, ".log") || endsWith(norm, ".json") ||
        endsWith(norm, ".db") || endsWith(norm, ".db-journal") ||
        endsWith(norm, ".db-wal") || endsWith(norm, ".db-shm") ||
        endsWith(norm, ".ini") || endsWith(norm, ".tmp") ||
        endsWith(norm, ".optmeta") || endsWith(norm, ".cache") ||
        endsWith(norm, ".cfg") || endsWith(norm, ".mdmp") || endsWith(norm, ".dmp")) {
        return true;
    }
    if (norm.rfind("data/", 0) == 0 || norm.rfind("logs/", 0) == 0 ||
        norm.rfind("cache/lua/", 0) == 0 || norm.rfind("cache/opt_mappack/", 0) == 0 ||
        norm.rfind("gmod_load_logs/", 0) == 0 || norm.rfind("cfg/", 0) == 0 ||
        norm == "cache/map_pack.bsp") {
        return true;
    }
    return false;
}

// v16.1: Dynamic multiplayer / Workshop / FastDL paths that can appear on disk at runtime
// (after initial NOT_FOUND probes) must never be served from stale negative/meta caches.
inline bool IsDynamicMpPathNarrow(std::string_view norm) noexcept {
    if (norm.empty()) return false;
    auto endsWith = [&](std::string_view suf) noexcept {
        return norm.size() >= suf.size() &&
               norm.compare(norm.size() - suf.size(), suf.size(), suf) == 0;
    };
    if (endsWith(".bsp") || endsWith(".gma") || endsWith(".bz2")) return true;
    if (norm.rfind("maps/", 0) == 0 || norm.rfind("download/", 0) == 0 ||
        norm.rfind("workshop/", 0) == 0 || norm.rfind("cache/lua/", 0) == 0 ||
        norm.rfind("cache/workshop/", 0) == 0) {
        return true;
    }
    if (norm.find("/maps/") != std::string_view::npos ||
        norm.find("/download/") != std::string_view::npos ||
        norm.find("/workshop/") != std::string_view::npos ||
        norm.find("/cache/lua/") != std::string_view::npos ||
        norm.find("/cache/workshop/") != std::string_view::npos) {
        return true;
    }
    return false;
}

void ClearDirCacheAll() {
    g_dirCacheGen.fetch_add(1, std::memory_order_acq_rel);
    for (size_t i = 0; i < kDirCacheShards; ++i) {
        AcquireSRWLockExclusive(&g_dirShards[i].lock);
        g_dirShards[i].map.clear();
        ReleaseSRWLockExclusive(&g_dirShards[i].lock);
    }
}

inline bool IsNonFsDevicePath(const wchar_t* rawPath) noexcept {
    if (!rawPath || !*rawPath) return true;
    if (rawPath[0] == L'\\' && rawPath[1] == L'\\' && rawPath[2] == L'.') return true;
    if (rawPath[0] == L'\\' && rawPath[1] == L'\\' && rawPath[2] == L'?' &&
        (wcsstr(rawPath, L"Pipe\\") || wcsstr(rawPath, L"pipe\\"))) return true;
    if (_wcsicmp(rawPath, L"CONOUT$") == 0 || _wcsicmp(rawPath, L"CONIN$") == 0 ||
        _wcsicmp(rawPath, L"CON") == 0 || _wcsicmp(rawPath, L"NUL") == 0 ||
        _wcsicmp(rawPath, L"PRN") == 0 || _wcsicmp(rawPath, L"AUX") == 0) return true;
    return false;
}

void InvalidateDirAndParent(const wchar_t* rawPath) {
    if (IsNonFsDevicePath(rawPath)) return;
    SplitPathStorage st;
    if (!SplitNormalizedWinPathFast(rawPath, st)) {
        // Device, pipe, wildcard, root, or unresolvable path: do not nuke VFS/DirCache!
        return;
    }
    g_dirCacheGen.fetch_add(1, std::memory_order_acq_rel);
    if (!IsNonAssetWriteLeaf(st.leaf, st.parent)) {
        g_vfsGeneration.fetch_add(1, std::memory_order_acq_rel);
        g_optStats.vfsGenBumps.fetch_add(1, std::memory_order_relaxed);
    }
    if (AffectsVtfCacheWide(st.leaf, st.parent)) {
        g_vtfGeneration.fetch_add(1, std::memory_order_acq_rel);
    }
    if (!g_cfg.enableOptimizer || !g_cfg.optDirCache) return;
    std::wstring parentStr(st.parent);
    std::wstring fullStr = (!parentStr.empty() && parentStr.back() == L'\\')
        ? (parentStr + std::wstring(st.leaf))
        : (parentStr + L"\\" + std::wstring(st.leaf));
    {
        auto& sh = g_dirShards[DirShardIndex(parentStr)];
        AcquireSRWLockExclusive(&sh.lock);
        sh.map.erase(parentStr);
        ReleaseSRWLockExclusive(&sh.lock);
    }
    {
        auto& sh = g_dirShards[DirShardIndex(fullStr)];
        AcquireSRWLockExclusive(&sh.lock);
        sh.map.erase(fullStr);
        ReleaseSRWLockExclusive(&sh.lock);
    }
}

enum class DirLookupResult : uint8_t {
    Bypass = 0,
    PathNotFound = 1,
    FileNotFound = 2,
    Found = 3
};

inline void RecordPrefetchDir(std::wstring_view dir);
inline void RecordPrefetchFile(std::wstring_view parent, std::wstring_view leaf);

// v8 Layer 1 Turbo v2: Precomputed single-pass 128-bit dual hash + lock-free Thread-Local L1 Negative Cache

struct TlNegEntry {
    uint64_t h1 = 0;
    uint64_t h2 = 0;
    uint64_t gen = 0;
};

constexpr size_t kTlNegDirSlots = 2048;   // Direct-mapped (32 KB, L1/L2 resident)
constexpr size_t kTlNegFileSlots = 4096;  // Direct-mapped (64 KB, L2 resident)

struct TlDirL1Cache {
    TlNegEntry negDir[kTlNegDirSlots];
    TlNegEntry negFile[kTlNegFileSlots];
};

thread_local TlDirL1Cache tl_dirL1;

inline DualHash128 CombineFileHash128(const DualHash128& ph, const DualHash128& lh) noexcept {
    uint64_t h1 = ph.h1 ^ (lh.h1 * 0x9e3779b97f4a7c15ULL + 0x517cc1b727220a95ULL);
    uint64_t h2 = ph.h2 ^ ((lh.h2 << 13) | (lh.h2 >> 51)) ^ 0xbf58476d1ce4e5b9ULL;
    return {h1, h2};
}

inline bool IsDynamicMpPathWide(std::wstring_view parentDir, std::wstring_view leafName) noexcept {
    auto endsWithW = [](std::wstring_view s, std::wstring_view suf) noexcept {
        return s.size() >= suf.size() &&
               s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };
    if (endsWithW(leafName, L".bsp") || endsWithW(leafName, L".gma") || endsWithW(leafName, L".bz2")) {
        return true;
    }
    if (parentDir.empty()) return false;
    if (endsWithW(parentDir, L"\\maps") || endsWithW(parentDir, L"\\download") ||
        endsWithW(parentDir, L"\\workshop") || endsWithW(parentDir, L"\\cache\\lua") ||
        endsWithW(parentDir, L"\\cache\\workshop")) {
        return true;
    }
    if (parentDir.find(L"\\maps\\") != std::wstring_view::npos ||
        parentDir.find(L"\\download\\") != std::wstring_view::npos ||
        parentDir.find(L"\\workshop\\") != std::wstring_view::npos ||
        parentDir.find(L"\\cache\\lua\\") != std::wstring_view::npos ||
        parentDir.find(L"\\cache\\workshop\\") != std::wstring_view::npos) {
        return true;
    }
    return false;
}

// Zero-alloc sharded lookup with lock-free Thread-Local L1 negative cache & precomputed hashes.
DirLookupResult LookupInDirCache(std::wstring_view parentDir, std::wstring_view leafName,
                                 WIN32_FILE_ATTRIBUTE_DATA* outAttr,
                                 const DualHash128* preParentHash = nullptr,
                                 const DualHash128* preLeafHash = nullptr) {
    if (leafName == L"map_pack.bsp" || IsDynamicMpPathWide(parentDir, leafName)) {
        return DirLookupResult::Bypass;
    }
    const uint64_t curGen = g_dirCacheGen.load(std::memory_order_acquire);
    const DualHash128 ph = preParentHash ? *preParentHash : HashWide128(parentDir);

    // 0a) Lock-free TL L1 check: missing or ancestor-pruned parentDir
    {
        const auto& e = tl_dirL1.negDir[ph.h1 & (kTlNegDirSlots - 1)];
        if (e.gen == curGen && e.h1 == ph.h1 && e.h2 == ph.h2) {
            g_optStats.dirTlL1Hits.fetch_add(1, std::memory_order_relaxed);
            return DirLookupResult::PathNotFound;
        }
    }

    const DualHash128 lh = preLeafHash ? *preLeafHash : HashWide128(leafName);
    const DualHash128 fh = CombineFileHash128(ph, lh);

    // 0b) Lock-free TL L1 check: known-missing file in existing dir
    {
        const auto& e = tl_dirL1.negFile[fh.h1 & (kTlNegFileSlots - 1)];
        if (e.gen == curGen && e.h1 == fh.h1 && e.h2 == fh.h2) {
            g_optStats.dirTlL1Hits.fetch_add(1, std::memory_order_relaxed);
            return DirLookupResult::FileNotFound;
        }
    }

    auto storeNegDirL1 = [&](uint64_t g) noexcept {
        auto& e = tl_dirL1.negDir[ph.h1 & (kTlNegDirSlots - 1)];
        e.h1 = ph.h1;
        e.h2 = ph.h2;
        e.gen = g;
    };
    auto storeNegFileL1 = [&](uint64_t g) noexcept {
        auto& e = tl_dirL1.negFile[fh.h1 & (kTlNegFileSlots - 1)];
        e.h1 = fh.h1;
        e.h2 = fh.h2;
        e.gen = g;
    };

    // 1) Fast sharded shared lookup (reuse ph.h1 for shard index!)
    const size_t shardIdx = static_cast<size_t>(ph.h1) & (kDirCacheShards - 1);
    auto& shard = g_dirShards[shardIdx];
    AcquireSRWLockShared(&shard.lock);
    auto it = shard.map.find(parentDir);
    if (it != shard.map.end()) {
        if (!it->second.exists) {
            ReleaseSRWLockShared(&shard.lock);
            storeNegDirL1(curGen);
            return DirLookupResult::PathNotFound;
        }
        auto eit = it->second.entries.find(leafName);
        if (eit == it->second.entries.end()) {
            ReleaseSRWLockShared(&shard.lock);
            storeNegFileL1(curGen);
            return DirLookupResult::FileNotFound;
        }
        if (outAttr) *outAttr = eit->second;
        ReleaseSRWLockShared(&shard.lock);
        return DirLookupResult::Found;
    }
    ReleaseSRWLockShared(&shard.lock);

    // 2) Zero-alloc sharded ancestor negative pruning -> NOW CACHED in TL L1!
    {
        std::wstring_view cur = parentDir;
        while (cur.size() > 3) {
            auto sl = cur.find_last_of(L'\\');
            if (sl == std::wstring_view::npos || sl < 2) break;
            std::wstring_view anc = (sl == 2 && cur[1] == L':') ? cur.substr(0, 3) : cur.substr(0, sl);
            std::wstring_view child = cur.substr(sl + 1);
            auto& ash = g_dirShards[DirShardIndex(anc)];
            AcquireSRWLockShared(&ash.lock);
            auto ait = ash.map.find(anc);
            if (ait != ash.map.end()) {
                bool pruned = false;
                if (!ait->second.exists) {
                    pruned = true;
                } else {
                    auto ce = ait->second.entries.find(child);
                    if (ce == ait->second.entries.end() ||
                        (ce->second.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                        pruned = true;
                    }
                }
                ReleaseSRWLockShared(&ash.lock);
                if (pruned) {
                    g_optStats.dirAncestorPruned.fetch_add(1, std::memory_order_relaxed);
                    storeNegDirL1(curGen);
                    return DirLookupResult::PathNotFound;
                }
                break;
            }
            ReleaseSRWLockShared(&ash.lock);
            if (anc.size() <= 3) break;
            cur = anc;
        }
    }

    // 3) Populate parentDir outside lock via single FindFirstFileExW
    std::wstring pattern(parentDir);
    if (!pattern.empty() && pattern.back() != L'\\') pattern.push_back(L'\\');
    pattern.push_back(L'*');

    WIN32_FIND_DATAW ffd{};
    HANDLE hFind = FindFirstFileExW(
        pattern.c_str(),
        FindExInfoBasic,
        &ffd,
        FindExSearchNameMatch,
        nullptr,
        FIND_FIRST_EX_LARGE_FETCH);

    DirSnapshot snap;
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND || err == ERROR_DIRECTORY) {
            snap.exists = false;
        } else {
            return DirLookupResult::Bypass;
        }
    } else {
        snap.exists = true;
        do {
            const wchar_t* fn = ffd.cFileName;
            if (fn[0] == L'.' && (fn[1] == L'\0' || (fn[1] == L'.' && fn[2] == L'\0'))) {
                continue;
            }
            std::wstring key(fn);
            LowerWideInPlace(key);
            WIN32_FILE_ATTRIBUTE_DATA ad{};
            ad.dwFileAttributes = ffd.dwFileAttributes;
            ad.ftCreationTime   = ffd.ftCreationTime;
            ad.ftLastAccessTime = ffd.ftLastAccessTime;
            ad.ftLastWriteTime  = ffd.ftLastWriteTime;
            ad.nFileSizeHigh    = ffd.nFileSizeHigh;
            ad.nFileSizeLow     = ffd.nFileSizeLow;
            snap.entries.emplace(std::move(key), ad);
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }

    g_optStats.dirSnapshotsIndexed.fetch_add(1, std::memory_order_relaxed);
    RecordPrefetchDir(parentDir);

    DirLookupResult res = DirLookupResult::FileNotFound;
    if (!snap.exists) {
        res = DirLookupResult::PathNotFound;
    } else {
        auto eit = snap.entries.find(leafName);
        if (eit != snap.entries.end()) {
            if (outAttr) *outAttr = eit->second;
            res = DirLookupResult::Found;
        }
    }

    std::wstring parentKey(parentDir);
    AcquireSRWLockExclusive(&shard.lock);
    if (shard.map.size() >= kMaxCachedDirsPerShard) {
        shard.map.clear();
    }
    shard.map[std::move(parentKey)] = std::move(snap);
    ReleaseSRWLockExclusive(&shard.lock);
    if (g_dirCacheGen.load(std::memory_order_acquire) == curGen) {
        if (res == DirLookupResult::PathNotFound) storeNegDirL1(curGen);
        else if (res == DirLookupResult::FileNotFound) storeNegFileL1(curGen);
    }
    return res;
}

std::shared_ptr<BufferedHandle> FindBufferedHandle(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE) return nullptr;
    AcquireSRWLockShared(&g_readBufMapLock);
    auto it = g_readBufMap.find(h);
    std::shared_ptr<BufferedHandle> res = (it != g_readBufMap.end()) ? it->second : nullptr;
    ReleaseSRWLockShared(&g_readBufMapLock);
    return res;
}

void RegisterBufferedHandle(HANDLE h, uint64_t fileSize) {
    if (!h || h == INVALID_HANDLE_VALUE || fileSize == 0) return;
    auto bh = std::make_shared<BufferedHandle>();
    bh->fileSize = fileSize;
    AcquireSRWLockExclusive(&g_readBufMapLock);
    if (g_readBufMap.find(h) == g_readBufMap.end()) {
        g_readBufCount.fetch_add(1, std::memory_order_release);
    }
    g_readBufMap[h] = std::move(bh);
    ReleaseSRWLockExclusive(&g_readBufMapLock);
}

void DisableAndSyncBufferedHandle(HANDLE h) {
    auto bh = FindBufferedHandle(h);
    if (!bh) return;
    AcquireSRWLockExclusive(&bh->lock);
    if (!bh->disabled) {
        if (bh->realPos != bh->pos && g_origSetFilePointerEx) {
            LARGE_INTEGER li{};
            li.QuadPart = static_cast<LONGLONG>(bh->pos);
            g_origSetFilePointerEx(h, li, nullptr, FILE_BEGIN);
            bh->realPos = bh->pos;
        }
        bh->disabled = true;
        bh->buf.clear();
        bh->bufLen = 0;
    }
    ReleaseSRWLockExclusive(&bh->lock);
}

inline bool IsPureReadOnlyOpen(DWORD dwDesiredAccess, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes) {
    (void)dwFlagsAndAttributes;
    constexpr DWORD kWriteMask = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA |
                                 FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | DELETE | WRITE_DAC | WRITE_OWNER;
    return (dwCreationDisposition == OPEN_EXISTING) &&
           ((dwDesiredAccess & kWriteMask) == 0);
}


// ============================================================================
// Layer 7: 4 MB Sequential Write-Coalescing Buffer (opt_write_buffer=1)
//          Collapses 350k+ x 4 KB WriteFile syscalls on cache/map_pack.bsp
// ============================================================================
void IndexBspPakLump(const std::wstring& bspPath, bool forceReindex = false);
void ClearBspPakIndex();
void ClearVtfRamCache();
void OnMapPackBspWriteClosed(const std::wstring& mapPackPath);

constexpr size_t kWriteBufBytes = 8 * 1024 * 1024; // 8 MB write coalescing buffer (matches 8 MB VFS read-ahead)

struct WriteBufferedHandle {
    SRWLOCK lock = SRWLOCK_INIT;
    std::wstring fullPath;
    std::vector<uint8_t> buf;
    size_t bufUsed = 0;
    uint32_t smallWriteStreak = 0;
    bool isBsp = false;
    bool disabled = false;
};

SRWLOCK g_writeBufMapLock = SRWLOCK_INIT;
std::unordered_map<HANDLE, std::shared_ptr<WriteBufferedHandle>> g_writeBufMap;
std::atomic<uint32_t> g_writeBufCount{0};
std::atomic<uint64_t> g_writeBufCloseGen{1};

struct WriteBufTlsSlot {
    HANDLE handle = nullptr;
    uint64_t gen = 0;
    std::shared_ptr<WriteBufferedHandle> wb;
};
thread_local WriteBufTlsSlot tl_writeBufSlot{};

inline bool ShouldBufferWritePath(const std::wstring& path, bool& outIsBsp) {
    outIsBsp = false;
    if (path.size() < 4) return false;
    if (path.rfind(L"\\\\.\\", 0) == 0 || path.rfind(L"\\\\?\\pipe", 0) == 0) return false;
    std::wstring lower = path;
    for (auto& c : lower) {
        if (c == L'/') c = L'\\';
        else if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    auto endsW = [&](std::wstring_view suf) {
        return lower.size() >= suf.size() && lower.compare(lower.size() - suf.size(), suf.size(), suf) == 0;
    };
    // Only buffer Workshop -> cache/map_pack.bsp extraction; never buffer MP/FastDL/Workshop downloads
    if (endsW(L"\\cache\\map_pack.bsp")) {
        outIsBsp = true;
        return true;
    }
    if (endsW(L".bsp") || endsW(L".gma") || endsW(L".bz2") ||
        lower.find(L"\\download\\") != std::wstring::npos ||
        lower.find(L"\\workshop\\") != std::wstring::npos ||
        lower.find(L"\\cache\\lua\\") != std::wstring::npos ||
        lower.find(L"\\cache\\workshop\\") != std::wstring::npos ||
        lower.find(L"\\maps\\") != std::wstring::npos) {
        return false;
    }
    // Never buffer logs, SQLite DBs, INI, or crash dumps
    if (endsW(L".log") || endsW(L".json") || endsW(L".db") || endsW(L".db-journal") ||
        endsW(L".db-wal") || endsW(L".db-shm") || endsW(L".ini") || endsW(L".dmp") ||
        endsW(L".mdmp") || lower.find(L"\\gmod_load_logs\\") != std::wstring::npos) {
        return false;
    }
    return true;
}

void RegisterWriteBufferedHandle(HANDLE h, const std::wstring& path) {
    if (!g_cfg.enableOptimizer || !g_cfg.optWriteBuffer || h == INVALID_HANDLE_VALUE || !h) return;
    bool isBsp = false;
    if (!ShouldBufferWritePath(path, isBsp)) return;
    auto wb = std::make_shared<WriteBufferedHandle>();
    wb->fullPath = path;
    wb->isBsp = isBsp;
    AcquireSRWLockExclusive(&g_writeBufMapLock);
    auto ins = g_writeBufMap.insert_or_assign(h, std::move(wb));
    if (ins.second) {
        g_writeBufCount.fetch_add(1, std::memory_order_release);
    }
    g_writeBufCloseGen.fetch_add(1, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_writeBufMapLock);
}

std::shared_ptr<WriteBufferedHandle> FindWriteBufferedHandle(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE || g_writeBufCount.load(std::memory_order_acquire) == 0) return nullptr;
    AcquireSRWLockShared(&g_writeBufMapLock);
    auto it = g_writeBufMap.find(h);
    std::shared_ptr<WriteBufferedHandle> res = (it != g_writeBufMap.end()) ? it->second : nullptr;
    ReleaseSRWLockShared(&g_writeBufMapLock);
    return res;
}

BOOL FlushWriteBufferLocked(HANDLE h, WriteBufferedHandle& wb) {
    if (wb.bufUsed == 0) return TRUE;
    if (!g_origWriteFile) {
        wb.bufUsed = 0;
        return FALSE;
    }
    size_t off = 0;
    while (off < wb.bufUsed) {
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(wb.bufUsed - off, 0x40000000ULL));
        DWORD written = 0;
        if (!g_origWriteFile(h, wb.buf.data() + off, chunk, &written, nullptr)) {
            wb.bufUsed = 0;
            return FALSE;
        }
        g_optStats.writeBufFlushes.fetch_add(1, std::memory_order_relaxed);
        if (written == 0) {
            wb.bufUsed = 0;
            return FALSE;
        }
        off += written;
    }
    wb.bufUsed = 0;
    return TRUE;
}

void FlushWriteHandleIfAny(HANDLE h, bool disableAfter) {
    if (g_writeBufCount.load(std::memory_order_acquire) == 0) return;
    auto wb = FindWriteBufferedHandle(h);
    if (!wb) return;
    AcquireSRWLockExclusive(&wb->lock);
    FlushWriteBufferLocked(h, *wb);
    if (disableAfter) {
        wb->disabled = true;
        wb->buf.clear();
        wb->buf.shrink_to_fit();
    }
    ReleaseSRWLockExclusive(&wb->lock);
}


// ============================================================================
// Layer 10 (v13): Read-Only VPK 1 MiB Block Cache (replaces Source Engine's 4-slot 4 MiB LRU)
//                 + Sharded Win32 ReadFile Attribution Tracker (X-ray into ZwReadFile!)
// ============================================================================

struct Win32ReadPhaseStats {
    uint64_t calls = 0;
    uint64_t bytes = 0;
    double totalUs = 0.0;
};

struct Win32ReadTrack {
    std::string path;
    Win32ReadPhaseStats byPhase[PHASE_COUNT];
    double maxUs = 0.0;
    bool isReadOnlyVpk = false;
    uint64_t vpkPathHash = 0;
    uint64_t fileSize = 0;
    // VPK handle pointer tracking (guarded by handleLock)
    SRWLOCK handleLock = SRWLOCK_INIT;
    bool posSynced = false;
    uint64_t logicalPos = 0;
    uint64_t realPos = 0;
};

struct Win32TrackShard {
    SRWLOCK lock = SRWLOCK_INIT;
    std::unordered_map<HANDLE, std::shared_ptr<Win32ReadTrack>> map;
};
constexpr size_t kWin32TrackShards = 16;
Win32TrackShard g_win32TrackShards[kWin32TrackShards];

inline size_t Win32TrackShardIdx(HANDLE h) noexcept {
    return (reinterpret_cast<uintptr_t>(h) >> 2) & (kWin32TrackShards - 1);
}

inline uint64_t HashStrFnv1a64(std::string_view sv) noexcept {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : sv) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

inline std::string NormalizeWin32PathAscii(const std::wstring& w) {
    std::string s = WideToAsciiLower(w.c_str());
    if (s.rfind("\\\\?\\", 0) == 0) s.erase(0, 4);
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    return s;
}

inline std::string QueryHandlePathAscii(HANDLE h) {
    wchar_t buf[520] = {};
    DWORD n = ::GetFinalPathNameByHandleW(h, buf, 519, FILE_NAME_NORMALIZED);
    if (n == 0 || n >= 519) return {};
    return NormalizeWin32PathAscii(std::wstring(buf, n));
}

// 512 MiB (configurable, up to 1024 MiB) 1 MiB-aligned persistent LRU block cache for read-only .vpk files
constexpr uint32_t kVpkBlockShift = 20;
constexpr uint64_t kVpkBlockSize  = 1ULL << kVpkBlockShift; // 1 MiB (1:1 with CPackedStoreReadCache::ReadBlock)
constexpr uint64_t kVpkBlockMask  = ~(kVpkBlockSize - 1ULL);

struct VpkBlockKey {
    uint64_t pathHash = 0;
    uint64_t blockIdx = 0;
    bool operator==(const VpkBlockKey& o) const noexcept {
        return pathHash == o.pathHash && blockIdx == o.blockIdx;
    }
};

struct VpkBlockKeyHash {
    size_t operator()(const VpkBlockKey& k) const noexcept {
        uint64_t h = k.pathHash ^ (k.blockIdx * 0x9e3779b97f4a7c15ULL);
        h ^= (h >> 33);
        h *= 0xff51afd7ed558ccdULL;
        h ^= (h >> 33);
        return static_cast<size_t>(h);
    }
};

struct VpkBlockBuf {
    std::unique_ptr<uint8_t[]> data;
    uint32_t validLen = 0;
};

struct VpkBlockEntry {
    std::shared_ptr<const VpkBlockBuf> buf;
    std::list<VpkBlockKey>::iterator lruIt;
};

SRWLOCK g_vpkCacheLock = SRWLOCK_INIT;
std::list<VpkBlockKey> g_vpkLruOrder;
std::unordered_map<VpkBlockKey, VpkBlockEntry, VpkBlockKeyHash> g_vpkLruMap;

void ClearVpkBlockCache() {
    AcquireSRWLockExclusive(&g_vpkCacheLock);
    g_vpkLruMap.clear();
    g_vpkLruOrder.clear();
    ReleaseSRWLockExclusive(&g_vpkCacheLock);
}

uint64_t TrimVpkBlockCacheHalf() {
    std::vector<std::shared_ptr<const VpkBlockBuf>> dropped;
    uint64_t freedBytes = 0;
    AcquireSRWLockExclusive(&g_vpkCacheLock);
    const size_t curCount = g_vpkLruOrder.size();
    const size_t targetCount = curCount / 2;
    if (curCount > targetCount) {
        dropped.reserve(curCount - targetCount);
        while (g_vpkLruOrder.size() > targetCount) {
            VpkBlockKey victim = g_vpkLruOrder.back();
            auto it = g_vpkLruMap.find(victim);
            if (it != g_vpkLruMap.end()) {
                if (it->second.buf) {
                    freedBytes += it->second.buf->validLen;
                    dropped.push_back(std::move(it->second.buf));
                }
                g_vpkLruMap.erase(it);
            }
            g_vpkLruOrder.pop_back();
        }
    }
    ReleaseSRWLockExclusive(&g_vpkCacheLock);
    return freedBytes;
}

bool HasVpkBlock(uint64_t pathHash, uint64_t blockIdx) {
    VpkBlockKey key{pathHash, blockIdx};
    AcquireSRWLockShared(&g_vpkCacheLock);
    bool found = (g_vpkLruMap.find(key) != g_vpkLruMap.end());
    ReleaseSRWLockShared(&g_vpkCacheLock);
    return found;
}

void InvalidateVpkBlocksForPath(uint64_t pathHash) {
    if (pathHash == 0) return;
    AcquireSRWLockExclusive(&g_vpkCacheLock);
    for (auto it = g_vpkLruOrder.begin(); it != g_vpkLruOrder.end(); ) {
        if (it->pathHash == pathHash) {
            g_vpkLruMap.erase(*it);
            it = g_vpkLruOrder.erase(it);
        } else {
            ++it;
        }
    }
    ReleaseSRWLockExclusive(&g_vpkCacheLock);
}

inline void InvalidateVpkIfNeededW(LPCWSTR lpFileName) {
    if (!lpFileName || !*lpFileName) return;
    std::wstring w(lpFileName);
    if (w.size() >= 4) {
        std::string norm = NormalizeWin32PathAscii(w);
        if (norm.size() >= 4 && norm.compare(norm.size() - 4, 4, ".vpk") == 0) {
            InvalidateVpkBlocksForPath(HashStrFnv1a64(norm));
        }
    }
}

struct RecordedVpkBlock {
    std::string path;
    uint64_t blockIdx = 0;
};
void RecordVpkBlockMiss(uint64_t pathHash, const std::string& normPath, uint64_t blockIdx);

std::shared_ptr<const VpkBlockBuf> LookupVpkBlock(uint64_t pathHash, uint64_t blockIdx) {
    VpkBlockKey key{pathHash, blockIdx};
    std::shared_ptr<const VpkBlockBuf> res;
    AcquireSRWLockExclusive(&g_vpkCacheLock);
    auto it = g_vpkLruMap.find(key);
    if (it != g_vpkLruMap.end()) {
        if (it->second.lruIt != g_vpkLruOrder.begin()) {
            g_vpkLruOrder.splice(g_vpkLruOrder.begin(), g_vpkLruOrder, it->second.lruIt);
        }
        res = it->second.buf;
    }
    ReleaseSRWLockExclusive(&g_vpkCacheLock);
    return res;
}

void InsertVpkBlock(uint64_t pathHash, uint64_t blockIdx, std::shared_ptr<const VpkBlockBuf> buf) {
    if (!buf || buf->validLen == 0 || g_ramGuardLevel.load(std::memory_order_relaxed) != 0) return;
    const size_t maxBlocks = std::max<size_t>(
        16, (static_cast<size_t>(g_cfg.vpkBlockCacheMb) * 1024ULL * 1024ULL) / kVpkBlockSize);
    VpkBlockKey key{pathHash, blockIdx};
    AcquireSRWLockExclusive(&g_vpkCacheLock);
    auto existing = g_vpkLruMap.find(key);
    if (existing != g_vpkLruMap.end()) {
        if (existing->second.lruIt != g_vpkLruOrder.begin()) {
            g_vpkLruOrder.splice(g_vpkLruOrder.begin(), g_vpkLruOrder, existing->second.lruIt);
        }
        ReleaseSRWLockExclusive(&g_vpkCacheLock);
        return;
    }
    while (g_vpkLruOrder.size() >= maxBlocks) {
        VpkBlockKey victim = g_vpkLruOrder.back();
        g_vpkLruMap.erase(victim);
        g_vpkLruOrder.pop_back();
    }
    g_vpkLruOrder.push_front(key);
    g_vpkLruMap.emplace(key, VpkBlockEntry{std::move(buf), g_vpkLruOrder.begin()});
    ReleaseSRWLockExclusive(&g_vpkCacheLock);
}

void RegisterWin32ReadHandle(HANDLE h, const std::string& normPath, bool isReadOnlyVpk, uint64_t fileSize) {
    if (h == INVALID_HANDLE_VALUE || !h) return;
    auto tr = std::make_shared<Win32ReadTrack>();
    tr->path = normPath;
    tr->isReadOnlyVpk = isReadOnlyVpk;
    tr->vpkPathHash = isReadOnlyVpk ? HashStrFnv1a64(normPath) : 0;
    tr->fileSize = fileSize;
    tr->posSynced = isReadOnlyVpk;
    tr->logicalPos = 0;
    tr->realPos = 0;
    auto& sh = g_win32TrackShards[Win32TrackShardIdx(h)];
    AcquireSRWLockExclusive(&sh.lock);
    sh.map[h] = std::move(tr);
    ReleaseSRWLockExclusive(&sh.lock);
}

std::shared_ptr<Win32ReadTrack> FindWin32ReadTrack(HANDLE h) {
    if (h == INVALID_HANDLE_VALUE || !h) return nullptr;
    auto& sh = g_win32TrackShards[Win32TrackShardIdx(h)];
    AcquireSRWLockShared(&sh.lock);
    auto it = sh.map.find(h);
    std::shared_ptr<Win32ReadTrack> res = (it != sh.map.end()) ? it->second : nullptr;
    ReleaseSRWLockShared(&sh.lock);
    return res;
}

void FlushWin32TrackToSession(const Win32ReadTrack& tr);
void FlushAllWin32TracksIntoSessionLocked(struct LoadSession& sess);

HANDLE WINAPI HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (!g_origCreateFileW) return INVALID_HANDLE_VALUE;
    if (!g_cfg.enableOptimizer || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                 dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    const bool pureRead = IsPureReadOnlyOpen(dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes);
    if (!pureRead) {
        InvalidateDirAndParent(lpFileName);
        InvalidateVpkIfNeededW(lpFileName);
        HANDLE hw = g_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                      dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
        if (hw != INVALID_HANDLE_VALUE && g_cfg.optWriteBuffer && lpFileName && *lpFileName &&
            (dwDesiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA)) != 0 &&
            (dwFlagsAndAttributes & (FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_BACKUP_SEMANTICS)) == 0) {
            RegisterWriteBufferedHandle(hw, std::wstring(lpFileName));
        }
        return hw;
    }

    WIN32_FILE_ATTRIBUTE_DATA attr{};
    bool hasAttr = false;
    SplitPathStorage st;
    bool splitOk = false;

    if (g_cfg.optDirCache) {
        splitOk = SplitNormalizedWinPathFast(lpFileName, st);
        if (splitOk) {
            DirLookupResult r = LookupInDirCache(st.parent, st.leaf, &attr, st.hasHashes ? &st.parentHash : nullptr, st.hasHashes ? &st.leafHash : nullptr);
            if (r == DirLookupResult::PathNotFound) {
                g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                SetLastError(ERROR_PATH_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            if (r == DirLookupResult::FileNotFound) {
                g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            if (r == DirLookupResult::Found) {
                hasAttr = true;
            }
        }
    }

    DWORD effectiveFlags = dwFlagsAndAttributes;
    if (g_cfg.optStripNoBuffering) {
        if ((effectiveFlags & FILE_FLAG_NO_BUFFERING) != 0) {
            effectiveFlags &= ~FILE_FLAG_NO_BUFFERING;
        }
        if (splitOk && (effectiveFlags & FILE_FLAG_RANDOM_ACCESS) == 0 && st.leaf.size() >= 4 &&
            (st.leaf.compare(st.leaf.size() - 4, 4, L".bsp") == 0 ||
             st.leaf.compare(st.leaf.size() - 4, 4, L".gma") == 0)) {
            effectiveFlags |= FILE_FLAG_SEQUENTIAL_SCAN;
        }
    }
    HANDLE h = g_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                 dwCreationDisposition, effectiveFlags, hTemplateFile);
    if (h != INVALID_HANDLE_VALUE && splitOk) {
        RecordPrefetchFile(st.parent, st.leaf);
        if (g_cfg.optBspPakFastPath && g_sessionActive.load(std::memory_order_acquire) &&
            st.leaf.size() >= 4 &&
            st.leaf.compare(st.leaf.size() - 4, 4, L".bsp") == 0 && lpFileName && *lpFileName) {
            IndexBspPakLump(std::wstring(lpFileName));
        }
    }

    if (h != INVALID_HANDLE_VALUE && g_cfg.optReadBuffer &&
        (dwFlagsAndAttributes & (FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING)) == 0) {
        uint64_t fsize = 0;
        if (hasAttr && (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            fsize = (static_cast<uint64_t>(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow;
        } else if (g_origGetFileSizeEx) {
            LARGE_INTEGER li{};
            if (g_origGetFileSizeEx(h, &li) && li.QuadPart > 0) {
                fsize = static_cast<uint64_t>(li.QuadPart);
            }
        }
        if (fsize > 0) {
            RegisterBufferedHandle(h, fsize);
        }
    }
    if (h != INVALID_HANDLE_VALUE && (dwDesiredAccess & GENERIC_READ) != 0 &&
        (dwDesiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)) == 0 &&
        (dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED) == 0 && lpFileName && *lpFileName) {
        const bool isVpk = splitOk && st.leaf.size() >= 4 && st.leaf.compare(st.leaf.size() - 4, 4, L".vpk") == 0;
        const bool wantVpk = g_cfg.enableOptimizer && g_cfg.optVpkBlockCache && isVpk;
        const bool wantProf = g_cfg.profilerEnabled && g_sessionActive.load(std::memory_order_relaxed);
        if (wantVpk || wantProf) {
            uint64_t fsz = 0;
            if (hasAttr && (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                fsz = (static_cast<uint64_t>(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow;
            } else if (wantVpk && g_origGetFileSizeEx) {
                LARGE_INTEGER li{};
                if (g_origGetFileSizeEx(h, &li) && li.QuadPart > 0) fsz = static_cast<uint64_t>(li.QuadPart);
            }
            if (!hasAttr || (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                RegisterWin32ReadHandle(h, NormalizeWin32PathAscii(std::wstring(lpFileName)), wantVpk && fsz > 0, fsz);
            }
        }
    }
    return h;
}

HANDLE WINAPI HookedCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (!g_origCreateFileA) return INVALID_HANDLE_VALUE;
    if (!g_cfg.enableOptimizer || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                 dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    const bool pureRead = IsPureReadOnlyOpen(dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes);
    if (!pureRead) {
        std::wstring widePath;
        bool hasW = AnsiToWide(lpFileName, widePath);
        if (hasW) {
            InvalidateDirAndParent(widePath.c_str());
            InvalidateVpkIfNeededW(widePath.c_str());
        } else {
            ClearDirCacheAll();
            g_vfsGeneration.fetch_add(1, std::memory_order_acq_rel);
        }
        HANDLE hw = g_origCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                      dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
        if (hw != INVALID_HANDLE_VALUE && g_cfg.optWriteBuffer && hasW &&
            (dwDesiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA)) != 0 &&
            (dwFlagsAndAttributes & (FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_BACKUP_SEMANTICS)) == 0) {
            RegisterWriteBufferedHandle(hw, widePath);
        }
        return hw;
    }

    WIN32_FILE_ATTRIBUTE_DATA attr{};
    bool hasAttr = false;
    SplitPathStorage st;
    bool splitOk = false;

    if (g_cfg.optDirCache) {
        splitOk = SplitNormalizedAnsiPathFast(lpFileName, st);
        if (splitOk) {
            DirLookupResult r = LookupInDirCache(st.parent, st.leaf, &attr, st.hasHashes ? &st.parentHash : nullptr, st.hasHashes ? &st.leafHash : nullptr);
            if (r == DirLookupResult::PathNotFound) {
                g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                SetLastError(ERROR_PATH_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            if (r == DirLookupResult::FileNotFound) {
                g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            if (r == DirLookupResult::Found) {
                hasAttr = true;
            }
        }
    }

    DWORD effectiveFlags = dwFlagsAndAttributes;
    if (g_cfg.optStripNoBuffering) {
        if ((effectiveFlags & FILE_FLAG_NO_BUFFERING) != 0) {
            effectiveFlags &= ~FILE_FLAG_NO_BUFFERING;
        }
        if (splitOk && (effectiveFlags & FILE_FLAG_RANDOM_ACCESS) == 0 && st.leaf.size() >= 4 &&
            (st.leaf.compare(st.leaf.size() - 4, 4, L".bsp") == 0 ||
             st.leaf.compare(st.leaf.size() - 4, 4, L".gma") == 0)) {
            effectiveFlags |= FILE_FLAG_SEQUENTIAL_SCAN;
        }
    }
    HANDLE h = g_origCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                 dwCreationDisposition, effectiveFlags, hTemplateFile);
    if (h != INVALID_HANDLE_VALUE && splitOk) {
        RecordPrefetchFile(st.parent, st.leaf);
        if (g_cfg.optBspPakFastPath && g_sessionActive.load(std::memory_order_acquire) &&
            st.leaf.size() >= 4 &&
            st.leaf.compare(st.leaf.size() - 4, 4, L".bsp") == 0) {
            std::wstring widePath;
            if (AnsiToWide(lpFileName, widePath)) {
                IndexBspPakLump(widePath);
            }
        }
    }

    if (h != INVALID_HANDLE_VALUE && g_cfg.optReadBuffer &&
        (dwFlagsAndAttributes & (FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING)) == 0) {
        uint64_t fsize = 0;
        if (hasAttr && (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            fsize = (static_cast<uint64_t>(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow;
        } else if (g_origGetFileSizeEx) {
            LARGE_INTEGER li{};
            if (g_origGetFileSizeEx(h, &li) && li.QuadPart > 0) {
                fsize = static_cast<uint64_t>(li.QuadPart);
            }
        }
        if (fsize > 0) {
            RegisterBufferedHandle(h, fsize);
        }
    }
    if (h != INVALID_HANDLE_VALUE && (dwDesiredAccess & GENERIC_READ) != 0 &&
        (dwDesiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)) == 0 &&
        (dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED) == 0 && lpFileName && *lpFileName) {
        const bool isVpk = splitOk && st.leaf.size() >= 4 && st.leaf.compare(st.leaf.size() - 4, 4, L".vpk") == 0;
        const bool wantVpk = g_cfg.enableOptimizer && g_cfg.optVpkBlockCache && isVpk;
        const bool wantProf = g_cfg.profilerEnabled && g_sessionActive.load(std::memory_order_relaxed);
        if (wantVpk || wantProf) {
            uint64_t fsz = 0;
            if (hasAttr && (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                fsz = (static_cast<uint64_t>(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow;
            } else if (wantVpk && g_origGetFileSizeEx) {
                LARGE_INTEGER li{};
                if (g_origGetFileSizeEx(h, &li) && li.QuadPart > 0) fsz = static_cast<uint64_t>(li.QuadPart);
            }
            if (!hasAttr || (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                std::wstring wp;
                if (AnsiToWide(lpFileName, wp)) {
                    RegisterWin32ReadHandle(h, NormalizeWin32PathAscii(wp), wantVpk && fsz > 0, fsz);
                }
            }
        }
    }
    return h;
}

BOOL WINAPI HookedGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
    if (!g_origGetFileAttributesExW) return FALSE;
    if (g_cfg.enableOptimizer && g_cfg.optDirCache && !g_shuttingDown.load(std::memory_order_relaxed) &&
        fInfoLevelId == GetFileExInfoStandard && lpFileInformation) {
        SplitPathStorage st;
        if (SplitNormalizedWinPathFast(lpFileName, st)) {
            WIN32_FILE_ATTRIBUTE_DATA attr{};
            DirLookupResult r = LookupInDirCache(st.parent, st.leaf, &attr, st.hasHashes ? &st.parentHash : nullptr, st.hasHashes ? &st.leafHash : nullptr);
            if (r == DirLookupResult::Found) {
                g_optStats.attrCacheHits.fetch_add(1, std::memory_order_relaxed);
                std::memcpy(lpFileInformation, &attr, sizeof(attr));
                return TRUE;
            }
            if (r == DirLookupResult::PathNotFound) {
                g_optStats.attrCacheNegBlocked.fetch_add(1, std::memory_order_relaxed);
                SetLastError(ERROR_PATH_NOT_FOUND);
                return FALSE;
            }
            if (r == DirLookupResult::FileNotFound) {
                g_optStats.attrCacheNegBlocked.fetch_add(1, std::memory_order_relaxed);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return FALSE;
            }
        }
    }
    return g_origGetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
}

BOOL WINAPI HookedGetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
    if (!g_origGetFileAttributesExA) return FALSE;
    if (g_cfg.enableOptimizer && g_cfg.optDirCache && !g_shuttingDown.load(std::memory_order_relaxed) &&
        fInfoLevelId == GetFileExInfoStandard && lpFileInformation) {
        SplitPathStorage st;
        if (SplitNormalizedAnsiPathFast(lpFileName, st)) {
            {
                WIN32_FILE_ATTRIBUTE_DATA attr{};
                DirLookupResult r = LookupInDirCache(st.parent, st.leaf, &attr, st.hasHashes ? &st.parentHash : nullptr, st.hasHashes ? &st.leafHash : nullptr);
                if (r == DirLookupResult::Found) {
                    g_optStats.attrCacheHits.fetch_add(1, std::memory_order_relaxed);
                    std::memcpy(lpFileInformation, &attr, sizeof(attr));
                    return TRUE;
                }
                if (r == DirLookupResult::PathNotFound) {
                    g_optStats.attrCacheNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_PATH_NOT_FOUND);
                    return FALSE;
                }
                if (r == DirLookupResult::FileNotFound) {
                    g_optStats.attrCacheNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_FILE_NOT_FOUND);
                    return FALSE;
                }
            }
        }
    }
    return g_origGetFileAttributesExA(lpFileName, fInfoLevelId, lpFileInformation);
}

BOOL WINAPI HookedReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                           LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    if (!g_origReadFile) return FALSE;
    FlushWriteHandleIfAny(hFile, false);

    if (!g_shuttingDown.load(std::memory_order_relaxed) && lpOverlapped == nullptr &&
        hFile && hFile != INVALID_HANDLE_VALUE) {
        const bool vpkOn = g_cfg.enableOptimizer && g_cfg.optVpkBlockCache;
        const bool profOn = g_cfg.profilerEnabled && g_sessionActive.load(std::memory_order_relaxed);
        if (vpkOn || profOn) {
            thread_local HANDLE tl_lastWin32H = INVALID_HANDLE_VALUE;
            thread_local std::shared_ptr<Win32ReadTrack> tl_lastWin32Track;
            if (hFile != tl_lastWin32H) {
                tl_lastWin32H = hFile;
                tl_lastWin32Track = FindWin32ReadTrack(hFile);
                if (!tl_lastWin32Track) {
                    // Lazy one-time discovery for persistent .vpk/.gma handles opened before session/hook!
                    std::string p = QueryHandlePathAscii(hFile);
                    if (!p.empty()) {
                        bool isVpk = (p.size() >= 4 && p.compare(p.size() - 4, 4, ".vpk") == 0);
                        uint64_t fsz = 0;
                        if (isVpk && g_origGetFileSizeEx) {
                            LARGE_INTEGER li{};
                            if (g_origGetFileSizeEx(hFile, &li) && li.QuadPart > 0) fsz = static_cast<uint64_t>(li.QuadPart);
                        }
                        RegisterWin32ReadHandle(hFile, p, vpkOn && isVpk && fsz > 0, fsz);
                        tl_lastWin32Track = FindWin32ReadTrack(hFile);
                        if (tl_lastWin32Track && tl_lastWin32Track->isReadOnlyVpk) {
                            AcquireSRWLockExclusive(&tl_lastWin32Track->handleLock);
                            tl_lastWin32Track->posSynced = false;
                            ReleaseSRWLockExclusive(&tl_lastWin32Track->handleLock);
                        }
                    }
                }
            }

            // Layer 10: Read-Only VPK 1 MiB Block Cache (eliminates Source's 4-slot 4 MiB VPK thrashing!)
            if (vpkOn && tl_lastWin32Track && tl_lastWin32Track->isReadOnlyVpk &&
                nNumberOfBytesToRead > 0 && nNumberOfBytesToRead <= kVpkBlockSize && lpBuffer) {
                auto& tr = *tl_lastWin32Track;
                AcquireSRWLockExclusive(&tr.handleLock);
                if (!tr.posSynced && g_origSetFilePointerEx) {
                    LARGE_INTEGER zero{}, cur{};
                    if (g_origSetFilePointerEx(hFile, zero, &cur, FILE_CURRENT)) {
                        tr.logicalPos = static_cast<uint64_t>(cur.QuadPart);
                        tr.realPos = tr.logicalPos;
                        tr.posSynced = true;
                    }
                }
                if (tr.posSynced) {
                    if (tr.logicalPos >= tr.fileSize) {
                        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
                        ReleaseSRWLockExclusive(&tr.handleLock);
                        return TRUE;
                    }
                    const uint64_t remain = tr.fileSize - tr.logicalPos;
                    const DWORD avail = static_cast<DWORD>(std::min<uint64_t>(nNumberOfBytesToRead, remain));
                    const uint64_t blkStart = tr.logicalPos >> kVpkBlockShift;
                    const uint64_t blkEnd = (tr.logicalPos + avail - 1ULL) >> kVpkBlockShift;
                    if (blkStart == blkEnd) {
                        const uint64_t offInBlk = tr.logicalPos & (kVpkBlockSize - 1ULL);
                        auto cached = LookupVpkBlock(tr.vpkPathHash, blkStart);
                        if (cached && offInBlk + avail <= cached->validLen) {
                            std::memcpy(lpBuffer, cached->data.get() + offInBlk, avail);
                            tr.logicalPos += avail;
                            if (lpNumberOfBytesRead) *lpNumberOfBytesRead = avail;
                            ReleaseSRWLockExclusive(&tr.handleLock);
                            g_optStats.vpkBlockHits.fetch_add(1, std::memory_order_relaxed);
                            g_optStats.vpkBlockSavedBytes.fetch_add(avail, std::memory_order_relaxed);
                            return TRUE;
                        }
                        if (g_ramGuardLevel.load(std::memory_order_relaxed) == 0) {
                            // Cache miss: read full aligned 1 MiB block from disk once and store in LRU!
                            const uint64_t blockFileOff = blkStart << kVpkBlockShift;
                            const DWORD blockWant = static_cast<DWORD>(std::min<uint64_t>(kVpkBlockSize, tr.fileSize - blockFileOff));
                            auto newBlk = std::make_shared<VpkBlockBuf>();
                            newBlk->data = std::make_unique_for_overwrite<uint8_t[]>(blockWant);
                            int64_t t0 = profOn ? g_timer.NowTick() : 0;
                            bool seekOk = true;
                            if (tr.realPos != blockFileOff && g_origSetFilePointerEx) {
                                LARGE_INTEGER li{};
                                li.QuadPart = static_cast<LONGLONG>(blockFileOff);
                                seekOk = (g_origSetFilePointerEx(hFile, li, nullptr, FILE_BEGIN) != FALSE);
                                if (seekOk) tr.realPos = blockFileOff;
                            }
                            DWORD totalGot = 0;
                            if (seekOk) {
                                while (totalGot < blockWant) {
                                    DWORD got = 0;
                                    if (!g_origReadFile(hFile, newBlk->data.get() + totalGot, blockWant - totalGot, &got, nullptr) || got == 0) break;
                                    totalGot += got;
                                }
                                tr.realPos += totalGot;
                            }
                            int64_t t1 = profOn ? g_timer.NowTick() : 0;
                            if (profOn) {
                                double durUs = g_timer.DeltaUs(t0, t1);
                                uint8_t ph = g_currentPhase.load(std::memory_order_relaxed);
                                auto& st = tr.byPhase[ph];
                                st.calls += 1;
                                st.bytes += totalGot;
                                st.totalUs += durUs;
                                if (durUs > tr.maxUs) tr.maxUs = durUs;
                            }
                            if (totalGot >= offInBlk + avail) {
                                newBlk->validLen = totalGot;
                                std::memcpy(lpBuffer, newBlk->data.get() + offInBlk, avail);
                                tr.logicalPos += avail;
                                if (lpNumberOfBytesRead) *lpNumberOfBytesRead = avail;
                                ReleaseSRWLockExclusive(&tr.handleLock);
                                InsertVpkBlock(tr.vpkPathHash, blkStart, std::move(newBlk));
                                RecordVpkBlockMiss(tr.vpkPathHash, tr.path, blkStart);
                                g_optStats.vpkBlockMisses.fetch_add(1, std::memory_order_relaxed);
                                return TRUE;
                            }
                        }
                        // v16.3: Still record VPK block into prefetch manifest (~64 KB) even when RAM Guard skips caching
                        RecordVpkBlockMiss(tr.vpkPathHash, tr.path, blkStart);
                        // Fallback if partial read or RAM guard active: sync kernel pointer to logicalPos and let normal ReadFile handle
                        if (tr.realPos != tr.logicalPos && g_origSetFilePointerEx) {
                            LARGE_INTEGER li{};
                            li.QuadPart = static_cast<LONGLONG>(tr.logicalPos);
                            if (g_origSetFilePointerEx(hFile, li, nullptr, FILE_BEGIN)) tr.realPos = tr.logicalPos;
                        }
                    } else {
                        // Cross-block read on VPK: sync realPos to logicalPos before raw ReadFile
                        if (tr.realPos != tr.logicalPos && g_origSetFilePointerEx) {
                            LARGE_INTEGER li{};
                            li.QuadPart = static_cast<LONGLONG>(tr.logicalPos);
                            if (g_origSetFilePointerEx(hFile, li, nullptr, FILE_BEGIN)) tr.realPos = tr.logicalPos;
                        }
                    }
                    // Execute uncached read while holding handleLock to keep pos consistent
                    int64_t t0 = profOn ? g_timer.NowTick() : 0;
                    DWORD localGot = 0;
                    BOOL ok = g_origReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &localGot, nullptr);
                    int64_t t1 = profOn ? g_timer.NowTick() : 0;
                    if (ok) {
                        tr.logicalPos += localGot;
                        tr.realPos += localGot;
                    }
                    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = localGot;
                    if (profOn) {
                        double durUs = g_timer.DeltaUs(t0, t1);
                        uint8_t ph = g_currentPhase.load(std::memory_order_relaxed);
                        auto& st = tr.byPhase[ph];
                        st.calls += 1;
                        st.bytes += localGot;
                        st.totalUs += durUs;
                        if (durUs > tr.maxUs) tr.maxUs = durUs;
                    }
                    ReleaseSRWLockExclusive(&tr.handleLock);
                    return ok;
                }
                ReleaseSRWLockExclusive(&tr.handleLock);
            }

            // Standard Win32 ReadFile attribution when profiling is enabled and optReadBuffer is OFF
            if (profOn && (!g_cfg.enableOptimizer || !g_cfg.optReadBuffer)) {
                int64_t t0 = g_timer.NowTick();
                DWORD localGot = 0;
                BOOL ok = g_origReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &localGot, nullptr);
                int64_t t1 = g_timer.NowTick();
                if (lpNumberOfBytesRead) *lpNumberOfBytesRead = localGot;
                if (tl_lastWin32Track) {
                    double durUs = g_timer.DeltaUs(t0, t1);
                    uint8_t ph = g_currentPhase.load(std::memory_order_relaxed);
                    AcquireSRWLockExclusive(&tl_lastWin32Track->handleLock);
                    auto& st = tl_lastWin32Track->byPhase[ph];
                    st.calls += 1;
                    st.bytes += localGot;
                    st.totalUs += durUs;
                    if (durUs > tl_lastWin32Track->maxUs) tl_lastWin32Track->maxUs = durUs;
                    ReleaseSRWLockExclusive(&tl_lastWin32Track->handleLock);
                }
                return ok;
            }
        }
    }

    if (!g_cfg.enableOptimizer || !g_cfg.optReadBuffer || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    }

    auto bh = FindBufferedHandle(hFile);
    if (!bh) {
        return g_origReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    }

    if (lpOverlapped != nullptr) {
        DisableAndSyncBufferedHandle(hFile);
        return g_origReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    }

    AcquireSRWLockExclusive(&bh->lock);
    if (bh->disabled) {
        ReleaseSRWLockExclusive(&bh->lock);
        return g_origReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    }

    if (nNumberOfBytesToRead == 0 || bh->pos >= bh->fileSize) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        ReleaseSRWLockExclusive(&bh->lock);
        return TRUE;
    }

    const uint64_t remain = bh->fileSize - bh->pos;
    const DWORD avail = static_cast<DWORD>(std::min<uint64_t>(nNumberOfBytesToRead, remain));

    // Fast path: fully served from RAM buffer
    if (bh->bufLen > 0 && bh->pos >= bh->bufStart &&
        (bh->pos + avail) <= (bh->bufStart + bh->bufLen)) {
        std::memcpy(lpBuffer, bh->buf.data() + (bh->pos - bh->bufStart), avail);
        bh->pos += avail;
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = avail;
        ReleaseSRWLockExclusive(&bh->lock);
        g_optStats.readBufHits.fetch_add(1, std::memory_order_relaxed);
        return TRUE;
    }

    const uint32_t bufCap = g_cfg.readBufferSizeKb * 1024U;

    // Small read: refill 64KB (configurable) sliding buffer
    if (avail < bufCap) {
        if (bh->realPos != bh->pos && g_origSetFilePointerEx) {
            LARGE_INTEGER li{};
            li.QuadPart = static_cast<LONGLONG>(bh->pos);
            if (!g_origSetFilePointerEx(hFile, li, nullptr, FILE_BEGIN)) {
                ReleaseSRWLockExclusive(&bh->lock);
                return FALSE;
            }
            bh->realPos = bh->pos;
        }

        const size_t targetSize = static_cast<size_t>(std::min<uint64_t>(bufCap, bh->fileSize));
        if (bh->buf.size() < targetSize) {
            bh->buf.resize(targetSize);
        }

        DWORD toRead = static_cast<DWORD>(std::min<uint64_t>(bh->buf.size(), remain));
        DWORD got = 0;
        BOOL ok = g_origReadFile(hFile, bh->buf.data(), toRead, &got, nullptr);
        if (!ok) {
            ReleaseSRWLockExclusive(&bh->lock);
            return FALSE;
        }
        bh->bufStart = bh->pos;
        bh->bufLen = got;
        bh->realPos += got;

        DWORD copyN = std::min<DWORD>(avail, got);
        if (copyN > 0) {
            std::memcpy(lpBuffer, bh->buf.data(), copyN);
            bh->pos += copyN;
        }
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = copyN;
        ReleaseSRWLockExclusive(&bh->lock);
        g_optStats.readBufRefills.fetch_add(1, std::memory_order_relaxed);
        return TRUE;
    }

    // Large read (>= buffer size): read directly into caller buffer (zero extra copy)
    if (bh->realPos != bh->pos && g_origSetFilePointerEx) {
        LARGE_INTEGER li{};
        li.QuadPart = static_cast<LONGLONG>(bh->pos);
        if (!g_origSetFilePointerEx(hFile, li, nullptr, FILE_BEGIN)) {
            ReleaseSRWLockExclusive(&bh->lock);
            return FALSE;
        }
        bh->realPos = bh->pos;
    }
    DWORD got = 0;
    BOOL ok = g_origReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &got, nullptr);
    if (ok) {
        bh->pos += got;
        bh->realPos += got;
    }
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = got;
    ReleaseSRWLockExclusive(&bh->lock);
    return ok;
}

DWORD WINAPI HookedSetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    if (!g_origSetFilePointer) return INVALID_SET_FILE_POINTER;
    FlushWriteHandleIfAny(hFile, false);
    if (g_cfg.enableOptimizer && g_cfg.optVpkBlockCache && !g_shuttingDown.load(std::memory_order_relaxed)) {
        if (auto tr = FindWin32ReadTrack(hFile); tr && tr->isReadOnlyVpk) {
            AcquireSRWLockExclusive(&tr->handleLock);
            if (!tr->posSynced && g_origSetFilePointerEx) {
                LARGE_INTEGER zero{}, cur{};
                if (g_origSetFilePointerEx(hFile, zero, &cur, FILE_CURRENT)) {
                    tr->logicalPos = static_cast<uint64_t>(cur.QuadPart);
                    tr->realPos = tr->logicalPos;
                    tr->posSynced = true;
                }
            }
            if (tr->posSynced) {
                int64_t dist = lpDistanceToMoveHigh
                    ? ((static_cast<int64_t>(*lpDistanceToMoveHigh) << 32) | static_cast<uint32_t>(lDistanceToMove))
                    : static_cast<int64_t>(lDistanceToMove);
                int64_t newPos = 0;
                if (dwMoveMethod == FILE_BEGIN) newPos = dist;
                else if (dwMoveMethod == FILE_CURRENT) newPos = static_cast<int64_t>(tr->logicalPos) + dist;
                else if (dwMoveMethod == FILE_END) newPos = static_cast<int64_t>(tr->fileSize) + dist;
                else {
                    ReleaseSRWLockExclusive(&tr->handleLock);
                    return g_origSetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
                }
                if (newPos < 0) {
                    ReleaseSRWLockExclusive(&tr->handleLock);
                    SetLastError(ERROR_NEGATIVE_SEEK);
                    return INVALID_SET_FILE_POINTER;
                }
                tr->logicalPos = static_cast<uint64_t>(newPos);
                if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = static_cast<LONG>(tr->logicalPos >> 32);
                DWORD low = static_cast<DWORD>(tr->logicalPos & 0xFFFFFFFFULL);
                ReleaseSRWLockExclusive(&tr->handleLock);
                g_optStats.vpkSeekSaved.fetch_add(1, std::memory_order_relaxed);
                SetLastError(NO_ERROR);
                return low;
            }
            ReleaseSRWLockExclusive(&tr->handleLock);
        }
    }
    auto bh = (g_cfg.enableOptimizer && g_cfg.optReadBuffer && !g_shuttingDown.load(std::memory_order_relaxed))
              ? FindBufferedHandle(hFile) : nullptr;
    if (!bh) {
        return g_origSetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    }

    AcquireSRWLockExclusive(&bh->lock);
    if (bh->disabled) {
        ReleaseSRWLockExclusive(&bh->lock);
        return g_origSetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    }

    int64_t dist = 0;
    if (lpDistanceToMoveHigh) {
        dist = (static_cast<int64_t>(*lpDistanceToMoveHigh) << 32) | static_cast<uint32_t>(lDistanceToMove);
    } else {
        dist = static_cast<int64_t>(lDistanceToMove);
    }

    int64_t newPos = 0;
    if (dwMoveMethod == FILE_BEGIN) newPos = dist;
    else if (dwMoveMethod == FILE_CURRENT) newPos = static_cast<int64_t>(bh->pos) + dist;
    else if (dwMoveMethod == FILE_END) newPos = static_cast<int64_t>(bh->fileSize) + dist;
    else {
        ReleaseSRWLockExclusive(&bh->lock);
        return g_origSetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    }

    if (newPos < 0) {
        ReleaseSRWLockExclusive(&bh->lock);
        SetLastError(ERROR_NEGATIVE_SEEK);
        return INVALID_SET_FILE_POINTER;
    }

    bh->pos = static_cast<uint64_t>(newPos);
    if (lpDistanceToMoveHigh) {
        *lpDistanceToMoveHigh = static_cast<LONG>(bh->pos >> 32);
    }
    DWORD low = static_cast<DWORD>(bh->pos & 0xFFFFFFFFULL);
    ReleaseSRWLockExclusive(&bh->lock);
    g_optStats.seekSyscallsSaved.fetch_add(1, std::memory_order_relaxed);
    SetLastError(NO_ERROR);
    return low;
}

BOOL WINAPI HookedSetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod) {
    if (!g_origSetFilePointerEx) return FALSE;
    FlushWriteHandleIfAny(hFile, false);
    if (g_cfg.enableOptimizer && g_cfg.optVpkBlockCache && !g_shuttingDown.load(std::memory_order_relaxed)) {
        if (auto tr = FindWin32ReadTrack(hFile); tr && tr->isReadOnlyVpk) {
            AcquireSRWLockExclusive(&tr->handleLock);
            if (!tr->posSynced) {
                LARGE_INTEGER zero{}, cur{};
                if (g_origSetFilePointerEx(hFile, zero, &cur, FILE_CURRENT)) {
                    tr->logicalPos = static_cast<uint64_t>(cur.QuadPart);
                    tr->realPos = tr->logicalPos;
                    tr->posSynced = true;
                }
            }
            if (tr->posSynced) {
                int64_t newPos = 0;
                if (dwMoveMethod == FILE_BEGIN) newPos = liDistanceToMove.QuadPart;
                else if (dwMoveMethod == FILE_CURRENT) newPos = static_cast<int64_t>(tr->logicalPos) + liDistanceToMove.QuadPart;
                else if (dwMoveMethod == FILE_END) newPos = static_cast<int64_t>(tr->fileSize) + liDistanceToMove.QuadPart;
                else {
                    ReleaseSRWLockExclusive(&tr->handleLock);
                    return g_origSetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
                }
                if (newPos < 0) {
                    ReleaseSRWLockExclusive(&tr->handleLock);
                    SetLastError(ERROR_NEGATIVE_SEEK);
                    return FALSE;
                }
                tr->logicalPos = static_cast<uint64_t>(newPos);
                if (lpNewFilePointer) lpNewFilePointer->QuadPart = static_cast<LONGLONG>(tr->logicalPos);
                ReleaseSRWLockExclusive(&tr->handleLock);
                g_optStats.vpkSeekSaved.fetch_add(1, std::memory_order_relaxed);
                return TRUE;
            }
            ReleaseSRWLockExclusive(&tr->handleLock);
        }
    }
    auto bh = (g_cfg.enableOptimizer && g_cfg.optReadBuffer && !g_shuttingDown.load(std::memory_order_relaxed))
              ? FindBufferedHandle(hFile) : nullptr;
    if (!bh) {
        return g_origSetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    }

    AcquireSRWLockExclusive(&bh->lock);
    if (bh->disabled) {
        ReleaseSRWLockExclusive(&bh->lock);
        return g_origSetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    }

    int64_t newPos = 0;
    if (dwMoveMethod == FILE_BEGIN) newPos = liDistanceToMove.QuadPart;
    else if (dwMoveMethod == FILE_CURRENT) newPos = static_cast<int64_t>(bh->pos) + liDistanceToMove.QuadPart;
    else if (dwMoveMethod == FILE_END) newPos = static_cast<int64_t>(bh->fileSize) + liDistanceToMove.QuadPart;
    else {
        ReleaseSRWLockExclusive(&bh->lock);
        return g_origSetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    }

    if (newPos < 0) {
        ReleaseSRWLockExclusive(&bh->lock);
        SetLastError(ERROR_NEGATIVE_SEEK);
        return FALSE;
    }

    bh->pos = static_cast<uint64_t>(newPos);
    if (lpNewFilePointer) {
        lpNewFilePointer->QuadPart = static_cast<LONGLONG>(bh->pos);
    }
    ReleaseSRWLockExclusive(&bh->lock);
    g_optStats.seekSyscallsSaved.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}

DWORD WINAPI HookedGetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    if (!g_origGetFileSize) return INVALID_FILE_SIZE;
    FlushWriteHandleIfAny(hFile, false);
    auto bh = (g_cfg.enableOptimizer && g_cfg.optReadBuffer && !g_shuttingDown.load(std::memory_order_relaxed))
              ? FindBufferedHandle(hFile) : nullptr;
    if (bh) {
        AcquireSRWLockShared(&bh->lock);
        if (!bh->disabled) {
            uint64_t sz = bh->fileSize;
            ReleaseSRWLockShared(&bh->lock);
            if (lpFileSizeHigh) *lpFileSizeHigh = static_cast<DWORD>(sz >> 32);
            g_optStats.sizeSyscallsSaved.fetch_add(1, std::memory_order_relaxed);
            SetLastError(NO_ERROR);
            return static_cast<DWORD>(sz & 0xFFFFFFFFULL);
        }
        ReleaseSRWLockShared(&bh->lock);
    }
    return g_origGetFileSize(hFile, lpFileSizeHigh);
}

BOOL WINAPI HookedGetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
    if (!g_origGetFileSizeEx) return FALSE;
    FlushWriteHandleIfAny(hFile, false);
    auto bh = (g_cfg.enableOptimizer && g_cfg.optReadBuffer && !g_shuttingDown.load(std::memory_order_relaxed))
              ? FindBufferedHandle(hFile) : nullptr;
    if (bh && lpFileSize) {
        AcquireSRWLockShared(&bh->lock);
        if (!bh->disabled) {
            lpFileSize->QuadPart = static_cast<LONGLONG>(bh->fileSize);
            ReleaseSRWLockShared(&bh->lock);
            g_optStats.sizeSyscallsSaved.fetch_add(1, std::memory_order_relaxed);
            return TRUE;
        }
        ReleaseSRWLockShared(&bh->lock);
    }
    return g_origGetFileSizeEx(hFile, lpFileSize);
}

BOOL WINAPI HookedCloseHandle(HANDLE hObject) {
    if (g_cfg.optReadBuffer && hObject && hObject != INVALID_HANDLE_VALUE &&
        g_readBufCount.load(std::memory_order_acquire) > 0 && !g_shuttingDown.load(std::memory_order_relaxed)) {
        AcquireSRWLockExclusive(&g_readBufMapLock);
        if (g_readBufMap.erase(hObject) > 0) {
            g_readBufCount.fetch_sub(1, std::memory_order_release);
        }
        ReleaseSRWLockExclusive(&g_readBufMapLock);
    }
    if (hObject && hObject != INVALID_HANDLE_VALUE && !g_shuttingDown.load(std::memory_order_relaxed)) {
        auto& sh = g_win32TrackShards[Win32TrackShardIdx(hObject)];
        std::shared_ptr<Win32ReadTrack> removedTrack;
        AcquireSRWLockExclusive(&sh.lock);
        auto it = sh.map.find(hObject);
        if (it != sh.map.end()) {
            removedTrack = std::move(it->second);
            sh.map.erase(it);
        }
        ReleaseSRWLockExclusive(&sh.lock);
        if (removedTrack && g_cfg.profilerEnabled && g_sessionActive.load(std::memory_order_relaxed)) {
            FlushWin32TrackToSession(*removedTrack);
        }
    }
    std::wstring bspToIndex;
    if (hObject && hObject != INVALID_HANDLE_VALUE && g_writeBufCount.load(std::memory_order_acquire) > 0) {
        std::shared_ptr<WriteBufferedHandle> wb;
        AcquireSRWLockExclusive(&g_writeBufMapLock);
        auto it = g_writeBufMap.find(hObject);
        if (it != g_writeBufMap.end()) {
            wb = std::move(it->second);
            g_writeBufMap.erase(it);
            g_writeBufCount.fetch_sub(1, std::memory_order_release);
            g_writeBufCloseGen.fetch_add(1, std::memory_order_release);
        }
        ReleaseSRWLockExclusive(&g_writeBufMapLock);
        if (wb && tl_writeBufSlot.handle == hObject) {
            tl_writeBufSlot.handle = nullptr;
            tl_writeBufSlot.wb.reset();
        }
        if (wb) {
            AcquireSRWLockExclusive(&wb->lock);
            FlushWriteBufferLocked(hObject, *wb);
            if (wb->isBsp && !wb->fullPath.empty()) {
                bspToIndex = std::move(wb->fullPath);
            }
            ReleaseSRWLockExclusive(&wb->lock);
        }
    }
    BOOL ok = g_origCloseHandle ? g_origCloseHandle(hObject) : FALSE;
    if (ok && !bspToIndex.empty() && g_cfg.enableOptimizer && !g_shuttingDown.load(std::memory_order_relaxed)) {
        if (g_cfg.optMapPackCache) {
            OnMapPackBspWriteClosed(bspToIndex);
        }
        if (g_cfg.optBspPakFastPath) {
            IndexBspPakLump(bspToIndex, true);
        }
    }
    return ok;
}

HANDLE WINAPI HookedCreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttr, DWORD flProtect,
                                       DWORD dwMaxHigh, DWORD dwMaxLow, LPCSTR lpName) {
    DisableAndSyncBufferedHandle(hFile);
    FlushWriteHandleIfAny(hFile, true);
    return g_origCreateFileMappingA ? g_origCreateFileMappingA(hFile, lpAttr, flProtect, dwMaxHigh, dwMaxLow, lpName) : nullptr;
}

BOOL WINAPI HookedWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nBytes, LPDWORD lpWritten, LPOVERLAPPED lpOv) {
    if (!g_origWriteFile) return FALSE;
    if (lpOv != nullptr) {
        DisableAndSyncBufferedHandle(hFile);
        FlushWriteHandleIfAny(hFile, true);
        return g_origWriteFile(hFile, lpBuffer, nBytes, lpWritten, lpOv);
    }
    if (g_cfg.enableOptimizer && g_cfg.optWriteBuffer &&
        g_writeBufCount.load(std::memory_order_acquire) > 0 &&
        !g_shuttingDown.load(std::memory_order_relaxed)) {
        std::shared_ptr<WriteBufferedHandle> wb;
        const uint64_t wGen = g_writeBufCloseGen.load(std::memory_order_acquire);
        if (tl_writeBufSlot.handle == hFile && tl_writeBufSlot.gen == wGen && tl_writeBufSlot.wb) {
            wb = tl_writeBufSlot.wb;
        } else {
            DisableAndSyncBufferedHandle(hFile);
            wb = FindWriteBufferedHandle(hFile);
            if (wb) {
                tl_writeBufSlot.handle = hFile;
                tl_writeBufSlot.gen = wGen;
                tl_writeBufSlot.wb = wb;
            }
        }
        if (wb) {
            AcquireSRWLockExclusive(&wb->lock);
            if (!wb->disabled) {
                if (nBytes == 0) {
                    if (lpWritten) *lpWritten = 0;
                    ReleaseSRWLockExclusive(&wb->lock);
                    return TRUE;
                }
                if (!wb->isBsp && wb->bufUsed == 0) {
                    if (nBytes > 65536) {
                        ReleaseSRWLockExclusive(&wb->lock);
                        return g_origWriteFile(hFile, lpBuffer, nBytes, lpWritten, lpOv);
                    }
                    if (wb->smallWriteStreak < 2) {
                        ++wb->smallWriteStreak;
                        ReleaseSRWLockExclusive(&wb->lock);
                        return g_origWriteFile(hFile, lpBuffer, nBytes, lpWritten, lpOv);
                    }
                }
                if (nBytes >= kWriteBufBytes) {
                    if (!FlushWriteBufferLocked(hFile, *wb)) {
                        ReleaseSRWLockExclusive(&wb->lock);
                        return FALSE;
                    }
                    ReleaseSRWLockExclusive(&wb->lock);
                    return g_origWriteFile(hFile, lpBuffer, nBytes, lpWritten, lpOv);
                }
                if (wb->buf.size() < kWriteBufBytes) {
                    wb->buf.resize(kWriteBufBytes);
                }
                if (wb->bufUsed + nBytes > kWriteBufBytes) {
                    if (!FlushWriteBufferLocked(hFile, *wb)) {
                        ReleaseSRWLockExclusive(&wb->lock);
                        return FALSE;
                    }
                }
                std::memcpy(wb->buf.data() + wb->bufUsed, lpBuffer, nBytes);
                wb->bufUsed += nBytes;
                if (lpWritten) *lpWritten = nBytes;
                g_optStats.writeBufHits.fetch_add(1, std::memory_order_relaxed);
                g_optStats.writeBufBytes.fetch_add(nBytes, std::memory_order_relaxed);
                ReleaseSRWLockExclusive(&wb->lock);
                return TRUE;
            }
            ReleaseSRWLockExclusive(&wb->lock);
        }
    }
    DisableAndSyncBufferedHandle(hFile);
    return g_origWriteFile(hFile, lpBuffer, nBytes, lpWritten, lpOv);
}

BOOL WINAPI HookedSetEndOfFile(HANDLE hFile) {
    DisableAndSyncBufferedHandle(hFile);
    FlushWriteHandleIfAny(hFile, false);
    return g_origSetEndOfFile ? g_origSetEndOfFile(hFile) : FALSE;
}

BOOL WINAPI HookedFlushFileBuffers(HANDLE hFile) {
    DisableAndSyncBufferedHandle(hFile);
    FlushWriteHandleIfAny(hFile, false);
    return g_origFlushFileBuffers ? g_origFlushFileBuffers(hFile) : FALSE;
}

static bool TryParkMapPackInsteadOfDelete(const std::wstring& fullPathW) {
    if (!g_cfg.enableOptimizer || !g_cfg.optMapPackCache || fullPathW.empty()) return false;
    std::wstring lower = fullPathW;
    for (auto& c : lower) {
        if (c == L'/') c = L'\\';
        else if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    const std::wstring suf = L"\\cache\\map_pack.bsp";
    if (lower.size() < suf.size() || lower.compare(lower.size() - suf.size(), suf.size(), suf) != 0) {
        return false;
    }

    std::error_code ec;
    std::filesystem::path diskPath(fullPathW);
    if (!std::filesystem::exists(diskPath, ec)) return false;

    std::wstring metaPathW = fullPathW + L".optmeta";
    std::ifstream metaIn(metaPathW, std::ios::binary);
    if (!metaIn) return false;
    std::string ver, metaSrc;
    uint64_t metaSize = 0, metaMtime = 0, metaHead = 0, metaTail = 0;
    if (!std::getline(metaIn, ver) || (ver != "v11" && ver != "v12" && ver != "v13" && ver != "v14")) return false;
    if (!std::getline(metaIn, metaSrc)) return false;
    if (!(metaIn >> metaSize >> metaMtime >> metaHead >> metaTail)) return false;
    metaIn.close();
    while (!metaSrc.empty() && (metaSrc.back() == '\r' || metaSrc.back() == '\n')) metaSrc.pop_back();
    if (metaSrc.empty() || metaSize < 65536ULL) return false;

    uint64_t h = HashStrFnv1a64(metaSrc);
    h ^= metaSize * 1099511628211ULL;
    wchar_t keyBuf[32];
    std::swprintf(keyBuf, 32, L"%016llx", static_cast<unsigned long long>(h));

    std::filesystem::path parkDir = diskPath.parent_path() / L"opt_mappack";
    std::filesystem::create_directories(parkDir, ec);
    std::filesystem::path parkBsp = parkDir / (std::wstring(keyBuf) + L".bsp");
    std::filesystem::path parkMeta = parkDir / (std::wstring(keyBuf) + L".bsp.optmeta");

    if (::MoveFileExW(fullPathW.c_str(), parkBsp.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        ::MoveFileExW(metaPathW.c_str(), parkMeta.c_str(), MOVEFILE_REPLACE_EXISTING);
        // Prune to at most 3 parked map packs
        std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> parked;
        for (const auto& entry : std::filesystem::directory_iterator(parkDir, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == L".bsp") {
                parked.emplace_back(entry.last_write_time(ec), entry.path());
            }
        }
        if (parked.size() > 3) {
            std::sort(parked.begin(), parked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (size_t i = 0; i + 3 < parked.size(); ++i) {
                std::filesystem::remove(parked[i].second, ec);
                std::filesystem::remove(std::filesystem::path(parked[i].second.wstring() + L".optmeta"), ec);
            }
        }
        InvalidateDirAndParent(fullPathW.c_str());
        WriteStatusLog("Layer 9 MapPack Park-on-Delete: preserved " + metaSrc + " (" +
                       std::to_string(metaSize / (1024ULL * 1024ULL)) + " MB) in cache/opt_mappack/ (<1 ms).");
        return true;
    }
    return false;
}

BOOL WINAPI HookedDeleteFileA(LPCSTR lpFileName) {
    std::wstring w;
    const bool hasW = AnsiToWide(lpFileName, w);
    if (hasW && TryParkMapPackInsteadOfDelete(w)) return TRUE;
    BOOL ok = g_origDeleteFileA ? g_origDeleteFileA(lpFileName) : FALSE;
    if (ok) {
        DWORD lastErr = GetLastError();
        if (hasW) {
            InvalidateDirAndParent(w.c_str());
        } else {
            ClearDirCacheAll();
            g_vfsGeneration.fetch_add(1, std::memory_order_acq_rel);
            g_optStats.vfsGenBumps.fetch_add(1, std::memory_order_relaxed);
        }
        SetLastError(lastErr);
    }
    return ok;
}

BOOL WINAPI HookedDeleteFileW(LPCWSTR lpFileName) {
    if (lpFileName && *lpFileName && TryParkMapPackInsteadOfDelete(std::wstring(lpFileName))) {
        return TRUE;
    }
    BOOL ok = g_origDeleteFileW ? g_origDeleteFileW(lpFileName) : FALSE;
    if (ok) {
        DWORD lastErr = GetLastError();
        InvalidateDirAndParent(lpFileName);
        SetLastError(lastErr);
    }
    return ok;
}

BOOL WINAPI HookedCreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSec) {
    BOOL ok = g_origCreateDirectoryW ? g_origCreateDirectoryW(lpPathName, lpSec) : FALSE;
    if (ok) {
        DWORD lastErr = GetLastError();
        InvalidateDirAndParent(lpPathName);
        SetLastError(lastErr);
    }
    return ok;
}

BOOL WINAPI HookedRemoveDirectoryW(LPCWSTR lpPathName) {
    BOOL ok = g_origRemoveDirectoryW ? g_origRemoveDirectoryW(lpPathName) : FALSE;
    if (ok) {
        DWORD lastErr = GetLastError();
        InvalidateDirAndParent(lpPathName);
        SetLastError(lastErr);
    }
    return ok;
}

BOOL WINAPI HookedMoveFileExW(LPCWSTR lpExisting, LPCWSTR lpNew, DWORD dwFlags) {
    BOOL ok = g_origMoveFileExW ? g_origMoveFileExW(lpExisting, lpNew, dwFlags) : FALSE;
    if (ok) {
        DWORD lastErr = GetLastError();
        InvalidateDirAndParent(lpExisting);
        InvalidateDirAndParent(lpNew);
        SetLastError(lastErr);
    }
    return ok;
}

// Extra DLL dir-cache hooks for engine.dll, resources.dll, datacache.dll, materialsystem.dll, lua_shared.dll
FnCreateFileA g_origEngineCreateFileA = nullptr;
FnCreateFileW g_origEngineCreateFileW = nullptr;
FnCreateFileW g_origResourcesCreateFileW = nullptr;
FnCreateFileW g_origDatacacheCreateFileW = nullptr;
FnCreateFileW g_origMatSysCreateFileW = nullptr;
FnCreateFileW g_origLuaCreateFileW = nullptr;
HMODULE g_engineModForIat = nullptr;
HMODULE g_resourcesModForIat = nullptr;
HMODULE g_datacacheModForIat = nullptr;
HMODULE g_matSysModForIat = nullptr;
HMODULE g_luaModForIat = nullptr;
HMODULE g_tier0ModForIat = nullptr;

inline HANDLE DirCacheCreateFileWHelper(FnCreateFileW origFn, LPCWSTR lpFileName, DWORD dwDesiredAccess,
                                        DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSec, DWORD dwDisp,
                                        DWORD dwFlags, HANDLE hTpl) {
    if (!origFn) return INVALID_HANDLE_VALUE;
    SplitPathStorage st;
    bool splitOk = false;
    bool pureRead = false;
    if (g_cfg.enableOptimizer && g_cfg.optDirCache && !g_shuttingDown.load(std::memory_order_relaxed)) {
        pureRead = IsPureReadOnlyOpen(dwDesiredAccess, dwDisp, dwFlags);
        if (!pureRead) {
            InvalidateDirAndParent(lpFileName);
        } else {
            splitOk = SplitNormalizedWinPathFast(lpFileName, st);
            if (splitOk) {
                DirLookupResult r = LookupInDirCache(st.parent, st.leaf, nullptr, st.hasHashes ? &st.parentHash : nullptr, st.hasHashes ? &st.leafHash : nullptr);
                if (r == DirLookupResult::PathNotFound) {
                    g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_PATH_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
                if (r == DirLookupResult::FileNotFound) {
                    g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_FILE_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
            }
        }
    }
    HANDLE h = origFn(lpFileName, dwDesiredAccess, dwShareMode, lpSec, dwDisp, dwFlags, hTpl);
    if (h != INVALID_HANDLE_VALUE && pureRead && splitOk) {
        RecordPrefetchFile(st.parent, st.leaf);
    }
    return h;
}

HANDLE WINAPI HookedEngineCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                      LPSECURITY_ATTRIBUTES lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTpl) {
    if (!g_origEngineCreateFileA) return INVALID_HANDLE_VALUE;
    if (g_cfg.enableOptimizer && g_cfg.optDirCache && !g_shuttingDown.load(std::memory_order_relaxed)) {
        if (!IsPureReadOnlyOpen(dwDesiredAccess, dwDisp, dwFlags)) {
            std::wstring w;
            if (AnsiToWide(lpFileName, w)) InvalidateDirAndParent(w.c_str());
            else ClearDirCacheAll();
        } else {
            SplitPathStorage st;
            if (SplitNormalizedAnsiPathFast(lpFileName, st)) {
                DirLookupResult r = LookupInDirCache(st.parent, st.leaf, nullptr, st.hasHashes ? &st.parentHash : nullptr, st.hasHashes ? &st.leafHash : nullptr);
                if (r == DirLookupResult::PathNotFound) {
                    g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_PATH_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
                if (r == DirLookupResult::FileNotFound) {
                    g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_FILE_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
            }
        }
    }
    return g_origEngineCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSec, dwDisp, dwFlags, hTpl);
}

HANDLE WINAPI HookedEngineCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                      LPSECURITY_ATTRIBUTES lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTpl) {
    if (!g_origEngineCreateFileW) return INVALID_HANDLE_VALUE;
    if (g_cfg.enableOptimizer && g_cfg.optDirCache && !g_shuttingDown.load(std::memory_order_relaxed)) {
        if (!IsPureReadOnlyOpen(dwDesiredAccess, dwDisp, dwFlags)) {
            InvalidateDirAndParent(lpFileName);
        } else {
            SplitPathStorage st;
            if (SplitNormalizedWinPathFast(lpFileName, st)) {
                DirLookupResult r = LookupInDirCache(st.parent, st.leaf, nullptr, st.hasHashes ? &st.parentHash : nullptr, st.hasHashes ? &st.leafHash : nullptr);
                if (r == DirLookupResult::PathNotFound) {
                    g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_PATH_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
                if (r == DirLookupResult::FileNotFound) {
                    g_optStats.createFileNegBlocked.fetch_add(1, std::memory_order_relaxed);
                    SetLastError(ERROR_FILE_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
            }
        }
    }
    return g_origEngineCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSec, dwDisp, dwFlags, hTpl);
}

HANDLE WINAPI HookedResourcesCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                         LPSECURITY_ATTRIBUTES lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTpl) {
    return DirCacheCreateFileWHelper(g_origResourcesCreateFileW, lpFileName, dwDesiredAccess, dwShareMode, lpSec, dwDisp, dwFlags, hTpl);
}

HANDLE WINAPI HookedDatacacheCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                         LPSECURITY_ATTRIBUTES lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTpl) {
    return DirCacheCreateFileWHelper(g_origDatacacheCreateFileW, lpFileName, dwDesiredAccess, dwShareMode, lpSec, dwDisp, dwFlags, hTpl);
}

HANDLE WINAPI HookedMatSysCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                      LPSECURITY_ATTRIBUTES lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTpl) {
    return DirCacheCreateFileWHelper(g_origMatSysCreateFileW, lpFileName, dwDesiredAccess, dwShareMode, lpSec, dwDisp, dwFlags, hTpl);
}

HANDLE WINAPI HookedLuaCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                   LPSECURITY_ATTRIBUTES lpSec, DWORD dwDisp, DWORD dwFlags, HANDLE hTpl) {
    return DirCacheCreateFileWHelper(g_origLuaCreateFileW, lpFileName, dwDesiredAccess, dwShareMode, lpSec, dwDisp, dwFlags, hTpl);
}

// ============================================================================
// 6b. Optimizer Layer 5: Multi-Threaded Per-Map Prefetcher
//     (Parallel NTFS Dir Pre-Indexing + OS Page Cache Warm-Up)
// ============================================================================
std::atomic<bool> g_prefetchRecording{false};
std::atomic<uint64_t> g_prefetchGen{1};
std::atomic<uint64_t> g_prefetchRecGen{1};
SRWLOCK g_prefetchRecLock = SRWLOCK_INIT;
std::unordered_set<std::wstring, WideViewHash, WideViewEqual> g_recDirsSet;
std::vector<std::wstring> g_recDirsOrder;
std::unordered_set<std::wstring, WideViewHash, WideViewEqual> g_recFilesSet;
std::vector<std::wstring> g_recFilesOrder;

constexpr size_t kMaxRecordedVpkBlocks = 1024;
SRWLOCK g_vpkRecLock = SRWLOCK_INIT;
std::unordered_set<VpkBlockKey, VpkBlockKeyHash> g_recVpkBlocksSet;
std::vector<RecordedVpkBlock> g_recVpkBlocksOrder;
std::atomic<size_t> g_recVpkBlockCount{0};

void RecordVpkBlockMiss(uint64_t pathHash, const std::string& normPath, uint64_t blockIdx) {
    if (!g_prefetchRecording.load(std::memory_order_relaxed) ||
        !g_sessionActive.load(std::memory_order_relaxed) ||
        g_recVpkBlockCount.load(std::memory_order_relaxed) >= kMaxRecordedVpkBlocks ||
        normPath.empty()) {
        return;
    }
    VpkBlockKey key{pathHash, blockIdx};
    AcquireSRWLockExclusive(&g_vpkRecLock);
    if (g_recVpkBlocksOrder.size() < kMaxRecordedVpkBlocks &&
        g_recVpkBlocksSet.emplace(key).second) {
        g_recVpkBlocksOrder.push_back(RecordedVpkBlock{normPath, blockIdx});
        g_recVpkBlockCount.store(g_recVpkBlocksOrder.size(), std::memory_order_relaxed);
    }
    ReleaseSRWLockExclusive(&g_vpkRecLock);
}

std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int needed = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return "";
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), needed);
    return out;
}

inline bool IsPrefetchableAssetLeaf(std::wstring_view leaf) {
    auto endsWith = [&](std::wstring_view suf) {
        return leaf.size() >= suf.size() && leaf.substr(leaf.size() - suf.size()) == suf;
    };
    // v6: Intentionally exclude .vpk (100-200 MB sparse archives that waste SSD bandwidth & starve .vtf!)
    return endsWith(L".vtf") || endsWith(L".gma") || endsWith(L".bsp") ||
           endsWith(L".mdl") || endsWith(L".vtx") ||
           endsWith(L".vvd") || endsWith(L".vhv") || endsWith(L".vmt") ||
           endsWith(L".phy") || endsWith(L".lua") || endsWith(L".ani") ||
           endsWith(L".vcs") || endsWith(L".pcf");
}

inline bool WideEndsWith(std::wstring_view s, std::wstring_view suf) noexcept {
    return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
}

// Track .gma files already warmed at process startup so map prefetch doesn't re-read them
SRWLOCK g_startupGmaLock = SRWLOCK_INIT;
std::unordered_set<std::wstring, WideViewHash, WideViewEqual> g_startupGmaWarmedSet;
std::atomic<bool> g_startupGmaLaunched{false};

inline void RecordPrefetchDir(std::wstring_view dir) {
    if (!g_prefetchRecording.load(std::memory_order_relaxed) || dir.empty()) return;
    AcquireSRWLockShared(&g_prefetchRecLock);
    bool exists = (g_recDirsSet.find(dir) != g_recDirsSet.end());
    size_t sz = g_recDirsOrder.size();
    ReleaseSRWLockShared(&g_prefetchRecLock);
    if (exists || sz >= 16384) return;

    std::wstring owned(dir);
    AcquireSRWLockExclusive(&g_prefetchRecLock);
    if (g_recDirsOrder.size() < 16384 && g_recDirsSet.find(owned) == g_recDirsSet.end()) {
        g_recDirsSet.insert(owned);
        g_recDirsOrder.push_back(std::move(owned));
    }
    ReleaseSRWLockExclusive(&g_prefetchRecLock);
}

struct TlPrefetchSeenEntry {
    uint64_t h1 = 0;
    uint64_t h2 = 0;
    uint64_t gen = 0;
};
constexpr size_t kTlPrefetchSeenSlots = 4096;
thread_local TlPrefetchSeenEntry tl_recFileSeen[kTlPrefetchSeenSlots];

inline void RecordPrefetchFile(std::wstring_view parent, std::wstring_view leaf) {
    if (!g_prefetchRecording.load(std::memory_order_relaxed) || parent.empty() || leaf.empty()) return;
    if (leaf == L"map_pack.bsp" || leaf == L"map_pack.bsp.optmeta") return;
    if (!IsPrefetchableAssetLeaf(leaf)) return;

    // Fast lock-free TLS duplicate filter (avoids std::wstring heap alloc + SRWLOCK on repeat opens)
    const uint64_t rgen = g_prefetchRecGen.load(std::memory_order_acquire);
    const DualHash128 fh = CombineFileHash128(HashWide128(parent), HashWide128(leaf));
    auto& slot = tl_recFileSeen[fh.h1 & (kTlPrefetchSeenSlots - 1)];
    if (slot.gen == rgen && slot.h1 == fh.h1 && slot.h2 == fh.h2) return;
    slot.h1 = fh.h1;
    slot.h2 = fh.h2;
    slot.gen = rgen;

    std::wstring full;
    full.reserve(parent.size() + 1 + leaf.size());
    full.append(parent);
    if (full.back() != L'\\') full.push_back(L'\\');
    full.append(leaf);

    AcquireSRWLockShared(&g_prefetchRecLock);
    bool exists = (g_recFilesSet.find(full) != g_recFilesSet.end());
    size_t sz = g_recFilesOrder.size();
    ReleaseSRWLockShared(&g_prefetchRecLock);
    if (exists || sz >= 32768) return;

    AcquireSRWLockExclusive(&g_prefetchRecLock);
    if (g_recFilesOrder.size() < 32768 && g_recFilesSet.find(full) == g_recFilesSet.end()) {
        g_recFilesSet.insert(full);
        g_recFilesOrder.push_back(std::move(full));
    }
    ReleaseSRWLockExclusive(&g_prefetchRecLock);
}

void ResetPrefetchRecorder(bool enable) {
    g_prefetchRecGen.fetch_add(1, std::memory_order_acq_rel);
    AcquireSRWLockExclusive(&g_prefetchRecLock);
    g_recDirsSet.clear();
    g_recDirsOrder.clear();
    g_recFilesSet.clear();
    g_recFilesOrder.clear();
    ReleaseSRWLockExclusive(&g_prefetchRecLock);
    AcquireSRWLockExclusive(&g_vpkRecLock);
    g_recVpkBlocksSet.clear();
    g_recVpkBlocksOrder.clear();
    g_recVpkBlockCount.store(0, std::memory_order_relaxed);
    ReleaseSRWLockExclusive(&g_vpkRecLock);
    g_prefetchRecording.store(enable, std::memory_order_release);
}

void ExtractPrefetchRecorded(std::vector<std::wstring>& outDirs,
                             std::vector<std::wstring>& outFiles,
                             std::vector<RecordedVpkBlock>& outVpkBlocks) {
    g_prefetchRecording.store(false, std::memory_order_release);
    AcquireSRWLockExclusive(&g_prefetchRecLock);
    outDirs = std::move(g_recDirsOrder);
    outFiles = std::move(g_recFilesOrder);
    g_recDirsSet.clear();
    g_recDirsOrder.clear();
    g_recFilesSet.clear();
    g_recFilesOrder.clear();
    ReleaseSRWLockExclusive(&g_prefetchRecLock);
    AcquireSRWLockExclusive(&g_vpkRecLock);
    outVpkBlocks = std::move(g_recVpkBlocksOrder);
    g_recVpkBlocksSet.clear();
    g_recVpkBlocksOrder.clear();
    g_recVpkBlockCount.store(0, std::memory_order_relaxed);
    ReleaseSRWLockExclusive(&g_vpkRecLock);
}

void PreindexDirIfMissing(const std::wstring& parentDir) {
    if (!g_cfg.enableOptimizer || !g_cfg.optDirCache) return;
    auto& shard = g_dirShards[DirShardIndex(parentDir)];
    AcquireSRWLockShared(&shard.lock);
    bool present = (shard.map.find(parentDir) != shard.map.end());
    ReleaseSRWLockShared(&shard.lock);
    if (present) return;

    std::wstring pattern(parentDir);
    if (!pattern.empty() && pattern.back() != L'\\') pattern.push_back(L'\\');
    pattern.push_back(L'*');

    WIN32_FIND_DATAW ffd{};
    HANDLE hFind = ::FindFirstFileExW(
        pattern.c_str(),
        FindExInfoBasic,
        &ffd,
        FindExSearchNameMatch,
        nullptr,
        FIND_FIRST_EX_LARGE_FETCH);

    DirSnapshot snap;
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND || err == ERROR_DIRECTORY) {
            snap.exists = false;
        } else {
            return;
        }
    } else {
        snap.exists = true;
        do {
            const wchar_t* fn = ffd.cFileName;
            if (fn[0] == L'.' && (fn[1] == L'\0' || (fn[1] == L'.' && fn[2] == L'\0'))) {
                continue;
            }
            std::wstring key(fn);
            LowerWideInPlace(key);
            WIN32_FILE_ATTRIBUTE_DATA ad{};
            ad.dwFileAttributes = ffd.dwFileAttributes;
            ad.ftCreationTime   = ffd.ftCreationTime;
            ad.ftLastAccessTime = ffd.ftLastAccessTime;
            ad.ftLastWriteTime  = ffd.ftLastWriteTime;
            ad.nFileSizeHigh    = ffd.nFileSizeHigh;
            ad.nFileSizeLow     = ffd.nFileSizeLow;
            snap.entries.emplace(std::move(key), ad);
        } while (::FindNextFileW(hFind, &ffd));
        ::FindClose(hFind);
    }

    AcquireSRWLockExclusive(&shard.lock);
    if (shard.map.size() < kMaxCachedDirsPerShard && shard.map.find(parentDir) == shard.map.end()) {
        shard.map.emplace(parentDir, std::move(snap));
        g_optStats.prefetchDirsIndexed.fetch_add(1, std::memory_order_relaxed);
    }
    ReleaseSRWLockExclusive(&shard.lock);
}

struct PrefetchJob {
    uint64_t gen = 0;
    uint64_t maxBytes = 0;
    std::vector<std::wstring> dirs;
    std::vector<std::wstring> files;
    std::atomic<size_t> dirCursor{0};
    std::atomic<size_t> fileCursor{0};
    std::atomic<uint64_t> bytesRead{0};
};

struct PrefetchWorkerArg {
    std::shared_ptr<PrefetchJob> job;
    bool filesFirst = false;
};

void WarmSingleFileOSCache(const std::wstring& path, std::vector<uint8_t>& buf,
                           uint64_t maxCapBytes, bool isStartupGma,
                           std::atomic<uint64_t>* jobBytesRead, uint64_t jobMaxBytes, uint64_t jobGen) {
    // Always skip .vpk (huge sparse archives that starve .vtf and pollute page cache)
    if (WideEndsWith(path, L".vpk")) return;

    const bool isGma = WideEndsWith(path, L".gma");
    if (isGma && !isStartupGma) {
        // Skip .gma if startup pre-warmer already loaded it into OS Page Cache
        AcquireSRWLockShared(&g_startupGmaLock);
        bool alreadyWarmed = (g_startupGmaWarmedSet.find(path) != g_startupGmaWarmedSet.end());
        ReleaseSRWLockShared(&g_startupGmaLock);
        if (alreadyWarmed) return;
    }

    HANDLE h = ::CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER fsz{};
    if (::GetFileSizeEx(h, &fsz) && fsz.QuadPart > 0) {
        uint64_t targetBytes = static_cast<uint64_t>(fsz.QuadPart);
        if (isGma) {
            // v13: Configurable menu-idle .gma cap (default 64 MB, yields immediately on map load!)
            const uint64_t gmaCap = static_cast<uint64_t>(g_cfg.startupGmaMaxFileMb) * 1024ULL * 1024ULL;
            if (targetBytes > gmaCap) targetBytes = gmaCap;
            if (isStartupGma) {
                const uint64_t totalBudget = static_cast<uint64_t>(g_cfg.startupGmaMaxTotalMb) * 1024ULL * 1024ULL;
                if (g_optStats.startupGmaBytesWarmed.load(std::memory_order_relaxed) >= totalBudget) {
                    ::CloseHandle(h);
                    return;
                }
            }
        } else if (WideEndsWith(path, L".bsp")) {
            // v13: Allow large .bsp files up to 512 MB to warm OS Page Cache
            const uint64_t bspCap = 512ULL * 1024ULL * 1024ULL;
            if (targetBytes > bspCap) targetBytes = bspCap;
        } else if (targetBytes > maxCapBytes) {
            ::CloseHandle(h);
            return;
        }

        uint64_t done = 0;
        uint64_t checkAccum = 0;
        while (done < targetBytes) {
            if (checkAccum >= 2ULL * 1024ULL * 1024ULL) {
                checkAccum = 0;
                if (g_shuttingDown.load(std::memory_order_relaxed) ||
                    g_ramGuardLevel.load(std::memory_order_relaxed) != 0) break;
                if (isStartupGma) {
                    // v8: Never compete with active map loading on SSD!
                    if (g_sessionActive.load(std::memory_order_acquire)) break;
                } else {
                    if (g_prefetchGen.load(std::memory_order_acquire) != jobGen) break;
                    if (jobBytesRead && jobBytesRead->load(std::memory_order_relaxed) >= jobMaxBytes) break;
                }
            }
            DWORD want = static_cast<DWORD>(std::min<uint64_t>(buf.size(), targetBytes - done));
            DWORD got = 0;
            if (!::ReadFile(h, buf.data(), want, &got, nullptr) || got == 0) break;
            done += got;
            checkAccum += got;
        }
        if (done > 0) {
            if (isStartupGma) {
                g_optStats.startupGmaFilesWarmed.fetch_add(1, std::memory_order_relaxed);
                g_optStats.startupGmaBytesWarmed.fetch_add(done, std::memory_order_relaxed);
                AcquireSRWLockExclusive(&g_startupGmaLock);
                g_startupGmaWarmedSet.insert(path);
                ReleaseSRWLockExclusive(&g_startupGmaLock);
            } else {
                if (jobBytesRead) jobBytesRead->fetch_add(done, std::memory_order_relaxed);
                g_optStats.prefetchFilesWarmed.fetch_add(1, std::memory_order_relaxed);
                g_optStats.prefetchBytesWarmed.fetch_add(done, std::memory_order_relaxed);
            }
        }
    }
    ::CloseHandle(h);
}

DWORD WINAPI PrefetchWorkerProc(LPVOID param) {
    try {
    std::unique_ptr<PrefetchWorkerArg> arg(static_cast<PrefetchWorkerArg*>(param));
    if (!arg || !arg->job) return 0;
    std::shared_ptr<PrefetchJob> job = arg->job;

    if (!SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN)) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    }

    auto runDirs = [&]() {
        while (!g_shuttingDown.load(std::memory_order_relaxed) &&
               g_prefetchGen.load(std::memory_order_acquire) == job->gen) {
            size_t i = job->dirCursor.fetch_add(1, std::memory_order_relaxed);
            if (i >= job->dirs.size()) break;
            PreindexDirIfMissing(job->dirs[i]);
        }
    };

    auto runFiles = [&]() {
        std::vector<uint8_t> buf(256 * 1024);
        while (!g_shuttingDown.load(std::memory_order_relaxed) &&
               g_prefetchGen.load(std::memory_order_acquire) == job->gen) {
            while (g_ramGuardLevel.load(std::memory_order_relaxed) != 0 &&
                   !g_shuttingDown.load(std::memory_order_relaxed) &&
                   g_prefetchGen.load(std::memory_order_acquire) == job->gen) {
                ::Sleep(250);
            }
            if (g_shuttingDown.load(std::memory_order_relaxed) ||
                g_prefetchGen.load(std::memory_order_acquire) != job->gen) break;
            if (job->bytesRead.load(std::memory_order_relaxed) >= job->maxBytes) break;
            size_t i = job->fileCursor.fetch_add(1, std::memory_order_relaxed);
            if (i >= job->files.size()) break;
            WarmSingleFileOSCache(job->files[i], buf, 128ULL * 1024ULL * 1024ULL, false,
                                  &job->bytesRead, job->maxBytes, job->gen);
        }
    };

    // Split pipeline: half of workers start on files (.bsp/.vtf/.gma) immediately,
    // half start on NTFS dir pre-indexing, then both help finish the remaining queue!
    if (arg->filesFirst) {
        runFiles();
        runDirs();
    } else {
        runDirs();
        runFiles();
    }
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    } catch (...) {}
    return 0;
}

struct StartupGmaJob {
    std::vector<std::wstring> gmaFiles;
    std::atomic<size_t> cursor{0};
};

DWORD WINAPI StartupGmaWorkerProc(LPVOID param) {
    std::unique_ptr<std::shared_ptr<StartupGmaJob>> holder(static_cast<std::shared_ptr<StartupGmaJob>*>(param));
    if (!holder || !(*holder)) return 0;
    std::shared_ptr<StartupGmaJob> job = *holder;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    std::vector<uint8_t> buf(256 * 1024);
    const uint64_t gmaCapBytes = static_cast<uint64_t>(g_cfg.startupGmaMaxFileMb) * 1024ULL * 1024ULL;
    const uint64_t gmaTotalBudget = static_cast<uint64_t>(g_cfg.startupGmaMaxTotalMb) * 1024ULL * 1024ULL;
    while (!g_shuttingDown.load(std::memory_order_relaxed) &&
           !g_sessionActive.load(std::memory_order_acquire) &&
           g_optStats.startupGmaBytesWarmed.load(std::memory_order_relaxed) < gmaTotalBudget) {
        while (g_ramGuardLevel.load(std::memory_order_relaxed) != 0 &&
               !g_shuttingDown.load(std::memory_order_relaxed) &&
               !g_sessionActive.load(std::memory_order_acquire)) {
            ::Sleep(250);
        }
        if (g_shuttingDown.load(std::memory_order_relaxed) ||
            g_sessionActive.load(std::memory_order_acquire)) break;
        size_t i = job->cursor.fetch_add(1, std::memory_order_relaxed);
        if (i >= job->gmaFiles.size()) break;
        WarmSingleFileOSCache(job->gmaFiles[i], buf, gmaCapBytes, true, nullptr, 0, 0);
    }
    return 0;
}

void LaunchStartupGmaWarmup() {
    if (!g_cfg.enableOptimizer || !g_cfg.optPrefetch || !g_cfg.optStartupGmaWarm) return;
    bool expected = false;
    if (!g_startupGmaLaunched.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

    std::filesystem::path gmaListPath = g_logDir / L"prefetch_gma.txt";
    std::error_code ec;
    auto job = std::make_shared<StartupGmaJob>();
    std::unordered_set<std::wstring, WideViewHash, WideViewEqual> seen;

    auto collectFromFile = [&](const std::filesystem::path& p) {
        std::ifstream in(p, std::ios::binary);
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            std::string raw = (line.size() >= 3 && line[1] == '|') ? line.substr(2) : line;
            if (raw.size() < 4) continue;
            if (raw.compare(raw.size() - 4, 4, ".gma") != 0) continue;
            std::wstring w = Utf8ToWide(raw);
            if (!w.empty() && seen.insert(w).second) {
                job->gmaFiles.push_back(std::move(w));
            }
        }
    };

    if (std::filesystem::exists(gmaListPath, ec)) {
        collectFromFile(gmaListPath);
    } else {
        // Bootstrap from any existing v5 prefetch_*.txt manifest on first v6 launch!
        for (const auto& entry : std::filesystem::directory_iterator(g_logDir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string fn = entry.path().filename().string();
            if (fn.rfind("prefetch_", 0) == 0 && fn.ends_with(".txt")) {
                collectFromFile(entry.path());
            }
        }
    }

    if (job->gmaFiles.empty()) return;

    WriteStatusLog("Layer 5 Startup GMA Warm-Up v16 launched: " + std::to_string(job->gmaFiles.size()) +
                   " workshop .gma files (" + std::to_string(g_cfg.startupGmaMaxFileMb) + "MB cap each, " +
                   std::to_string(g_cfg.startupGmaMaxTotalMb) + "MB total, auto-yield on load) on 2 background threads.");
    for (int t = 0; t < 2; ++t) {
        auto* arg = new std::shared_ptr<StartupGmaJob>(job);
        HANDLE h = CreateThread(nullptr, 0, StartupGmaWorkerProc, arg, 0, nullptr);
        if (h) CloseHandle(h);
        else delete arg;
    }
}

std::string SanitizeFilename(const std::string& s);

std::atomic<uint64_t> g_vpkPrefetchGen{0};

struct VpkPrefetchArg {
    std::string mapName;
    std::filesystem::path manifestPath;
    uint64_t gen = 0;
};

DWORD WINAPI VpkPrefetchWorkerProc(LPVOID param) {
    try {
    std::unique_ptr<VpkPrefetchArg> arg(static_cast<VpkPrefetchArg*>(param));
    if (!arg) return 0;

    if (!SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN)) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    }

    const size_t maxBlocks = std::max<size_t>(
        16, (static_cast<size_t>(g_cfg.vpkBlockCacheMb) * 1024ULL * 1024ULL) / kVpkBlockSize);

    struct VpkGroup {
        std::string normPath;
        uint64_t pathHash = 0;
        std::vector<uint64_t> blocks;
    };
    std::vector<VpkGroup> groups;
    std::unordered_map<std::string, size_t> groupIdx;
    size_t totalPlanned = 0;

    {
        std::ifstream in(arg->manifestPath, std::ios::binary);
        std::string line;
        while (totalPlanned < maxBlocks && std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            size_t sep = line.rfind('|');
            if (sep == std::string::npos || sep == 0 || sep + 1 >= line.size()) continue;
            std::string p = line.substr(0, sep);
            uint64_t idx = std::strtoull(line.c_str() + sep + 1, nullptr, 10);
            auto it = groupIdx.find(p);
            if (it == groupIdx.end()) {
                size_t gi = groups.size();
                groupIdx.emplace(p, gi);
                uint64_t h = HashStrFnv1a64(p);
                groups.push_back(VpkGroup{std::move(p), h, {idx}});
            } else {
                groups[it->second].blocks.push_back(idx);
            }
            ++totalPlanned;
        }
    }

    size_t warmedBlocks = 0;
    size_t skippedCached = 0;

    for (auto& grp : groups) {
        while (g_ramGuardLevel.load(std::memory_order_relaxed) != 0 &&
               !g_shuttingDown.load(std::memory_order_relaxed) &&
               g_vpkPrefetchGen.load(std::memory_order_acquire) == arg->gen) {
            ::Sleep(250);
        }
        if (g_shuttingDown.load(std::memory_order_relaxed) ||
            g_vpkPrefetchGen.load(std::memory_order_acquire) != arg->gen) break;

        std::sort(grp.blocks.begin(), grp.blocks.end());
        grp.blocks.erase(std::unique(grp.blocks.begin(), grp.blocks.end()), grp.blocks.end());

        bool needAny = false;
        for (uint64_t blkIdx : grp.blocks) {
            if (!HasVpkBlock(grp.pathHash, blkIdx)) { needAny = true; break; }
        }
        if (!needAny) {
            skippedCached += grp.blocks.size();
            continue;
        }

        std::wstring widePath = Utf8ToWide(grp.normPath);
        if (widePath.empty()) continue;
        for (wchar_t& c : widePath) { if (c == L'/') c = L'\\'; }

        HANDLE h = ::CreateFileW(
            widePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        LARGE_INTEGER fsz{};
        if (::GetFileSizeEx(h, &fsz) && fsz.QuadPart > 0) {
            const uint64_t fileSize = static_cast<uint64_t>(fsz.QuadPart);
            uint64_t curPos = 0;

            for (uint64_t blkIdx : grp.blocks) {
                while (g_ramGuardLevel.load(std::memory_order_relaxed) != 0 &&
                       !g_shuttingDown.load(std::memory_order_relaxed) &&
                       g_vpkPrefetchGen.load(std::memory_order_acquire) == arg->gen) {
                    ::Sleep(250);
                }
                if (g_shuttingDown.load(std::memory_order_relaxed) ||
                    g_vpkPrefetchGen.load(std::memory_order_acquire) != arg->gen) break;

                if (HasVpkBlock(grp.pathHash, blkIdx)) {
                    ++skippedCached;
                    continue;
                }

                const uint64_t blockFileOff = blkIdx << kVpkBlockShift;
                if (blockFileOff >= fileSize) continue;
                const DWORD blockWant = static_cast<DWORD>(
                    std::min<uint64_t>(kVpkBlockSize, fileSize - blockFileOff));

                if (curPos != blockFileOff) {
                    LARGE_INTEGER li{};
                    li.QuadPart = static_cast<LONGLONG>(blockFileOff);
                    if (!::SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) continue;
                    curPos = blockFileOff;
                }

                auto newBlk = std::make_shared<VpkBlockBuf>();
                newBlk->data = std::make_unique_for_overwrite<uint8_t[]>(blockWant);

                DWORD totalGot = 0;
                while (totalGot < blockWant) {
                    DWORD got = 0;
                    if (!::ReadFile(h, newBlk->data.get() + totalGot, blockWant - totalGot, &got, nullptr) || got == 0) {
                        break;
                    }
                    totalGot += got;
                }
                curPos += totalGot;

                if (totalGot > 0) {
                    newBlk->validLen = totalGot;
                    InsertVpkBlock(grp.pathHash, blkIdx, std::move(newBlk));
                    ++warmedBlocks;
                }
            }
        }
        ::CloseHandle(h);
    }

    if (warmedBlocks > 0 || skippedCached > 0) {
        WriteStatusLog("Layer 10 VPK Prewarm finished for " + arg->mapName + ": warmed=" +
                       std::to_string(warmedBlocks) + " blocks, already_cached=" +
                       std::to_string(skippedCached) + " blocks.");
    }

    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    } catch (...) {}
    return 0;
}

void LaunchVpkBlockPrewarm(const std::string& mapName) {
    if (!g_cfg.enableOptimizer || !g_cfg.optPrefetch || !g_cfg.optVpkBlockCache ||
        mapName.empty() || mapName == "unknown_map") return;

    std::filesystem::path vpkManifest = g_logDir / ("vpk_prefetch_" + SanitizeFilename(mapName) + ".txt");
    std::error_code ec;
    if (!std::filesystem::exists(vpkManifest, ec)) return;

    uint64_t gen = g_vpkPrefetchGen.fetch_add(1, std::memory_order_acq_rel) + 1;
    auto* arg = new VpkPrefetchArg{mapName, std::move(vpkManifest), gen};
    HANDLE h = CreateThread(nullptr, 0, VpkPrefetchWorkerProc, arg, 0, nullptr);
    if (h) CloseHandle(h);
    else delete arg;
}

void CancelActivePrefetch() {
    g_prefetchGen.fetch_add(1, std::memory_order_acq_rel);
    g_vpkPrefetchGen.fetch_add(1, std::memory_order_acq_rel);
}

void LaunchMapPrefetch(const std::string& mapName) {
    if (!g_cfg.enableOptimizer || !g_cfg.optPrefetch || mapName.empty() || mapName == "unknown_map") return;
    LaunchVpkBlockPrewarm(mapName);

    std::filesystem::path manifestPath = g_logDir / ("prefetch_" + SanitizeFilename(mapName) + ".txt");
    std::error_code ec;
    if (!std::filesystem::exists(manifestPath, ec)) {
        WriteStatusLog("Layer 5 Prefetch: no manifest yet for " + mapName + " (recording this load for next launch).");
        return;
    }

    auto job = std::make_shared<PrefetchJob>();
    job->gen = g_prefetchGen.fetch_add(1, std::memory_order_acq_rel) + 1;
    job->maxBytes = static_cast<uint64_t>(g_cfg.prefetchMaxMb) * 1024ULL * 1024ULL;

    std::vector<std::wstring> prioGma, prioBsp, prioVtf, prioOther;
    {
        std::ifstream in(manifestPath, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (line.size() < 3 || line[1] != '|') continue;
            if (line[0] == 'D') {
                std::wstring w = Utf8ToWide(line.substr(2));
                if (!w.empty()) job->dirs.push_back(std::move(w));
            } else if (line[0] == 'F') {
                std::wstring w = Utf8ToWide(line.substr(2));
                if (w.empty() || WideEndsWith(w, L".vpk")) continue;
                if (WideEndsWith(w, L".gma")) prioGma.push_back(std::move(w));
                else if (WideEndsWith(w, L".bsp")) prioBsp.push_back(std::move(w));
                else if (WideEndsWith(w, L".vtf")) prioVtf.push_back(std::move(w));
                else prioOther.push_back(std::move(w));
            }
        }
    }

    // Optimal warm-up priority order: .gma (skipped if startup-warmed) -> .bsp -> .vtf -> .mdl/.vtx/.vvd/.lua/.vmt
    job->files.reserve(prioGma.size() + prioBsp.size() + prioVtf.size() + prioOther.size());
    for (auto& p : prioGma) job->files.push_back(std::move(p));
    for (auto& p : prioBsp) job->files.push_back(std::move(p));
    for (auto& p : prioVtf) job->files.push_back(std::move(p));
    for (auto& p : prioOther) job->files.push_back(std::move(p));

    if (job->dirs.empty() && job->files.empty()) return;

    const uint32_t numThreads = std::clamp<uint32_t>(g_cfg.prefetchThreads, 1, 8);
    WriteStatusLog("Layer 5 Prefetch v3 launched for " + mapName + ": " +
                   std::to_string(job->dirs.size()) + " dirs, " +
                   std::to_string(job->files.size()) + " files (vtf=" + std::to_string(prioVtf.size()) +
                   ", gma=" + std::to_string(prioGma.size()) + ", .vpk skipped) on " +
                   std::to_string(numThreads) + " split-pipeline threads.");

    for (uint32_t t = 0; t < numThreads; ++t) {
        // v8: Balanced 50/50 split pipeline (2 files-first + 2 dirs-first on default 4T)
        auto* arg = new PrefetchWorkerArg{job, (t % 2 == 1)};
        HANDLE h = CreateThread(nullptr, 0, PrefetchWorkerProc, arg, 0, nullptr);
        if (h) {
            CloseHandle(h);
        } else {
            delete arg;
        }
    }
}

void SavePrefetchManifest(const std::string& mapName,
                          const std::vector<std::wstring>& dirs,
                          const std::vector<std::wstring>& files) {
    if (!g_cfg.enableOptimizer || !g_cfg.optPrefetch || mapName.empty() || mapName == "unknown_map") return;
    if (dirs.empty() && files.empty()) return;

    std::filesystem::path finalPath = g_logDir / ("prefetch_" + SanitizeFilename(mapName) + ".txt");
    std::filesystem::path tmpPath = g_logDir / ("prefetch_" + SanitizeFilename(mapName) + ".tmp");

    // v6: Merge with existing manifest so pre-indexed dirs/files are NEVER lost on Run 2+!
    std::unordered_set<std::wstring, WideViewHash, WideViewEqual> seenDirs, seenFiles;
    std::vector<std::wstring> mergedDirs, mergedFiles;
    std::vector<std::wstring> gmaFiles;

    auto isSanePrefetchPath = [](const std::wstring& p) -> bool {
        if (p.empty() || p.size() > 260) return false;
        if (WideEndsWith(p, L"\\map_pack.bsp") || WideEndsWith(p, L"\\map_pack.bsp.optmeta")) return false;
        // Reject malformed doubled segments like ...\models\props_x\models\props_x
        size_t pos1 = p.find(L"\\models\\");
        if (pos1 != std::wstring::npos && p.find(L"\\models\\", pos1 + 8) != std::wstring::npos) return false;
        return true;
    };
    auto addDir = [&](const std::wstring& d) {
        if (isSanePrefetchPath(d) && mergedDirs.size() < 16384 && seenDirs.insert(d).second) {
            mergedDirs.push_back(d);
        }
    };
    auto addFile = [&](const std::wstring& f) {
        if (!isSanePrefetchPath(f) || WideEndsWith(f, L".vpk")) return;
        if (mergedFiles.size() < 32768 && seenFiles.insert(f).second) {
            mergedFiles.push_back(f);
            if (WideEndsWith(f, L".gma")) gmaFiles.push_back(f);
        }
    };

    // v13 Fix: ALWAYS merge previous per-map manifest so pre-indexed dirs (which hit DirCache and aren't re-recorded)
    // are NEVER wiped out! Prioritize newly recorded dirs/files first, then retain previous entries up to cap.
    for (const auto& d : dirs) addDir(d);
    for (const auto& f : files) addFile(f);
    {
        std::ifstream prev(finalPath, std::ios::binary);
        std::string line;
        while (std::getline(prev, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (line.size() < 3 || line[1] != '|') continue;
            if (line[0] == 'D') addDir(Utf8ToWide(line.substr(2)));
            else if (line[0] == 'F') addFile(Utf8ToWide(line.substr(2)));
        }
    }

    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) return;
        for (const auto& d : mergedDirs) {
            out << "D|" << WideToUtf8(d) << "\n";
        }
        for (const auto& f : mergedFiles) {
            out << "F|" << WideToUtf8(f) << "\n";
        }
    }
    ::MoveFileExW(tmpPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING);

    // Also merge shared prefetch_gma.txt for early startup .gma warm-up on next launch (never lose known GMAs!)
    if (!gmaFiles.empty()) {
        std::filesystem::path gmaFinal = g_logDir / L"prefetch_gma.txt";
        std::filesystem::path gmaTmp = g_logDir / L"prefetch_gma.tmp";
        std::unordered_set<std::wstring> seenGma(gmaFiles.begin(), gmaFiles.end());
        {
            std::ifstream prevGma(gmaFinal, std::ios::binary);
            std::string line;
            while (std::getline(prevGma, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                if (line.size() >= 3 && line[0] == 'F' && line[1] == '|') {
                    std::wstring gp = Utf8ToWide(line.substr(2));
                    if (WideEndsWith(gp, L".gma") && seenGma.insert(gp).second) {
                        gmaFiles.push_back(std::move(gp));
                    }
                }
            }
        }
        std::ofstream gout(gmaTmp, std::ios::binary | std::ios::trunc);
        if (gout) {
            for (const auto& g : gmaFiles) {
                gout << "F|" << WideToUtf8(g) << "\n";
            }
            gout.close();
            ::MoveFileExW(gmaTmp.c_str(), gmaFinal.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }
}



void SaveVpkPrefetchManifest(const std::string& mapName,
                             const std::vector<RecordedVpkBlock>& blocks) {
    if (!g_cfg.enableOptimizer || !g_cfg.optPrefetch || !g_cfg.optVpkBlockCache ||
        mapName.empty() || mapName == "unknown_map") return;

    std::filesystem::path finalPath = g_logDir / ("vpk_prefetch_" + SanitizeFilename(mapName) + ".txt");
    std::filesystem::path tmpPath   = g_logDir / ("vpk_prefetch_" + SanitizeFilename(mapName) + ".tmp");

    std::unordered_set<VpkBlockKey, VpkBlockKeyHash> seen;
    std::vector<RecordedVpkBlock> merged;
    merged.reserve(kMaxRecordedVpkBlocks);

    auto addBlk = [&](const std::string& path, uint64_t idx) {
        if (path.empty() || merged.size() >= kMaxRecordedVpkBlocks) return;
        VpkBlockKey key{HashStrFnv1a64(path), idx};
        if (seen.emplace(key).second) {
            merged.push_back(RecordedVpkBlock{path, idx});
        }
    };

    for (const auto& b : blocks) addBlk(b.path, b.blockIdx);

    {
        std::ifstream prev(finalPath, std::ios::binary);
        std::string line;
        while (std::getline(prev, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            size_t sep = line.rfind('|');
            if (sep == std::string::npos || sep == 0 || sep + 1 >= line.size()) continue;
            uint64_t idx = std::strtoull(line.c_str() + sep + 1, nullptr, 10);
            addBlk(line.substr(0, sep), idx);
        }
    }

    if (merged.empty()) return;

    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) return;
        for (const auto& b : merged) {
            out << b.path << "|" << b.blockIdx << "\n";
        }
    }
    ::MoveFileExW(tmpPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// ============================================================================
// 7. Optimizer Layer 3 & 4: VFS Metadata Cache + In-Memory Small-File RAM Cache
// ============================================================================
struct NarrowViewHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const noexcept {
        uint64_t h = 14695981039346656037ULL;
        for (unsigned char c : sv) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        return static_cast<size_t>(h);
    }
};

struct NarrowViewEqual {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return a == b;
    }
};

SRWLOCK g_vfsCacheLock = SRWLOCK_INIT;
uint64_t g_vfsCacheGen = 0;
std::unordered_map<std::string, bool, NarrowViewHash, NarrowViewEqual> g_vfsExistsCache;
std::unordered_map<std::string, long, NarrowViewHash, NarrowViewEqual> g_vfsTimeCache;
std::unordered_map<std::string, unsigned int, NarrowViewHash, NarrowViewEqual> g_vfsSizeCache;
std::unordered_map<std::string, bool, NarrowViewHash, NarrowViewEqual> g_vfsOpenMissCache;
enum class ResolvedState : uint8_t {
    Unknown = 0, // First OpenEx had ppszResolvedFilename == nullptr
    Null    = 1, // First OpenEx had non-null ppszResolvedFilename and g_origOpenEx wrote nullptr (e.g. BSP/GMA/VPK)
    Valid   = 2  // First OpenEx wrote a non-empty resolved path
};

struct RawByteBuf {
    std::unique_ptr<uint8_t[]> ptr;
    size_t sz = 0;
    size_t cap = 0;
    RawByteBuf() = default;
    explicit RawByteBuf(size_t n) { if (n > 0) resize_noinit(n); }
    const uint8_t* data() const noexcept { return ptr.get(); }
    uint8_t* data() noexcept { return ptr.get(); }
    size_t size() const noexcept { return sz; }
    size_t capacity() const noexcept { return cap; }
    bool empty() const noexcept { return sz == 0; }
    void resize_noinit(size_t n) {
        if (n > cap) {
            ptr = std::make_unique_for_overwrite<uint8_t[]>(n);
            cap = n;
        }
        sz = n;
    }
};

struct VfsRamEntry {
    std::shared_ptr<const RawByteBuf> data;
    std::string resolvedFilename;
    ResolvedState resolvedState = ResolvedState::Unknown;
};
std::unordered_map<std::string, VfsRamEntry, NarrowViewHash, NarrowViewEqual> g_vfsRamCache;
std::atomic<uint64_t> g_vfsRamBytesUsed{0};

// Persistent cross-map cache for shaders/fxc/*.vcs (immutable during gameplay; cleared only on writes to shaders/)
SRWLOCK g_vcsCacheLock = SRWLOCK_INIT;
std::unordered_map<std::string, bool, NarrowViewHash, NarrowViewEqual> g_vcsMissCache;
std::unordered_map<std::string, VfsRamEntry, NarrowViewHash, NarrowViewEqual> g_vcsRamCache;
uint64_t g_vcsRamBytesUsed = 0;

// Reusable buffer pool for .vtx per-handle slurp (eliminates VirtualAlloc + zero-init + ZwFreeVirtualMemory churn)
SRWLOCK g_vtxPoolLock = SRWLOCK_INIT;
std::vector<RawByteBuf*> g_vtxFreeBuffers;
constexpr size_t kVtxPoolMaxCached = 6;
constexpr size_t kVtxPoolMaxBufBytes = 16 * 1024 * 1024;

inline std::shared_ptr<RawByteBuf> AcquirePooledVtxVector(size_t fsize) {
    RawByteBuf* raw = nullptr;
    AcquireSRWLockExclusive(&g_vtxPoolLock);
    if (!g_vtxFreeBuffers.empty()) {
        raw = g_vtxFreeBuffers.back();
        g_vtxFreeBuffers.pop_back();
    }
    ReleaseSRWLockExclusive(&g_vtxPoolLock);
    if (!raw) raw = new RawByteBuf();
    raw->resize_noinit(fsize);
    return std::shared_ptr<RawByteBuf>(raw, [](RawByteBuf* p) {
        if (!p) return;
        if (p->capacity() <= kVtxPoolMaxBufBytes && !g_shuttingDown.load(std::memory_order_relaxed)) {
            AcquireSRWLockExclusive(&g_vtxPoolLock);
            if (g_vtxFreeBuffers.size() < kVtxPoolMaxCached) {
                g_vtxFreeBuffers.push_back(p);
                ReleaseSRWLockExclusive(&g_vtxPoolLock);
                return;
            }
            ReleaseSRWLockExclusive(&g_vtxPoolLock);
        }
        delete p;
    });
}

struct alignas(128) VirtualFileHandle {
    std::shared_ptr<const RawByteBuf> data;
    const uint8_t* basePtr = nullptr;
    size_t dataOffset = 0;
    size_t dataLen = 0;
    size_t pos = 0;
    std::string normPath;
    uint64_t readCalls = 0;
    uint64_t readBytes = 0;
    double readUs = 0.0;
    uint32_t slotIndex = 0;
    bool inUse = false;
    bool isWriteSink = false;
};
static_assert((sizeof(VirtualFileHandle) & (sizeof(VirtualFileHandle) - 1)) == 0, "VirtualFileHandle must be power-of-2 sized");

constexpr size_t kVirtPoolSize = 8192;
VirtualFileHandle g_virtPool[kVirtPoolSize];
SRWLOCK g_virtPoolLock = SRWLOCK_INIT;
bool g_virtPoolInited = false;
uint32_t g_virtFreeStack[kVirtPoolSize];
size_t g_virtFreeTop = 0;

void InitVirtPoolLocked() {
    if (g_virtPoolInited) return;
    for (uint32_t i = 0; i < kVirtPoolSize; ++i) {
        g_virtPool[i].slotIndex = i;
        g_virtPool[i].inUse = false;
        g_virtFreeStack[i] = static_cast<uint32_t>(kVirtPoolSize - 1 - i);
    }
    g_virtFreeTop = kVirtPoolSize;
    g_virtPoolInited = true;
}

inline bool IsVirtualHandle(const void* h) noexcept {
    const uintptr_t p = reinterpret_cast<uintptr_t>(h);
    const uintptr_t base = reinterpret_cast<uintptr_t>(g_virtPool);
    if (p < base || p >= base + sizeof(g_virtPool)) return false;
    return ((p - base) & (sizeof(VirtualFileHandle) - 1)) == 0;
}

VirtualFileHandle* AllocVirtualHandleSlice(std::shared_ptr<const RawByteBuf> data,
                                           size_t dataOffset, size_t dataLen, const std::string& normPath) {
    const uint8_t* ptr = (data && dataOffset <= data->size()) ? (data->data() + dataOffset) : nullptr;
    AcquireSRWLockExclusive(&g_virtPoolLock);
    if (!g_virtPoolInited) InitVirtPoolLocked();
    if (g_virtFreeTop == 0) {
        ReleaseSRWLockExclusive(&g_virtPoolLock);
        return nullptr;
    }
    uint32_t idx = g_virtFreeStack[--g_virtFreeTop];
    VirtualFileHandle* vh = &g_virtPool[idx];
    vh->data = std::move(data);
    vh->basePtr = ptr;
    vh->dataOffset = dataOffset;
    vh->dataLen = dataLen;
    vh->pos = 0;
    vh->readCalls = 0;
    vh->readBytes = 0;
    vh->readUs = 0.0;
    vh->inUse = true;
    vh->isWriteSink = false;
    ReleaseSRWLockExclusive(&g_virtPoolLock);
    if (g_cfg.profilerEnabled) {
        vh->normPath = normPath;
    } else {
        vh->normPath.clear();
    }
    return vh;
}

VirtualFileHandle* AllocVirtualHandle(std::shared_ptr<const RawByteBuf> data, const std::string& normPath) {
    size_t len = data ? data->size() : 0;
    return AllocVirtualHandleSlice(std::move(data), 0, len, normPath);
}

VirtualFileHandle* AllocVirtualWriteSinkHandle(const std::string& normPath) {
    VirtualFileHandle* vh = AllocVirtualHandleSlice(nullptr, 0, 0, normPath);
    if (vh) {
        vh->isWriteSink = true;
    }
    return vh;
}

void FreeVirtualHandle(VirtualFileHandle* vh) {
    if (!vh) return;
    AcquireSRWLockExclusive(&g_virtPoolLock);
    if (vh->inUse) {
        vh->data.reset();
        vh->dataOffset = 0;
        vh->dataLen = 0;
        vh->normPath.clear();
        vh->pos = 0;
        vh->inUse = false;
        vh->isWriteSink = false;
        if (g_virtFreeTop < kVirtPoolSize) {
            g_virtFreeStack[g_virtFreeTop++] = vh->slotIndex;
        }
    }
    ReleaseSRWLockExclusive(&g_virtPoolLock);
}

// High-reuse small assets that benefit from persistent cross-open RAM caching
inline bool IsSharedRamCacheExt(std::string_view norm) noexcept {
    auto endsWith = [&](std::string_view suf) {
        return norm.size() >= suf.size() && norm.substr(norm.size() - suf.size()) == suf;
    };
    return endsWith(".vmt") || endsWith(".vhv") || endsWith(".ani") ||
           endsWith(".vcs") || endsWith(".txt") ||
           endsWith(".cfg") || endsWith(".res") || endsWith(".pcf");
}

// Micro-read-heavy assets (e.g. .vtx: up to 434k tiny reads on gravitia.vtx) that are opened
// once -> slurp per-handle up to 16 MB and free immediately on Close (zero shared RAM bloat!)
inline bool IsSlurpOnlyExt(std::string_view norm) noexcept {
    return norm.size() >= 4 && norm.substr(norm.size() - 4) == ".vtx";
}

struct StackVfsKey {
    char buf[512];
    std::string heap;
    std::string_view view;

    void Build(std::string_view normPath, const char* pathID, unsigned flags = 0, char modeTag = 0) {
        size_t pidLen = (pathID && *pathID) ? std::strlen(pathID) : 0;
        char fbuf[16];
        size_t flen = 0;
        if (flags != 0) {
            flen = static_cast<size_t>(std::snprintf(fbuf, sizeof(fbuf), "%x", flags));
        }
        size_t total = normPath.size() + 1 + pidLen + (flen ? (1 + flen) : 0) + (modeTag ? 2 : 0);
        char* dst = nullptr;
        if (total < sizeof(buf)) {
            dst = buf;
            view = std::string_view(buf, total);
        } else {
            heap.resize(total);
            dst = heap.data();
            view = heap;
        }
        size_t off = 0;
        std::memcpy(dst + off, normPath.data(), normPath.size());
        off += normPath.size();
        dst[off++] = '\x01';
        for (size_t i = 0; i < pidLen; ++i) {
            char c = pathID[i];
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            dst[off++] = c;
        }
        if (flen) {
            dst[off++] = '\x03';
            std::memcpy(dst + off, fbuf, flen);
            off += flen;
        }
        if (modeTag) {
            dst[off++] = '\x02';
            dst[off++] = modeTag;
        }
    }
};

inline void EnsureVfsCacheGenLocked(uint64_t curGen) {
    if (g_vfsCacheGen != curGen) {
        g_vfsExistsCache.clear();
        g_vfsTimeCache.clear();
        g_vfsSizeCache.clear();
        g_vfsOpenMissCache.clear();
        g_vfsRamCache.clear();
        g_vfsRamBytesUsed.store(0, std::memory_order_relaxed);
        g_vfsCacheGen = curGen;
    }
}

uint64_t ClearVfsCacheAll() {
    uint64_t freed = 0;
    const uint64_t curGen = g_vfsGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    AcquireSRWLockExclusive(&g_vfsCacheLock);
    freed = g_vfsRamBytesUsed.load(std::memory_order_relaxed);
    EnsureVfsCacheGenLocked(curGen);
    ReleaseSRWLockExclusive(&g_vfsCacheLock);
    return freed;
}

// ============================================================================
// Layer 8: BSP Lump 40 (LUMP_PAKFILE) Direct ZIP Pre-Indexer & Zero-Copy Slab
//          Eliminates ~969 ms of sp_*.vhv OpenEx + 4,332 sp_*.ppl misses
// ============================================================================
struct BspPakEntry {
    size_t slabOffset = 0;
    size_t slabLen = 0;
    bool inSlab = false;
};

SRWLOCK g_bspPakLock = SRWLOCK_INIT;
std::atomic<bool> g_bspPakActive{false};
std::wstring g_bspPakIndexedPath;
std::shared_ptr<const RawByteBuf> g_bspPakSlab;
std::unordered_map<std::string, BspPakEntry, NarrowViewHash, NarrowViewEqual> g_bspPakEntries;

inline bool IsBspPakStaticPropLightPath(std::string_view norm) noexcept {
    if (norm.size() < 7) return false;
    bool isVhv = (norm.compare(norm.size() - 4, 4, ".vhv") == 0);
    bool isPpl = !isVhv && (norm.compare(norm.size() - 4, 4, ".ppl") == 0);
    if (!isVhv && !isPpl) return false;
    auto slash = norm.find_last_of('/');
    std::string_view leaf = (slash == std::string_view::npos) ? norm : norm.substr(slash + 1);
    if (leaf.size() >= 7 && leaf.compare(0, 3, "sp_") == 0) return true;
    if (isPpl && leaf.size() >= 18 && leaf.compare(0, 15, "texelslighting_") == 0) return true;
    return false;
}

inline bool IsBspPakVmtPath(std::string_view norm) noexcept {
    return norm.size() > 4 && norm.compare(norm.size() - 4, 4, ".vmt") == 0;
}

inline bool IsBspPakPreloadablePath(std::string_view norm) noexcept {
    return IsBspPakStaticPropLightPath(norm) || IsBspPakVmtPath(norm);
}

inline bool IsBspPathId(const char* pathID) noexcept {
    if (!pathID) return false;
    return (pathID[0] == 'B' || pathID[0] == 'b') &&
           (pathID[1] == 'S' || pathID[1] == 's') &&
           (pathID[2] == 'P' || pathID[2] == 'p') &&
           pathID[3] == '\0';
}

void ClearBspPakIndex() {
    g_bspPakActive.store(false, std::memory_order_release);
    AcquireSRWLockExclusive(&g_bspPakLock);
    g_bspPakIndexedPath.clear();
    g_bspPakEntries.clear();
    g_bspPakSlab.reset();
    ReleaseSRWLockExclusive(&g_bspPakLock);
}

static bool ReadFileExactAt(HANDLE h, uint64_t offset, void* dst, size_t len) {
    LARGE_INTEGER li{};
    li.QuadPart = static_cast<LONGLONG>(offset);
    if (!::SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) return false;
    uint8_t* p = static_cast<uint8_t*>(dst);
    size_t done = 0;
    while (done < len) {
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(len - done, 0x40000000ULL));
        DWORD got = 0;
        if (!::ReadFile(h, p + done, chunk, &got, nullptr) || got == 0) return false;
        done += got;
    }
    return true;
}


// ============================================================================
// Layer 12 (v14): Async BSP Non-PakLump Pre-Warmer (Lumps 0..39, 41..63)
//                 Warms ~117 MB of BSP geometry/lightmaps into OS Page Cache
//                 at background I/O priority while engine initializes!
// ============================================================================
struct BspLumpWarmJob {
    std::wstring path;
    std::vector<std::pair<uint64_t, uint64_t>> ranges; // (offset, length)
    uint64_t gen = 0;
};
std::atomic<uint64_t> g_bspWarmGen{0};

void LaunchBspNonPakLumpPrewarm(const std::wstring& bspPath, const uint8_t hdr[1036], uint64_t fileSize) {
    if (!g_cfg.enableOptimizer || !g_cfg.optBspLumpPrewarm || bspPath.empty()) return;
    auto* job = new BspLumpWarmJob();
    job->path = bspPath;
    job->gen = g_bspWarmGen.fetch_add(1, std::memory_order_acq_rel) + 1;

    std::vector<std::pair<uint64_t, uint64_t>> raw;
    raw.reserve(63);
    for (int l = 0; l < 64; ++l) {
        if (l == 40) continue; // Skip LUMP_PAKFILE (already in Layer 8 RAM slab!)
        uint32_t ofs = 0, len = 0;
        std::memcpy(&ofs, hdr + 8 + l * 16, 4);
        std::memcpy(&len, hdr + 8 + l * 16 + 4, 4);
        if (ofs > 0 && len > 0 && static_cast<uint64_t>(ofs) + len <= fileSize) {
            raw.emplace_back(static_cast<uint64_t>(ofs), static_cast<uint64_t>(len));
        }
    }
    if (raw.empty()) {
        delete job;
        return;
    }
    std::sort(raw.begin(), raw.end());
    // Coalesce adjacent/nearby ranges within 64 KB to minimize ReadFile syscalls
    for (const auto& r : raw) {
        if (!job->ranges.empty() && r.first <= job->ranges.back().first + job->ranges.back().second + 65536ULL) {
            uint64_t newEnd = std::max(job->ranges.back().first + job->ranges.back().second, r.first + r.second);
            job->ranges.back().second = newEnd - job->ranges.back().first;
        } else {
            job->ranges.push_back(r);
        }
    }

    HANDLE th = ::CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
        std::unique_ptr<BspLumpWarmJob> j(static_cast<BspLumpWarmJob*>(param));
        if (!::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN)) {
            ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        }
        HANDLE h = ::CreateFileW(j->path.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            auto buf = std::make_unique_for_overwrite<uint8_t[]>(512 * 1024);
            uint64_t warmed = 0;
            const uint64_t maxBspWarmBytes =
                (g_effectiveRamProfile == "low") ? (128ULL * 1024ULL * 1024ULL)
                                                 : (256ULL * 1024ULL * 1024ULL);
            for (const auto& rng : j->ranges) {
                if (g_shuttingDown.load(std::memory_order_relaxed) ||
                    g_bspWarmGen.load(std::memory_order_acquire) != j->gen ||
                    !g_sessionActive.load(std::memory_order_acquire) ||
                    g_ramGuardLevel.load(std::memory_order_relaxed) != 0 ||
                    warmed >= maxBspWarmBytes) break;
                LARGE_INTEGER li{};
                li.QuadPart = static_cast<LONGLONG>(rng.first);
                if (!::SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) continue;
                uint64_t rem = std::min<uint64_t>(rng.second, maxBspWarmBytes - warmed);
                while (rem > 0) {
                    if (g_shuttingDown.load(std::memory_order_relaxed) ||
                        g_bspWarmGen.load(std::memory_order_acquire) != j->gen ||
                        !g_sessionActive.load(std::memory_order_acquire) ||
                        g_ramGuardLevel.load(std::memory_order_relaxed) != 0) break;
                    DWORD want = static_cast<DWORD>(std::min<uint64_t>(512ULL * 1024ULL, rem));
                    DWORD got = 0;
                    if (!::ReadFile(h, buf.get(), want, &got, nullptr) || got == 0) break;
                    rem -= got;
                    warmed += got;
                }
            }
            ::CloseHandle(h);
            if (warmed > 0) {
                g_optStats.bspLumpPrewarmBytes.fetch_add(warmed, std::memory_order_relaxed);
            }
        }
        ::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
        return 0;
    }, job, 0, nullptr);
    if (th) ::CloseHandle(th);
    else delete job;
}

void IndexBspPakLump(const std::wstring& bspPath, bool forceReindex) {
    if (!g_cfg.enableOptimizer || !g_cfg.optBspPakFastPath || bspPath.empty()) return;
    if (g_ramGuardLevel.load(std::memory_order_relaxed) >= 2) return;

    // Normalize path for dedup
    std::wstring normW = bspPath;
    for (auto& c : normW) {
        if (c == L'/') c = L'\\';
        else if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    if (!forceReindex) {
        AcquireSRWLockShared(&g_bspPakLock);
        bool same = (g_bspPakActive.load(std::memory_order_acquire) && g_bspPakIndexedPath == normW);
        ReleaseSRWLockShared(&g_bspPakLock);
        if (same) return;
    }

    HANDLE h = ::CreateFileW(bspPath.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER fszLi{};
    if (!::GetFileSizeEx(h, &fszLi) || fszLi.QuadPart < 1036) {
        ::CloseHandle(h);
        return;
    }
    const uint64_t fileSize = static_cast<uint64_t>(fszLi.QuadPart);

    // VBSP header: int32 ident ("VBSP"), int32 version, lump_t[64] (16 bytes each)
    uint8_t hdr[1036];
    if (!ReadFileExactAt(h, 0, hdr, sizeof(hdr))) {
        ::CloseHandle(h);
        return;
    }
    uint32_t ident = 0;
    int32_t version = 0;
    std::memcpy(&ident, hdr, 4);
    std::memcpy(&version, hdr + 4, 4);
    if (ident != 0x50534256U || version < 19 || version > 29) {
        ::CloseHandle(h);
        return;
    }
    // Layer 12 (v14): Asynchronously warm non-PakLump geometry/lightmap lumps into OS Page Cache!
    LaunchBspNonPakLumpPrewarm(bspPath, hdr, fileSize);
    // Lump 40 = LUMP_PAKFILE at offset 8 + 40*16 = 648
    uint32_t pakOfs = 0, pakLen = 0;
    std::memcpy(&pakOfs, hdr + 8 + 40 * 16, 4);
    std::memcpy(&pakLen, hdr + 8 + 40 * 16 + 4, 4);
    if (pakOfs == 0 || pakLen < 22 || static_cast<uint64_t>(pakOfs) + pakLen > fileSize) {
        ::CloseHandle(h);
        return;
    }

    // Locate ZIP End of Central Directory (EOCD: 0x06054b50)
    const size_t tailLen = static_cast<size_t>(std::min<uint64_t>(pakLen, 65557ULL));
    std::vector<uint8_t> tail(tailLen);
    if (!ReadFileExactAt(h, static_cast<uint64_t>(pakOfs) + pakLen - tailLen, tail.data(), tailLen)) {
        ::CloseHandle(h);
        return;
    }
    int eocdPos = -1;
    for (int i = static_cast<int>(tailLen) - 22; i >= 0; --i) {
        if (tail[i] == 0x50 && tail[i + 1] == 0x4B && tail[i + 2] == 0x05 && tail[i + 3] == 0x06) {
            eocdPos = i;
            break;
        }
    }
    if (eocdPos < 0) {
        ::CloseHandle(h);
        return;
    }
    uint32_t cdSize = 0, cdOfs = 0;
    std::memcpy(&cdSize, tail.data() + eocdPos + 12, 4);
    std::memcpy(&cdOfs, tail.data() + eocdPos + 16, 4);
    if (cdSize == 0 || cdSize > 64U * 1024U * 1024U ||
        static_cast<uint64_t>(cdOfs) + cdSize > pakLen) {
        ::CloseHandle(h);
        return;
    }

    std::vector<uint8_t> cd(cdSize);
    if (!ReadFileExactAt(h, static_cast<uint64_t>(pakOfs) + cdOfs, cd.data(), cdSize)) {
        ::CloseHandle(h);
        return;
    }

    struct RawSpItem {
        std::string name;
        uint64_t localHdrAbsOfs;
        uint32_t uncompSize;
    };

    std::unordered_map<std::string, BspPakEntry, NarrowViewHash, NarrowViewEqual> newEntries;
    newEntries.reserve(8192);
    std::vector<RawSpItem> spItems;
    spItems.reserve(4096);

    size_t p = 0;
    uint64_t totalSpBytes = 0;
    const uint64_t maxSlabBytes = static_cast<uint64_t>(g_cfg.bspPakSlabMb) * 1024ULL * 1024ULL;
    while (p + 46 <= cdSize) {
        uint32_t sig = 0;
        std::memcpy(&sig, cd.data() + p, 4);
        if (sig != 0x02014B50U) break;
        uint16_t method = 0, nameLen = 0, extraLen = 0, commentLen = 0;
        uint32_t compSize = 0, uncompSize = 0, localRelOfs = 0;
        std::memcpy(&method, cd.data() + p + 10, 2);
        std::memcpy(&compSize, cd.data() + p + 20, 4);
        std::memcpy(&uncompSize, cd.data() + p + 24, 4);
        std::memcpy(&nameLen, cd.data() + p + 28, 2);
        std::memcpy(&extraLen, cd.data() + p + 30, 2);
        std::memcpy(&commentLen, cd.data() + p + 32, 2);
        std::memcpy(&localRelOfs, cd.data() + p + 42, 4);
        if (p + 46 + nameLen + extraLen + commentLen > cdSize) break;

        std::string ename(reinterpret_cast<const char*>(cd.data() + p + 46), nameLen);
        for (auto& ch : ename) {
            if (ch == '\\') ch = '/';
            else if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        while (!ename.empty() && (ename[0] == '/' || (ename.size() >= 2 && ename[0] == '.' && ename[1] == '/'))) {
            if (ename[0] == '/') ename.erase(0, 1);
            else ename.erase(0, 2);
        }

        if (!ename.empty()) {
            newEntries.emplace(ename, BspPakEntry{});
            if (method == 0 && compSize == uncompSize && uncompSize > 0 &&
                IsBspPakPreloadablePath(ename) &&
                (totalSpBytes + uncompSize) <= maxSlabBytes &&
                static_cast<uint64_t>(localRelOfs) + 30ULL + uncompSize <= pakLen) {
                spItems.push_back(RawSpItem{ename, static_cast<uint64_t>(pakOfs) + localRelOfs, uncompSize});
                totalSpBytes += uncompSize;
            }
        }
        p += 46 + nameLen + extraLen + commentLen;
    }

    if (newEntries.empty()) {
        ::CloseHandle(h);
        return;
    }

    // Bulk-read all uncompressed sp_*.vhv / sp_*.ppl into a single zero-copy RAM slab using coalesced sequential runs!
    std::shared_ptr<RawByteBuf> slab;
    size_t slabLoadedCount = 0;
    if (!spItems.empty() && totalSpBytes > 0) {
        std::sort(spItems.begin(), spItems.end(), [](const RawSpItem& a, const RawSpItem& b) {
            return a.localHdrAbsOfs < b.localHdrAbsOfs;
        });
        slab = std::make_shared<RawByteBuf>(static_cast<size_t>(totalSpBytes));
        size_t slabCursor = 0;
        RawByteBuf runBuf;
        constexpr uint64_t kMaxRunGap = 256ULL * 1024ULL;
        constexpr uint64_t kMaxRunBytes = 8ULL * 1024ULL * 1024ULL;

        size_t idx = 0;
        while (idx < spItems.size()) {
            const uint64_t runStart = spItems[idx].localHdrAbsOfs;
            uint64_t runEnd = runStart + 512ULL + spItems[idx].uncompSize;
            size_t j = idx + 1;
            while (j < spItems.size()) {
                uint64_t nextEnd = spItems[j].localHdrAbsOfs + 512ULL + spItems[j].uncompSize;
                if (spItems[j].localHdrAbsOfs > runEnd + kMaxRunGap) break;
                if (nextEnd - runStart > kMaxRunBytes) break;
                if (nextEnd > runEnd) runEnd = nextEnd;
                ++j;
            }
            if (runEnd > fileSize) runEnd = fileSize;
            const size_t runLen = static_cast<size_t>(runEnd - runStart);
            if (runBuf.size() < runLen) runBuf.resize_noinit(runLen);
            if (ReadFileExactAt(h, runStart, runBuf.data(), runLen)) {
                for (size_t k = idx; k < j; ++k) {
                    const uint64_t rel = spItems[k].localHdrAbsOfs - runStart;
                    if (rel + 30 > runLen) continue;
                    uint32_t lsig = 0;
                    std::memcpy(&lsig, runBuf.data() + rel, 4);
                    if (lsig != 0x04034B50U) continue;
                    uint16_t lNameLen = 0, lExtraLen = 0;
                    std::memcpy(&lNameLen, runBuf.data() + rel + 26, 2);
                    std::memcpy(&lExtraLen, runBuf.data() + rel + 28, 2);
                    const uint64_t dataRel = rel + 30ULL + lNameLen + lExtraLen;
                    if (dataRel + spItems[k].uncompSize > runLen) continue;
                    if (slabCursor + spItems[k].uncompSize > slab->size()) continue;
                    std::memcpy(slab->data() + slabCursor, runBuf.data() + dataRel, spItems[k].uncompSize);
                    auto it = newEntries.find(spItems[k].name);
                    if (it != newEntries.end()) {
                        it->second.slabOffset = slabCursor;
                        it->second.slabLen = spItems[k].uncompSize;
                        it->second.inSlab = true;
                        ++slabLoadedCount;
                    }
                    slabCursor += spItems[k].uncompSize;
                }
            }
            idx = j;
        }
    }
    ::CloseHandle(h);

    const size_t entryCount = newEntries.size();
    const uint64_t slabBytes = slab ? slab->size() : 0ULL;

    AcquireSRWLockExclusive(&g_bspPakLock);
    g_bspPakIndexedPath = std::move(normW);
    g_bspPakEntries = std::move(newEntries);
    g_bspPakSlab = std::move(slab);
    g_bspPakActive.store(true, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_bspPakLock);

    g_optStats.bspPakIndexed.fetch_add(entryCount, std::memory_order_relaxed);
    g_optStats.bspPakSlabBytes.fetch_add(slabBytes, std::memory_order_relaxed);
    WriteStatusLog("Layer 8 BSP PakLump indexed: " + std::to_string(entryCount) + " ZIP entries, " +
                   std::to_string(slabLoadedCount) + " sp_*.vhv/.ppl/map .vmt preloaded (" +
                   std::to_string(slabBytes / (1024ULL * 1024ULL)) + " MB zero-copy slab).");
}

// ============================================================================
// Layer 4c: Bounded Load-Time .vtf LRU RAM Cache (opt_vtf_ram_cache=1)
//           Eliminates duplicate reads of giant skybox/lightning .vtf textures;
//           100% freed automatically after map load (0 MB gameplay RAM!).
// ============================================================================
// Verified in materialsystem.dll (0x1800441c0 / 0x1800441e0):
// tier0.dll!g_pMemAlloc is IMemAlloc*, where vtable[1] (+0x08) = Alloc(size_t).
using FnTier0MemAlloc = void* (*)(void* self, size_t nSize);

char* AllocTier0String(const std::string& s) {
    if (s.empty()) return nullptr;
    static std::atomic<void**> s_ppMemAlloc{nullptr};
    void** pp = s_ppMemAlloc.load(std::memory_order_acquire);
    if (!pp) {
        HMODULE hTier0 = ::GetModuleHandleW(L"tier0.dll");
        if (hTier0) {
            pp = reinterpret_cast<void**>(::GetProcAddress(hTier0, "g_pMemAlloc"));
            if (pp) s_ppMemAlloc.store(pp, std::memory_order_release);
        }
    }
    if (!pp || !*pp) return nullptr;
    void* memAlloc = *pp;
    void** vt = *reinterpret_cast<void***>(memAlloc);
    if (!vt || !vt[1]) return nullptr;
    auto fnAlloc = reinterpret_cast<FnTier0MemAlloc>(vt[1]);
    char* dst = static_cast<char*>(fnAlloc(memAlloc, s.size() + 1));
    if (!dst) return nullptr;
    std::memcpy(dst, s.data(), s.size() + 1);
    return dst;
}

bool HasTier0MemAlloc() {
    static std::atomic<void**> s_ppMemAlloc{nullptr};
    void** pp = s_ppMemAlloc.load(std::memory_order_acquire);
    if (!pp) {
        HMODULE hTier0 = ::GetModuleHandleW(L"tier0.dll");
        if (hTier0) {
            pp = reinterpret_cast<void**>(::GetProcAddress(hTier0, "g_pMemAlloc"));
            if (pp) s_ppMemAlloc.store(pp, std::memory_order_release);
        }
    }
    return pp && *pp;
}

struct VtfLruEntry {
    std::shared_ptr<const RawByteBuf> data;
    std::string resolvedFilename;
    ResolvedState resolvedState = ResolvedState::Unknown;
    std::list<std::string>::iterator lruIt;
};

SRWLOCK g_vtfCacheLock = SRWLOCK_INIT;
std::list<std::string> g_vtfLruOrder;
std::unordered_map<std::string, VtfLruEntry, NarrowViewHash, NarrowViewEqual> g_vtfLruMap;
uint64_t g_vtfCacheBytesUsed = 0;
uint64_t g_vtfCacheGen = 0;

void ClearVtfRamCache() {
    AcquireSRWLockExclusive(&g_vtfCacheLock);
    g_vtfLruMap.clear();
    g_vtfLruOrder.clear();
    g_vtfCacheBytesUsed = 0;
    g_vtfCacheGen = g_vtfGeneration.load(std::memory_order_acquire);
    ReleaseSRWLockExclusive(&g_vtfCacheLock);
}

uint64_t TrimVtfRamCacheHalf() {
    std::vector<std::shared_ptr<const RawByteBuf>> dropped;
    uint64_t freedBytes = 0;
    AcquireSRWLockExclusive(&g_vtfCacheLock);
    const uint64_t targetBytes = g_vtfCacheBytesUsed / 2;
    while (!g_vtfLruOrder.empty() && g_vtfCacheBytesUsed > targetBytes) {
        const std::string& victimKey = g_vtfLruOrder.back();
        auto vit = g_vtfLruMap.find(victimKey);
        if (vit != g_vtfLruMap.end()) {
            if (vit->second.data) {
                const uint64_t sz = vit->second.data->size();
                g_vtfCacheBytesUsed = (g_vtfCacheBytesUsed >= sz) ? (g_vtfCacheBytesUsed - sz) : 0;
                freedBytes += sz;
                dropped.push_back(std::move(vit->second.data));
            }
            g_vtfLruMap.erase(vit);
        }
        g_vtfLruOrder.pop_back();
        g_optStats.vtfLruEvictions.fetch_add(1, std::memory_order_relaxed);
    }
    ReleaseSRWLockExclusive(&g_vtfCacheLock);
    return freedBytes;
}

std::shared_ptr<const RawByteBuf> LookupVtfRamCache(std::string_view key, uint64_t curGen,
                                                              std::string* outResolved = nullptr,
                                                              ResolvedState* outState = nullptr) {
    std::shared_ptr<const RawByteBuf> res;
    AcquireSRWLockExclusive(&g_vtfCacheLock);
    if (g_vtfCacheGen != curGen) {
        if (!g_vtfLruMap.empty()) {
            g_optStats.vtfGenClears.fetch_add(1, std::memory_order_relaxed);
        }
        g_vtfLruMap.clear();
        g_vtfLruOrder.clear();
        g_vtfCacheBytesUsed = 0;
        g_vtfCacheGen = curGen;
        ReleaseSRWLockExclusive(&g_vtfCacheLock);
        return res;
    }
    auto it = g_vtfLruMap.find(key);
    if (it != g_vtfLruMap.end()) {
        if (it->second.lruIt != g_vtfLruOrder.begin()) {
            g_vtfLruOrder.splice(g_vtfLruOrder.begin(), g_vtfLruOrder, it->second.lruIt);
        }
        res = it->second.data;
        if (outResolved) *outResolved = it->second.resolvedFilename;
        if (outState) *outState = it->second.resolvedState;
    }
    ReleaseSRWLockExclusive(&g_vtfCacheLock);
    return res;
}

void InsertVtfRamCache(const std::string& key, uint64_t curGen, std::shared_ptr<const RawByteBuf> data,
                       std::string resolvedFilename, ResolvedState resolvedState) {
    const uint8_t guardLvl = g_ramGuardLevel.load(std::memory_order_relaxed);
    if (!data || data->empty() || guardLvl >= 2) return;
    uint64_t maxTotal = static_cast<uint64_t>(g_cfg.vtfRamCacheMaxMb) * 1024ULL * 1024ULL;
    if (guardLvl == 1) {
        // v16.3: Keep a compact 64 MB ring during RAM Guard LEVEL 1 to catch back-to-back .vtf double-reads
        maxTotal = std::min<uint64_t>(maxTotal, 64ULL * 1024ULL * 1024ULL);
    }
    const uint64_t sz = data->size();
    if (sz > maxTotal) return;

    AcquireSRWLockExclusive(&g_vtfCacheLock);
    if (g_vtfCacheGen != curGen) {
        if (!g_vtfLruMap.empty()) {
            g_optStats.vtfGenClears.fetch_add(1, std::memory_order_relaxed);
        }
        g_vtfLruMap.clear();
        g_vtfLruOrder.clear();
        g_vtfCacheBytesUsed = 0;
        g_vtfCacheGen = curGen;
    }
    auto existing = g_vtfLruMap.find(key);
    if (existing != g_vtfLruMap.end()) {
        if (existing->second.resolvedState == ResolvedState::Unknown && resolvedState != ResolvedState::Unknown) {
            existing->second.resolvedFilename = std::move(resolvedFilename);
            existing->second.resolvedState = resolvedState;
        }
        if (existing->second.lruIt != g_vtfLruOrder.begin()) {
            g_vtfLruOrder.splice(g_vtfLruOrder.begin(), g_vtfLruOrder, existing->second.lruIt);
        }
        ReleaseSRWLockExclusive(&g_vtfCacheLock);
        return;
    }
    // v14 Anti-Thrash Guard: if cache is already >75% full, skip caching giant (>32 MB) one-off VTFs
    // so they don't evict dozens of smaller reusable textures!
    if (sz > 32ULL * 1024ULL * 1024ULL && (g_vtfCacheBytesUsed * 4ULL) >= (maxTotal * 3ULL)) {
        ReleaseSRWLockExclusive(&g_vtfCacheLock);
        return;
    }
    while (!g_vtfLruOrder.empty() && (g_vtfCacheBytesUsed + sz) > maxTotal) {
        const std::string& victimKey = g_vtfLruOrder.back();
        auto vit = g_vtfLruMap.find(victimKey);
        if (vit != g_vtfLruMap.end()) {
            if (vit->second.data && g_vtfCacheBytesUsed >= vit->second.data->size()) {
                g_vtfCacheBytesUsed -= vit->second.data->size();
            }
            g_vtfLruMap.erase(vit);
        }
        g_vtfLruOrder.pop_back();
        g_optStats.vtfLruEvictions.fetch_add(1, std::memory_order_relaxed);
    }
    g_vtfLruOrder.push_front(key);
    g_vtfLruMap.emplace(key, VtfLruEntry{std::move(data), std::move(resolvedFilename), resolvedState, g_vtfLruOrder.begin()});
    g_vtfCacheBytesUsed += sz;
    g_optStats.vtfRamCached.fetch_add(1, std::memory_order_relaxed);
    ReleaseSRWLockExclusive(&g_vtfCacheLock);
}

// ============================================================================
// Layer 9: Deterministic cache/map_pack.bsp Zero-Copy Extraction Bypass
//          Eliminates redundant 1.37 GB Workshop .gma -> cache/map_pack.bsp copy
// ============================================================================
constexpr size_t kMapPackHashSampleBytes = 64 * 1024;

inline uint64_t HashBytesFnv1a64(const uint8_t* p, size_t len) noexcept {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

std::filesystem::path GetMapPackDiskPath() {
    std::filesystem::path candidate = g_dllDir;
    if (candidate.has_parent_path() && candidate.parent_path().has_parent_path()) {
        std::wstring leaf = candidate.filename().wstring();
        std::wstring parentLeaf = candidate.parent_path().filename().wstring();
        for (auto& ch : leaf) ch = std::towlower(ch);
        for (auto& ch : parentLeaf) ch = std::towlower(ch);
        if (leaf == L"win64" && parentLeaf == L"bin") {
            return candidate.parent_path().parent_path() / L"garrysmod" / L"cache" / L"map_pack.bsp";
        }
    }
    std::error_code ec;
    if (std::filesystem::is_directory(g_dllDir / L"garrysmod", ec)) {
        return g_dllDir / L"garrysmod" / L"cache" / L"map_pack.bsp";
    }
    return {};
}

bool HashDiskFileHeadTail(const std::wstring& path, uint64_t& outSize, uint64_t& outMtime,
                          uint64_t& outHeadHash, uint64_t& outTailHash) {
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION fi{};
    if (!::GetFileInformationByHandle(h, &fi)) {
        ::CloseHandle(h);
        return false;
    }
    outSize = (static_cast<uint64_t>(fi.nFileSizeHigh) << 32) | fi.nFileSizeLow;
    outMtime = (static_cast<uint64_t>(fi.ftLastWriteTime.dwHighDateTime) << 32) | fi.ftLastWriteTime.dwLowDateTime;
    if (outSize < kMapPackHashSampleBytes) {
        ::CloseHandle(h);
        return false;
    }
    std::vector<uint8_t> buf(kMapPackHashSampleBytes);
    if (!ReadFileExactAt(h, 0, buf.data(), kMapPackHashSampleBytes)) {
        ::CloseHandle(h);
        return false;
    }
    outHeadHash = HashBytesFnv1a64(buf.data(), kMapPackHashSampleBytes);
    if (!ReadFileExactAt(h, outSize - kMapPackHashSampleBytes, buf.data(), kMapPackHashSampleBytes)) {
        ::CloseHandle(h);
        return false;
    }
    outTailHash = HashBytesFnv1a64(buf.data(), kMapPackHashSampleBytes);
    ::CloseHandle(h);
    return true;
}

struct PendingMapPackMeta {
    std::string srcNormPath;
    uint64_t srcSize = 0;
};

SRWLOCK g_pendingMapPackLock = SRWLOCK_INIT;
PendingMapPackMeta g_pendingMapPack;

void ClearPendingMapPack() {
    AcquireSRWLockExclusive(&g_pendingMapPackLock);
    g_pendingMapPack = PendingMapPackMeta{};
    ReleaseSRWLockExclusive(&g_pendingMapPackLock);
}

void SetPendingMapPackSource(const std::string& srcNormPath, uint64_t srcSize) {
    AcquireSRWLockExclusive(&g_pendingMapPackLock);
    g_pendingMapPack.srcNormPath = srcNormPath;
    g_pendingMapPack.srcSize = srcSize;
    ReleaseSRWLockExclusive(&g_pendingMapPackLock);
}

void OnMapPackBspWriteClosed(const std::wstring& mapPackPath) {
    std::wstring lower = mapPackPath;
    for (auto& c : lower) {
        if (c == L'/') c = L'\\';
        else if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    if (!WideEndsWith(lower, L"\\cache\\map_pack.bsp")) return;

    PendingMapPackMeta pending;
    AcquireSRWLockExclusive(&g_pendingMapPackLock);
    pending = g_pendingMapPack;
    g_pendingMapPack = PendingMapPackMeta{};
    ReleaseSRWLockExclusive(&g_pendingMapPackLock);

    if (pending.srcNormPath.empty() || pending.srcSize < kMapPackHashSampleBytes) return;

    uint64_t dstSize = 0, dstMtime = 0, headH = 0, tailH = 0;
    if (!HashDiskFileHeadTail(mapPackPath, dstSize, dstMtime, headH, tailH)) return;
    if (dstSize != pending.srcSize) return;

    std::wstring metaPath = mapPackPath + L".optmeta";
    std::ofstream out(metaPath, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "v12\n"
        << pending.srcNormPath << "\n"
        << dstSize << "\n"
        << dstMtime << "\n"
        << headH << "\n"
        << tailH << "\n";
    WriteStatusLog("Layer 9 MapPack Cache: saved deterministic metadata for " + pending.srcNormPath +
                   " (" + std::to_string(dstSize / (1024ULL * 1024ULL)) + " MB).");
}


// ============================================================================
// 8. Data structures for a single map-load session + Main-Thread RIP Sampler
// ============================================================================
enum class EventKind : uint8_t {
    Milestone     = 0,
    Console       = 1,
    FsOpen        = 2,
    FsExists      = 3,
    FsRead        = 4,
    FsFindFirst   = 5,
    FsGetFileTime = 6
};

struct TimelineEvent {
    int64_t startTick = 0;
    double durUs = 0.0;
    uint32_t tid = 0;
    EventKind kind = EventKind::Console;
    bool hit = false;
    uint64_t bytes = 0;
    std::string primary;
    std::string secondary;
    std::string caller;
};

struct MilestoneEntry {
    std::string name;
    double tMs = 0.0;
    double deltaMs = 0.0;
    uint32_t tid = 0;
    std::string detail;
};

struct FileAggregate {
    uint64_t calls = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    double totalUs = 0.0;
    double maxUs = 0.0;
    double firstMs = 0.0;
};

struct ReadAggregate {
    uint64_t calls = 0;
    uint64_t bytes = 0;
    double totalUs = 0.0;
    double maxUs = 0.0;
};

struct CallerAggregate {
    uint64_t openCalls = 0;
    uint64_t openMisses = 0;
    double openUs = 0.0;
    uint64_t readCalls = 0;
    uint64_t readBytes = 0;
    double readUs = 0.0;
    uint64_t existsCalls = 0;
    double existsUs = 0.0;
    uint64_t findCalls = 0;
    double findUs = 0.0;
    uint64_t fileTimeCalls = 0;
    double fileTimeUs = 0.0;
};

struct ExtAggregate {
    uint64_t calls = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t uniqueFiles = 0;
    double totalUs = 0.0;
    double maxUs = 0.0;
    uint64_t readCalls = 0;
    uint64_t readBytes = 0;
    double readUs = 0.0;
};

struct RipSample {
    int64_t tick = 0;
    uintptr_t rip = 0;
    uintptr_t callerRip = 0;
};

constexpr size_t kMaxSamples = 131072;
RipSample g_sampleBuf[kMaxSamples];
std::atomic<size_t> g_sampleCount{0};
std::atomic<bool> g_samplerActive{false};
std::atomic<DWORD> g_mainThreadId{0};
HANDLE g_mainThreadHandle = nullptr;
SRWLOCK g_mainThreadLock = SRWLOCK_INIT;

void CaptureMainThreadIfNeeded(DWORD tid) {
    if (g_mainThreadId.load(std::memory_order_acquire) != 0) return;
    AcquireSRWLockExclusive(&g_mainThreadLock);
    if (g_mainThreadId.load(std::memory_order_relaxed) == 0) {
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
        if (h) {
            g_mainThreadHandle = h;
            g_mainThreadId.store(tid, std::memory_order_release);
            WriteStatusLog("Main thread captured for RIP sampler: TID=" + std::to_string(tid));
        }
    }
    ReleaseSRWLockExclusive(&g_mainThreadLock);
}

struct Win32ReadAgg {
    Win32ReadPhaseStats byPhase[PHASE_COUNT];
    double maxUs = 0.0;
};

struct LoadSession {
    bool active = false;
    bool luaStartupSeen = false;
    bool spawnSeen = false;
    bool lightmapsSeen = false;
    bool compactSeen = false;
    bool prefetchLaunched = false;
    int64_t startTick = 0;
    int64_t luaStartupTick = 0;
    int64_t spawnTick = 0;
    int64_t lightmapsTick = 0;
    int64_t compactTick = 0;
    int64_t lastMilestoneTick = 0;
    SYSTEMTIME startWall{};
    std::string mapName = "unknown_map";
    std::string trigger;
    uint32_t sessionIndex = 0;
    OptStatsSnapshot optStart{};
    OptStatsSnapshot optEnd{};

    std::vector<TimelineEvent> events;
    std::vector<MilestoneEntry> milestones;
    std::unordered_map<std::string, ReadAggregate> readByFile;
    std::unordered_map<std::string, CallerAggregate> byCaller;
    std::unordered_map<std::string, Win32ReadAgg> win32ReadByFile;
    std::vector<RipSample> samples;
    std::vector<std::wstring> recDirs;
    std::vector<std::wstring> recFiles;
    std::vector<RecordedVpkBlock> recVpkBlocks;

    void Reset(uint32_t idx, int64_t nowTick, const std::string& trig) {
        active = true;
        luaStartupSeen = false;
        spawnSeen = false;
        lightmapsSeen = false;
        compactSeen = false;
        prefetchLaunched = false;
        startTick = nowTick;
        luaStartupTick = 0;
        spawnTick = 0;
        lightmapsTick = 0;
        compactTick = 0;
        lastMilestoneTick = nowTick;
        GetLocalTime(&startWall);
        mapName = "unknown_map";
        trigger = trig;
        sessionIndex = idx;
        optStart = g_optStats.Snapshot();
        optEnd = optStart;
        events.clear();
        events.reserve(65536);
        milestones.clear();
        milestones.reserve(64);
        readByFile.clear();
        readByFile.reserve(8192);
        byCaller.clear();
        byCaller.reserve(64);
        win32ReadByFile.clear();
        win32ReadByFile.reserve(1024);
        samples.clear();
        recDirs.clear();
        recFiles.clear();
        recVpkBlocks.clear();
    }
};

SRWLOCK g_sessionLock = SRWLOCK_INIT;
LoadSession g_session;
std::string g_loadedMapName;          // v16.4: map currently active in-game after normal load completion
bool        g_inGameOnMap = false;    // v16.4: blocks false BSP_Open_Fallback during DX9 restore / Lua BSP reads
int64_t     g_lastSessionEndTick = 0;

void FlushWin32TrackToSession(const Win32ReadTrack& tr) {
    uint64_t totalCalls = 0;
    for (int p = 0; p < PHASE_COUNT; ++p) totalCalls += tr.byPhase[p].calls;
    if (totalCalls == 0 || tr.path.empty()) return;
    AcquireSRWLockExclusive(&g_sessionLock);
    if (g_session.active) {
        auto& dst = g_session.win32ReadByFile[tr.path];
        for (int p = 0; p < PHASE_COUNT; ++p) {
            dst.byPhase[p].calls += tr.byPhase[p].calls;
            dst.byPhase[p].bytes += tr.byPhase[p].bytes;
            dst.byPhase[p].totalUs += tr.byPhase[p].totalUs;
        }
        if (tr.maxUs > dst.maxUs) dst.maxUs = tr.maxUs;
    }
    ReleaseSRWLockExclusive(&g_sessionLock);
}

void FlushAllWin32TracksIntoSessionLocked(LoadSession& sess) {
    for (size_t i = 0; i < kWin32TrackShards; ++i) {
        auto& sh = g_win32TrackShards[i];
        AcquireSRWLockShared(&sh.lock);
        for (auto& kv : sh.map) {
            auto& tr = *kv.second;
            AcquireSRWLockExclusive(&tr.handleLock);
            uint64_t totalCalls = 0;
            for (int p = 0; p < PHASE_COUNT; ++p) totalCalls += tr.byPhase[p].calls;
            if (totalCalls > 0 && !tr.path.empty()) {
                auto& dst = sess.win32ReadByFile[tr.path];
                for (int p = 0; p < PHASE_COUNT; ++p) {
                    dst.byPhase[p].calls += tr.byPhase[p].calls;
                    dst.byPhase[p].bytes += tr.byPhase[p].bytes;
                    dst.byPhase[p].totalUs += tr.byPhase[p].totalUs;
                    tr.byPhase[p] = {};
                }
                if (tr.maxUs > dst.maxUs) dst.maxUs = tr.maxUs;
                tr.maxUs = 0.0;
            }
            ReleaseSRWLockExclusive(&tr.handleLock);
        }
        ReleaseSRWLockShared(&sh.lock);
    }
}
std::atomic<uint32_t> g_sessionCounter{0};
std::atomic<uint64_t> g_logMessagesReceived{0};
thread_local bool tl_inHook = false;

std::string NormalizePath(const char* raw) {
    if (!raw || !*raw) return "";
    std::string s;
    s.reserve(std::strlen(raw));
    bool prevSlash = false;
    for (const char* p = raw; *p; ++p) {
        char c = *p;
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c == '/') {
            if (prevSlash) continue;
            prevSlash = true;
        } else {
            prevSlash = false;
        }
        s.push_back(c);
    }
    while (!s.empty() && (s[0] == '/' || (s.size() >= 2 && s[0] == '.' && s[1] == '/'))) {
        if (s[0] == '/') s.erase(0, 1);
        else s.erase(0, 2);
    }
    return s;
}

std::string SanitizeFilename(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') out.push_back(c);
        else out.push_back('_');
    }
    if (out.empty()) out = "unknown_map";
    if (out.size() > 64) out.resize(64);
    return out;
}

std::string ExtractExtension(const std::string& path) {
    auto slash = path.find_last_of('/');
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return "(none)";
    }
    std::string ext = path.substr(dot);
    if (ext.size() > 12) return "(other)";
    return ext;
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

std::string TrimLine(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    while (!s.empty() && (s.front() == '\r' || s.front() == '\n')) s.erase(s.begin());
    return s;
}

void RecordMilestoneLocked(const std::string& name, const std::string& detail, int64_t nowTick, uint32_t tid) {
    double tMs = g_timer.DeltaUs(g_session.startTick, nowTick) / 1000.0;
    double deltaMs = g_timer.DeltaUs(g_session.lastMilestoneTick, nowTick) / 1000.0;
    g_session.lastMilestoneTick = nowTick;

    MilestoneEntry m;
    m.name = name;
    m.tMs = tMs;
    m.deltaMs = deltaMs;
    m.tid = tid;
    m.detail = detail;
    g_session.milestones.push_back(std::move(m));

    TimelineEvent ev;
    ev.startTick = nowTick;
    ev.durUs = 0.0;
    ev.tid = tid;
    ev.kind = EventKind::Milestone;
    ev.hit = true;
    ev.primary = name;
    ev.secondary = detail;
    if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
}

// ============================================================================
// 9. Session Flush -> writes .log + .json (Chrome Trace + Sampler + OptStats)
// ============================================================================
void WriteSessionFiles(LoadSession&& sess, const std::string& endReason, int64_t endTick) {
    if (sess.active) {
        SavePrefetchManifest(sess.mapName, sess.recDirs, sess.recFiles);
        SaveVpkPrefetchManifest(sess.mapName, sess.recVpkBlocks);
    }
    if (!g_cfg.profilerEnabled || !sess.active || sess.events.empty()) return;

    const double totalWallMs = g_timer.DeltaUs(sess.startTick, endTick) / 1000.0;
    const double spawnMs = sess.spawnSeen ? (g_timer.DeltaUs(sess.startTick, sess.spawnTick) / 1000.0) : -1.0;
    const OptStatsSnapshot optDelta = SubOptStats(sess.optEnd, sess.optStart);

    AcquireSRWLockExclusive(&g_modCacheLock);
    RefreshModuleCacheLocked();
    ReleaseSRWLockExclusive(&g_modCacheLock);

    char tsBuf[64];
    std::snprintf(tsBuf, sizeof(tsBuf), "%04u%02u%02u_%02u%02u%02u",
                  sess.startWall.wYear, sess.startWall.wMonth, sess.startWall.wDay,
                  sess.startWall.wHour, sess.startWall.wMinute, sess.startWall.wSecond);
    std::string baseName = std::string("load_") + tsBuf + "_" + SanitizeFilename(sess.mapName);
    std::filesystem::path logPath = g_logDir / (baseName + ".log");
    std::filesystem::path jsonPath = g_logDir / (baseName + ".json");

    // 1) Aggregate main-thread RIP samples (v13: Per-Phase + .pdata Function Grouping + Per-Module Hotspots!)
    const double phaseWallMs[PHASE_COUNT] = {
        sess.luaStartupSeen ? (g_timer.DeltaUs(sess.startTick, sess.luaStartupTick) / 1000.0)
                            : (sess.spawnSeen ? spawnMs : totalWallMs),
        (sess.luaStartupSeen && sess.spawnSeen) ? (g_timer.DeltaUs(sess.luaStartupTick, sess.spawnTick) / 1000.0)
                                                : (sess.luaStartupSeen ? (g_timer.DeltaUs(sess.luaStartupTick, endTick) / 1000.0) : 0.0),
        sess.spawnSeen ? (g_timer.DeltaUs(sess.spawnTick, endTick) / 1000.0) : 0.0
    };
    auto classifyPhase = [&](int64_t tick) -> uint8_t {
        if (sess.spawnSeen && tick >= sess.spawnTick) return PHASE_POST_SPAWN;
        if (sess.luaStartupSeen && tick >= sess.luaStartupTick) return PHASE_LUA_TO_SPAWN;
        return PHASE_PRE_LUA;
    };

    struct FnHotspotAgg {
        uint64_t total = 0;
        uint64_t byPhase[PHASE_COUNT] = {0, 0, 0};
        uint32_t fnBegin = 0;
        uint32_t fnSize = 0;
    };

    std::unordered_map<std::string, uint64_t> samplesByMod;
    std::unordered_map<std::string, uint64_t> samplesByModPhase[PHASE_COUNT];
    std::unordered_map<std::string, uint64_t> samplesByAddr;
    std::unordered_map<std::string, uint64_t> samplesByWaitCaller;
    std::unordered_map<std::string, std::unordered_map<std::string, FnHotspotAgg>> perModuleFn;
    std::unordered_map<std::string, ModuleExportTable> exportCache;
    uint64_t phaseSampleCount[PHASE_COUNT] = {0, 0, 0};
    uint64_t totalSamples = sess.samples.size();

    for (const auto& s : sess.samples) {
        uint8_t ph = classifyPhase(s.tick);
        ++phaseSampleCount[ph];

        uintptr_t rva = 0, base = 0, end = 0;
        std::string mod = ResolveCallerModule(reinterpret_cast<void*>(s.rip), &rva, &base, &end);
        ++samplesByMod[mod];
        ++samplesByModPhase[ph][mod];

        std::string sym;
        if (base && end > base) {
            sym = ResolveExportSymbol(mod, base, end, rva, exportCache);
        }
        if (!sym.empty()) {
            ++samplesByAddr[sym];
        } else {
            char abuf[128];
            std::snprintf(abuf, sizeof(abuf), "%s+0x%llx", mod.c_str(), static_cast<unsigned long long>(rva));
            ++samplesByAddr[abuf];
        }

        // .pdata function-level grouping per module
        uint32_t fnBegin = static_cast<uint32_t>(rva), fnEnd = 0, fnSz = 0;
        bool hasPdata = (base && end > base) && LookupRuntimeFunctionBounds(base, end, static_cast<uint32_t>(rva), fnBegin, fnEnd);
        if (hasPdata && fnEnd > fnBegin) fnSz = fnEnd - fnBegin;
        std::string fnKey;
        if (!sym.empty()) {
            auto plusPos = sym.rfind("+0x");
            fnKey = (plusPos != std::string::npos) ? sym.substr(0, plusPos) : sym;
        } else {
            char fbuf[160];
            if (hasPdata) {
                std::snprintf(fbuf, sizeof(fbuf), "%s+fn:0x%x (sz=0x%x)", mod.c_str(), fnBegin, fnSz);
            } else {
                std::snprintf(fbuf, sizeof(fbuf), "%s+0x%llx", mod.c_str(), static_cast<unsigned long long>(rva & ~0x3FULL));
            }
            fnKey = fbuf;
        }
        auto& agg = perModuleFn[mod][fnKey];
        agg.total += 1;
        agg.byPhase[ph] += 1;
        agg.fnBegin = fnBegin;
        agg.fnSize = fnSz;

        if (s.callerRip != 0) {
            uintptr_t crva = 0, cbase = 0, cend = 0;
            std::string cmod = ResolveCallerModule(reinterpret_cast<void*>(s.callerRip), &crva, &cbase, &cend);
            std::string csym;
            if (cbase && cend > cbase) {
                csym = ResolveExportSymbol(cmod, cbase, cend, crva, exportCache);
            }
            if (!csym.empty()) {
                ++samplesByWaitCaller[csym];
            } else {
                char cbuf[128];
                std::snprintf(cbuf, sizeof(cbuf), "%s+0x%llx", cmod.c_str(), static_cast<unsigned long long>(crva));
                ++samplesByWaitCaller[cbuf];
            }
        }
    }

    // Prepare Top 20 Win32 ReadFile paths sorted by totalUs desc
    struct Win32ReadRow {
        std::string path;
        uint64_t calls = 0;
        uint64_t bytes = 0;
        double totalMs = 0.0;
        double phaseMs[PHASE_COUNT] = {0.0, 0.0, 0.0};
        double maxMs = 0.0;
    };
    std::vector<Win32ReadRow> win32ReadRows;
    win32ReadRows.reserve(sess.win32ReadByFile.size());
    double totalWin32ReadMs = 0.0;
    for (const auto& kv : sess.win32ReadByFile) {
        Win32ReadRow r;
        r.path = kv.first;
        r.maxMs = kv.second.maxUs / 1000.0;
        for (int p = 0; p < PHASE_COUNT; ++p) {
            r.calls += kv.second.byPhase[p].calls;
            r.bytes += kv.second.byPhase[p].bytes;
            r.phaseMs[p] = kv.second.byPhase[p].totalUs / 1000.0;
            r.totalMs += r.phaseMs[p];
        }
        totalWin32ReadMs += r.totalMs;
        win32ReadRows.push_back(std::move(r));
    }
    std::sort(win32ReadRows.begin(), win32ReadRows.end(), [](const Win32ReadRow& a, const Win32ReadRow& b) {
        return a.totalMs > b.totalMs;
    });

    std::vector<std::pair<std::string, uint64_t>> modSampleRows(samplesByMod.begin(), samplesByMod.end());
    std::sort(modSampleRows.begin(), modSampleRows.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    std::vector<std::pair<std::string, uint64_t>> hotspotRows(samplesByAddr.begin(), samplesByAddr.end());
    std::sort(hotspotRows.begin(), hotspotRows.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    if (hotspotRows.size() > 50) hotspotRows.resize(50);
    std::vector<std::pair<std::string, uint64_t>> waitCallerRows(samplesByWaitCaller.begin(), samplesByWaitCaller.end());
    std::sort(waitCallerRows.begin(), waitCallerRows.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    if (waitCallerRows.size() > 25) waitCallerRows.resize(25);

    // 2) Aggregate FS stats & compute determinism hashes
    std::unordered_map<std::string, FileAggregate> openByFile;
    std::unordered_map<std::string, FileAggregate> findByPattern;
    openByFile.reserve(8192);
    findByPattern.reserve(1024);

    uint64_t totalOpenCalls = 0, totalOpenHits = 0, totalOpenMisses = 0;
    uint64_t totalExistsCalls = 0, totalExistsHits = 0, totalExistsMisses = 0;
    uint64_t totalFindCalls = 0, totalFindHits = 0, totalFindMisses = 0;
    uint64_t totalFileTimeCalls = 0;
    uint64_t totalConsoleLines = 0;
    double totalOpenUs = 0.0, totalExistsUs = 0.0, totalFindUs = 0.0, totalFileTimeUs = 0.0;
    uint64_t orderedOpenHash = Fnv1a64Init();

    struct SlowOp {
        double durUs;
        double tMs;
        uint32_t tid;
        EventKind kind;
        bool hit;
        uint64_t bytes;
        std::string path;
        std::string extra;
        std::string caller;
    };
    std::vector<SlowOp> allFsOps;
    allFsOps.reserve(sess.events.size());

    for (const auto& ev : sess.events) {
        if (ev.kind == EventKind::Console) {
            ++totalConsoleLines;
            continue;
        }
        if (ev.kind == EventKind::Milestone) continue;

        double tMs = g_timer.DeltaUs(sess.startTick, ev.startTick) / 1000.0;
        allFsOps.push_back({ev.durUs, tMs, ev.tid, ev.kind, ev.hit, ev.bytes, ev.primary, ev.secondary, ev.caller});

        if (ev.kind == EventKind::FsOpen) {
            ++totalOpenCalls;
            totalOpenUs += ev.durUs;
            if (ev.hit) {
                ++totalOpenHits;
                orderedOpenHash = Fnv1a64UpdateStr(orderedOpenHash, ev.primary);
            } else {
                ++totalOpenMisses;
            }
            auto& agg = openByFile[ev.primary];
            if (agg.calls == 0) agg.firstMs = tMs;
            ++agg.calls;
            if (ev.hit) ++agg.hits; else ++agg.misses;
            agg.totalUs += ev.durUs;
            if (ev.durUs > agg.maxUs) agg.maxUs = ev.durUs;
        } else if (ev.kind == EventKind::FsExists) {
            ++totalExistsCalls;
            totalExistsUs += ev.durUs;
            if (ev.hit) ++totalExistsHits; else ++totalExistsMisses;
        } else if (ev.kind == EventKind::FsFindFirst) {
            ++totalFindCalls;
            totalFindUs += ev.durUs;
            if (ev.hit) ++totalFindHits; else ++totalFindMisses;
            auto& agg = findByPattern[ev.primary];
            if (agg.calls == 0) agg.firstMs = tMs;
            ++agg.calls;
            if (ev.hit) ++agg.hits; else ++agg.misses;
            agg.totalUs += ev.durUs;
            if (ev.durUs > agg.maxUs) agg.maxUs = ev.durUs;
        } else if (ev.kind == EventKind::FsGetFileTime) {
            ++totalFileTimeCalls;
            totalFileTimeUs += ev.durUs;
        }
    }

    uint64_t totalReadCalls = 0, totalReadBytes = 0;
    double totalReadUs = 0.0;
    for (const auto& kv : sess.readByFile) {
        totalReadCalls += kv.second.calls;
        totalReadBytes += kv.second.bytes;
        totalReadUs += kv.second.totalUs;
    }

    std::unordered_map<std::string, ExtAggregate> extMap;
    std::vector<std::string> uniqueLoadedFiles;
    uniqueLoadedFiles.reserve(openByFile.size());
    uint64_t uniqueMissCount = 0;

    for (const auto& kv : openByFile) {
        const std::string& path = kv.first;
        const FileAggregate& fa = kv.second;
        std::string ext = ExtractExtension(path);
        auto& ea = extMap[ext];
        ea.calls += fa.calls;
        ea.hits += fa.hits;
        ea.misses += fa.misses;
        ea.uniqueFiles += 1;
        ea.totalUs += fa.totalUs;
        if (fa.maxUs > ea.maxUs) ea.maxUs = fa.maxUs;

        if (fa.hits > 0) uniqueLoadedFiles.push_back(path);
        else ++uniqueMissCount;
    }
    for (const auto& kv : sess.readByFile) {
        std::string ext = ExtractExtension(kv.first);
        auto& ea = extMap[ext];
        ea.readCalls += kv.second.calls;
        ea.readBytes += kv.second.bytes;
        ea.readUs += kv.second.totalUs;
    }

    std::sort(uniqueLoadedFiles.begin(), uniqueLoadedFiles.end());
    uint64_t uniqueSetHash = Fnv1a64Init();
    for (const auto& f : uniqueLoadedFiles) {
        uniqueSetHash = Fnv1a64UpdateStr(uniqueSetHash, f);
    }

    std::vector<std::pair<std::string, ExtAggregate>> extRows(extMap.begin(), extMap.end());
    std::sort(extRows.begin(), extRows.end(), [](const auto& a, const auto& b) {
        return (a.second.totalUs + a.second.readUs) > (b.second.totalUs + b.second.readUs);
    });

    std::vector<std::pair<std::string, CallerAggregate>> callerRows(sess.byCaller.begin(), sess.byCaller.end());
    std::sort(callerRows.begin(), callerRows.end(), [](const auto& a, const auto& b) {
        double ta = a.second.openUs + a.second.readUs + a.second.existsUs + a.second.findUs + a.second.fileTimeUs;
        double tb = b.second.openUs + b.second.readUs + b.second.existsUs + b.second.findUs + b.second.fileTimeUs;
        return ta > tb;
    });

    std::vector<std::pair<std::string, FileAggregate>> repeated(openByFile.begin(), openByFile.end());
    std::sort(repeated.begin(), repeated.end(), [](const auto& a, const auto& b) {
        if (a.second.calls != b.second.calls) return a.second.calls > b.second.calls;
        return a.second.totalUs > b.second.totalUs;
    });
    if (repeated.size() > 40) repeated.resize(40);

    std::vector<std::pair<std::string, ReadAggregate>> topReads(sess.readByFile.begin(), sess.readByFile.end());
    std::sort(topReads.begin(), topReads.end(), [](const auto& a, const auto& b) {
        return a.second.totalUs > b.second.totalUs;
    });
    if (topReads.size() > 40) topReads.resize(40);

    std::vector<std::pair<std::string, FileAggregate>> topFinds(findByPattern.begin(), findByPattern.end());
    std::sort(topFinds.begin(), topFinds.end(), [](const auto& a, const auto& b) {
        return a.second.totalUs > b.second.totalUs;
    });
    if (topFinds.size() > 30) topFinds.resize(30);

    std::sort(allFsOps.begin(), allFsOps.end(), [](const auto& a, const auto& b) {
        return a.durUs > b.durUs;
    });
    if (allFsOps.size() > 40) allFsOps.resize(40);

    // ---------------- Write human-readable .log ----------------
    {
        std::ofstream out(logPath, std::ios::binary);
        if (out) {
            char hbuf[32];
            out << "================================================================================\n";
            out << "GMOD x64 MAP LOAD OPTIMIZER + PROFILER v16 REPORT\n";
            out << "================================================================================\n";
            out << "Session       : #" << sess.sessionIndex << " (" << baseName << ")\n";
            out << "Map           : " << sess.mapName << "\n";
            out << "Trigger       : " << sess.trigger << "\n";
            out << "End Reason    : " << endReason << "\n";
            out << "System RAM    : " << g_sysTotalRamMb << " MB total, "
                << g_sysAvailRamMbAtStart << " MB avail at start (profile="
                << g_cfg.ramProfile << "->" << g_effectiveRamProfile
                << ", storage=" << (g_storageIsHdd ? "HDD" : "SSD") << ")\n";
            out << "Optimizer     : " << (g_cfg.enableOptimizer ? "ENABLED" : "DISABLED")
                << " (dir_cache=" << (g_cfg.optDirCache ? 1 : 0)
                << ", extra_dll=" << (g_cfg.optExtraDllDirCache ? 1 : 0)
                << ", read_buf=" << (g_cfg.optReadBuffer ? g_cfg.readBufferSizeKb : 0) << "KB"
                << ", vfs_meta=" << (g_cfg.optVfsMetaCache ? 1 : 0)
                << ", vfs_ram=" << (g_cfg.optVfsRamCache ? g_cfg.vfsRamCacheMaxMb : 0) << "MB/" << g_cfg.vfsRamCacheMaxFileKb << "KB(vtx=" << g_cfg.vfsVtxSlurpMaxKb << "KB)"
                << ", prefetch=" << (g_cfg.optPrefetch ? g_cfg.prefetchThreads : 0) << "T"
                << ", startup_gma=" << (g_cfg.optStartupGmaWarm ? 1 : 0)
                << ", prio_boost=" << (g_cfg.optBoostLoadPriority ? 1 : 0)
                << ", setcursor_dedup=" << (g_cfg.optDedupSetCursor ? 1 : 0)
                << ", vfs_ra=" << (g_cfg.optVfsReadAhead ? 8192 : 0) << "KB"
                << ", write_buf=" << (g_cfg.optWriteBuffer ? 4096 : 0) << "KB"
                << ", bsp_pak=" << (g_cfg.optBspPakFastPath ? g_cfg.bspPakSlabMb : 0) << "MB"
                << ", vpk_cache=" << (g_cfg.optVpkBlockCache ? g_cfg.vpkBlockCacheMb : 0) << "MB"
                << ", vfree_quar=" << (g_cfg.optVirtualFreeQuarantine ? g_cfg.vfreeQuarantineMb : 0) << "MB"
                << ", vtf_ram=" << (g_cfg.optVtfRamCache ? g_cfg.vtfRamCacheMaxMb : 0) << "MB"
                << ", ram_guard=" << (g_cfg.optRamGuard ? g_cfg.ramGuardPct : 0) << "%)\n";
            out << std::fixed << std::setprecision(3);
            if (spawnMs >= 0.0) {
                out << "Time To Spawn : " << spawnMs << " ms (" << (spawnMs / 1000.0) << " s)\n";
            } else {
                out << "Time To Spawn : (not reached)\n";
            }
            out << "Total Window  : " << totalWallMs << " ms (" << (totalWallMs / 1000.0) << " s)\n";
            out << "Console Lines : " << totalConsoleLines << " captured in session ("
                << g_logMessagesReceived.load(std::memory_order_relaxed) << " process total)\n";
            out << "RIP Samples   : " << totalSamples << " (interval=" << g_cfg.sampleIntervalMs
                << " ms, main_tid=" << g_mainThreadId.load(std::memory_order_relaxed) << ")\n";
            out << "FS OpenEx     : " << totalOpenCalls << " calls (" << totalOpenHits << " hit, "
                << totalOpenMisses << " miss), sum=" << (totalOpenUs / 1000.0) << " ms\n";
            out << "FS ReadEx     : " << totalReadCalls << " calls, "
                << (static_cast<double>(totalReadBytes) / (1024.0 * 1024.0)) << " MB read, sum="
                << (totalReadUs / 1000.0) << " ms\n";
            out << "FS FileExists : " << totalExistsCalls << " calls (" << totalExistsHits << " hit, "
                << totalExistsMisses << " miss), sum=" << (totalExistsUs / 1000.0) << " ms\n";
            out << "FS FindFirstEx: " << totalFindCalls << " calls (" << totalFindHits << " hit, "
                << totalFindMisses << " miss), sum=" << (totalFindUs / 1000.0) << " ms\n";
            out << "FS GetFileTime: " << totalFileTimeCalls << " calls, sum=" << (totalFileTimeUs / 1000.0) << " ms\n";
            std::snprintf(hbuf, sizeof(hbuf), "0x%016llx", static_cast<unsigned long long>(uniqueSetHash));
            out << "Determinism   : unique_loaded=" << uniqueLoadedFiles.size()
                << ", unique_miss=" << uniqueMissCount
                << ", set_hash=" << hbuf;
            std::snprintf(hbuf, sizeof(hbuf), "0x%016llx", static_cast<unsigned long long>(orderedOpenHash));
            out << ", ordered_hash=" << hbuf << "\n";
            out << "Opt Counters  : dir_indexed=" << optDelta.dirSnapshotsIndexed
                << ", ancestor_pruned=" << optDelta.dirAncestorPruned
                << ", dir_fast_path=" << optDelta.dirFastPathHits
                << ", createfile_neg_blocked=" << optDelta.createFileNegBlocked
                << ", attr_hits=" << optDelta.attrCacheHits
                << ", attr_neg_blocked=" << optDelta.attrCacheNegBlocked
                << ", read_buf_hits=" << optDelta.readBufHits
                << ", read_buf_refills=" << optDelta.readBufRefills
                << ", seek_saved=" << optDelta.seekSyscallsSaved
                << ", size_saved=" << optDelta.sizeSyscallsSaved
                << ", vfs_exists_hits=" << optDelta.vfsExistsHits
                << ", vfs_time_hits=" << optDelta.vfsTimeHits
                << ", vfs_size_hits=" << optDelta.vfsSizeHits
                << ", vfs_open_miss_hits=" << optDelta.vfsOpenMissHits
                << ", vfs_ram_cached=" << optDelta.vfsRamCachedFiles
                << ", vfs_ram_open_hits=" << optDelta.vfsRamOpenHits
                << ", vfs_ram_read_hits=" << optDelta.vfsRamReadHits
                << ", vfs_ram_read_mb=" << (static_cast<double>(optDelta.vfsRamReadBytes) / (1024.0 * 1024.0))
                << ", vfs_ram_slurped=" << optDelta.vfsRamSlurpedHandles
                << ", vfs_slurp_skip_sz=" << optDelta.vfsRamSlurpSkipSize
                << ", vfs_slurp_fail=" << optDelta.vfsRamSlurpFail
                << ", prefetch_dirs=" << optDelta.prefetchDirsIndexed
                << ", prefetch_files=" << optDelta.prefetchFilesWarmed
                << ", prefetch_mb=" << (static_cast<double>(optDelta.prefetchBytesWarmed) / (1024.0 * 1024.0))
                << ", startup_gma_files=" << optDelta.startupGmaFilesWarmed
                << ", startup_gma_mb=" << (static_cast<double>(optDelta.startupGmaBytesWarmed) / (1024.0 * 1024.0))
                << ", dir_tl_l1_hits=" << optDelta.dirTlL1Hits
                << ", setcursor_deduped=" << optDelta.setCursorDeduped
                << ", vfs_ra_hits=" << optDelta.vfsRaHits
                << ", vfs_ra_refills=" << optDelta.vfsRaRefills
                << ", vfs_ra_mb=" << (static_cast<double>(optDelta.vfsRaBytes) / (1024.0 * 1024.0))
                << ", write_buf_hits=" << optDelta.writeBufHits
                << ", write_buf_flushes=" << optDelta.writeBufFlushes
                << ", write_buf_mb=" << (static_cast<double>(optDelta.writeBufBytes) / (1024.0 * 1024.0))
                << ", bsp_pak_indexed=" << optDelta.bspPakIndexed
                << ", bsp_pak_vhv_hits=" << optDelta.bspPakVhvHits
                << ", bsp_pak_neg_hits=" << optDelta.bspPakNegHits
                << ", bsp_pak_slab_mb=" << (static_cast<double>(optDelta.bspPakSlabBytes) / (1024.0 * 1024.0))
                << ", vtf_ram_cached=" << optDelta.vtfRamCached
                << ", vtf_ram_open_hits=" << optDelta.vtfRamOpenHits
                << ", vtf_ram_saved_mb=" << (static_cast<double>(optDelta.vtfRamSavedBytes) / (1024.0 * 1024.0))
                << ", map_pack_bypass_hits=" << optDelta.mapPackBypassHits
                << ", map_pack_saved_mb=" << (static_cast<double>(optDelta.mapPackSavedBytes) / (1024.0 * 1024.0)) << "\n"
                << "              vpk_block_hits=" << optDelta.vpkBlockHits
                << ", vpk_block_misses=" << optDelta.vpkBlockMisses
                << ", vpk_block_saved_mb=" << (static_cast<double>(optDelta.vpkBlockSavedBytes) / (1024.0 * 1024.0))
                << ", vpk_seek_saved=" << optDelta.vpkSeekSaved
                << ", win32_read_total_ms=" << totalWin32ReadMs
                << ", vfree_quar=" << optDelta.vfreeQuarantined
                << " (" << (static_cast<double>(optDelta.vfreeQuarantinedBytes) / (1024.0 * 1024.0)) << " MB)"
                << ", bsp_lump_warm_mb=" << (static_cast<double>(optDelta.bspLumpPrewarmBytes) / (1024.0 * 1024.0))
                << ", ram_guard_triggers=" << optDelta.ramGuardTriggers
                << ", ram_guard_freed_mb=" << (static_cast<double>(optDelta.ramGuardBytesFreed) / (1024.0 * 1024.0)) << "\n"
                << "              open_subtiming_ms: native=" << (static_cast<double>(optDelta.openNativeUs) / 1000.0)
                << ", vfs_slurp=" << (static_cast<double>(optDelta.openVfsSlurpUs) / 1000.0)
                << ", vtf_slurp=" << (static_cast<double>(optDelta.openVtfSlurpUs) / 1000.0) << "\n"
                << "              cache_diag: vfs_gen_bumps=" << optDelta.vfsGenBumps
                << ", vtf_miss=" << optDelta.vtfLookupMiss
                << ", vtf_rej_unk_res=" << optDelta.vtfRejUnknownResolved
                << ", vtf_evict=" << optDelta.vtfLruEvictions
                << ", vtf_gen_clear=" << optDelta.vtfGenClears
                << ", vtf_skip_sz=" << optDelta.vtfSlurpSkipSize
                << ", vfs_rej_unk_res=" << optDelta.vfsRejUnknownResolved
                << ", mp_rej(no_bsp=" << optDelta.mpRejNoRecentBsp
                << ",disk=" << optDelta.mpRejDiskHashFail
                << ",sz=" << optDelta.mpRejSizeMismatch
                << ",meta=" << optDelta.mpRejMetaMismatch
                << ",live=" << optDelta.mpRejLiveHashMismatch << ")\n\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "1. LOADING MILESTONES\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(12) << "t_ms" << std::setw(12) << "delta_ms"
                << std::setw(8) << "TID" << std::setw(28) << "Milestone" << "Detail\n";
            for (const auto& m : sess.milestones) {
                out << std::left << std::setw(12) << m.tMs
                    << std::setw(12) << m.deltaMs
                    << std::setw(8) << m.tid
                    << std::setw(28) << m.name
                    << m.detail << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "2. WALL-CLOCK TIME BY MODULE (Main-Thread RIP Sampler)\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(32) << "Module" << std::setw(10) << "Samples"
                << std::setw(10) << "Pct_%" << "Est_ms\n";
            for (const auto& kv : modSampleRows) {
                double pct = totalSamples ? (100.0 * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                double estMs = totalSamples ? (totalWallMs * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                out << std::left << std::setw(32) << kv.first
                    << std::setw(10) << kv.second
                    << std::setw(10) << pct
                    << estMs << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "3. TOP 50 HOTTEST CODE ADDRESSES (module.dll+0xRVA)\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(42) << "Address" << std::setw(10) << "Samples"
                << std::setw(10) << "Pct_%" << "Est_ms\n";
            for (const auto& kv : hotspotRows) {
                double pct = totalSamples ? (100.0 * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                double estMs = totalSamples ? (totalWallMs * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                out << std::left << std::setw(42) << kv.first
                    << std::setw(10) << kv.second
                    << std::setw(10) << pct
                    << estMs << "\n";
            }
            out << "\n";

            if (!waitCallerRows.empty()) {
                out << "--------------------------------------------------------------------------------\n";
                out << "3b. TOP 25 OS-WAIT CALLERS (Non-OS Stack Frame behind ntdll/kernelbase/win32u)\n";
                out << "--------------------------------------------------------------------------------\n";
                out << std::left << std::setw(42) << "Caller_Address" << std::setw(10) << "Samples"
                    << std::setw(10) << "Pct_%" << "Est_ms\n";
                for (const auto& kv : waitCallerRows) {
                    double pct = totalSamples ? (100.0 * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                    double estMs = totalSamples ? (totalWallMs * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                    out << std::left << std::setw(42) << kv.first
                        << std::setw(10) << kv.second
                        << std::setw(10) << pct
                        << estMs << "\n";
                }
                out << "\n";
            }

            out << "--------------------------------------------------------------------------------\n";
            out << "3c. PER-PHASE BREAKDOWN (Pre-Lua vs Lua->Spawn vs Post-Spawn Tail)\n";
            out << "--------------------------------------------------------------------------------\n";
            const char* phaseNames[PHASE_COUNT] = {"Phase0_Pre_Lua", "Phase1_Lua_to_Spawn", "Phase2_Post_Spawn"};
            for (int p = 0; p < PHASE_COUNT; ++p) {
                out << phaseNames[p] << ": wall_ms=" << phaseWallMs[p] << ", samples=" << phaseSampleCount[p] << "\n";
                std::vector<std::pair<std::string, uint64_t>> pMods(samplesByModPhase[p].begin(), samplesByModPhase[p].end());
                std::sort(pMods.begin(), pMods.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
                if (pMods.size() > 8) pMods.resize(8);
                for (const auto& kv : pMods) {
                    double est = phaseSampleCount[p] ? (phaseWallMs[p] * static_cast<double>(kv.second) / static_cast<double>(phaseSampleCount[p])) : 0.0;
                    out << "    " << std::left << std::setw(28) << kv.first << std::setw(8) << kv.second << est << " ms\n";
                }
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "3d. PER-MODULE CPU HOTSPOTS (.pdata Function-Grouped, Top 10 Pre-Spawn)\n";
            out << "--------------------------------------------------------------------------------\n";
            const char* focusMods[] = {"lua_shared.dll", "engine.dll", "shaderapidx9.dll", "materialsystem.dll", "filesystem_stdio.dll", "winmm.dll"};
            for (const char* fm : focusMods) {
                auto mit = perModuleFn.find(fm);
                if (mit == perModuleFn.end() || mit->second.empty()) continue;
                std::vector<std::pair<std::string, FnHotspotAgg>> frows(mit->second.begin(), mit->second.end());
                std::sort(frows.begin(), frows.end(), [](const auto& a, const auto& b) {
                    uint64_t ap = a.second.byPhase[0] + a.second.byPhase[1];
                    uint64_t bp = b.second.byPhase[0] + b.second.byPhase[1];
                    return ap != bp ? (ap > bp) : (a.second.total > b.second.total);
                });
                if (frows.size() > 10) frows.resize(10);
                out << "[" << fm << "]\n";
                for (const auto& kv : frows) {
                    double p0ms = phaseSampleCount[0] ? (phaseWallMs[0] * static_cast<double>(kv.second.byPhase[0]) / static_cast<double>(phaseSampleCount[0])) : 0.0;
                    double p1ms = phaseSampleCount[1] ? (phaseWallMs[1] * static_cast<double>(kv.second.byPhase[1]) / static_cast<double>(phaseSampleCount[1])) : 0.0;
                    double p2ms = phaseSampleCount[2] ? (phaseWallMs[2] * static_cast<double>(kv.second.byPhase[2]) / static_cast<double>(phaseSampleCount[2])) : 0.0;
                    out << "    " << std::left << std::setw(46) << kv.first
                        << " pre=" << (p0ms + p1ms) << "ms (p0=" << p0ms << ", p1=" << p1ms << ", post=" << p2ms
                        << ", n=" << kv.second.total << ")\n";
                }
            }
            out << "\n";

            if (!win32ReadRows.empty()) {
                out << "--------------------------------------------------------------------------------\n";
                out << "3e. TOP 20 SLOWEST WIN32 ReadFile PATHS (ZwReadFile Attribution)\n";
                out << "--------------------------------------------------------------------------------\n";
                out << std::left << std::setw(10) << "Total_ms" << std::setw(10) << "P0_ms"
                    << std::setw(10) << "P1_ms" << std::setw(9) << "Calls"
                    << std::setw(10) << "Read_MB" << std::setw(10) << "Max_ms" << "Path\n";
                size_t lim = std::min<size_t>(20, win32ReadRows.size());
                for (size_t i = 0; i < lim; ++i) {
                    const auto& r = win32ReadRows[i];
                    out << std::left << std::setw(10) << r.totalMs
                        << std::setw(10) << r.phaseMs[0]
                        << std::setw(10) << r.phaseMs[1]
                        << std::setw(9) << r.calls
                        << std::setw(10) << (static_cast<double>(r.bytes) / (1024.0 * 1024.0))
                        << std::setw(10) << r.maxMs
                        << r.path << "\n";
                }
                out << "\n";
            }

            out << "--------------------------------------------------------------------------------\n";
            out << "4. FILESYSTEM I/O BY CALLER DLL\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(26) << "CallerDLL" << std::setw(9) << "Opens"
                << std::setw(10) << "Open_ms" << std::setw(9) << "Reads"
                << std::setw(10) << "Read_MB" << std::setw(10) << "Read_ms"
                << std::setw(8) << "Exists" << std::setw(8) << "Finds" << "Find_ms\n";
            for (const auto& kv : callerRows) {
                const auto& c = kv.second;
                out << std::left << std::setw(26) << kv.first
                    << std::setw(9) << c.openCalls
                    << std::setw(10) << (c.openUs / 1000.0)
                    << std::setw(9) << c.readCalls
                    << std::setw(10) << (static_cast<double>(c.readBytes) / (1024.0 * 1024.0))
                    << std::setw(10) << (c.readUs / 1000.0)
                    << std::setw(8) << c.existsCalls
                    << std::setw(8) << c.findCalls
                    << (c.findUs / 1000.0) << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "5. FILE BREAKDOWN BY EXTENSION (Open + Read combined)\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(10) << "Ext" << std::setw(8) << "Opens"
                << std::setw(8) << "Uniq" << std::setw(8) << "Miss"
                << std::setw(10) << "Open_ms" << std::setw(9) << "Reads"
                << std::setw(10) << "Read_MB" << "Read_ms\n";
            for (const auto& kv : extRows) {
                const auto& e = kv.second;
                out << std::left << std::setw(10) << kv.first
                    << std::setw(8) << e.calls
                    << std::setw(8) << e.uniqueFiles
                    << std::setw(8) << e.misses
                    << std::setw(10) << (e.totalUs / 1000.0)
                    << std::setw(9) << e.readCalls
                    << std::setw(10) << (static_cast<double>(e.readBytes) / (1024.0 * 1024.0))
                    << (e.readUs / 1000.0) << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "6. TOP 40 MOST-REPEATED FILE OPENS\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(8) << "Calls" << std::setw(8) << "Hits"
                << std::setw(8) << "Miss" << std::setw(12) << "Total_ms"
                << std::setw(12) << "First_ms" << "Path\n";
            for (const auto& kv : repeated) {
                if (kv.second.calls < 2) break;
                out << std::left << std::setw(8) << kv.second.calls
                    << std::setw(8) << kv.second.hits
                    << std::setw(8) << kv.second.misses
                    << std::setw(12) << (kv.second.totalUs / 1000.0)
                    << std::setw(12) << kv.second.firstMs
                    << kv.first << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "7. TOP 40 SLOWEST READ FILES (by cumulative ReadEx time)\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(12) << "Read_ms" << std::setw(10) << "Calls"
                << std::setw(12) << "Read_MB" << std::setw(10) << "Max_ms" << "Path\n";
            for (const auto& kv : topReads) {
                out << std::left << std::setw(12) << (kv.second.totalUs / 1000.0)
                    << std::setw(10) << kv.second.calls
                    << std::setw(12) << (static_cast<double>(kv.second.bytes) / (1024.0 * 1024.0))
                    << std::setw(10) << (kv.second.maxUs / 1000.0)
                    << kv.first << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "8. TOP 30 DIRECTORY SCANS (FindFirstEx)\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(12) << "Total_ms" << std::setw(8) << "Calls"
                << std::setw(8) << "Miss" << std::setw(10) << "Max_ms" << "Pattern\n";
            for (const auto& kv : topFinds) {
                out << std::left << std::setw(12) << (kv.second.totalUs / 1000.0)
                    << std::setw(8) << kv.second.calls
                    << std::setw(8) << kv.second.misses
                    << std::setw(10) << (kv.second.maxUs / 1000.0)
                    << kv.first << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "9. TOP 40 SLOWEST SINGLE FS OPERATIONS\n";
            out << "--------------------------------------------------------------------------------\n";
            out << std::left << std::setw(11) << "Dur_ms" << std::setw(11) << "t_ms"
                << std::setw(8) << "Op" << std::setw(6) << "Res"
                << std::setw(22) << "Caller" << "Path\n";
            for (const auto& op : allFsOps) {
                const char* opName = "FS";
                switch (op.kind) {
                    case EventKind::FsOpen: opName = "OPEN"; break;
                    case EventKind::FsExists: opName = "EXISTS"; break;
                    case EventKind::FsRead: opName = "READ"; break;
                    case EventKind::FsFindFirst: opName = "FIND"; break;
                    case EventKind::FsGetFileTime: opName = "FTIME"; break;
                    default: break;
                }
                out << std::left << std::setw(11) << (op.durUs / 1000.0)
                    << std::setw(11) << op.tMs
                    << std::setw(8) << opName
                    << std::setw(6) << (op.hit ? "HIT" : "MISS")
                    << std::setw(22) << op.caller
                    << op.path;
                if (op.bytes > 0) out << " (" << op.bytes << " B)";
                if (!op.extra.empty()) out << " [" << op.extra << "]";
                out << "\n";
            }
            out << "\n";

            out << "--------------------------------------------------------------------------------\n";
            out << "10. CHRONOLOGICAL TIMELINE\n";
            out << "--------------------------------------------------------------------------------\n";
            for (const auto& ev : sess.events) {
                if (ev.kind == EventKind::Console && !g_cfg.logConsoleLines) continue;
                if (ev.kind != EventKind::Console && ev.kind != EventKind::Milestone && !g_cfg.logEveryFileEvent) continue;
                double tMs = g_timer.DeltaUs(sess.startTick, ev.startTick) / 1000.0;
                char prefix[112];
                const char* tag = "CON";
                switch (ev.kind) {
                    case EventKind::Milestone: tag = "MILESTONE"; break;
                    case EventKind::Console: tag = "CON"; break;
                    case EventKind::FsOpen: tag = ev.hit ? "OPEN_OK" : "OPEN_MISS"; break;
                    case EventKind::FsExists: tag = ev.hit ? "EX_OK" : "EX_MISS"; break;
                    case EventKind::FsRead: tag = "READ"; break;
                    case EventKind::FsFindFirst: tag = ev.hit ? "FIND_OK" : "FIND_MISS"; break;
                    case EventKind::FsGetFileTime: tag = ev.hit ? "FTIME_OK" : "FTIME_MS"; break;
                }
                std::snprintf(prefix, sizeof(prefix), "[+%10.3f ms] [%8.1f us] [T:%5u] [%-9s] ",
                              tMs, ev.durUs, ev.tid, tag);
                out << prefix << ev.primary;
                if (ev.bytes > 0) out << " (" << ev.bytes << " B)";
                if (!ev.caller.empty()) out << " @" << ev.caller;
                if (!ev.secondary.empty()) out << " | " << ev.secondary;
                out << "\n";
            }
        }
    }

    // ---------------- Write machine-readable .json ----------------
    {
        std::ofstream j(jsonPath, std::ios::binary);
        if (j) {
            char setHashHex[32], ordHashHex[32];
            std::snprintf(setHashHex, sizeof(setHashHex), "0x%016llx", static_cast<unsigned long long>(uniqueSetHash));
            std::snprintf(ordHashHex, sizeof(ordHashHex), "0x%016llx", static_cast<unsigned long long>(orderedOpenHash));

            j << std::fixed << std::setprecision(3);
            j << "{\n";
            j << "  \"meta\": {\n";
            j << "    \"profiler_version\": 16,\n";
            j << "    \"optimizer_enabled\": " << (g_cfg.enableOptimizer ? "true" : "false") << ",\n";
            j << "    \"ram_profile_cfg\": \"" << JsonEscape(g_cfg.ramProfile) << "\",\n";
            j << "    \"ram_profile_effective\": \"" << JsonEscape(g_effectiveRamProfile) << "\",\n";
            j << "    \"sys_total_ram_mb\": " << g_sysTotalRamMb << ",\n";
            j << "    \"sys_avail_ram_mb_at_start\": " << g_sysAvailRamMbAtStart << ",\n";
            j << "    \"storage_is_hdd\": " << (g_storageIsHdd ? "true" : "false") << ",\n";
            j << "    \"session_id\": \"" << JsonEscape(baseName) << "\",\n";
            j << "    \"map\": \"" << JsonEscape(sess.mapName) << "\",\n";
            j << "    \"trigger\": \"" << JsonEscape(sess.trigger) << "\",\n";
            j << "    \"end_reason\": \"" << JsonEscape(endReason) << "\",\n";
            j << "    \"time_to_spawn_ms\": " << spawnMs << ",\n";
            j << "    \"total_window_ms\": " << totalWallMs << ",\n";
            j << "    \"console_lines\": " << totalConsoleLines << ",\n";
            j << "    \"rip_samples\": " << totalSamples << ",\n";
            j << "    \"fs_open_calls\": " << totalOpenCalls << ",\n";
            j << "    \"fs_open_hits\": " << totalOpenHits << ",\n";
            j << "    \"fs_open_misses\": " << totalOpenMisses << ",\n";
            j << "    \"fs_open_total_ms\": " << (totalOpenUs / 1000.0) << ",\n";
            j << "    \"fs_read_calls\": " << totalReadCalls << ",\n";
            j << "    \"fs_read_mb\": " << (static_cast<double>(totalReadBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"fs_read_total_ms\": " << (totalReadUs / 1000.0) << ",\n";
            j << "    \"fs_exists_calls\": " << totalExistsCalls << ",\n";
            j << "    \"fs_exists_hits\": " << totalExistsHits << ",\n";
            j << "    \"fs_exists_misses\": " << totalExistsMisses << ",\n";
            j << "    \"fs_exists_total_ms\": " << (totalExistsUs / 1000.0) << ",\n";
            j << "    \"fs_find_calls\": " << totalFindCalls << ",\n";
            j << "    \"fs_find_total_ms\": " << (totalFindUs / 1000.0) << ",\n";
            j << "    \"fs_filetime_calls\": " << totalFileTimeCalls << ",\n";
            j << "    \"fs_filetime_total_ms\": " << (totalFileTimeUs / 1000.0) << "\n";
            j << "  },\n";

            j << "  \"optimizer\": {\n";
            j << "    \"dir_snapshots_indexed\": " << optDelta.dirSnapshotsIndexed << ",\n";
            j << "    \"dir_ancestor_pruned\": " << optDelta.dirAncestorPruned << ",\n";
            j << "    \"dir_fast_path_hits\": " << optDelta.dirFastPathHits << ",\n";
            j << "    \"createfile_neg_blocked\": " << optDelta.createFileNegBlocked << ",\n";
            j << "    \"attr_cache_hits\": " << optDelta.attrCacheHits << ",\n";
            j << "    \"attr_cache_neg_blocked\": " << optDelta.attrCacheNegBlocked << ",\n";
            j << "    \"read_buf_hits\": " << optDelta.readBufHits << ",\n";
            j << "    \"read_buf_refills\": " << optDelta.readBufRefills << ",\n";
            j << "    \"seek_syscalls_saved\": " << optDelta.seekSyscallsSaved << ",\n";
            j << "    \"size_syscalls_saved\": " << optDelta.sizeSyscallsSaved << ",\n";
            j << "    \"vfs_exists_hits\": " << optDelta.vfsExistsHits << ",\n";
            j << "    \"vfs_time_hits\": " << optDelta.vfsTimeHits << ",\n";
            j << "    \"vfs_size_hits\": " << optDelta.vfsSizeHits << ",\n";
            j << "    \"vfs_open_miss_hits\": " << optDelta.vfsOpenMissHits << ",\n";
            j << "    \"vfs_ram_cached_files\": " << optDelta.vfsRamCachedFiles << ",\n";
            j << "    \"vfs_ram_open_hits\": " << optDelta.vfsRamOpenHits << ",\n";
            j << "    \"vfs_ram_read_hits\": " << optDelta.vfsRamReadHits << ",\n";
            j << "    \"vfs_ram_read_mb\": " << (static_cast<double>(optDelta.vfsRamReadBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"vfs_ram_slurped_handles\": " << optDelta.vfsRamSlurpedHandles << ",\n";
            j << "    \"vfs_ram_slurp_skip_size\": " << optDelta.vfsRamSlurpSkipSize << ",\n";
            j << "    \"vfs_ram_slurp_fail\": " << optDelta.vfsRamSlurpFail << ",\n";
            j << "    \"prefetch_dirs_indexed\": " << optDelta.prefetchDirsIndexed << ",\n";
            j << "    \"prefetch_files_warmed\": " << optDelta.prefetchFilesWarmed << ",\n";
            j << "    \"prefetch_mb_warmed\": " << (static_cast<double>(optDelta.prefetchBytesWarmed) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"startup_gma_files_warmed\": " << optDelta.startupGmaFilesWarmed << ",\n";
            j << "    \"startup_gma_mb_warmed\": " << (static_cast<double>(optDelta.startupGmaBytesWarmed) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"dir_tl_l1_hits\": " << optDelta.dirTlL1Hits << ",\n";
            j << "    \"setcursor_deduped\": " << optDelta.setCursorDeduped << ",\n";
            j << "    \"vfs_ra_hits\": " << optDelta.vfsRaHits << ",\n";
            j << "    \"vfs_ra_refills\": " << optDelta.vfsRaRefills << ",\n";
            j << "    \"vfs_ra_mb\": " << (static_cast<double>(optDelta.vfsRaBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"write_buf_hits\": " << optDelta.writeBufHits << ",\n";
            j << "    \"write_buf_flushes\": " << optDelta.writeBufFlushes << ",\n";
            j << "    \"write_buf_mb\": " << (static_cast<double>(optDelta.writeBufBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"bsp_pak_indexed\": " << optDelta.bspPakIndexed << ",\n";
            j << "    \"bsp_pak_vhv_hits\": " << optDelta.bspPakVhvHits << ",\n";
            j << "    \"bsp_pak_neg_hits\": " << optDelta.bspPakNegHits << ",\n";
            j << "    \"bsp_pak_slab_mb\": " << (static_cast<double>(optDelta.bspPakSlabBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"vtf_ram_cached\": " << optDelta.vtfRamCached << ",\n";
            j << "    \"vtf_ram_open_hits\": " << optDelta.vtfRamOpenHits << ",\n";
            j << "    \"vtf_ram_saved_mb\": " << (static_cast<double>(optDelta.vtfRamSavedBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"map_pack_bypass_hits\": " << optDelta.mapPackBypassHits << ",\n";
            j << "    \"map_pack_saved_mb\": " << (static_cast<double>(optDelta.mapPackSavedBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"vpk_block_hits\": " << optDelta.vpkBlockHits << ",\n";
            j << "    \"vpk_block_misses\": " << optDelta.vpkBlockMisses << ",\n";
            j << "    \"vpk_block_saved_mb\": " << (static_cast<double>(optDelta.vpkBlockSavedBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"vpk_seek_saved\": " << optDelta.vpkSeekSaved << ",\n";
            j << "    \"open_native_ms\": " << (static_cast<double>(optDelta.openNativeUs) / 1000.0) << ",\n";
            j << "    \"open_vfs_slurp_ms\": " << (static_cast<double>(optDelta.openVfsSlurpUs) / 1000.0) << ",\n";
            j << "    \"open_vtf_slurp_ms\": " << (static_cast<double>(optDelta.openVtfSlurpUs) / 1000.0) << ",\n";
            j << "    \"vfs_gen_bumps\": " << optDelta.vfsGenBumps << ",\n";
            j << "    \"vtf_lookup_miss\": " << optDelta.vtfLookupMiss << ",\n";
            j << "    \"vtf_rej_unknown_resolved\": " << optDelta.vtfRejUnknownResolved << ",\n";
            j << "    \"vtf_lru_evictions\": " << optDelta.vtfLruEvictions << ",\n";
            j << "    \"vtf_gen_clears\": " << optDelta.vtfGenClears << ",\n";
            j << "    \"vtf_slurp_skip_size\": " << optDelta.vtfSlurpSkipSize << ",\n";
            j << "    \"vfs_rej_unknown_resolved\": " << optDelta.vfsRejUnknownResolved << ",\n";
            j << "    \"mp_rej_no_recent_bsp\": " << optDelta.mpRejNoRecentBsp << ",\n";
            j << "    \"mp_rej_disk_hash_fail\": " << optDelta.mpRejDiskHashFail << ",\n";
            j << "    \"mp_rej_size_mismatch\": " << optDelta.mpRejSizeMismatch << ",\n";
            j << "    \"mp_rej_meta_mismatch\": " << optDelta.mpRejMetaMismatch << ",\n";
            j << "    \"mp_rej_live_hash_mismatch\": " << optDelta.mpRejLiveHashMismatch << ",\n";
            j << "    \"vfree_quarantined\": " << optDelta.vfreeQuarantined << ",\n";
            j << "    \"vfree_quarantined_mb\": " << (static_cast<double>(optDelta.vfreeQuarantinedBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"bsp_lump_prewarm_mb\": " << (static_cast<double>(optDelta.bspLumpPrewarmBytes) / (1024.0 * 1024.0)) << ",\n";
            j << "    \"ram_guard_triggers\": " << optDelta.ramGuardTriggers << ",\n";
            j << "    \"ram_guard_freed_mb\": " << (static_cast<double>(optDelta.ramGuardBytesFreed) / (1024.0 * 1024.0)) << "\n";
            j << "  },\n";

            j << "  \"phases\": [\n";
            const char* jPhaseNames[PHASE_COUNT] = {"pre_lua", "lua_to_spawn", "post_spawn"};
            for (int p = 0; p < PHASE_COUNT; ++p) {
                j << "    {\"name\": \"" << jPhaseNames[p] << "\", \"wall_ms\": " << phaseWallMs[p]
                  << ", \"samples\": " << phaseSampleCount[p] << ", \"top_modules\": [";
                std::vector<std::pair<std::string, uint64_t>> pMods(samplesByModPhase[p].begin(), samplesByModPhase[p].end());
                std::sort(pMods.begin(), pMods.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
                if (pMods.size() > 8) pMods.resize(8);
                for (size_t k = 0; k < pMods.size(); ++k) {
                    double est = phaseSampleCount[p] ? (phaseWallMs[p] * static_cast<double>(pMods[k].second) / static_cast<double>(phaseSampleCount[p])) : 0.0;
                    j << "{\"module\": \"" << JsonEscape(pMods[k].first) << "\", \"samples\": " << pMods[k].second
                      << ", \"est_ms\": " << est << "}" << (k + 1 < pMods.size() ? ", " : "");
                }
                j << "]}" << (p + 1 < PHASE_COUNT ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"top_win32_reads\": [\n";
            size_t wLim = std::min<size_t>(20, win32ReadRows.size());
            for (size_t i = 0; i < wLim; ++i) {
                const auto& r = win32ReadRows[i];
                j << "    {\"path\": \"" << JsonEscape(r.path) << "\", \"total_ms\": " << r.totalMs
                  << ", \"p0_ms\": " << r.phaseMs[0] << ", \"p1_ms\": " << r.phaseMs[1]
                  << ", \"calls\": " << r.calls << ", \"mb\": " << (static_cast<double>(r.bytes) / (1024.0 * 1024.0))
                  << ", \"max_ms\": " << r.maxMs << "}" << (i + 1 < wLim ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"milestones\": [\n";
            for (size_t i = 0; i < sess.milestones.size(); ++i) {
                const auto& m = sess.milestones[i];
                j << "    {\"name\": \"" << JsonEscape(m.name) << "\", \"t_ms\": " << m.tMs
                  << ", \"delta_ms\": " << m.deltaMs << ", \"tid\": " << m.tid
                  << ", \"detail\": \"" << JsonEscape(m.detail) << "\"}"
                  << (i + 1 < sess.milestones.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"sampler_modules\": [\n";
            for (size_t i = 0; i < modSampleRows.size(); ++i) {
                const auto& kv = modSampleRows[i];
                double pct = totalSamples ? (100.0 * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                double estMs = totalSamples ? (totalWallMs * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                j << "    {\"module\": \"" << JsonEscape(kv.first) << "\", \"samples\": " << kv.second
                  << ", \"pct\": " << pct << ", \"est_ms\": " << estMs << "}"
                  << (i + 1 < modSampleRows.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"sampler_hotspots\": [\n";
            for (size_t i = 0; i < hotspotRows.size(); ++i) {
                const auto& kv = hotspotRows[i];
                double pct = totalSamples ? (100.0 * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                double estMs = totalSamples ? (totalWallMs * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                j << "    {\"addr\": \"" << JsonEscape(kv.first) << "\", \"samples\": " << kv.second
                  << ", \"pct\": " << pct << ", \"est_ms\": " << estMs << "}"
                  << (i + 1 < hotspotRows.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"sampler_wait_callers\": [\n";
            for (size_t i = 0; i < waitCallerRows.size(); ++i) {
                const auto& kv = waitCallerRows[i];
                double pct = totalSamples ? (100.0 * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                double estMs = totalSamples ? (totalWallMs * static_cast<double>(kv.second) / static_cast<double>(totalSamples)) : 0.0;
                j << "    {\"addr\": \"" << JsonEscape(kv.first) << "\", \"samples\": " << kv.second
                  << ", \"pct\": " << pct << ", \"est_ms\": " << estMs << "}"
                  << (i + 1 < waitCallerRows.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"fs_by_caller\": [\n";
            for (size_t i = 0; i < callerRows.size(); ++i) {
                const auto& kv = callerRows[i];
                const auto& c = kv.second;
                j << "    {\"caller\": \"" << JsonEscape(kv.first) << "\", \"open_calls\": " << c.openCalls
                  << ", \"open_ms\": " << (c.openUs / 1000.0) << ", \"read_calls\": " << c.readCalls
                  << ", \"read_mb\": " << (static_cast<double>(c.readBytes) / (1024.0 * 1024.0))
                  << ", \"read_ms\": " << (c.readUs / 1000.0) << ", \"exists_calls\": " << c.existsCalls
                  << ", \"exists_ms\": " << (c.existsUs / 1000.0) << ", \"find_calls\": " << c.findCalls
                  << ", \"find_ms\": " << (c.findUs / 1000.0) << "}"
                  << (i + 1 < callerRows.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"extensions\": [\n";
            for (size_t i = 0; i < extRows.size(); ++i) {
                const auto& kv = extRows[i];
                const auto& e = kv.second;
                j << "    {\"ext\": \"" << JsonEscape(kv.first) << "\", \"calls\": " << e.calls
                  << ", \"unique\": " << e.uniqueFiles << ", \"hits\": " << e.hits
                  << ", \"misses\": " << e.misses << ", \"total_ms\": " << (e.totalUs / 1000.0)
                  << ", \"max_ms\": " << (e.maxUs / 1000.0) << ", \"read_calls\": " << e.readCalls
                  << ", \"read_mb\": " << (static_cast<double>(e.readBytes) / (1024.0 * 1024.0))
                  << ", \"read_ms\": " << (e.readUs / 1000.0) << "}"
                  << (i + 1 < extRows.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"top_repeated\": [\n";
            for (size_t i = 0; i < repeated.size(); ++i) {
                const auto& kv = repeated[i];
                j << "    {\"path\": \"" << JsonEscape(kv.first) << "\", \"calls\": " << kv.second.calls
                  << ", \"hits\": " << kv.second.hits << ", \"misses\": " << kv.second.misses
                  << ", \"total_ms\": " << (kv.second.totalUs / 1000.0) << "}"
                  << (i + 1 < repeated.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"top_reads\": [\n";
            for (size_t i = 0; i < topReads.size(); ++i) {
                const auto& kv = topReads[i];
                j << "    {\"path\": \"" << JsonEscape(kv.first) << "\", \"calls\": " << kv.second.calls
                  << ", \"read_mb\": " << (static_cast<double>(kv.second.bytes) / (1024.0 * 1024.0))
                  << ", \"read_ms\": " << (kv.second.totalUs / 1000.0) << "}"
                  << (i + 1 < topReads.size() ? "," : "") << "\n";
            }
            j << "  ],\n";

            j << "  \"determinism\": {\n";
            j << "    \"unique_loaded_count\": " << uniqueLoadedFiles.size() << ",\n";
            j << "    \"total_loaded_opens\": " << totalOpenHits << ",\n";
            j << "    \"unique_miss_count\": " << uniqueMissCount << ",\n";
            j << "    \"hash_unique_set_fnv1a64\": \"" << setHashHex << "\",\n";
            j << "    \"hash_ordered_opens_fnv1a64\": \"" << ordHashHex << "\",\n";
            j << "    \"unique_loaded_files\": [\n";
            for (size_t i = 0; i < uniqueLoadedFiles.size(); ++i) {
                j << "      \"" << JsonEscape(uniqueLoadedFiles[i]) << "\""
                  << (i + 1 < uniqueLoadedFiles.size() ? "," : "") << "\n";
            }
            j << "    ]\n";
            j << "  },\n";

            j << "  \"traceEvents\": [\n";
            bool firstTrace = true;
            for (const auto& ev : sess.events) {
                if (ev.kind == EventKind::Milestone) {
                    double tsUs = g_timer.DeltaUs(sess.startTick, ev.startTick);
                    if (!firstTrace) j << ",\n";
                    firstTrace = false;
                    j << "    {\"name\": \"" << JsonEscape(ev.primary)
                      << "\", \"cat\": \"milestone\", \"ph\": \"i\", \"s\": \"g\", \"pid\": 1, \"tid\": "
                      << ev.tid << ", \"ts\": " << tsUs
                      << ", \"args\": {\"detail\": \"" << JsonEscape(ev.secondary) << "\"}}";
                } else if (ev.kind != EventKind::Console) {
                    if (ev.durUs < static_cast<double>(g_cfg.chromeTraceMinUs)) continue;
                    const char* cat = "fs";
                    switch (ev.kind) {
                        case EventKind::FsOpen: cat = "fs.open"; break;
                        case EventKind::FsExists: cat = "fs.exists"; break;
                        case EventKind::FsRead: cat = "fs.read"; break;
                        case EventKind::FsFindFirst: cat = "fs.find"; break;
                        case EventKind::FsGetFileTime: cat = "fs.filetime"; break;
                        default: break;
                    }
                    double tsUs = g_timer.DeltaUs(sess.startTick, ev.startTick);
                    if (!firstTrace) j << ",\n";
                    firstTrace = false;
                    j << "    {\"name\": \"" << JsonEscape(ev.primary)
                      << "\", \"cat\": \"" << cat << "\", \"ph\": \"X\", \"pid\": 1, \"tid\": " << ev.tid
                      << ", \"ts\": " << tsUs << ", \"dur\": " << ev.durUs
                      << ", \"args\": {\"hit\": " << (ev.hit ? "true" : "false")
                      << ", \"caller\": \"" << JsonEscape(ev.caller)
                      << "\", \"bytes\": " << ev.bytes
                      << ", \"extra\": \"" << JsonEscape(ev.secondary) << "\"}}";
                }
            }
            j << "\n  ]\n";
            j << "}\n";
        }
    }

    WriteStatusLog("Saved load report: " + baseName + ".log / .json (spawn_ms=" +
                   std::to_string(spawnMs) + ", total_ms=" + std::to_string(totalWallMs) +
                   ", neg_blocked=" + std::to_string(optDelta.createFileNegBlocked) +
                   ", vfs_ram_opens=" + std::to_string(optDelta.vfsRamOpenHits) +
                   ", vfs_ram_reads=" + std::to_string(optDelta.vfsRamReadHits) +
                   ", vfs_slurped=" + std::to_string(optDelta.vfsRamSlurpedHandles) +
                   ", pf_dirs=" + std::to_string(optDelta.prefetchDirsIndexed) +
                   ", pf_files=" + std::to_string(optDelta.prefetchFilesWarmed) +
                   ", vfs_meta_hits=" + std::to_string(optDelta.vfsExistsHits + optDelta.vfsTimeHits + optDelta.vfsSizeHits + optDelta.vfsOpenMissHits) +
                   ", tl_l1=" + std::to_string(optDelta.dirTlL1Hits) +
                   ", setcursor_dedup=" + std::to_string(optDelta.setCursorDeduped) +
                   ", vfs_ra_hits=" + std::to_string(optDelta.vfsRaHits) +
                   ", vfs_ra_refills=" + std::to_string(optDelta.vfsRaRefills) +
                   ", wb_hits=" + std::to_string(optDelta.writeBufHits) +
                   ", wb_flushes=" + std::to_string(optDelta.writeBufFlushes) +
                   ", pak_vhv=" + std::to_string(optDelta.bspPakVhvHits) +
                   ", pak_neg=" + std::to_string(optDelta.bspPakNegHits) +
                   ", vtf_hits=" + std::to_string(optDelta.vtfRamOpenHits) +
                   ", vpk_hits=" + std::to_string(optDelta.vpkBlockHits) +
                   ", mp_hits=" + std::to_string(optDelta.mapPackBypassHits) +
                   ", vfree_q=" + std::to_string(optDelta.vfreeQuarantined) +
                   ", ram_guard=" + std::to_string(optDelta.ramGuardTriggers) +
                   "/" + std::to_string(optDelta.ramGuardBytesFreed / (1024ULL * 1024ULL)) + "MB)");
}

void FlushAllOpenRealHandleStatsIntoSessionLocked(LoadSession& sess);

void FinalizeSessionAsyncOrSync(const std::string& reason, bool sync) {
    ApplyLoadPriorityBoost(false);
    CancelActivePrefetch();
    g_vtfGeneration.fetch_add(1, std::memory_order_acq_rel);
    g_bspWarmGen.fetch_add(1, std::memory_order_acq_rel);
    ClearVtfRamCache();
    ClearBspPakIndex();
    // v15: Layer 10 VPK Block Cache stays persistent across map loads (cleared only on .vpk write or process exit!)
    DrainVirtualFreeQuarantine(!sync);
    LoadSession finished;
    int64_t endTick = g_timer.NowTick();

    g_samplerActive.store(false, std::memory_order_release);
    size_t sampleN = std::min(g_sampleCount.load(std::memory_order_acquire), kMaxSamples);

    std::vector<std::wstring> recDirs, recFiles;
    std::vector<RecordedVpkBlock> recVpkBlocks;
    ExtractPrefetchRecorded(recDirs, recFiles, recVpkBlocks);

    AcquireSRWLockExclusive(&g_sessionLock);
    if (!g_session.active) {
        ReleaseSRWLockExclusive(&g_sessionLock);
        return;
    }
    FlushAllOpenRealHandleStatsIntoSessionLocked(g_session);
    FlushAllWin32TracksIntoSessionLocked(g_session);
    g_session.optEnd = g_optStats.Snapshot();
    RecordMilestoneLocked("Session_Finalized", reason, endTick, GetCurrentThreadId());
    g_session.recDirs = std::move(recDirs);
    g_session.recFiles = std::move(recFiles);
    g_session.recVpkBlocks = std::move(recVpkBlocks);
    const bool isNormalComplete =
        (reason == "post_spawn_tail_complete" ||
         reason == "mp_lightmaps_tail_complete" ||
         reason == "mp_compact_tail_complete" ||
         reason == "mp_lua_startup_timeout" ||
         reason == "no_listener_timeout");
    if (isNormalComplete && g_session.mapName != "unknown_map") {
        g_loadedMapName = g_session.mapName;
        g_inGameOnMap = true;
    } else if (reason == "disconnect_or_host_error") {
        g_loadedMapName.clear();
        g_inGameOnMap = false;
    }
    g_lastSessionEndTick = endTick;
    finished = std::move(g_session);
    g_session.active = false;
    g_sessionActive.store(false, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_sessionLock);

    if (sampleN > 0) {
        finished.samples.assign(g_sampleBuf, g_sampleBuf + sampleN);
    }

    if (!g_cfg.profilerEnabled) {
        SavePrefetchManifest(finished.mapName, finished.recDirs, finished.recFiles);
        SaveVpkPrefetchManifest(finished.mapName, finished.recVpkBlocks);
        return;
    }

    if (sync) {
        WriteSessionFiles(std::move(finished), reason, endTick);
    } else {
        struct FlushJob {
            LoadSession sess;
            std::string reason;
            int64_t endTick;
        };
        auto* job = new FlushJob{std::move(finished), reason, endTick};
        HANDLE h = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
            auto* j = static_cast<FlushJob*>(p);
            WriteSessionFiles(std::move(j->sess), j->reason, j->endTick);
            delete j;
            return 0;
        }, job, 0, nullptr);
        if (h) CloseHandle(h);
        else {
            WriteSessionFiles(std::move(job->sess), job->reason, job->endTick);
            delete job;
        }
    }
}

void StartNewSession(const std::string& trigger, const std::string& initialMap) {
    // Refresh caches on new map load so freshly compiled maps/addons are always seen
    ClearVfsCacheAll();
    g_vtfGeneration.fetch_add(1, std::memory_order_acq_rel);
    g_currentPhase.store(PHASE_PRE_LUA, std::memory_order_release);
    ClearVtfRamCache();
    ClearBspPakIndex();
    ClearPendingMapPack();
    DrainVirtualFreeQuarantine(true);
    if (g_cfg.clearDirCacheOnNewMap) {
        ClearDirCacheAll();
    }
    ApplyLoadPriorityBoost(true);

    LoadSession previous;
    int64_t nowTick = g_timer.NowTick();
    uint32_t tid = GetCurrentThreadId();
    if (g_cfg.profilerEnabled) CaptureMainThreadIfNeeded(tid);

    bool hadPrevious = false;
    g_samplerActive.store(false, std::memory_order_release);
    size_t prevSamples = std::min(g_sampleCount.load(std::memory_order_acquire), kMaxSamples);

    AcquireSRWLockExclusive(&g_sessionLock);
    if (g_session.active) {
        g_session.optEnd = g_optStats.Snapshot();
        RecordMilestoneLocked("Session_Finalized", "superseded_by_new_load", nowTick, tid);
        previous = std::move(g_session);
        hadPrevious = true;
    }
    g_inGameOnMap = false;
    g_loadedMapName.clear();
    uint32_t idx = ++g_sessionCounter;
    g_session.Reset(idx, nowTick, trigger);
    bool launchPfNow = false;
    if (!initialMap.empty()) {
        g_session.mapName = initialMap;
        g_session.prefetchLaunched = true;
        launchPfNow = true;
    }
    g_sessionActive.store(true, std::memory_order_release);
    RecordMilestoneLocked("Session_Start", trigger, nowTick, tid);
    ReleaseSRWLockExclusive(&g_sessionLock);

    ResetPrefetchRecorder(g_cfg.enableOptimizer && g_cfg.optPrefetch);
    if (launchPfNow) {
        LaunchMapPrefetch(initialMap);
    }

    if (hadPrevious && prevSamples > 0) {
        previous.samples.assign(g_sampleBuf, g_sampleBuf + prevSamples);
    }
    g_sampleCount.store(0, std::memory_order_release);
    if (g_cfg.profilerEnabled && g_cfg.sampleIntervalMs > 0) {
        g_samplerActive.store(true, std::memory_order_release);
    }

    if (hadPrevious && g_cfg.profilerEnabled) {
        struct FlushJob { LoadSession sess; int64_t endTick; };
        auto* job = new FlushJob{std::move(previous), nowTick};
        HANDLE h = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
            auto* j = static_cast<FlushJob*>(p);
            WriteSessionFiles(std::move(j->sess), "superseded_by_new_load", j->endTick);
            delete j;
            return 0;
        }, job, 0, nullptr);
        if (h) CloseHandle(h);
        else {
            WriteSessionFiles(std::move(job->sess), "superseded_by_new_load", job->endTick);
            delete job;
        }
    }
}

// ============================================================================
// 10. tier0.dll ILoggingListener + IAT persistence hooks
// ============================================================================
struct LoggingContext_t {
    int m_ChannelID;
    int m_Flags;
    int m_Severity;
    uint32_t m_Color;
};

using FnRegisterLoggingListener = void(*)(void* pListener);
using FnUnregisterLoggingListener = void(*)(void* pListener);
using FnResetCurrentLoggingState = void(*)();
using FnPushLoggingState = void(*)(bool bThreadLocal, bool bClearState);

FnRegisterLoggingListener g_fnRegisterListener = nullptr;
FnUnregisterLoggingListener g_fnUnregisterListener = nullptr;
FnResetCurrentLoggingState g_origResetLoggingState = nullptr;
FnPushLoggingState g_origPushLoggingState = nullptr;
bool g_listenerRegistered = false;

struct RawLoggingListener {
    void** vtable;
};

void OnSourceLogMessage(void* /*self*/, const LoggingContext_t* /*ctx*/, const char* pMessage) {
    if (!pMessage || !*pMessage || g_shuttingDown.load(std::memory_order_relaxed)) return;
    if (tl_inHook) return;
    tl_inHook = true;

    g_logMessagesReceived.fetch_add(1, std::memory_order_relaxed);
    std::string line = TrimLine(pMessage);
    if (!line.empty()) {
        int64_t nowTick = g_timer.NowTick();
        uint32_t tid = GetCurrentThreadId();

        if (line.find("---- Host_NewGame ----") != std::string::npos) {
            StartNewSession("Host_NewGame", "");
        } else {
            // v16.1: Invalidate VFS + DirCache whenever addons/FastDL/Workshop content mounts,
            // downloads, connects, or errors out so newly downloaded maps/addons are never missed.
            const bool isAbortMsg =
                (line.find("Host Error:") != std::string::npos ||
                 line.find("Disconnect:") != std::string::npos ||
                 line.find("Host_EndGame") != std::string::npos ||
                 line.find("Server shutting down") != std::string::npos);
            const bool isHardRemountMsg = isAbortMsg ||
                (line.find("Addons have changes - remounting") != std::string::npos) ||
                (line.find("workshop addons!") != std::string::npos && line.find("Mounted ") != std::string::npos);
            const bool isLoadTimeMpMsg = g_sessionActive.load(std::memory_order_relaxed) && (
                (line.find("Downloading") != std::string::npos) ||
                (line.find("Downloaded ") != std::string::npos) ||
                (line.find("Mounting") != std::string::npos) ||
                (line.find("Connected to") != std::string::npos) ||
                (line.find("Loading server cache") != std::string::npos));
            if (isHardRemountMsg || isLoadTimeMpMsg) {
                static std::atomic<ULONGLONG> s_lastMpCacheClearMs{0};
                const ULONGLONG nowMs = ::GetTickCount64();
                const ULONGLONG prevMs = s_lastMpCacheClearMs.load(std::memory_order_relaxed);
                if (isHardRemountMsg || (nowMs - prevMs) >= 500ULL) {
                    s_lastMpCacheClearMs.store(nowMs, std::memory_order_relaxed);
                    ClearDirCacheAll();
                    ClearPendingMapPack();
                    g_vfsGeneration.fetch_add(1, std::memory_order_acq_rel);
                    g_vtfGeneration.fetch_add(1, std::memory_order_acq_rel);
                    g_optStats.vfsGenBumps.fetch_add(1, std::memory_order_relaxed);
                }
            }

            const bool isNewConnectOrChange = isAbortMsg ||
                (line.find("Connected to ") != std::string::npos) ||
                (line.find("Sending client info...") != std::string::npos) ||
                (line.find("Changelevel") != std::string::npos) ||
                (line.find("Lua cache is up to date") != std::string::npos);

            std::string mapToPrefetch;
            bool cancelPfNow = false;
            bool drainVfreeNow = false;
            bool abortSessionNow = false;
            AcquireSRWLockExclusive(&g_sessionLock);
            if (isNewConnectOrChange) {
                g_inGameOnMap = false;
                g_loadedMapName.clear();
            }
            if (g_session.active) {
                if (g_session.mapName == "unknown_map") {
                    auto p1 = line.find("Host_NewGame on map ");
                    if (p1 != std::string::npos) {
                        std::string m = TrimLine(line.substr(p1 + 20));
                        auto sp = m.find_first_of(" \t\r\n");
                        if (sp != std::string::npos) m.resize(sp);
                        if (!m.empty()) g_session.mapName = m;
                    } else {
                        auto p2 = line.find("Loading map \"");
                        if (p2 != std::string::npos) {
                            auto q = line.find('\"', p2 + 13);
                            if (q != std::string::npos && q > p2 + 13) {
                                g_session.mapName = line.substr(p2 + 13, q - (p2 + 13));
                            }
                        }
                    }
                }
                if (!g_session.prefetchLaunched && g_session.mapName != "unknown_map") {
                    g_session.prefetchLaunched = true;
                    mapToPrefetch = g_session.mapName;
                }

                if (isAbortMsg) {
                    RecordMilestoneLocked("Session_Aborted", line, nowTick, tid);
                    abortSessionNow = true;
                } else if (line.find("Addons have changes - remounting") != std::string::npos) {
                    RecordMilestoneLocked("Addons_Remount_Start", line, nowTick, tid);
                } else if (line.find("workshop addons!") != std::string::npos && line.find("Mounted ") != std::string::npos) {
                    RecordMilestoneLocked("Workshop_Mounted", line, nowTick, tid);
                } else if (line.find("Clientside Lua startup!") != std::string::npos) {
                    if (!g_session.luaStartupSeen) {
                        g_session.luaStartupSeen = true;
                        g_session.luaStartupTick = nowTick;
                        g_currentPhase.store(PHASE_LUA_TO_SPAWN, std::memory_order_release);
                    }
                    RecordMilestoneLocked("Clientside_Lua_Startup", line, nowTick, tid);
                } else if (line.find("Sending full update to Client") != std::string::npos) {
                    RecordMilestoneLocked("Full_Update_Sent", line, nowTick, tid);
                } else if (line.find("spawned in server") != std::string::npos ||
                           line.find("Redownloading all lightmaps") != std::string::npos) {
                    if (line.find("spawned in server") != std::string::npos) {
                        g_session.spawnSeen = true;
                        g_session.spawnTick = nowTick;
                        g_currentPhase.store(PHASE_POST_SPAWN, std::memory_order_release);
                        RecordMilestoneLocked("Client_Spawned", line, nowTick, tid);
                        ApplyLoadPriorityBoost(false);
                        g_bspWarmGen.fetch_add(1, std::memory_order_acq_rel);
                        drainVfreeNow = true;
                        cancelPfNow = true;
                    } else {
                        if (!g_session.lightmapsSeen && g_session.luaStartupSeen) {
                            g_session.lightmapsSeen = true;
                            g_session.lightmapsTick = nowTick;
                            g_currentPhase.store(PHASE_POST_SPAWN, std::memory_order_release);
                            ApplyLoadPriorityBoost(false);
                            g_bspWarmGen.fetch_add(1, std::memory_order_acq_rel);
                            drainVfreeNow = true;
                            cancelPfNow = true;
                        }
                        RecordMilestoneLocked("Lightmaps_Redownload", line, nowTick, tid);
                    }
                } else if (line.find("R_RedownloadAllLightmaps") != std::string::npos) {
                    if (!g_session.lightmapsSeen && g_session.luaStartupSeen) {
                        g_session.lightmapsSeen = true;
                        g_session.lightmapsTick = nowTick;
                        g_currentPhase.store(PHASE_POST_SPAWN, std::memory_order_release);
                        ApplyLoadPriorityBoost(false);
                        g_bspWarmGen.fetch_add(1, std::memory_order_acq_rel);
                        drainVfreeNow = true;
                        cancelPfNow = true;
                    }
                    RecordMilestoneLocked("Lightmaps_Redownload", line, nowTick, tid);
                } else if (line.find("Queued Material System: ENABLED!") != std::string::npos) {
                    RecordMilestoneLocked("Queued_MatSystem_Enabled", line, nowTick, tid);
                } else if (line.find("Compact freed") != std::string::npos) {
                    if (!g_session.compactSeen) {
                        g_session.compactSeen = true;
                        g_session.compactTick = nowTick;
                    }
                    RecordMilestoneLocked("Compact_Memory", line, nowTick, tid);
                } else if (g_cfg.profilerEnabled && g_cfg.logConsoleLines && g_session.events.size() < 262144) {
                    TimelineEvent ev;
                    ev.startTick = nowTick;
                    ev.durUs = 0.0;
                    ev.tid = tid;
                    ev.kind = EventKind::Console;
                    ev.hit = true;
                    ev.primary = line;
                    if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
                }
            }
            ReleaseSRWLockExclusive(&g_sessionLock);
            if (drainVfreeNow) DrainVirtualFreeQuarantine(true);
            if (!mapToPrefetch.empty()) LaunchMapPrefetch(mapToPrefetch);
            if (cancelPfNow) CancelActivePrefetch();
            if (abortSessionNow) FinalizeSessionAsyncOrSync("disconnect_or_host_error", false);
        }
    }

    tl_inHook = false;
}

void* g_listenerVtable[1] = { reinterpret_cast<void*>(&OnSourceLogMessage) };
RawLoggingListener g_listenerObj = { g_listenerVtable };

void HookedResetCurrentLoggingState() {
    if (g_origResetLoggingState) g_origResetLoggingState();
    if (!g_shuttingDown.load(std::memory_order_relaxed) && g_fnRegisterListener) {
        g_fnRegisterListener(&g_listenerObj);
    }
}

void HookedPushLoggingState(bool bThreadLocal, bool bClearState) {
    if (g_origPushLoggingState) g_origPushLoggingState(bThreadLocal, bClearState);
    if (!g_shuttingDown.load(std::memory_order_relaxed) && bClearState && g_fnRegisterListener) {
        g_fnRegisterListener(&g_listenerObj);
    }
}

bool PatchModuleIat(HMODULE mod, const char* targetDll, const char* targetFunc, void* newFn, void** outOrig) {
    if (!mod) return false;
    auto base = reinterpret_cast<uintptr_t>(mod);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress || !dir.Size) return false;

    auto imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (; imp->Name; ++imp) {
        const char* dllName = reinterpret_cast<const char*>(base + imp->Name);
        if (_stricmp(dllName, targetDll) != 0) continue;

        auto origThunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(
            base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto iatThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + imp->FirstThunk);

        for (; origThunk->u1.AddressOfData; ++origThunk, ++iatThunk) {
            if (IMAGE_SNAP_BY_ORDINAL64(origThunk->u1.Ordinal)) continue;
            auto ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + origThunk->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(ibn->Name), targetFunc) == 0) {
                DWORD oldProt = 0;
                if (!VirtualProtect(&iatThunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProt)) return false;
                if (outOrig && !*outOrig) {
                    *outOrig = reinterpret_cast<void*>(iatThunk->u1.Function);
                }
                iatThunk->u1.Function = reinterpret_cast<ULONGLONG>(newFn);
                DWORD dummy = 0;
                VirtualProtect(&iatThunk->u1.Function, sizeof(void*), oldProt, &dummy);
                return true;
            }
        }
    }
    return false;
}

void InstallFsIatHooks(HMODULE hFs) {
    if (!hFs || g_fsIatHooked || !g_cfg.enableOptimizer) return;
    g_fsModuleHandle = hFs;

    int hooked = 0;
    if (g_cfg.optDirCache || g_cfg.optReadBuffer || g_cfg.optWriteBuffer) {
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "CreateFileA", reinterpret_cast<void*>(&HookedCreateFileA), reinterpret_cast<void**>(&g_origCreateFileA)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(&HookedCreateFileW), reinterpret_cast<void**>(&g_origCreateFileW)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "DeleteFileA", reinterpret_cast<void*>(&HookedDeleteFileA), reinterpret_cast<void**>(&g_origDeleteFileA)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "DeleteFileW", reinterpret_cast<void*>(&HookedDeleteFileW), reinterpret_cast<void**>(&g_origDeleteFileW)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "CreateDirectoryW", reinterpret_cast<void*>(&HookedCreateDirectoryW), reinterpret_cast<void**>(&g_origCreateDirectoryW)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "RemoveDirectoryW", reinterpret_cast<void*>(&HookedRemoveDirectoryW), reinterpret_cast<void**>(&g_origRemoveDirectoryW)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "MoveFileExW", reinterpret_cast<void*>(&HookedMoveFileExW), reinterpret_cast<void**>(&g_origMoveFileExW)) ? 1 : 0;
    }
    if (g_cfg.optDirCache) {
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "GetFileAttributesExA", reinterpret_cast<void*>(&HookedGetFileAttributesExA), reinterpret_cast<void**>(&g_origGetFileAttributesExA)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "GetFileAttributesExW", reinterpret_cast<void*>(&HookedGetFileAttributesExW), reinterpret_cast<void**>(&g_origGetFileAttributesExW)) ? 1 : 0;
    }
    if (g_cfg.optReadBuffer || g_cfg.optWriteBuffer) {
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "ReadFile", reinterpret_cast<void*>(&HookedReadFile), reinterpret_cast<void**>(&g_origReadFile)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "SetFilePointer", reinterpret_cast<void*>(&HookedSetFilePointer), reinterpret_cast<void**>(&g_origSetFilePointer)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "SetFilePointerEx", reinterpret_cast<void*>(&HookedSetFilePointerEx), reinterpret_cast<void**>(&g_origSetFilePointerEx)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "GetFileSize", reinterpret_cast<void*>(&HookedGetFileSize), reinterpret_cast<void**>(&g_origGetFileSize)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "GetFileSizeEx", reinterpret_cast<void*>(&HookedGetFileSizeEx), reinterpret_cast<void**>(&g_origGetFileSizeEx)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "CloseHandle", reinterpret_cast<void*>(&HookedCloseHandle), reinterpret_cast<void**>(&g_origCloseHandle)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "CreateFileMappingA", reinterpret_cast<void*>(&HookedCreateFileMappingA), reinterpret_cast<void**>(&g_origCreateFileMappingA)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "WriteFile", reinterpret_cast<void*>(&HookedWriteFile), reinterpret_cast<void**>(&g_origWriteFile)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "SetEndOfFile", reinterpret_cast<void*>(&HookedSetEndOfFile), reinterpret_cast<void**>(&g_origSetEndOfFile)) ? 1 : 0;
        hooked += PatchModuleIat(hFs, "KERNEL32.dll", "FlushFileBuffers", reinterpret_cast<void*>(&HookedFlushFileBuffers), reinterpret_cast<void**>(&g_origFlushFileBuffers)) ? 1 : 0;
    }

    g_fsIatHooked = true;
    WriteStatusLog("filesystem_stdio.dll Win32 IAT accelerator hooks installed: " + std::to_string(hooked) + " functions.");
}

void RestoreFsIatHooks() {
    if (g_fsIatHooked && g_fsModuleHandle) {
        HMODULE hFs = GetModuleHandleW(L"filesystem_stdio.dll");
        if (hFs && hFs == g_fsModuleHandle) {
            if (g_origCreateFileA)          PatchModuleIat(hFs, "KERNEL32.dll", "CreateFileA", reinterpret_cast<void*>(g_origCreateFileA), nullptr);
            if (g_origCreateFileW)          PatchModuleIat(hFs, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(g_origCreateFileW), nullptr);
            if (g_origGetFileAttributesExA) PatchModuleIat(hFs, "KERNEL32.dll", "GetFileAttributesExA", reinterpret_cast<void*>(g_origGetFileAttributesExA), nullptr);
            if (g_origGetFileAttributesExW) PatchModuleIat(hFs, "KERNEL32.dll", "GetFileAttributesExW", reinterpret_cast<void*>(g_origGetFileAttributesExW), nullptr);
            if (g_origReadFile)             PatchModuleIat(hFs, "KERNEL32.dll", "ReadFile", reinterpret_cast<void*>(g_origReadFile), nullptr);
            if (g_origSetFilePointer)       PatchModuleIat(hFs, "KERNEL32.dll", "SetFilePointer", reinterpret_cast<void*>(g_origSetFilePointer), nullptr);
            if (g_origSetFilePointerEx)     PatchModuleIat(hFs, "KERNEL32.dll", "SetFilePointerEx", reinterpret_cast<void*>(g_origSetFilePointerEx), nullptr);
            if (g_origGetFileSize)          PatchModuleIat(hFs, "KERNEL32.dll", "GetFileSize", reinterpret_cast<void*>(g_origGetFileSize), nullptr);
            if (g_origGetFileSizeEx)        PatchModuleIat(hFs, "KERNEL32.dll", "GetFileSizeEx", reinterpret_cast<void*>(g_origGetFileSizeEx), nullptr);
            if (g_origCloseHandle)          PatchModuleIat(hFs, "KERNEL32.dll", "CloseHandle", reinterpret_cast<void*>(g_origCloseHandle), nullptr);
            if (g_origCreateFileMappingA)   PatchModuleIat(hFs, "KERNEL32.dll", "CreateFileMappingA", reinterpret_cast<void*>(g_origCreateFileMappingA), nullptr);
            if (g_origWriteFile)            PatchModuleIat(hFs, "KERNEL32.dll", "WriteFile", reinterpret_cast<void*>(g_origWriteFile), nullptr);
            if (g_origSetEndOfFile)         PatchModuleIat(hFs, "KERNEL32.dll", "SetEndOfFile", reinterpret_cast<void*>(g_origSetEndOfFile), nullptr);
            if (g_origFlushFileBuffers)     PatchModuleIat(hFs, "KERNEL32.dll", "FlushFileBuffers", reinterpret_cast<void*>(g_origFlushFileBuffers), nullptr);
            if (g_origDeleteFileA)          PatchModuleIat(hFs, "KERNEL32.dll", "DeleteFileA", reinterpret_cast<void*>(g_origDeleteFileA), nullptr);
            if (g_origDeleteFileW)          PatchModuleIat(hFs, "KERNEL32.dll", "DeleteFileW", reinterpret_cast<void*>(g_origDeleteFileW), nullptr);
            if (g_origCreateDirectoryW)     PatchModuleIat(hFs, "KERNEL32.dll", "CreateDirectoryW", reinterpret_cast<void*>(g_origCreateDirectoryW), nullptr);
            if (g_origRemoveDirectoryW)     PatchModuleIat(hFs, "KERNEL32.dll", "RemoveDirectoryW", reinterpret_cast<void*>(g_origRemoveDirectoryW), nullptr);
            if (g_origMoveFileExW)          PatchModuleIat(hFs, "KERNEL32.dll", "MoveFileExW", reinterpret_cast<void*>(g_origMoveFileExW), nullptr);
        }
        g_fsIatHooked = false;
    }
    if (g_engineModForIat) {
        if (GetModuleHandleW(L"engine.dll") == g_engineModForIat) {
            if (g_origEngineCreateFileA) PatchModuleIat(g_engineModForIat, "KERNEL32.dll", "CreateFileA", reinterpret_cast<void*>(g_origEngineCreateFileA), nullptr);
            if (g_origEngineCreateFileW) PatchModuleIat(g_engineModForIat, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(g_origEngineCreateFileW), nullptr);
        }
        g_engineModForIat = nullptr;
    }
    if (g_resourcesModForIat) {
        if (GetModuleHandleW(L"resources.dll") == g_resourcesModForIat && g_origResourcesCreateFileW) {
            PatchModuleIat(g_resourcesModForIat, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(g_origResourcesCreateFileW), nullptr);
        }
        g_resourcesModForIat = nullptr;
    }
    if (g_datacacheModForIat) {
        if (GetModuleHandleW(L"datacache.dll") == g_datacacheModForIat && g_origDatacacheCreateFileW) {
            PatchModuleIat(g_datacacheModForIat, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(g_origDatacacheCreateFileW), nullptr);
        }
        g_datacacheModForIat = nullptr;
    }
    if (g_matSysModForIat) {
        if (GetModuleHandleW(L"materialsystem.dll") == g_matSysModForIat && g_origMatSysCreateFileW) {
            PatchModuleIat(g_matSysModForIat, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(g_origMatSysCreateFileW), nullptr);
        }
        g_matSysModForIat = nullptr;
    }
    if (g_luaModForIat) {
        if (GetModuleHandleW(L"lua_shared.dll") == g_luaModForIat) {
            if (g_origLuaCreateFileW) PatchModuleIat(g_luaModForIat, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(g_origLuaCreateFileW), nullptr);
            if (g_origLuaVirtualFree) PatchModuleIat(g_luaModForIat, "KERNEL32.dll", "VirtualFree", reinterpret_cast<void*>(g_origLuaVirtualFree), nullptr);
        }
        g_luaModForIat = nullptr;
    }
    if (g_tier0ModForIat) {
        if (GetModuleHandleW(L"tier0.dll") == g_tier0ModForIat && g_origTier0VirtualFree) {
            PatchModuleIat(g_tier0ModForIat, "KERNEL32.dll", "VirtualFree", reinterpret_cast<void*>(g_origTier0VirtualFree), nullptr);
        }
        g_tier0ModForIat = nullptr;
    }
}

// ============================================================================
// 11. filesystem_stdio.dll IFileSystem / IBaseFileSystem vtable hooks (15 verified win64 slots)
// ============================================================================
using FnOpenEx        = void*(*)(void* self, const char* pFileName, const char* pOptions,
                                 unsigned flags, const char* pathID, char** ppszResolvedFilename);
using FnReadEx        = int(*)(void* self, void* pOutput, int sizeDest, int size, void* file);
using FnSetBufferSize = void(*)(void* self, void* file, unsigned nBytes);
using FnIsOk          = bool(*)(void* self, void* file);
using FnEndOfFile     = bool(*)(void* self, void* file);
using FnReadLine      = char*(*)(void* self, char* pOutput, int maxChars, void* file);
using FnFindFirstEx   = const char*(*)(void* self, const char* pWildCard, const char* pPathID, int* pHandle);

using FnClose         = void(*)(void* self, void* file);
using FnSeek          = void(*)(void* self, void* file, int pos, int seekType);
using FnTell          = unsigned int(*)(void* self, void* file);
using FnSizePath      = unsigned int(*)(void* self, const char* pFileName, const char* pPathID);
using FnSizeHandle    = unsigned int(*)(void* self, void* file);
using FnFlush         = void(*)(void* self, void* file);
using FnFileExists    = bool(*)(void* self, const char* pFileName, const char* pPathID);
using FnGetFileTime   = long(*)(void* self, const char* pFileName, const char* pPathID);

// v16.6: Dynamically resolved at runtime from IBaseFileSystem::Open (subVt[2]) & Read (subVt[0]) thunks!
// Automatically supports BOTH x86-64 beta (OpenEx=73, shift=0) AND main branch gmod_win64.exe (OpenEx=69, shift=-4).
size_t g_vtableIdxSetBufferSize = 24;
size_t g_vtableIdxIsOk          = 25;
size_t g_vtableIdxEndOfFile     = 26;
size_t g_vtableIdxReadLine      = 27;
size_t g_vtableIdxFindFirstEx   = 35;
size_t g_vtableIdxOpenEx        = 73;
size_t g_vtableIdxReadEx        = 74;

constexpr size_t kSubVtableIdxWrite      = 1;  // +0x008 sub (+0x8)
constexpr size_t kSubVtableIdxClose      = 3;  // +0x018 sub (+0x8)
constexpr size_t kSubVtableIdxSeek       = 4;  // +0x020 sub (+0x8)
constexpr size_t kSubVtableIdxTell       = 5;  // +0x028 sub (+0x8)
constexpr size_t kSubVtableIdxSizePath   = 6;  // +0x030 sub (+0x8)
constexpr size_t kSubVtableIdxSizeHandle = 7;  // +0x038 sub (+0x8)
constexpr size_t kSubVtableIdxFlush      = 8;  // +0x040 sub (+0x8)
constexpr size_t kSubVtableIdxExists     = 10; // +0x050 sub (+0x8)
constexpr size_t kSubVtableIdxFileTime   = 13; // +0x068 sub (+0x8)

void** g_fsPrimaryVtable = nullptr;
void** g_fsSubVtable = nullptr;
FnSetBufferSize g_origSetBufferSize = nullptr;
FnIsOk          g_origIsOk          = nullptr;
FnEndOfFile     g_origEndOfFile     = nullptr;
FnReadLine      g_origReadLine      = nullptr;
FnFindFirstEx   g_origFindFirstEx   = nullptr;
FnOpenEx        g_origOpenEx        = nullptr;
FnReadEx        g_origReadEx        = nullptr;

using FnWrite = int (*)(void* self, const void* pInput, int size, void* file);
FnWrite         g_origWrite         = nullptr;
FnClose         g_origClose         = nullptr;
FnSeek          g_origSeek          = nullptr;
FnTell          g_origTell          = nullptr;
FnSizePath      g_origSizePath      = nullptr;
FnSizeHandle    g_origSizeHandle    = nullptr;
FnFlush         g_origFlush         = nullptr;
FnFileExists    g_origFileExists    = nullptr;
FnGetFileTime   g_origGetFileTime   = nullptr;

constexpr size_t kVfsReadAheadChunkBytes = 8 * 1024 * 1024; // 8 MB streaming read-ahead buffer
std::atomic<uint64_t> g_handleCloseGen{1};

struct RealHandleInfo {
    SRWLOCK lock = SRWLOCK_INIT;
    std::string normPath;
    uint64_t readCalls = 0;
    uint64_t readBytes = 0;
    uint64_t raHits = 0;
    uint64_t raBytes = 0;
    double readUs = 0.0;
    double maxReadUs = 0.0;
    void* lastCaller = nullptr;

    // Layer 4b: Streaming read-ahead state for binary read-only handles (e.g. giant .bsp micro-reads)
    bool raEligible = false;
    bool raActive = false;
    bool raInstantEof = false;
    uint32_t smallReadStreak = 0;
    uint64_t raFileSize = 0;
    uint64_t raLogicalPos = 0;
    uint64_t raRealPos = 0;
    uint64_t raBufStart = 0;
    size_t raBufLen = 0;
    std::vector<uint8_t> raBuf;
};

SRWLOCK g_handleMapLock = SRWLOCK_INIT;
std::unordered_map<void*, std::shared_ptr<RealHandleInfo>> g_openHandles;

void FlushAllOpenRealHandleStatsIntoSessionLocked(LoadSession& sess) {
    AcquireSRWLockShared(&g_handleMapLock);
    for (const auto& kv : g_openHandles) {
        const auto& info = kv.second;
        if (!info) continue;
        AcquireSRWLockExclusive(&info->lock);
        if (info->raHits > 0) {
            g_optStats.vfsRaHits.fetch_add(info->raHits, std::memory_order_relaxed);
            g_optStats.vfsRaBytes.fetch_add(info->raBytes, std::memory_order_relaxed);
            info->raHits = 0;
            info->raBytes = 0;
        }
        if (info->readCalls > 0) {
            auto& ra = sess.readByFile[info->normPath];
            ra.calls += info->readCalls;
            ra.bytes += info->readBytes;
            ra.totalUs += info->readUs;
            if (info->maxReadUs > ra.maxUs) ra.maxUs = info->maxReadUs;
            if (info->lastCaller) {
                const std::string& caller = ResolveCallerModuleFast(info->lastCaller);
                auto& ca = sess.byCaller[caller];
                ca.readCalls += info->readCalls;
                ca.readBytes += info->readBytes;
                ca.readUs += info->readUs;
            }
            info->readCalls = 0;
            info->readBytes = 0;
            info->readUs = 0.0;
            info->maxReadUs = 0.0;
        }
        ReleaseSRWLockExclusive(&info->lock);
    }
    ReleaseSRWLockShared(&g_handleMapLock);
}

bool IsAddressInModule(void* addr, HMODULE mod) {
    if (!addr || !mod) return false;
    auto base = reinterpret_cast<uintptr_t>(mod);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    uintptr_t a = reinterpret_cast<uintptr_t>(addr);
    return a >= base && a < base + nt->OptionalHeader.SizeOfImage;
}

std::string ExtractMapFromBspPath(const std::string& norm) {
    if (norm.size() > 9 && norm.rfind("maps/", 0) == 0 && norm.ends_with(".bsp")) {
        std::string m = norm.substr(5, norm.size() - 9);
        auto slash = m.find_last_of('/');
        if (slash != std::string::npos) m = m.substr(slash + 1);
        return m;
    }
    return "";
}

struct LastBspReadInfo {
    void* handle = nullptr;
    std::string normPath;
    uint64_t fileSize = 0;
    int64_t openTick = 0;
};
thread_local LastBspReadInfo tl_lastBspRead{};

bool TryVerifyAndBypassMapPackExtract(void* self, const std::string& norm, void*& outHandle) {
    if (!g_cfg.enableOptimizer || !g_cfg.optMapPackCache) return false;
    if (norm != "cache/map_pack.bsp") return false;
    if (!tl_lastBspRead.handle || tl_lastBspRead.normPath.empty() || tl_lastBspRead.fileSize < kMapPackHashSampleBytes) {
        g_optStats.mpRejNoRecentBsp.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (!g_origSeek || !g_origReadEx) return false;

    const double ageMs = g_timer.DeltaUs(tl_lastBspRead.openTick, g_timer.NowTick()) / 1000.0;
    if (ageMs < 0.0 || ageMs > 5000.0) return false;

    // Record pending source so if verification fails and a real copy happens, OnMapPackBspWriteClosed saves metadata
    SetPendingMapPackSource(tl_lastBspRead.normPath, tl_lastBspRead.fileSize);

    std::filesystem::path diskPath = GetMapPackDiskPath();
    if (diskPath.empty()) return false;
    std::wstring diskPathW = diskPath.wstring();
    std::wstring metaPathW = diskPathW + L".optmeta";

    auto readMetaFile = [&](const std::wstring& mPath, std::string& outSrc, uint64_t& outSize,
                            uint64_t& outMtime, uint64_t& outHead, uint64_t& outTail) -> bool {
        std::ifstream metaIn(mPath, std::ios::binary);
        if (!metaIn) return false;
        std::string ver;
        if (!std::getline(metaIn, ver) || (ver != "v11" && ver != "v12" && ver != "v13" && ver != "v14")) return false;
        if (!std::getline(metaIn, outSrc)) return false;
        if (!(metaIn >> outSize >> outMtime >> outHead >> outTail)) return false;
        while (!outSrc.empty() && (outSrc.back() == '\r' || outSrc.back() == '\n')) outSrc.pop_back();
        return !outSrc.empty() && outSize >= kMapPackHashSampleBytes;
    };

    auto mapKeyHash = [](const std::string& src, uint64_t sz) -> std::wstring {
        uint64_t h = HashBytesFnv1a64(reinterpret_cast<const uint8_t*>(src.data()), src.size());
        h ^= sz * 1099511628211ULL;
        wchar_t buf[32];
        std::swprintf(buf, 32, L"%016llx", static_cast<unsigned long long>(h));
        return std::wstring(buf);
    };

    std::string metaSrc;
    uint64_t metaSize = 0, metaMtime = 0, metaHead = 0, metaTail = 0;
    bool metaMatchesCurrent = readMetaFile(metaPathW, metaSrc, metaSize, metaMtime, metaHead, metaTail) &&
                              metaSrc == tl_lastBspRead.normPath && metaSize == tl_lastBspRead.fileSize;

    // Multi-map cache: if current map_pack.bsp belongs to a different map, park it in cache/opt_mappack/
    // and restore a previously parked copy of tl_lastBspRead if available (<1 ms!).
    std::error_code ec;
    std::filesystem::path parkDir = diskPath.parent_path() / L"opt_mappack";
    const bool diskExists = std::filesystem::exists(diskPath, ec);
    if (!metaMatchesCurrent || !diskExists) {
        std::filesystem::create_directories(parkDir, ec);
        if (!metaSrc.empty() && diskExists && !metaMatchesCurrent) {
            std::wstring parkKey = mapKeyHash(metaSrc, metaSize);
            std::filesystem::path parkBsp = parkDir / (parkKey + L".bsp");
            std::filesystem::path parkMeta = parkDir / (parkKey + L".bsp.optmeta");
            ::MoveFileExW(diskPathW.c_str(), parkBsp.c_str(), MOVEFILE_REPLACE_EXISTING);
            ::MoveFileExW(metaPathW.c_str(), parkMeta.c_str(), MOVEFILE_REPLACE_EXISTING);
            InvalidateDirAndParent(diskPathW.c_str());
        }
        std::wstring wantKey = mapKeyHash(tl_lastBspRead.normPath, tl_lastBspRead.fileSize);
        std::filesystem::path wantBsp = parkDir / (wantKey + L".bsp");
        std::filesystem::path wantMeta = parkDir / (wantKey + L".bsp.optmeta");
        if (std::filesystem::exists(wantBsp, ec)) {
            ::MoveFileExW(wantBsp.c_str(), diskPathW.c_str(), MOVEFILE_REPLACE_EXISTING);
            ::MoveFileExW(wantMeta.c_str(), metaPathW.c_str(), MOVEFILE_REPLACE_EXISTING);
            InvalidateDirAndParent(diskPathW.c_str());
            metaMatchesCurrent = readMetaFile(metaPathW, metaSrc, metaSize, metaMtime, metaHead, metaTail) &&
                                 metaSrc == tl_lastBspRead.normPath && metaSize == tl_lastBspRead.fileSize;
        }
        // Keep at most 3 parked map_pack files to bound disk usage
        std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> parked;
        for (const auto& entry : std::filesystem::directory_iterator(parkDir, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == L".bsp") {
                parked.emplace_back(entry.last_write_time(ec), entry.path());
            }
        }
        if (parked.size() > 3) {
            std::sort(parked.begin(), parked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (size_t i = 0; i + 3 < parked.size(); ++i) {
                std::filesystem::remove(parked[i].second, ec);
                std::filesystem::remove(std::filesystem::path(parked[i].second.wstring() + L".optmeta"), ec);
            }
        }
    }

    uint64_t dstSize = 0, dstMtime = 0, dstHead = 0, dstTail = 0;
    if (!HashDiskFileHeadTail(diskPathW, dstSize, dstMtime, dstHead, dstTail)) {
        g_optStats.mpRejDiskHashFail.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (dstSize != tl_lastBspRead.fileSize) {
        g_optStats.mpRejSizeMismatch.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (metaMatchesCurrent && (dstSize != metaSize || dstMtime != metaMtime || dstHead != metaHead || dstTail != metaTail)) {
        g_optStats.mpRejMetaMismatch.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Live 64 KB head + 64 KB tail verification against the currently opened source .bsp handle!
    void* subThis = reinterpret_cast<uint8_t*>(self) + 8;
    void* srcH = tl_lastBspRead.handle;
    std::vector<uint8_t> buf(kMapPackHashSampleBytes);

    auto readExactVfs = [&](uint64_t off, uint8_t* dst, size_t len) -> bool {
        g_origSeek(subThis, srcH, static_cast<int>(off), 0);
        size_t done = 0;
        while (done < len) {
            int rem = static_cast<int>(len - done);
            int got = g_origReadEx(self, dst + done, rem, rem, srcH);
            if (got <= 0) return false;
            done += static_cast<size_t>(got);
        }
        return true;
    };

    bool match = false;
    if (readExactVfs(0, buf.data(), kMapPackHashSampleBytes) &&
        HashBytesFnv1a64(buf.data(), kMapPackHashSampleBytes) == dstHead &&
        readExactVfs(tl_lastBspRead.fileSize - kMapPackHashSampleBytes, buf.data(), kMapPackHashSampleBytes) &&
        HashBytesFnv1a64(buf.data(), kMapPackHashSampleBytes) == dstTail) {
        match = true;
    }
    // Always rewind source handle cleanly to 0
    g_origSeek(subThis, srcH, 0, 0);

    if (!match) { g_optStats.mpRejLiveHashMismatch.fetch_add(1, std::memory_order_relaxed); return false; }

    // 100% deterministic match! Mark source handle for instant EOF and return virtual write-sink handle.
    std::shared_ptr<RealHandleInfo> srcInfo;
    AcquireSRWLockShared(&g_handleMapLock);
    auto it = g_openHandles.find(srcH);
    if (it != g_openHandles.end()) srcInfo = it->second;
    ReleaseSRWLockShared(&g_handleMapLock);
    if (!srcInfo) return false;

    VirtualFileHandle* sinkVh = AllocVirtualWriteSinkHandle(norm);
    if (!sinkVh) return false;

    AcquireSRWLockExclusive(&srcInfo->lock);
    srcInfo->raInstantEof = true;
    ReleaseSRWLockExclusive(&srcInfo->lock);

    if (!metaMatchesCurrent) {
        std::ofstream metaOut(metaPathW, std::ios::binary | std::ios::trunc);
        if (metaOut) {
            metaOut << "v12\n"
                    << tl_lastBspRead.normPath << "\n"
                    << dstSize << "\n"
                    << dstMtime << "\n"
                    << dstHead << "\n"
                    << dstTail << "\n";
        }
    }
    outHandle = static_cast<void*>(sinkVh);
    g_optStats.mapPackBypassHits.fetch_add(1, std::memory_order_relaxed);
    g_optStats.mapPackSavedBytes.fetch_add(dstSize, std::memory_order_relaxed);
    WriteStatusLog("Layer 9 MapPack Bypass HIT: reused verified cache/map_pack.bsp (" +
                   std::to_string(dstSize / (1024ULL * 1024ULL)) + " MB skipped in <2 ms!) for " + tl_lastBspRead.normPath);
    if (g_cfg.optBspPakFastPath) {
        IndexBspPakLump(diskPathW, false);
    }
    return true;
}

void* HookedOpenEx(void* self, const char* pFileName, const char* pOptions,
                   unsigned flags, const char* pathID, char** ppszResolvedFilename) {
    if (tl_inHook || !g_origOpenEx || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origOpenEx ? g_origOpenEx(self, pFileName, pOptions, flags, pathID, ppszResolvedFilename) : nullptr;
    }

    void* retAddr = g_cfg.profilerEnabled ? PROFILER_RETURN_ADDRESS() : nullptr;
    tl_inHook = true;
    int64_t t0 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;

    std::string norm;
    if (pFileName && *pFileName) norm = NormalizePath(pFileName);

    const bool isPureRead = (!pOptions || (pOptions[0] == 'r' && std::strchr(pOptions, '+') == nullptr));
    const bool isBinary = (pOptions && std::strchr(pOptions, 'b') != nullptr);
    if (!isPureRead && (norm.empty() || !IsNonAssetNarrowWritePath(norm))) {
        g_vfsGeneration.fetch_add(1, std::memory_order_acq_rel);
        if (norm.empty() || norm.rfind("shaders/", 0) == 0) {
            AcquireSRWLockExclusive(&g_vcsCacheLock);
            g_vcsMissCache.clear();
            g_vcsRamCache.clear();
            g_vcsRamBytesUsed = 0;
            ReleaseSRWLockExclusive(&g_vcsCacheLock);
        }
    }

    bool servedFromNegCache = false;
    bool servedFromRamCache = false;
    bool servedFromBspPak = false;
    StackVfsKey vfsKey;
    StackVfsKey ramKey;
    uint64_t genBefore = 0;

    const bool isVcsShader = !norm.empty() && norm.rfind("shaders/fxc/", 0) == 0 && norm.ends_with(".vcs");
    const bool isSharedExt = !norm.empty() && !isVcsShader && IsSharedRamCacheExt(norm);
    const bool isSlurpOnly = !norm.empty() && IsSlurpOnlyExt(norm);
    const bool isVtfExt = !norm.empty() && norm.size() >= 4 && norm.compare(norm.size() - 4, 4, ".vtf") == 0;
    const bool isSkyboxVtf = isVtfExt && norm.rfind("materials/skybox/", 0) == 0;

    const bool canUseNegCache = g_cfg.enableOptimizer && g_cfg.optVfsMetaCache &&
                                isPureRead && !norm.empty() && !isVcsShader &&
                                !norm.ends_with(".vpk") && !IsDynamicMpPathNarrow(norm);

    const bool canSlurpHandle = g_cfg.enableOptimizer && g_cfg.optVfsRamCache &&
                                isPureRead && (isSharedExt || isSlurpOnly) &&
                                g_origSizeHandle && g_origReadEx && g_origClose && g_origSeek;

    const bool canShareRamCache = canSlurpHandle && isSharedExt &&
                                  (ppszResolvedFilename == nullptr || HasTier0MemAlloc());

    const bool canVcsPersistent = g_cfg.enableOptimizer && g_cfg.optVfsRamCache &&
                                  isPureRead && isVcsShader &&
                                  (ppszResolvedFilename == nullptr || HasTier0MemAlloc()) &&
                                  g_origSizeHandle && g_origReadEx && g_origClose && g_origSeek;

    (void)isSkyboxVtf;
    const bool canVtfRamCache = g_cfg.enableOptimizer && g_cfg.optVtfRamCache &&
                                isPureRead && isBinary && isVtfExt &&
                                (ppszResolvedFilename == nullptr || HasTier0MemAlloc()) &&
                                g_sessionActive.load(std::memory_order_relaxed) &&
                                g_origSizeHandle && g_origReadEx && g_origClose && g_origSeek;
    uint64_t vtfGenBefore = 0;

    void* handle = nullptr;

    // Layer 9: Deterministic zero-copy bypass for redundant Workshop -> cache/map_pack.bsp extraction
    if (!isPureRead && norm == "cache/map_pack.bsp") {
        if (TryVerifyAndBypassMapPackExtract(self, norm, handle)) {
            servedFromRamCache = true;
        }
    }

    // Layer 8: Instant O(1) BSP Lump 40 zero-copy hit / negative hit for sp_*.vhv, texelslighting_*.ppl,
    // pathID=="BSP" lookups, and BSP-embedded materials/maps/...vmt files
    const bool bspPathId = IsBspPathId(pathID);
    const bool bspPreloadable = IsBspPakPreloadablePath(norm);
    if (!servedFromRamCache && g_cfg.enableOptimizer && g_cfg.optBspPakFastPath && isPureRead &&
        ppszResolvedFilename == nullptr && g_bspPakActive.load(std::memory_order_acquire) &&
        (bspPathId || bspPreloadable)) {
        std::shared_ptr<const RawByteBuf> pakSlab;
        size_t sOff = 0, sLen = 0;
        bool foundEntry = false;
        bool inSlab = false;
        AcquireSRWLockShared(&g_bspPakLock);
        if (g_bspPakActive.load(std::memory_order_relaxed) && !g_bspPakEntries.empty()) {
            auto it = g_bspPakEntries.find(norm);
            if (it != g_bspPakEntries.end()) {
                foundEntry = true;
                if (it->second.inSlab && g_bspPakSlab) {
                    pakSlab = g_bspPakSlab;
                    sOff = it->second.slabOffset;
                    sLen = it->second.slabLen;
                    inSlab = true;
                }
            } else if (IsBspPakStaticPropLightPath(norm)) {
                servedFromNegCache = true;
                servedFromBspPak = true;
            }
        }
        ReleaseSRWLockShared(&g_bspPakLock);

        if (servedFromNegCache) {
            g_optStats.bspPakNegHits.fetch_add(1, std::memory_order_relaxed);
        } else if (foundEntry && inSlab && pakSlab) {
            VirtualFileHandle* vh = AllocVirtualHandleSlice(std::move(pakSlab), sOff, sLen, norm);
            if (vh) {
                handle = static_cast<void*>(vh);
                servedFromRamCache = true;
                servedFromBspPak = true;
                g_optStats.bspPakVhvHits.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // Layer 4c: Bounded Load-Time .vtf LRU RAM Cache lookup (v13: dedicated g_vtfGeneration + 3-state ResolvedState!)
    if (!servedFromNegCache && !servedFromRamCache && canVtfRamCache) {
        vtfGenBefore = g_vtfGeneration.load(std::memory_order_acquire);
        ramKey.Build(norm, pathID, 0, 'b');
        std::string cachedResolved;
        ResolvedState cachedState = ResolvedState::Unknown;
        if (auto vtfData = LookupVtfRamCache(ramKey.view, vtfGenBefore,
                                             ppszResolvedFilename ? &cachedResolved : nullptr,
                                             &cachedState)) {
            bool resolvedOk = true;
            char* allocResolved = nullptr;
            if (ppszResolvedFilename) {
                if (cachedState == ResolvedState::Valid) {
                    allocResolved = AllocTier0String(cachedResolved);
                    if (!allocResolved) resolvedOk = false;
                } else if (cachedState == ResolvedState::Null) {
                    allocResolved = nullptr;
                } else {
                    resolvedOk = false;
                    g_optStats.vtfRejUnknownResolved.fetch_add(1, std::memory_order_relaxed);
                }
            }
            if (resolvedOk) {
                VirtualFileHandle* vh = AllocVirtualHandle(vtfData, norm);
                if (vh) {
                    if (ppszResolvedFilename) *ppszResolvedFilename = allocResolved;
                    handle = static_cast<void*>(vh);
                    servedFromRamCache = true;
                    g_optStats.vtfRamOpenHits.fetch_add(1, std::memory_order_relaxed);
                    g_optStats.vtfRamSavedBytes.fetch_add(vtfData->size(), std::memory_order_relaxed);
                }
            }
        } else {
            g_optStats.vtfLookupMiss.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Persistent cross-map cache for shaders/fxc/*.vcs
    if (!servedFromNegCache && !servedFromRamCache && canVcsPersistent) {
        vfsKey.Build(norm, pathID, 0, 0);
        ramKey.Build(norm, pathID, 0, isBinary ? 'b' : 't');
        std::shared_ptr<const RawByteBuf> vcsData;
        std::string vcsResolved;
        ResolvedState vcsState = ResolvedState::Unknown;
        AcquireSRWLockShared(&g_vcsCacheLock);
        auto rit = g_vcsRamCache.find(ramKey.view);
        if (rit != g_vcsRamCache.end()) {
            vcsData = rit->second.data;
            vcsResolved = rit->second.resolvedFilename;
            vcsState = rit->second.resolvedState;
        } else if (g_vcsMissCache.find(vfsKey.view) != g_vcsMissCache.end()) {
            servedFromNegCache = true;
        }
        ReleaseSRWLockShared(&g_vcsCacheLock);
        if (vcsData) {
            bool resolvedOk = true;
            char* allocResolved = nullptr;
            if (ppszResolvedFilename) {
                if (vcsState == ResolvedState::Valid) {
                    allocResolved = AllocTier0String(vcsResolved);
                    if (!allocResolved) resolvedOk = false;
                } else if (vcsState == ResolvedState::Null) {
                    allocResolved = nullptr;
                } else {
                    resolvedOk = false;
                    g_optStats.vfsRejUnknownResolved.fetch_add(1, std::memory_order_relaxed);
                }
            }
            if (resolvedOk) {
                VirtualFileHandle* vh = AllocVirtualHandle(std::move(vcsData), norm);
                if (vh) {
                    if (ppszResolvedFilename) *ppszResolvedFilename = allocResolved;
                    handle = static_cast<void*>(vh);
                    servedFromRamCache = true;
                    g_optStats.vfsRamOpenHits.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    if (!servedFromNegCache && !servedFromRamCache && (canUseNegCache || canShareRamCache)) {
        vfsKey.Build(norm, pathID, flags, 0);
        genBefore = g_vfsGeneration.load(std::memory_order_acquire);
        if (canShareRamCache) {
            ramKey.Build(norm, pathID, 0, isBinary ? 'b' : 't');
        }

        std::shared_ptr<const RawByteBuf> ramHitData;
        std::string ramHitResolved;
        ResolvedState ramHitState = ResolvedState::Unknown;
        AcquireSRWLockShared(&g_vfsCacheLock);
        if (g_vfsCacheGen == genBefore) {
            if (canShareRamCache) {
                auto rit = g_vfsRamCache.find(ramKey.view);
                if (rit != g_vfsRamCache.end()) {
                    ramHitData = rit->second.data;
                    ramHitResolved = rit->second.resolvedFilename;
                    ramHitState = rit->second.resolvedState;
                }
            }
            if (!ramHitData && canUseNegCache && g_vfsOpenMissCache.find(vfsKey.view) != g_vfsOpenMissCache.end()) {
                servedFromNegCache = true;
            }
        }
        ReleaseSRWLockShared(&g_vfsCacheLock);

        if (ramHitData) {
            bool resolvedOk = true;
            char* allocResolved = nullptr;
            if (ppszResolvedFilename) {
                if (ramHitState == ResolvedState::Valid) {
                    allocResolved = AllocTier0String(ramHitResolved);
                    if (!allocResolved) resolvedOk = false;
                } else if (ramHitState == ResolvedState::Null) {
                    allocResolved = nullptr;
                } else {
                    resolvedOk = false;
                    g_optStats.vfsRejUnknownResolved.fetch_add(1, std::memory_order_relaxed);
                }
            }
            if (resolvedOk) {
                VirtualFileHandle* vh = AllocVirtualHandle(std::move(ramHitData), norm);
                if (vh) {
                    if (ppszResolvedFilename) *ppszResolvedFilename = allocResolved;
                    handle = static_cast<void*>(vh);
                    servedFromRamCache = true;
                    g_optStats.vfsRamOpenHits.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    if (servedFromNegCache) {
        if (!servedFromBspPak) {
            g_optStats.vfsOpenMissHits.fetch_add(1, std::memory_order_relaxed);
        }
        handle = nullptr;
    } else if (!servedFromRamCache) {
        int64_t tNat0 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;
        handle = g_origOpenEx(self, pFileName, pOptions, flags, pathID, ppszResolvedFilename);
        int64_t tNat1 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;
        if (g_cfg.profilerEnabled) {
            g_optStats.openNativeUs.fetch_add(static_cast<uint64_t>(std::max(0.0, g_timer.DeltaUs(tNat0, tNat1))), std::memory_order_relaxed);
        }
        if (handle) {
            // Layer 4 v6: Smart slurp policy:
            //   - .vtx: per-handle slurp up to 16 MB with read-loop (eliminates 434k micro-reads, freed on Close)
            //   - High-reuse (.vmt/.vhv/.ani/.vcs/...): slurp up to 2 MB and store in shared g_vfsRamCache
            //   - One-shot single-read (.vvd/.mdl/.lua/.phy): skip slurp entirely (zero wasted alloc/memcpy)
            if (canSlurpHandle || canVcsPersistent) {
                void* subThis = reinterpret_cast<uint8_t*>(self) + 8;
                const uint32_t maxFileBytes = isSlurpOnly
                    ? (g_cfg.vfsVtxSlurpMaxKb * 1024U)
                    : (g_cfg.vfsRamCacheMaxFileKb * 1024U);
                const uint64_t maxTotalBytes = static_cast<uint64_t>(g_cfg.vfsRamCacheMaxMb) * 1024ULL * 1024ULL;
                unsigned int fsize = g_origSizeHandle(subThis, handle);
                if (fsize > maxFileBytes) {
                    g_optStats.vfsRamSlurpSkipSize.fetch_add(1, std::memory_order_relaxed);
                } else if (fsize > 0) {
                    auto vec = isSlurpOnly ? AcquirePooledVtxVector(fsize) : std::make_shared<RawByteBuf>(fsize);
                    size_t totalGot = 0;
                    while (totalGot < fsize) {
                        int rem = static_cast<int>(fsize - totalGot);
                        int got = g_origReadEx(self, vec->data() + totalGot, rem, rem, handle);
                        if (got <= 0) break;
                        totalGot += static_cast<size_t>(got);
                    }
                    bool slurpOk = false;
                    if (isBinary || totalGot == static_cast<size_t>(fsize)) {
                        // v13: totalGot == fsize in text mode already guarantees EOF without extra 1-byte ReadEx!
                        slurpOk = (totalGot == static_cast<size_t>(fsize));
                    } else if (totalGot < static_cast<size_t>(fsize)) {
                        // Text mode shrank \r\n -> \n; verify true EOF via 1-byte probe
                        char probe = 0;
                        int p = g_origReadEx(self, &probe, 1, 1, handle);
                        if (p == 0) {
                            vec->resize_noinit(totalGot);
                            slurpOk = true;
                        }
                    }

                    if (slurpOk) {
                        std::shared_ptr<const RawByteBuf> constVec = vec;
                        VirtualFileHandle* vh = AllocVirtualHandle(constVec, norm);
                        if (vh) {
                            g_origClose(subThis, handle);
                            handle = static_cast<void*>(vh);
                            g_optStats.vfsRamSlurpedHandles.fetch_add(1, std::memory_order_relaxed);
                            ResolvedState rState = ResolvedState::Unknown;
                            std::string resolvedStr;
                            if (ppszResolvedFilename) {
                                if (*ppszResolvedFilename && **ppszResolvedFilename) {
                                    rState = ResolvedState::Valid;
                                    resolvedStr = *ppszResolvedFilename;
                                } else {
                                    rState = ResolvedState::Null;
                                }
                            }
                            if (canVcsPersistent) {
                                AcquireSRWLockExclusive(&g_vcsCacheLock);
                                auto existingVcs = g_vcsRamCache.find(ramKey.view);
                                if (existingVcs != g_vcsRamCache.end()) {
                                    if (existingVcs->second.resolvedState == ResolvedState::Unknown && rState != ResolvedState::Unknown) {
                                        existingVcs->second.resolvedFilename = std::move(resolvedStr);
                                        existingVcs->second.resolvedState = rState;
                                    }
                                } else if (g_vcsRamBytesUsed + vec->size() <= 64ULL * 1024ULL * 1024ULL) {
                                    g_vcsRamCache.emplace(std::string(ramKey.view), VfsRamEntry{std::move(constVec), std::move(resolvedStr), rState});
                                    g_vcsRamBytesUsed += vec->size();
                                    g_optStats.vfsRamCachedFiles.fetch_add(1, std::memory_order_relaxed);
                                }
                                ReleaseSRWLockExclusive(&g_vcsCacheLock);
                            } else if (canShareRamCache) {
                                if (g_vfsGeneration.load(std::memory_order_acquire) != genBefore) {
                                    g_optStats.vfsSlurpSkipGen.fetch_add(1, std::memory_order_relaxed);
                                } else if ((g_vfsRamBytesUsed.load(std::memory_order_relaxed) + vec->size()) > maxTotalBytes) {
                                    g_optStats.vfsSlurpSkipBudget.fetch_add(1, std::memory_order_relaxed);
                                } else {
                                    AcquireSRWLockExclusive(&g_vfsCacheLock);
                                    EnsureVfsCacheGenLocked(genBefore);
                                    auto existingVfs = g_vfsRamCache.find(ramKey.view);
                                    if (existingVfs != g_vfsRamCache.end()) {
                                        if (existingVfs->second.resolvedState == ResolvedState::Unknown && rState != ResolvedState::Unknown) {
                                            existingVfs->second.resolvedFilename = std::move(resolvedStr);
                                            existingVfs->second.resolvedState = rState;
                                        }
                                    } else {
                                        g_vfsRamCache.emplace(std::string(ramKey.view), VfsRamEntry{std::move(constVec), std::move(resolvedStr), rState});
                                        g_vfsRamBytesUsed.fetch_add(vec->size(), std::memory_order_relaxed);
                                        g_optStats.vfsRamCachedFiles.fetch_add(1, std::memory_order_relaxed);
                                    }
                                    ReleaseSRWLockExclusive(&g_vfsCacheLock);
                                }
                            }
                        } else {
                            g_origSeek(subThis, handle, 0, 0);
                        }
                    } else {
                        g_optStats.vfsRamSlurpFail.fetch_add(1, std::memory_order_relaxed);
                        // Rewind real handle cleanly and fall back without caching
                        g_origSeek(subThis, handle, 0, 0);
                    }
                }
                if (g_cfg.profilerEnabled) {
                    int64_t tSlurpEnd = g_timer.NowTick();
                    g_optStats.openVfsSlurpUs.fetch_add(static_cast<uint64_t>(std::max(0.0, g_timer.DeltaUs(tNat1, tSlurpEnd))), std::memory_order_relaxed);
                }
            } else if (canVtfRamCache && g_ramGuardLevel.load(std::memory_order_relaxed) <= 1) {
                // Layer 4c: Slurp .vtf into bounded load-time LRU RAM cache (v13: dedicated g_vtfGeneration + 3-state ResolvedState!)
                void* subThis = reinterpret_cast<uint8_t*>(self) + 8;
                const uint64_t maxVtfBytes = static_cast<uint64_t>(g_cfg.vtfRamCacheMaxFileMb) * 1024ULL * 1024ULL;
                unsigned int fsize = g_origSizeHandle(subThis, handle);
                if (fsize > 0 && static_cast<uint64_t>(fsize) <= maxVtfBytes) {
                    auto vec = std::make_shared<RawByteBuf>(fsize);
                    size_t totalGot = 0;
                    while (totalGot < fsize) {
                        int rem = static_cast<int>(fsize - totalGot);
                        int got = g_origReadEx(self, vec->data() + totalGot, rem, rem, handle);
                        if (got <= 0) break;
                        totalGot += static_cast<size_t>(got);
                    }
                    if (totalGot == static_cast<size_t>(fsize)) {
                        std::shared_ptr<const RawByteBuf> constVec = vec;
                        VirtualFileHandle* vh = AllocVirtualHandle(constVec, norm);
                        if (vh) {
                            g_origClose(subThis, handle);
                            handle = static_cast<void*>(vh);
                            ResolvedState rState = ResolvedState::Unknown;
                            std::string resolvedStr;
                            if (ppszResolvedFilename) {
                                if (*ppszResolvedFilename && **ppszResolvedFilename) {
                                    rState = ResolvedState::Valid;
                                    resolvedStr = *ppszResolvedFilename;
                                } else {
                                    rState = ResolvedState::Null;
                                }
                            }
                            InsertVtfRamCache(std::string(ramKey.view), vtfGenBefore, std::move(constVec), std::move(resolvedStr), rState);
                        } else {
                            g_origSeek(subThis, handle, 0, 0);
                        }
                    } else {
                        g_origSeek(subThis, handle, 0, 0);
                    }
                } else if (fsize > maxVtfBytes) {
                    g_optStats.vtfSlurpSkipSize.fetch_add(1, std::memory_order_relaxed);
                }
                if (g_cfg.profilerEnabled) {
                    int64_t tSlurpEnd = g_timer.NowTick();
                    g_optStats.openVtfSlurpUs.fetch_add(static_cast<uint64_t>(std::max(0.0, g_timer.DeltaUs(tNat1, tSlurpEnd))), std::memory_order_relaxed);
                }
            }
        } else if (canVcsPersistent) {
            AcquireSRWLockExclusive(&g_vcsCacheLock);
            if (g_vcsMissCache.size() < 16384) {
                g_vcsMissCache.emplace(std::string(vfsKey.view), true);
            }
            ReleaseSRWLockExclusive(&g_vcsCacheLock);
        } else if (canUseNegCache && g_vfsGeneration.load(std::memory_order_acquire) == genBefore) {
            AcquireSRWLockExclusive(&g_vfsCacheLock);
            EnsureVfsCacheGenLocked(genBefore);
            if (g_vfsOpenMissCache.size() < 65536) {
                g_vfsOpenMissCache.emplace(std::string(vfsKey.view), true);
            }
            ReleaseSRWLockExclusive(&g_vfsCacheLock);
        }
    }

    int64_t t1 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;

    if (!norm.empty()) {
        std::string bspMap = ExtractMapFromBspPath(norm);
        if (!bspMap.empty() && handle != nullptr && isPureRead) {
            void* bspRetAddr = retAddr ? retAddr : PROFILER_RETURN_ADDRESS();
            const std::string& bspCaller = ResolveCallerModuleFast(bspRetAddr);
            const bool isEngineOrFs = (bspCaller == "filesystem_stdio.dll" || bspCaller == "engine.dll");
            const uint32_t curTid = GetCurrentThreadId();
            const uint32_t mainTid = g_mainThreadId.load(std::memory_order_relaxed);
            const bool isMainThread = (mainTid == 0 || curTid == mainTid);

            if (isEngineOrFs && isMainThread) {
                bool needStart = false;
                const int64_t nowTk = g_timer.NowTick();
                AcquireSRWLockShared(&g_sessionLock);
                if (!g_session.active) {
                    const bool sameLoadedMap = g_inGameOnMap && (g_loadedMapName == bspMap);
                    const double sinceEndMs = (g_lastSessionEndTick > 0)
                        ? (g_timer.DeltaUs(g_lastSessionEndTick, nowTk) / 1000.0)
                        : 1e9;
                    if (!sameLoadedMap && (sinceEndMs >= 3000.0 || !g_inGameOnMap)) {
                        needStart = true;
                    }
                } else if (g_session.mapName != "unknown_map" && g_session.mapName != bspMap) {
                    needStart = true;
                }
                ReleaseSRWLockShared(&g_sessionLock);
                if (needStart) {
                    StartNewSession("BSP_Open_Fallback", bspMap);
                }
            }
        }

        if (handle && !IsVirtualHandle(handle) &&
            (g_cfg.profilerEnabled || (g_cfg.enableOptimizer && g_cfg.optVfsReadAhead))) {
            auto info = std::make_shared<RealHandleInfo>();
            info->normPath = norm;
            const bool raOk = g_cfg.enableOptimizer && g_cfg.optVfsReadAhead &&
                              isPureRead && isBinary &&
                              g_origSeek && g_origTell && g_origSizeHandle && g_origReadEx;
            info->raEligible = raOk;
            if (raOk && norm.ends_with(".bsp")) {
                void* subThis = reinterpret_cast<uint8_t*>(self) + 8;
                unsigned int fsz = g_origSizeHandle(subThis, handle);
                if (fsz > 256U * 1024U && fsz < 0x7F000000U) {
                    info->raFileSize = fsz;
                    info->raLogicalPos = 0;
                    info->raRealPos = 0;
                    info->raActive = true;
                    if (g_cfg.optMapPackCache && norm != "cache/map_pack.bsp") {
                        tl_lastBspRead.handle = handle;
                        tl_lastBspRead.normPath = norm;
                        tl_lastBspRead.fileSize = fsz;
                        tl_lastBspRead.openTick = g_timer.NowTick();
                    }
                }
            }
            AcquireSRWLockExclusive(&g_handleMapLock);
            g_openHandles[handle] = std::move(info);
            ReleaseSRWLockExclusive(&g_handleMapLock);
        }

        if (g_cfg.profilerEnabled) {

            if (g_sessionActive.load(std::memory_order_relaxed)) {
                double durUs = g_timer.DeltaUs(t0, t1);
                uint32_t tid = GetCurrentThreadId();
                const std::string& caller = ResolveCallerModuleFast(retAddr);
                std::string mapToPrefetch;

                AcquireSRWLockExclusive(&g_sessionLock);
                if (g_session.active) {
                    if (!bspMap.empty() && g_session.mapName == "unknown_map") {
                        g_session.mapName = bspMap;
                        RecordMilestoneLocked("Map_BSP_FirstOpen", norm, t0, tid);
                        if (!g_session.prefetchLaunched) {
                            g_session.prefetchLaunched = true;
                            mapToPrefetch = bspMap;
                        }
                    }
                auto& ca = g_session.byCaller[caller];
                ++ca.openCalls;
                if (!handle) ++ca.openMisses;
                ca.openUs += durUs;

                TimelineEvent ev;
                ev.startTick = t0;
                ev.durUs = durUs;
                ev.tid = tid;
                ev.kind = EventKind::FsOpen;
                ev.hit = (handle != nullptr);
                    ev.primary = std::move(norm);
                    ev.caller = caller;
                    if (pathID && *pathID) {
                        ev.secondary = pathID;
                        if (pOptions && *pOptions) {
                            ev.secondary.push_back(':');
                            ev.secondary += pOptions;
                        }
                    } else if (pOptions && *pOptions) {
                        ev.secondary = pOptions;
                    }
                    if (servedFromNegCache) {
                        ev.secondary += " [VFS_NEG_CACHE]";
                    } else if (servedFromRamCache) {
                        ev.secondary += " [VFS_RAM_CACHE]";
                    }
                    if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
                }
                ReleaseSRWLockExclusive(&g_sessionLock);
                if (!mapToPrefetch.empty()) LaunchMapPrefetch(mapToPrefetch);
            }
        }
    }

    tl_inHook = false;
    return handle;
}

int HookedReadEx(void* self, void* pOutput, int sizeDest, int size, void* file) {
    // Ultra-fast RAM path for virtual handles (zero locks, zero syscalls, slice-aware!)
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        if (!vh->basePtr || !pOutput || size <= 0 || sizeDest <= 0) return 0;
        const size_t want = static_cast<size_t>(std::min(size, sizeDest));
        const size_t total = vh->dataLen;
        if (vh->pos >= total) return 0;
        const size_t avail = std::min<size_t>(want, total - vh->pos);
        std::memcpy(pOutput, vh->basePtr + vh->pos, avail);
        vh->pos += avail;
        vh->readCalls += 1;
        vh->readBytes += avail;
        return static_cast<int>(avail);
    }

    if (tl_inHook || !g_origReadEx || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origReadEx ? g_origReadEx(self, pOutput, sizeDest, size, file) : 0;
    }

    // Layer 4b TLS 1-slot handle fast-path (avoids 350k+ g_handleMapLock acquisitions on .bsp micro-reads!)
    thread_local uint64_t tl_lastHandleGen = 0;
    thread_local void* tl_lastHandlePtr = nullptr;
    thread_local std::weak_ptr<RealHandleInfo> tl_lastHandleWeak;

    std::shared_ptr<RealHandleInfo> info;
    if (file && (g_cfg.profilerEnabled || (g_cfg.enableOptimizer && g_cfg.optVfsReadAhead))) {
        const uint64_t curGen = g_handleCloseGen.load(std::memory_order_acquire);
        if (tl_lastHandleGen == curGen && tl_lastHandlePtr == file) {
            info = tl_lastHandleWeak.lock();
        }
        if (!info) {
            AcquireSRWLockShared(&g_handleMapLock);
            auto it = g_openHandles.find(file);
            if (it != g_openHandles.end()) info = it->second;
            ReleaseSRWLockShared(&g_handleMapLock);
            tl_lastHandleGen = curGen;
            tl_lastHandlePtr = file;
            tl_lastHandleWeak = info;
        }
    }

    // Layer 4b: 4 MB streaming read-ahead buffer for .bsp & micro-read binary handles
    if (info && info->raEligible && pOutput && size > 0 && sizeDest > 0) {
        const size_t want = static_cast<size_t>(std::min(size, sizeDest));
        void* retAddrRa = g_cfg.profilerEnabled ? PROFILER_RETURN_ADDRESS() : nullptr;
        tl_inHook = true;
        void* subThis = reinterpret_cast<uint8_t*>(self) + 8;

        AcquireSRWLockExclusive(&info->lock);
        if (info->raInstantEof) {
            ReleaseSRWLockExclusive(&info->lock);
            tl_inHook = false;
            return 0;
        }
        if (!info->raActive && want <= 65536) {
            ++info->smallReadStreak;
            if (info->smallReadStreak >= 4) {
                unsigned int fsz = g_origSizeHandle(subThis, file);
                if (fsz > 256U * 1024U && fsz < 0x7F000000U) {
                    unsigned int curP = g_origTell(subThis, file);
                    info->raFileSize = fsz;
                    info->raLogicalPos = curP;
                    info->raRealPos = curP;
                    info->raActive = true;
                } else {
                    info->raEligible = false;
                }
            }
        }

        if (info->raActive) {
            int64_t t0 = 0;
            uint8_t* dst = static_cast<uint8_t*>(pOutput);
            size_t copied = 0;
            bool didRefill = false;

            while (copied < want) {
                if (info->raLogicalPos >= info->raFileSize) break;
                // 1) Serve directly from 4 MB in-memory buffer if hit
                if (info->raBufLen > 0 &&
                    info->raLogicalPos >= info->raBufStart &&
                    info->raLogicalPos < info->raBufStart + info->raBufLen) {
                    const size_t off = static_cast<size_t>(info->raLogicalPos - info->raBufStart);
                    const size_t avail = std::min<size_t>(want - copied, info->raBufLen - off);
                    std::memcpy(dst + copied, info->raBuf.data() + off, avail);
                    info->raLogicalPos += avail;
                    copied += avail;
                    continue;
                }

                // 2) Buffer miss: sync underlying file pointer if needed
                if (info->raRealPos != info->raLogicalPos) {
                    g_origSeek(subThis, file, static_cast<int>(info->raLogicalPos), 0);
                    info->raRealPos = info->raLogicalPos;
                }

                const size_t rem = want - copied;
                if (rem >= (kVfsReadAheadChunkBytes / 2)) {
                    // Large direct read (>= 2 MB): bypass buffer copy
                    if (!didRefill && g_cfg.profilerEnabled) t0 = g_timer.NowTick();
                    int got = g_origReadEx(self, dst + copied, static_cast<int>(rem), static_cast<int>(rem), file);
                    didRefill = true;
                    if (got <= 0) break;
                    info->raLogicalPos += static_cast<uint64_t>(got);
                    info->raRealPos += static_cast<uint64_t>(got);
                    copied += static_cast<size_t>(got);
                } else {
                    // Small read: refill 4 MB read-ahead chunk in one single IFileSystem call!
                    if (info->raBuf.size() < kVfsReadAheadChunkBytes) {
                        info->raBuf.resize(kVfsReadAheadChunkBytes);
                    }
                    const uint64_t maxAvail = info->raFileSize - info->raLogicalPos;
                    const int toRead = static_cast<int>(std::min<uint64_t>(kVfsReadAheadChunkBytes, maxAvail));
                    if (!didRefill && g_cfg.profilerEnabled) t0 = g_timer.NowTick();
                    int got = g_origReadEx(self, info->raBuf.data(), toRead, toRead, file);
                    didRefill = true;
                    g_optStats.vfsRaRefills.fetch_add(1, std::memory_order_relaxed);
                    if (got <= 0) {
                        info->raBufLen = 0;
                        break;
                    }
                    info->raBufStart = info->raLogicalPos;
                    info->raBufLen = static_cast<size_t>(got);
                    info->raRealPos = info->raLogicalPos + static_cast<uint64_t>(got);
                    const size_t take = std::min<size_t>(rem, info->raBufLen);
                    std::memcpy(dst + copied, info->raBuf.data(), take);
                    info->raLogicalPos += take;
                    copied += take;
                }
            }

            if (!didRefill && copied > 0) {
                info->raHits += 1;
                info->raBytes += copied;
            }

            if (g_cfg.profilerEnabled && g_sessionActive.load(std::memory_order_relaxed)) {
                double durUs = didRefill ? g_timer.DeltaUs(t0, g_timer.NowTick()) : 0.0;
                ++info->readCalls;
                info->readBytes += copied;
                info->readUs += durUs;
                if (durUs > info->maxReadUs) info->maxReadUs = durUs;
                info->lastCaller = retAddrRa;
            }
            ReleaseSRWLockExclusive(&info->lock);
            tl_inHook = false;
            return static_cast<int>(copied);
        }
        ReleaseSRWLockExclusive(&info->lock);
        tl_inHook = false;
    }

    if (!g_cfg.profilerEnabled || !g_sessionActive.load(std::memory_order_relaxed)) {
        return g_origReadEx(self, pOutput, sizeDest, size, file);
    }

    void* retAddr = PROFILER_RETURN_ADDRESS();
    tl_inHook = true;
    int64_t t0 = g_timer.NowTick();
    int bytesRead = g_origReadEx(self, pOutput, sizeDest, size, file);
    int64_t t1 = g_timer.NowTick();

    double durUs = g_timer.DeltaUs(t0, t1);
    uint64_t b = bytesRead > 0 ? static_cast<uint64_t>(bytesRead) : 0ULL;

    if (info) {
        AcquireSRWLockExclusive(&info->lock);
        ++info->readCalls;
        info->readBytes += b;
        info->readUs += durUs;
        if (durUs > info->maxReadUs) info->maxReadUs = durUs;
        info->lastCaller = retAddr;
        ReleaseSRWLockExclusive(&info->lock);

        if (durUs >= static_cast<double>(g_cfg.readTraceMinUs)) {
            const std::string& caller = ResolveCallerModuleFast(retAddr);
            AcquireSRWLockExclusive(&g_sessionLock);
            if (g_session.active) {
                TimelineEvent ev;
                ev.startTick = t0;
                ev.durUs = durUs;
                ev.tid = GetCurrentThreadId();
                ev.kind = EventKind::FsRead;
                ev.hit = (bytesRead > 0);
                ev.bytes = b;
                ev.primary = info->normPath;
                ev.caller = caller;
                if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
            }
            ReleaseSRWLockExclusive(&g_sessionLock);
        }
    } else {
        const std::string& caller = ResolveCallerModuleFast(retAddr);
        AcquireSRWLockExclusive(&g_sessionLock);
        if (g_session.active) {
            auto& ra = g_session.readByFile["(unknown_handle)"];
            ++ra.calls;
            ra.bytes += b;
            ra.totalUs += durUs;
            if (durUs > ra.maxUs) ra.maxUs = durUs;

            auto& ca = g_session.byCaller[caller];
            ++ca.readCalls;
            ca.readBytes += b;
            ca.readUs += durUs;

            if (durUs >= static_cast<double>(g_cfg.readTraceMinUs)) {
                TimelineEvent ev;
                ev.startTick = t0;
                ev.durUs = durUs;
                ev.tid = GetCurrentThreadId();
                ev.kind = EventKind::FsRead;
                ev.hit = (bytesRead > 0);
                ev.bytes = b;
                ev.primary = "(unknown_handle)";
                ev.caller = caller;
                if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
            }
        }
        ReleaseSRWLockExclusive(&g_sessionLock);
    }

    tl_inHook = false;
    return bytesRead;
}

void HookedSetBufferSize(void* self, void* file, unsigned nBytes) {
    if (IsVirtualHandle(file)) return;
    if (g_origSetBufferSize) g_origSetBufferSize(self, file, nBytes);
}

int HookedWrite(void* self, const void* pInput, int size, void* file) {
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        if (vh->isWriteSink && size >= 0) {
            vh->pos += static_cast<size_t>(size);
            return size;
        }
        return 0;
    }
    return g_origWrite ? g_origWrite(self, pInput, size, file) : 0;
}

bool HookedIsOk(void* self, void* file) {
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        return vh->inUse && (vh->isWriteSink || vh->data != nullptr);
    }
    return g_origIsOk ? g_origIsOk(self, file) : false;
}

std::shared_ptr<RealHandleInfo> FindRealHandleInfo(void* file) {
    if (!file || g_shuttingDown.load(std::memory_order_relaxed)) return nullptr;
    AcquireSRWLockShared(&g_handleMapLock);
    auto it = g_openHandles.find(file);
    std::shared_ptr<RealHandleInfo> res = (it != g_openHandles.end()) ? it->second : nullptr;
    ReleaseSRWLockShared(&g_handleMapLock);
    return res;
}

bool HookedEndOfFile(void* self, void* file) {
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        return !vh->data || vh->pos >= vh->dataLen;
    }
    if (tl_inHook) {
        return g_origEndOfFile ? g_origEndOfFile(self, file) : true;
    }
    if (g_cfg.enableOptimizer && g_cfg.optVfsReadAhead) {
        if (auto info = FindRealHandleInfo(file)) {
            AcquireSRWLockShared(&info->lock);
            if (info->raActive) {
                bool eof = (info->raLogicalPos >= info->raFileSize);
                ReleaseSRWLockShared(&info->lock);
                return eof;
            }
            ReleaseSRWLockShared(&info->lock);
        }
    }
    return g_origEndOfFile ? g_origEndOfFile(self, file) : true;
}

char* HookedReadLine(void* self, char* pOutput, int maxChars, void* file) {
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        if (!vh->basePtr || !pOutput || maxChars <= 0) return nullptr;
        const size_t total = vh->dataLen;
        if (vh->pos >= total) {
            pOutput[0] = '\0';
            return nullptr;
        }
        const uint8_t* bytes = vh->basePtr;
        int written = 0;
        while (written < maxChars - 1 && vh->pos < total) {
            char c = static_cast<char>(bytes[vh->pos++]);
            pOutput[written++] = c;
            if (c == '\n') break;
        }
        pOutput[written] = '\0';
        vh->readCalls += 1;
        vh->readBytes += static_cast<uint64_t>(written);
        return pOutput;
    }
    if (tl_inHook) {
        return g_origReadLine ? g_origReadLine(self, pOutput, maxChars, file) : nullptr;
    }
    if (g_cfg.enableOptimizer && g_cfg.optVfsReadAhead) {
        if (auto info = FindRealHandleInfo(file)) {
            AcquireSRWLockExclusive(&info->lock);
            if (info->raActive) {
                if (info->raRealPos != info->raLogicalPos && g_origSeek) {
                    void* subThis = reinterpret_cast<uint8_t*>(self) + 8;
                    tl_inHook = true;
                    g_origSeek(subThis, file, static_cast<int>(info->raLogicalPos), 0);
                    tl_inHook = false;
                    info->raRealPos = info->raLogicalPos;
                }
                info->raActive = false;
                info->raEligible = false;
                info->raBufLen = 0;
                info->raBuf.clear();
            }
            ReleaseSRWLockExclusive(&info->lock);
        }
    }
    return g_origReadLine ? g_origReadLine(self, pOutput, maxChars, file) : nullptr;
}

void HookedSeek(void* self, void* file, int pos, int seekType) {
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        if (!vh->data) return;
        int64_t total = static_cast<int64_t>(vh->dataLen);
        int64_t target = 0;
        if (seekType == 0) target = pos;                                      // FILESYSTEM_SEEK_HEAD
        else if (seekType == 1) target = static_cast<int64_t>(vh->pos) + pos; // FILESYSTEM_SEEK_CURRENT
        else if (seekType == 2) target = total + pos;                         // FILESYSTEM_SEEK_TAIL
        if (target < 0) target = 0;
        if (target > total) target = total;
        vh->pos = static_cast<size_t>(target);
        return;
    }
    if (tl_inHook) {
        if (g_origSeek) g_origSeek(self, file, pos, seekType);
        return;
    }
    if (g_cfg.enableOptimizer && g_cfg.optVfsReadAhead) {
        if (auto info = FindRealHandleInfo(file)) {
            AcquireSRWLockExclusive(&info->lock);
            if (info->raActive) {
                int64_t total = static_cast<int64_t>(info->raFileSize);
                int64_t target = 0;
                if (seekType == 0) target = pos;
                else if (seekType == 1) target = static_cast<int64_t>(info->raLogicalPos) + pos;
                else if (seekType == 2) target = total + pos;
                if (target < 0) target = 0;
                if (target > total) target = total;
                info->raLogicalPos = static_cast<uint64_t>(target);
                ReleaseSRWLockExclusive(&info->lock);
                return;
            }
            ReleaseSRWLockExclusive(&info->lock);
        }
    }
    if (g_origSeek) g_origSeek(self, file, pos, seekType);
}

unsigned int HookedTell(void* self, void* file) {
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        return static_cast<unsigned int>(vh->pos);
    }
    if (tl_inHook) {
        return g_origTell ? g_origTell(self, file) : 0;
    }
    if (g_cfg.enableOptimizer && g_cfg.optVfsReadAhead) {
        if (auto info = FindRealHandleInfo(file)) {
            AcquireSRWLockShared(&info->lock);
            if (info->raActive) {
                unsigned int p = static_cast<unsigned int>(info->raLogicalPos);
                ReleaseSRWLockShared(&info->lock);
                return p;
            }
            ReleaseSRWLockShared(&info->lock);
        }
    }
    return g_origTell ? g_origTell(self, file) : 0;
}

unsigned int HookedSizeHandle(void* self, void* file) {
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        return vh->data ? static_cast<unsigned int>(vh->dataLen) : 0U;
    }
    return g_origSizeHandle ? g_origSizeHandle(self, file) : 0U;
}

unsigned int HookedSizePath(void* self, const char* pFileName, const char* pPathID) {
    if (tl_inHook || !g_origSizePath || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origSizePath ? g_origSizePath(self, pFileName, pPathID) : 0U;
    }
    if (g_cfg.enableOptimizer && g_cfg.optVfsMetaCache && pFileName && *pFileName) {
        tl_inHook = true;
        std::string norm = NormalizePath(pFileName);
        if (IsDynamicMpPathNarrow(norm)) {
            unsigned int sz = g_origSizePath(self, pFileName, pPathID);
            tl_inHook = false;
            return sz;
        }
        StackVfsKey key;
        key.Build(norm, pPathID);
        uint64_t gen = g_vfsGeneration.load(std::memory_order_acquire);
        unsigned int sz = 0;
        bool hit = false;
        if (g_cfg.optBspPakFastPath && g_bspPakActive.load(std::memory_order_acquire) &&
            (IsBspPathId(pPathID) || IsBspPakPreloadablePath(norm))) {
            AcquireSRWLockShared(&g_bspPakLock);
            auto bit = g_bspPakEntries.find(norm);
            if (bit != g_bspPakEntries.end() && bit->second.inSlab) {
                sz = static_cast<unsigned int>(bit->second.slabLen);
                hit = true;
            }
            ReleaseSRWLockShared(&g_bspPakLock);
        }
        if (!hit) {
            AcquireSRWLockShared(&g_vfsCacheLock);
            if (g_vfsCacheGen == gen) {
                auto it = g_vfsSizeCache.find(key.view);
                if (it != g_vfsSizeCache.end()) {
                    sz = it->second;
                    hit = true;
                } else {
                    StackVfsKey rkey;
                    rkey.Build(norm, pPathID, 0, 'b');
                    auto rit = g_vfsRamCache.find(rkey.view);
                    if (rit != g_vfsRamCache.end() && rit->second.data) {
                        sz = static_cast<unsigned int>(rit->second.data->size());
                        hit = true;
                    }
                }
            }
            ReleaseSRWLockShared(&g_vfsCacheLock);
        }
        if (hit) {
            g_optStats.vfsSizeHits.fetch_add(1, std::memory_order_relaxed);
            tl_inHook = false;
            return sz;
        }
        sz = g_origSizePath(self, pFileName, pPathID);
        if (g_vfsGeneration.load(std::memory_order_acquire) == gen) {
            AcquireSRWLockExclusive(&g_vfsCacheLock);
            EnsureVfsCacheGenLocked(gen);
            if (g_vfsSizeCache.size() < 65536) {
                g_vfsSizeCache.emplace(std::string(key.view), sz);
            }
            ReleaseSRWLockExclusive(&g_vfsCacheLock);
        }
        tl_inHook = false;
        return sz;
    }
    return g_origSizePath(self, pFileName, pPathID);
}

void HookedFlush(void* self, void* file) {
    if (IsVirtualHandle(file)) return;
    if (g_origFlush) g_origFlush(self, file);
}

void HookedClose(void* self, void* file) {
    if (file && tl_lastBspRead.handle == file) {
        tl_lastBspRead.handle = nullptr;
        tl_lastBspRead.normPath.clear();
        tl_lastBspRead.fileSize = 0;
    }
    if (IsVirtualHandle(file)) {
        auto* vh = static_cast<VirtualFileHandle*>(file);
        if (vh->readCalls > 0) {
            g_optStats.vfsRamReadHits.fetch_add(vh->readCalls, std::memory_order_relaxed);
            g_optStats.vfsRamReadBytes.fetch_add(vh->readBytes, std::memory_order_relaxed);
        }
        // Flush batched virtual-read counters once on Close (zero per-read lock overhead!)
        if (g_cfg.profilerEnabled && vh->readCalls > 0 && !g_shuttingDown.load(std::memory_order_relaxed)) {
            AcquireSRWLockExclusive(&g_sessionLock);
            if (g_session.active) {
                auto& ra = g_session.readByFile[vh->normPath];
                ra.calls += vh->readCalls;
                ra.bytes += vh->readBytes;
                ra.totalUs += vh->readUs;
            }
            ReleaseSRWLockExclusive(&g_sessionLock);
        }
        FreeVirtualHandle(vh);
        return;
    }
    if ((g_cfg.profilerEnabled || (g_cfg.enableOptimizer && g_cfg.optVfsReadAhead)) &&
        file && !g_shuttingDown.load(std::memory_order_relaxed)) {
        std::shared_ptr<RealHandleInfo> info;
        AcquireSRWLockExclusive(&g_handleMapLock);
        auto it = g_openHandles.find(file);
        if (it != g_openHandles.end()) {
            info = std::move(it->second);
            g_openHandles.erase(it);
        }
        ReleaseSRWLockExclusive(&g_handleMapLock);
        if (info) {
            g_handleCloseGen.fetch_add(1, std::memory_order_release);
            if (info->raHits > 0) {
                g_optStats.vfsRaHits.fetch_add(info->raHits, std::memory_order_relaxed);
                g_optStats.vfsRaBytes.fetch_add(info->raBytes, std::memory_order_relaxed);
            }
        }

        if (g_cfg.profilerEnabled && info && info->readCalls > 0 && g_sessionActive.load(std::memory_order_relaxed)) {
            static const std::string kUnknownCaller("(unknown)");
            const std::string& caller = info->lastCaller ? ResolveCallerModuleFast(info->lastCaller) : kUnknownCaller;
            AcquireSRWLockExclusive(&g_sessionLock);
            if (g_session.active) {
                auto& ra = g_session.readByFile[info->normPath];
                ra.calls += info->readCalls;
                ra.bytes += info->readBytes;
                ra.totalUs += info->readUs;
                if (info->maxReadUs > ra.maxUs) ra.maxUs = info->maxReadUs;

                auto& ca = g_session.byCaller[caller];
                ca.readCalls += info->readCalls;
                ca.readBytes += info->readBytes;
                ca.readUs += info->readUs;
            }
            ReleaseSRWLockExclusive(&g_sessionLock);
        }
    }
    if (g_origClose) g_origClose(self, file);
}

bool HookedFileExists(void* self, const char* pFileName, const char* pPathID) {
    if (tl_inHook || !g_origFileExists || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origFileExists ? g_origFileExists(self, pFileName, pPathID) : false;
    }

    void* retAddr = g_cfg.profilerEnabled ? PROFILER_RETURN_ADDRESS() : nullptr;
    tl_inHook = true;
    int64_t t0 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;

    std::string norm = (pFileName && *pFileName) ? NormalizePath(pFileName) : "";
    bool exists = false;
    bool cacheHit = false;

    if (g_cfg.enableOptimizer && g_cfg.optVfsMetaCache && !norm.empty() && !IsDynamicMpPathNarrow(norm)) {
        StackVfsKey key;
        key.Build(norm, pPathID);
        uint64_t gen = g_vfsGeneration.load(std::memory_order_acquire);
        AcquireSRWLockShared(&g_vfsCacheLock);
        if (g_vfsCacheGen == gen) {
            auto it = g_vfsExistsCache.find(key.view);
            if (it != g_vfsExistsCache.end()) {
                exists = it->second;
                cacheHit = true;
            }
        }
        ReleaseSRWLockShared(&g_vfsCacheLock);

        if (cacheHit) {
            g_optStats.vfsExistsHits.fetch_add(1, std::memory_order_relaxed);
        } else {
            exists = g_origFileExists(self, pFileName, pPathID);
            if (g_vfsGeneration.load(std::memory_order_acquire) == gen) {
                AcquireSRWLockExclusive(&g_vfsCacheLock);
                EnsureVfsCacheGenLocked(gen);
                if (g_vfsExistsCache.size() < 65536) {
                    g_vfsExistsCache.emplace(std::string(key.view), exists);
                }
                ReleaseSRWLockExclusive(&g_vfsCacheLock);
            }
        }
    } else {
        exists = g_origFileExists(self, pFileName, pPathID);
    }

    int64_t t1 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;

    if (g_cfg.profilerEnabled && g_sessionActive.load(std::memory_order_relaxed) && !norm.empty()) {
        double durUs = g_timer.DeltaUs(t0, t1);
        const std::string& caller = ResolveCallerModuleFast(retAddr);

        AcquireSRWLockExclusive(&g_sessionLock);
        if (g_session.active) {
            auto& ca = g_session.byCaller[caller];
            ++ca.existsCalls;
            ca.existsUs += durUs;

            TimelineEvent ev;
            ev.startTick = t0;
            ev.durUs = durUs;
            ev.tid = GetCurrentThreadId();
            ev.kind = EventKind::FsExists;
            ev.hit = exists;
            ev.primary = std::move(norm);
            ev.caller = caller;
            if (pPathID && *pPathID) ev.secondary = pPathID;
            if (cacheHit) ev.secondary += " [VFS_CACHE]";
            if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
        }
        ReleaseSRWLockExclusive(&g_sessionLock);
    }

    tl_inHook = false;
    return exists;
}

long HookedGetFileTime(void* self, const char* pFileName, const char* pPathID) {
    if (tl_inHook || !g_origGetFileTime || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origGetFileTime ? g_origGetFileTime(self, pFileName, pPathID) : 0;
    }

    void* retAddr = g_cfg.profilerEnabled ? PROFILER_RETURN_ADDRESS() : nullptr;
    tl_inHook = true;
    int64_t t0 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;

    std::string norm = (pFileName && *pFileName) ? NormalizePath(pFileName) : "";
    long res = 0;
    bool cacheHit = false;

    if (g_cfg.enableOptimizer && g_cfg.optVfsMetaCache && !norm.empty() && !IsDynamicMpPathNarrow(norm)) {
        StackVfsKey key;
        key.Build(norm, pPathID);
        uint64_t gen = g_vfsGeneration.load(std::memory_order_acquire);
        AcquireSRWLockShared(&g_vfsCacheLock);
        if (g_vfsCacheGen == gen) {
            auto it = g_vfsTimeCache.find(key.view);
            if (it != g_vfsTimeCache.end()) {
                res = it->second;
                cacheHit = true;
            }
        }
        ReleaseSRWLockShared(&g_vfsCacheLock);

        if (cacheHit) {
            g_optStats.vfsTimeHits.fetch_add(1, std::memory_order_relaxed);
        } else {
            res = g_origGetFileTime(self, pFileName, pPathID);
            if (g_vfsGeneration.load(std::memory_order_acquire) == gen) {
                AcquireSRWLockExclusive(&g_vfsCacheLock);
                EnsureVfsCacheGenLocked(gen);
                if (g_vfsTimeCache.size() < 65536) {
                    g_vfsTimeCache.emplace(std::string(key.view), res);
                }
                ReleaseSRWLockExclusive(&g_vfsCacheLock);
            }
        }
    } else {
        res = g_origGetFileTime(self, pFileName, pPathID);
    }

    int64_t t1 = g_cfg.profilerEnabled ? g_timer.NowTick() : 0;

    if (g_cfg.profilerEnabled && g_sessionActive.load(std::memory_order_relaxed) && !norm.empty()) {
        double durUs = g_timer.DeltaUs(t0, t1);
        const std::string& caller = ResolveCallerModuleFast(retAddr);

        AcquireSRWLockExclusive(&g_sessionLock);
        if (g_session.active) {
            auto& ca = g_session.byCaller[caller];
            ++ca.fileTimeCalls;
            ca.fileTimeUs += durUs;

            TimelineEvent ev;
            ev.startTick = t0;
            ev.durUs = durUs;
            ev.tid = GetCurrentThreadId();
            ev.kind = EventKind::FsGetFileTime;
            ev.hit = (res != 0);
            ev.primary = std::move(norm);
            ev.caller = caller;
            if (pPathID && *pPathID) ev.secondary = pPathID;
            if (cacheHit) ev.secondary += " [VFS_CACHE]";
            if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
        }
        ReleaseSRWLockExclusive(&g_sessionLock);
    }

    tl_inHook = false;
    return res;
}

const char* HookedFindFirstEx(void* self, const char* pWildCard, const char* pPathID, int* pHandle) {
    if (!g_cfg.profilerEnabled || tl_inHook || !g_origFindFirstEx || g_shuttingDown.load(std::memory_order_relaxed)) {
        return g_origFindFirstEx ? g_origFindFirstEx(self, pWildCard, pPathID, pHandle) : nullptr;
    }

    void* retAddr = PROFILER_RETURN_ADDRESS();
    tl_inHook = true;
    int64_t t0 = g_timer.NowTick();
    const char* res = g_origFindFirstEx(self, pWildCard, pPathID, pHandle);
    int64_t t1 = g_timer.NowTick();

    if (pWildCard && *pWildCard && g_sessionActive.load(std::memory_order_relaxed)) {
        double durUs = g_timer.DeltaUs(t0, t1);
        const std::string& caller = ResolveCallerModuleFast(retAddr);

        AcquireSRWLockExclusive(&g_sessionLock);
        if (g_session.active) {
            auto& ca = g_session.byCaller[caller];
            ++ca.findCalls;
            ca.findUs += durUs;

            TimelineEvent ev;
            ev.startTick = t0;
            ev.durUs = durUs;
            ev.tid = GetCurrentThreadId();
            ev.kind = EventKind::FsFindFirst;
            ev.hit = (res != nullptr);
            ev.primary = NormalizePath(pWildCard);
            ev.caller = caller;
            if (pPathID && *pPathID) ev.secondary = pPathID;
            if (g_session.events.size() < 262144) g_session.events.push_back(std::move(ev));
        }
        ReleaseSRWLockExclusive(&g_sessionLock);
    }

    tl_inHook = false;
    return res;
}

bool PatchVtableSlot(void** vtable, size_t idx, void* newFn, void** outOrig) {
    DWORD oldProt = 0;
    if (!VirtualProtect(&vtable[idx], sizeof(void*), PAGE_READWRITE, &oldProt)) {
        return false;
    }
    if (outOrig) *outOrig = vtable[idx];
    vtable[idx] = newFn;
    DWORD dummy = 0;
    VirtualProtect(&vtable[idx], sizeof(void*), oldProt, &dummy);
    return true;
}

thread_local bool tl_inSehProbe = false;

static bool ProbeFileSystemAbiSeh(void* subThis, void* fnExistsPtr, void* fnTimePtr) {
    auto fnEx = reinterpret_cast<FnFileExists>(fnExistsPtr);
    auto fnTm = reinterpret_cast<FnGetFileTime>(fnTimePtr);
    const char* probe = "__gmod_opt_abi_probe_nonexistent_9f8e7d__.xyz";
    tl_inSehProbe = true;
    bool ok = false;
#ifdef _MSC_VER
    __try {
        if (!fnEx(subThis, probe, nullptr) && fnTm(subThis, probe, nullptr) == 0) {
            ok = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
#else
    if (!fnEx(subThis, probe, nullptr) && fnTm(subThis, probe, nullptr) == 0) {
        ok = true;
    }
#endif
    tl_inSehProbe = false;
    return ok;
}

static size_t ExtractVtableCallSlotFromThunk(const void* fnPtr) noexcept {
    if (!fnPtr) return 0;
    const auto* b = reinterpret_cast<const uint8_t*>(fnPtr);
    // Scan x64 thunk for `call qword ptr [rax + disp32]` (FF 90) or `jmp qword ptr [rax + disp32]` (FF A0)
    for (size_t i = 0; i + 6 <= 48; ++i) {
        if (b[i] == 0xFF && (b[i + 1] == 0x90 || b[i + 1] == 0xA0)) {
            uint32_t disp = 0;
            std::memcpy(&disp, b + i + 2, sizeof(disp));
            if (disp >= 0x80 && disp <= 0x600 && (disp % 8U) == 0U) {
                return static_cast<size_t>(disp / 8U);
            }
        }
    }
    return 0;
}

bool TryInstallFilesystemHook() {
    HMODULE hFs = GetModuleHandleW(L"filesystem_stdio.dll");
    if (!hFs) return false;

    InstallFsIatHooks(hFs);

    if (!g_cfg.hookFilesystem && !g_cfg.optVfsMetaCache && !g_cfg.optVfsRamCache) return true;

    using FnCreateInterface = void*(*)(const char*, int*);
    auto ci = reinterpret_cast<FnCreateInterface>(GetProcAddress(hFs, "CreateInterface"));
    void* fsByInterface = ci ? ci("VFileSystem022", nullptr) : nullptr;
    if (ci && !fsByInterface) {
        WriteStatusLog("INFO: VFileSystem022 interface not found (newer GMod ABI); VFS vtable hooks skipped safely. Win32 IAT optimizations remain active.");
        return true;
    }

    void* fsObj = fsByInterface;
    if (!fsObj) {
        auto ppGlobal = reinterpret_cast<void**>(GetProcAddress(hFs, "?g_pFullFileSystem@@3PEAVIFileSystem@@EA"));
        if (ppGlobal && *ppGlobal) fsObj = *ppGlobal;
    }
    if (!fsObj) return false;

    void** primaryVt = *reinterpret_cast<void***>(fsObj);
    void** subVt = *reinterpret_cast<void***>(reinterpret_cast<uint8_t*>(fsObj) + 8);
    if (!primaryVt || !subVt) return false;

    // Verify both vtables are readable and all 16 slots point to executable code inside filesystem_stdio.dll
    auto isReadableRange = [](const void* ptr, size_t bytes) -> bool {
        MEMORY_BASIC_INFORMATION mbi{};
        if (::VirtualQuery(ptr, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT) return false;
        const uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return (start + bytes <= end);
    };
    if (!isReadableRange(primaryVt, 76 * sizeof(void*)) || !isReadableRange(subVt, 16 * sizeof(void*))) {
        WriteStatusLog("WARNING: IFileSystem vtable memory range unreadable; vtable hooks skipped safely.");
        return true;
    }

    // v16.6: Auto-detect primary vtable slot shift from IBaseFileSystem::Read (subVt[0]) & Open (subVt[2]) thunks.
    // Beta x86-64: OpenEx=73, ReadEx=74 (shift=0). Main branch gmod_win64.exe: OpenEx=69, ReadEx=70 (shift=-4).
    const size_t detectedReadEx = IsAddressInModule(subVt[0], hFs) ? ExtractVtableCallSlotFromThunk(subVt[0]) : 0;
    const size_t detectedOpenEx = IsAddressInModule(subVt[2], hFs) ? ExtractVtableCallSlotFromThunk(subVt[2]) : 0;
    if (detectedOpenEx == 0 || detectedReadEx != detectedOpenEx + 1 || detectedOpenEx < 50 || detectedOpenEx > 120) {
        WriteStatusLog("WARNING: Could not auto-detect IFileSystem OpenEx/ReadEx vtable slots (open=" +
                       std::to_string(detectedOpenEx) + ", read=" + std::to_string(detectedReadEx) +
                       "); vtable hooks skipped safely (Win32 IAT optimizations remain active).");
        return true;
    }
    const int primaryShift = static_cast<int>(detectedOpenEx) - 73;
    if (primaryShift < -16 || primaryShift > 16) {
        WriteStatusLog("WARNING: Unexpected IFileSystem primary vtable shift (" + std::to_string(primaryShift) +
                       "); vtable hooks skipped safely.");
        return true;
    }
    g_vtableIdxSetBufferSize = static_cast<size_t>(24 + primaryShift);
    g_vtableIdxIsOk          = static_cast<size_t>(25 + primaryShift);
    g_vtableIdxEndOfFile     = static_cast<size_t>(26 + primaryShift);
    g_vtableIdxReadLine      = static_cast<size_t>(27 + primaryShift);
    g_vtableIdxFindFirstEx   = static_cast<size_t>(35 + primaryShift);
    g_vtableIdxOpenEx        = detectedOpenEx;
    g_vtableIdxReadEx        = detectedReadEx;

    // Cross-check FindFirst (FindFirstEx - 4) thunk jumps directly to FindFirstEx!
    const size_t findFirstSlot = g_vtableIdxFindFirstEx - 4;
    const size_t crossFindEx = IsAddressInModule(primaryVt[findFirstSlot], hFs)
                               ? ExtractVtableCallSlotFromThunk(primaryVt[findFirstSlot]) : 0;
    if (crossFindEx != g_vtableIdxFindFirstEx) {
        WriteStatusLog("WARNING: IFileSystem FindFirst->FindFirstEx cross-check failed (expected=" +
                       std::to_string(g_vtableIdxFindFirstEx) + ", got=" + std::to_string(crossFindEx) +
                       "); vtable hooks skipped safely.");
        return true;
    }
    WriteStatusLog("IFileSystem vtable auto-detected: shift=" + std::to_string(primaryShift) +
                   " (OpenEx=" + std::to_string(g_vtableIdxOpenEx) +
                   ", ReadEx=" + std::to_string(g_vtableIdxReadEx) +
                   ", FindFirstEx=" + std::to_string(g_vtableIdxFindFirstEx) + ").");

    if (!IsAddressInModule(primaryVt[g_vtableIdxSetBufferSize], hFs) ||
        !IsAddressInModule(primaryVt[g_vtableIdxIsOk], hFs) ||
        !IsAddressInModule(primaryVt[g_vtableIdxEndOfFile], hFs) ||
        !IsAddressInModule(primaryVt[g_vtableIdxReadLine], hFs) ||
        !IsAddressInModule(primaryVt[g_vtableIdxOpenEx], hFs) ||
        !IsAddressInModule(primaryVt[g_vtableIdxReadEx], hFs) ||
        !IsAddressInModule(primaryVt[g_vtableIdxFindFirstEx], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxWrite], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxClose], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxSeek], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxTell], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxSizePath], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxSizeHandle], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxFlush], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxExists], hFs) ||
        !IsAddressInModule(subVt[kSubVtableIdxFileTime], hFs)) {
        WriteStatusLog("WARNING: IFileSystem vtable layout did not match expected win64 offsets; vtable hooks skipped safely.");
        return true;
    }

    // Live non-destructive ABI self-test on FileExists(10) and GetFileTime(13) before patching any slot
    const bool distinctSlots =
        (primaryVt[g_vtableIdxOpenEx] != primaryVt[g_vtableIdxReadEx]) &&
        (subVt[kSubVtableIdxExists] != subVt[kSubVtableIdxFileTime]) &&
        (subVt[kSubVtableIdxClose] != subVt[kSubVtableIdxSeek]);

    if (!distinctSlots || !ProbeFileSystemAbiSeh(reinterpret_cast<uint8_t*>(fsObj) + 8,
                                                 subVt[kSubVtableIdxExists],
                                                 subVt[kSubVtableIdxFileTime])) {
        WriteStatusLog("WARNING: IFileSystem ABI self-test failed (possible GMod update); VFS vtable hooks skipped safely. Win32 IAT optimizations remain active.");
        return true;
    }

    auto hookSlot = [](void** vt, size_t idx, void* fn, auto& origOut) -> bool {
        void* orig = nullptr;
        if (PatchVtableSlot(vt, idx, fn, &orig)) {
            origOut = reinterpret_cast<std::remove_reference_t<decltype(origOut)>>(orig);
            return true;
        }
        return false;
    };

    // Hook handle-consuming methods BEFORE OpenEx so no virtual handle can ever reach an unhooked slot
    int okCount = 0;
    okCount += hookSlot(primaryVt, g_vtableIdxSetBufferSize, reinterpret_cast<void*>(&HookedSetBufferSize), g_origSetBufferSize) ? 1 : 0;
    okCount += hookSlot(primaryVt, g_vtableIdxIsOk,          reinterpret_cast<void*>(&HookedIsOk),          g_origIsOk) ? 1 : 0;
    okCount += hookSlot(primaryVt, g_vtableIdxEndOfFile,     reinterpret_cast<void*>(&HookedEndOfFile),     g_origEndOfFile) ? 1 : 0;
    okCount += hookSlot(primaryVt, g_vtableIdxReadLine,      reinterpret_cast<void*>(&HookedReadLine),      g_origReadLine) ? 1 : 0;
    okCount += hookSlot(primaryVt, g_vtableIdxFindFirstEx,   reinterpret_cast<void*>(&HookedFindFirstEx),   g_origFindFirstEx) ? 1 : 0;
    okCount += hookSlot(primaryVt, g_vtableIdxReadEx,        reinterpret_cast<void*>(&HookedReadEx),        g_origReadEx) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxWrite,      reinterpret_cast<void*>(&HookedWrite),         g_origWrite) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxClose,      reinterpret_cast<void*>(&HookedClose),         g_origClose) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxSeek,       reinterpret_cast<void*>(&HookedSeek),          g_origSeek) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxTell,       reinterpret_cast<void*>(&HookedTell),          g_origTell) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxSizePath,   reinterpret_cast<void*>(&HookedSizePath),      g_origSizePath) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxSizeHandle, reinterpret_cast<void*>(&HookedSizeHandle),    g_origSizeHandle) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxFlush,      reinterpret_cast<void*>(&HookedFlush),         g_origFlush) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxExists,     reinterpret_cast<void*>(&HookedFileExists),    g_origFileExists) ? 1 : 0;
    okCount += hookSlot(subVt,     kSubVtableIdxFileTime,   reinterpret_cast<void*>(&HookedGetFileTime),   g_origGetFileTime) ? 1 : 0;
    okCount += hookSlot(primaryVt, g_vtableIdxOpenEx,        reinterpret_cast<void*>(&HookedOpenEx),        g_origOpenEx) ? 1 : 0;

    g_fsPrimaryVtable = primaryVt;
    g_fsSubVtable = subVt;

    WriteStatusLog("IFileSystem v16 vtable hooks installed: " + std::to_string(okCount) + "/16 slots (RAM cache=" +
                   (g_cfg.optVfsRamCache ? "ON" : "OFF") + ", vtx_slurp=" + std::to_string(g_cfg.vfsVtxSlurpMaxKb) + "KB).");
    return true;
}

// ============================================================================
// 12. Background Sampler Thread & Watcher Thread
// ============================================================================
HANDLE g_watcherThread = nullptr;
HANDLE g_samplerThread = nullptr;

DWORD WINAPI SamplerThreadProc(LPVOID) {
    while (!g_shuttingDown.load(std::memory_order_relaxed)) {
        int interval = g_cfg.sampleIntervalMs;
        if (!g_cfg.profilerEnabled || interval <= 0 || !g_samplerActive.load(std::memory_order_acquire) || !g_mainThreadHandle) {
            Sleep(20);
            continue;
        }
        HANDLE h = g_mainThreadHandle;
        if (h) {
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            uintptr_t stackSnap[64] = {};
            SIZE_T stackRead = 0;
            DWORD prev = SuspendThread(h);
            BOOL ok = FALSE;
            if (prev != static_cast<DWORD>(-1)) {
                ok = GetThreadContext(h, &ctx);
                if (ok && ctx.Rsp != 0) {
                    ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(ctx.Rsp),
                                      stackSnap, sizeof(stackSnap), &stackRead);
                }
                ResumeThread(h);
            }
            if (ok) {
                uintptr_t foundCaller = 0;
                const size_t qwords = stackRead / sizeof(uintptr_t);
                if (qwords > 0) {
                    AcquireSRWLockShared(&g_modCacheLock);
                    auto findMod = [](uintptr_t addr) -> const ModuleRange* {
                        for (const auto& m : g_modRanges) {
                            if (addr >= m.base && addr < m.end) return &m;
                        }
                        return nullptr;
                    };
                    auto isOsMod = [](const std::string& name) -> bool {
                        return name == "ntdll.dll" || name == "kernelbase.dll" ||
                               name == "kernel32.dll" || name == "win32u.dll" ||
                               name == "user32.dll" || name == "ucrtbase.dll";
                    };
                    const ModuleRange* ripMod = findMod(static_cast<uintptr_t>(ctx.Rip));
                    if (ripMod && isOsMod(ripMod->name)) {
                        for (size_t i = 0; i < qwords; ++i) {
                            const uintptr_t cand = stackSnap[i];
                            if (cand < 0x10000ULL) continue;
                            const ModuleRange* cm = findMod(cand);
                            if (cm && !isOsMod(cm->name) && cm->name != "winmm.dll") {
                                foundCaller = cand;
                                break;
                            }
                        }
                    }
                    ReleaseSRWLockShared(&g_modCacheLock);
                }
                size_t idx = g_sampleCount.fetch_add(1, std::memory_order_relaxed);
                if (idx < kMaxSamples) {
                    g_sampleBuf[idx].tick = g_timer.NowTick();
                    g_sampleBuf[idx].rip = static_cast<uintptr_t>(ctx.Rip);
                    g_sampleBuf[idx].callerRip = foundCaller;
                }
            }
        }
        Sleep(static_cast<DWORD>(interval));
    }
    return 0;
}


// ============================================================================
// 6c. Optimizer Layer 6: Loading-Screen USER32!SetCursor Deduplicator
//     Eliminates ~178 ms of redundant win32u!NtUserSetCursor syscalls during load
// ============================================================================
using FnSetCursor = HCURSOR(WINAPI*)(HCURSOR);

struct SetCursorHookSlot {
    const wchar_t* modName;
    HMODULE modHandle;
    FnSetCursor origFn;
    FnSetCursor hookFn;
    bool attempted;
};

HCURSOR RunDedupSetCursor(FnSetCursor origFn, HCURSOR hCursor) {
    if (!origFn) return nullptr;
    if (g_cfg.enableOptimizer && g_cfg.optDedupSetCursor &&
        g_sessionActive.load(std::memory_order_relaxed) &&
        !g_shuttingDown.load(std::memory_order_relaxed)) {
        const uint32_t sessId = g_sessionCounter.load(std::memory_order_relaxed);
        thread_local uint32_t tl_lastSess = 0;
        thread_local HCURSOR tl_lastCursor = nullptr;
        thread_local bool tl_hasCursor = false;
        if (tl_hasCursor && tl_lastSess == sessId && hCursor == tl_lastCursor) {
            g_optStats.setCursorDeduped.fetch_add(1, std::memory_order_relaxed);
            return hCursor;
        }
        tl_lastSess = sessId;
        tl_lastCursor = hCursor;
        tl_hasCursor = true;
    }
    return origFn(hCursor);
}

FnSetCursor g_origSetCursorVguiMat = nullptr;
FnSetCursor g_origSetCursorVgui2 = nullptr;
FnSetCursor g_origSetCursorInput = nullptr;
FnSetCursor g_origSetCursorMenu = nullptr;
FnSetCursor g_origSetCursorSdl2 = nullptr;
FnSetCursor g_origSetCursorClient = nullptr;

HCURSOR WINAPI HookedSetCursorVguiMat(HCURSOR h) { return RunDedupSetCursor(g_origSetCursorVguiMat, h); }
HCURSOR WINAPI HookedSetCursorVgui2(HCURSOR h)   { return RunDedupSetCursor(g_origSetCursorVgui2, h); }
HCURSOR WINAPI HookedSetCursorInput(HCURSOR h)   { return RunDedupSetCursor(g_origSetCursorInput, h); }
HCURSOR WINAPI HookedSetCursorMenu(HCURSOR h)    { return RunDedupSetCursor(g_origSetCursorMenu, h); }
HCURSOR WINAPI HookedSetCursorSdl2(HCURSOR h)    { return RunDedupSetCursor(g_origSetCursorSdl2, h); }
HCURSOR WINAPI HookedSetCursorClient(HCURSOR h)  { return RunDedupSetCursor(g_origSetCursorClient, h); }

struct SetCursorTarget {
    const wchar_t* dllName;
    const char* label;
    FnSetCursor* origSlot;
    FnSetCursor hookFn;
    HMODULE hookedMod;
    bool checked;
};

SetCursorTarget g_setCursorTargets[] = {
    { L"vguimatsurface.dll", "vguimatsurface.dll", &g_origSetCursorVguiMat, &HookedSetCursorVguiMat, nullptr, false },
    { L"vgui2.dll",          "vgui2.dll",          &g_origSetCursorVgui2,   &HookedSetCursorVgui2,   nullptr, false },
    { L"inputsystem.dll",    "inputsystem.dll",    &g_origSetCursorInput,   &HookedSetCursorInput,   nullptr, false },
    { L"menusystem.dll",     "menusystem.dll",     &g_origSetCursorMenu,    &HookedSetCursorMenu,    nullptr, false },
    { L"SDL2.dll",           "SDL2.dll",           &g_origSetCursorSdl2,    &HookedSetCursorSdl2,    nullptr, false },
    { L"client.dll",         "client.dll",         &g_origSetCursorClient,  &HookedSetCursorClient,  nullptr, false },
};

void TryInstallSetCursorHooks() {
    if (!g_cfg.enableOptimizer || !g_cfg.optDedupSetCursor) return;
    for (auto& t : g_setCursorTargets) {
        if (t.checked) continue;
        HMODULE hMod = GetModuleHandleW(t.dllName);
        if (!hMod) continue;
        t.checked = true;
        bool ok = PatchModuleIat(hMod, "USER32.dll", "SetCursor",
                                 reinterpret_cast<void*>(t.hookFn),
                                 reinterpret_cast<void**>(t.origSlot));
        if (ok) {
            t.hookedMod = hMod;
            WriteStatusLog(std::string(t.label) + " IAT hook USER32!SetCursor=OK");
        }
    }
}

void RestoreSetCursorHooks() {
    for (auto& t : g_setCursorTargets) {
        if (t.hookedMod && t.origSlot && *t.origSlot) {
            if (GetModuleHandleW(t.dllName) == t.hookedMod) {
                PatchModuleIat(t.hookedMod, "USER32.dll", "SetCursor",
                               reinterpret_cast<void*>(*t.origSlot), nullptr);
            }
            t.hookedMod = nullptr;
        }
    }
}

void CheckMemoryPressureAndRelieve() {
    if (!g_cfg.enableOptimizer || !g_cfg.optRamGuard ||
        g_shuttingDown.load(std::memory_order_relaxed)) {
        return;
    }

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!::GlobalMemoryStatusEx(&ms)) return;

    const DWORD softPct    = static_cast<DWORD>(std::clamp<uint32_t>(g_cfg.ramGuardPct, 70U, 96U));
    const DWORD hardPct    = std::min<DWORD>(96UL, softPct + 5UL);
    const DWORD recoverPct = (softPct > 6UL) ? (softPct - 6UL) : 0UL;

    constexpr DWORDLONG kSoftAvailBytes    = 1200ULL * 1024ULL * 1024ULL;
    constexpr DWORDLONG kHardAvailBytes    = 700ULL  * 1024ULL * 1024ULL;
    constexpr DWORDLONG kRecoverAvailBytes = 1600ULL * 1024ULL * 1024ULL;

    const bool hardHit    = (ms.dwMemoryLoad >= hardPct) || (ms.ullAvailPhys < kHardAvailBytes);
    const bool softHit    = (ms.dwMemoryLoad >= softPct) || (ms.ullAvailPhys < kSoftAvailBytes);
    const bool canRecover = (ms.dwMemoryLoad <= recoverPct) && (ms.ullAvailPhys >= kRecoverAvailBytes);

    static ULONGLONG s_lastHardTrimTick = 0;
    const uint8_t prevLevel = g_ramGuardLevel.load(std::memory_order_relaxed);
    const ULONGLONG nowTick = ::GetTickCount64();

    if (hardHit) {
        g_ramGuardLevel.store(2, std::memory_order_release);
        g_ramGuardActive.store(true, std::memory_order_release);

        // Isolated leaf lock 1: synchronously release VirtualFree quarantine
        uint64_t freedQ = DrainVirtualFreeQuarantine(false);

        uint64_t freedVtf = 0;
        uint64_t freedVpk = 0;
        if (prevLevel < 2 || (nowTick - s_lastHardTrimTick) >= 10000ULL) {
            s_lastHardTrimTick = nowTick;
            // Isolated leaf lock 2: trim VTF RAM cache by 50%
            freedVtf = TrimVtfRamCacheHalf();
            // Isolated leaf lock 3: trim VPK block cache by 50%
            freedVpk = TrimVpkBlockCacheHalf();
            // Isolated leaf lock 4: release VFS RAM cache only if it actually holds bytes (no empty gen bumps!)
            if (g_vfsRamBytesUsed.load(std::memory_order_relaxed) > 0) {
                freedVtf += ClearVfsCacheAll();
            }
        }

        const uint64_t totalFreed = freedQ + freedVtf + freedVpk;
        if (totalFreed > 0) {
            g_optStats.ramGuardBytesFreed.fetch_add(totalFreed, std::memory_order_relaxed);
        }
        if (prevLevel < 2) {
            g_optStats.ramGuardTriggers.fetch_add(1, std::memory_order_relaxed);
            WriteStatusLog("Layer 13 RAM Guard LEVEL 2 (hard trim): load=" + std::to_string(ms.dwMemoryLoad) +
                           "%, avail=" + std::to_string(ms.ullAvailPhys / (1024ULL * 1024ULL)) +
                           " MB, freed=" + std::to_string(totalFreed / (1024ULL * 1024ULL)) +
                           " MB (quar=" + std::to_string(freedQ / (1024ULL * 1024ULL)) +
                           ", vtf=" + std::to_string(freedVtf / (1024ULL * 1024ULL)) +
                           ", vpk=" + std::to_string(freedVpk / (1024ULL * 1024ULL)) + ").");
        }
    } else if (softHit) {
        const uint8_t nextLevel = (prevLevel >= 2) ? 2 : 1;
        g_ramGuardLevel.store(nextLevel, std::memory_order_release);
        g_ramGuardActive.store(true, std::memory_order_release);

        uint64_t freedQ = DrainVirtualFreeQuarantine(false);
        if (freedQ > 0) {
            g_optStats.ramGuardBytesFreed.fetch_add(freedQ, std::memory_order_relaxed);
        }
        if (prevLevel == 0) {
            g_optStats.ramGuardTriggers.fetch_add(1, std::memory_order_relaxed);
            WriteStatusLog("Layer 13 RAM Guard LEVEL 1 (soft pause): load=" + std::to_string(ms.dwMemoryLoad) +
                           "%, avail=" + std::to_string(ms.ullAvailPhys / (1024ULL * 1024ULL)) +
                           " MB, quar_freed=" + std::to_string(freedQ / (1024ULL * 1024ULL)) + " MB.");
        }
    } else if (prevLevel > 0 && canRecover) {
        g_ramGuardLevel.store(0, std::memory_order_release);
        g_ramGuardActive.store(false, std::memory_order_release);
        WriteStatusLog("Layer 13 RAM Guard RECOVERED to LEVEL 0: load=" + std::to_string(ms.dwMemoryLoad) +
                       "%, avail=" + std::to_string(ms.ullAvailPhys / (1024ULL * 1024ULL)) + " MB.");
    }
}


// ============================================================================
// Built-in Crash Logger (crash_log.txt + crash_*.mdmp in gmod_load_logs/)
// ============================================================================
#pragma pack(push, 4)
struct MinidumpExceptionInfoLocal {
    DWORD              ThreadId;
    PEXCEPTION_POINTERS ExceptionPointers;
    BOOL               ClientPointers;
};
#pragma pack(pop)

using FnMiniDumpWriteDump = BOOL (WINAPI*)(
    HANDLE hProcess, DWORD ProcessId, HANDLE hFile, DWORD DumpType,
    MinidumpExceptionInfoLocal* ExceptionParam, void* UserStreamParam, void* CallbackParam);

LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;
PVOID g_vehHandle = nullptr;
std::atomic<bool> g_crashHandlerInstalled{false};
std::atomic<bool> g_inCrashHandler{false};

inline bool IsFatalHardwareException(DWORD code) noexcept {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_STACK_OVERFLOW:
        case 0xC0000374UL: // STATUS_HEAP_CORRUPTION
        case 0xC0000409UL: // STATUS_STACK_BUFFER_OVERRUN
            return true;
        default:
            return false;
    }
}

void FormatModuleAddr(void* addr, char* outBuf, size_t outCap) {
    HMODULE hMod = nullptr;
    wchar_t modPath[MAX_PATH] = {};
    if (addr && ::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(addr), &hMod) && hMod) {
        if (::GetModuleFileNameW(hMod, modPath, MAX_PATH) > 0) {
            const wchar_t* base = wcsrchr(modPath, L'\\');
            base = base ? (base + 1) : modPath;
            char nameA[128] = {};
            size_t ni = 0;
            for (; base[ni] && ni + 1 < sizeof(nameA); ++ni) {
                wchar_t wc = base[ni];
                char c = (wc < 128) ? static_cast<char>(wc) : '?';
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                nameA[ni] = c;
            }
            nameA[ni] = '\0';
            uintptr_t rva = reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(hMod);
            std::snprintf(outBuf, outCap, "%s+0x%llX", nameA, static_cast<unsigned long long>(rva));
            return;
        }
    }
    std::snprintf(outBuf, outCap, "0x%llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(addr)));
}

void WriteCrashReportAndDump(EXCEPTION_POINTERS* ep, const char* sourceTag) {
    if (g_inCrashHandler.exchange(true, std::memory_order_acq_rel)) return;

    SYSTEMTIME st{};
    ::GetLocalTime(&st);
    char ts[32];
    std::snprintf(ts, sizeof(ts), "%04u%02u%02u_%02u%02u%02u",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0;
    void* faultAddr = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    char faultMod[192] = "(unknown)";
    if (faultAddr) FormatModuleAddr(faultAddr, faultMod, sizeof(faultMod));

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    ::GlobalMemoryStatusEx(&ms);

    void* frames[16] = {};
    USHORT nFrames = ::RtlCaptureStackBackTrace(0, 16, frames, nullptr);

    // Write human-readable crash_log.txt (with 16-frame stack backtrace)
    std::ofstream cf(g_logDir / L"crash_log.txt", std::ios::app);
    if (cf) {
        char buf[640];
        std::snprintf(buf, sizeof(buf),
            "[%04u-%02u-%02u %02u:%02u:%02u] CRASH[%s] PID=%u TID=%u (%s) | code=0x%08lX | addr=0x%llX (%s) | "
            "ver=v16.7 | ram=%s->%s (load=%lu%%, avail=%lluMB/%lluMB) | guard=L%u | storage=%s | profiler=%s\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            sourceTag ? sourceTag : "?",
            static_cast<unsigned>(g_processPid),
            static_cast<unsigned>(::GetCurrentThreadId()),
            g_processExeName.c_str(),
            static_cast<unsigned long>(code),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(faultAddr)),
            faultMod,
            g_cfg.ramProfile.c_str(), g_effectiveRamProfile.c_str(),
            static_cast<unsigned long>(ms.dwMemoryLoad),
            static_cast<unsigned long long>(ms.ullAvailPhys / (1024ULL * 1024ULL)),
            static_cast<unsigned long long>(g_sysTotalRamMb),
            static_cast<unsigned>(g_ramGuardLevel.load(std::memory_order_relaxed)),
            g_storageIsHdd ? "HDD" : "SSD",
            g_cfg.profilerEnabled ? "ON" : "OFF");
        cf << buf;
        if (nFrames > 0) {
            cf << "  stack:";
            for (USHORT i = 0; i < nFrames; ++i) {
                char fbuf[192];
                FormatModuleAddr(frames[i], fbuf, sizeof(fbuf));
                cf << " #" << i << "=" << fbuf;
            }
            cf << "\n";
        }
        cf.flush();
    }

    // Write minidump via dynamically loaded dbghelp.dll (zero link-time dependency)
    HMODULE hDbg = ::LoadLibraryW(L"dbghelp.dll");
    if (hDbg) {
        auto fnDump = reinterpret_cast<FnMiniDumpWriteDump>(::GetProcAddress(hDbg, "MiniDumpWriteDump"));
        if (fnDump) {
            std::filesystem::path dmpPath = g_logDir / (std::string("crash_") + ts + ".mdmp");
            HANDLE hFile = ::CreateFileW(dmpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                MinidumpExceptionInfoLocal mei{};
                mei.ThreadId = ::GetCurrentThreadId();
                mei.ExceptionPointers = ep;
                mei.ClientPointers = FALSE;
                fnDump(::GetCurrentProcess(), ::GetCurrentProcessId(), hFile, 0x00001000u,
                       ep ? &mei : nullptr, nullptr, nullptr);
                ::CloseHandle(hFile);
            }
        }
        ::FreeLibrary(hDbg);
    }
}

LONG WINAPI OptimizerVectoredExceptionHandler(EXCEPTION_POINTERS* ep) {
    if (!tl_inSehProbe && ep && ep->ExceptionRecord &&
        IsFatalHardwareException(ep->ExceptionRecord->ExceptionCode)) {
        WriteCrashReportAndDump(ep, "VEH");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI OptimizerUnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord) {
        WriteCrashReportAndDump(ep, "UEH");
    }
    if (g_prevExceptionFilter) {
        return g_prevExceptionFilter(ep);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void OptimizerTerminateHandler() {
    WriteCrashReportAndDump(nullptr, "TERMINATE");
    std::abort();
}

void InstallCrashHandlerOnce() {
    if (!g_crashHandlerInstalled.exchange(true, std::memory_order_acq_rel)) {
        g_vehHandle = ::AddVectoredExceptionHandler(1, OptimizerVectoredExceptionHandler);
        g_prevExceptionFilter = ::SetUnhandledExceptionFilter(OptimizerUnhandledExceptionFilter);
        std::set_terminate(OptimizerTerminateHandler);
    }
}

DWORD WINAPI WatcherThreadProc(LPVOID) {
    const wchar_t* cmdLine = ::GetCommandLineW();
    if (cmdLine && wcsstr(cmdLine, L"--type=") != nullptr) {
        // Chromium renderer/GPU/utility subprocess: pure winmm pass-through, zero hooks or log noise.
        return 0;
    }

    InitPathsAndConfig();
    if (g_processExeName != "gmod.exe" && g_processExeName != "gmod_win64.exe" &&
        g_processExeName != "srcds.exe" && g_processExeName != "srcds_win64.exe") {
        WriteStatusLog("Non-game process detected; proxy pass-through only (no hooks).");
        return 0;
    }

    InstallCrashHandlerOnce();
    ReserveDirShardsOnce();

    g_storageIsHdd = DetectStorageSeekPenalty(g_dllDir);
    if (g_storageIsHdd) {
        if (g_cfg.optHddAutoTune && g_cfg.ramProfile != "manual") {
            g_cfg.prefetchThreads = 1;
            g_cfg.optStartupGmaWarm = false;
            WriteStatusLog("Storage detected: HDD (seek penalty=true) — throttled prefetch threads to 1, startup GMA warm-up disabled.");
        } else {
            WriteStatusLog("Storage detected: HDD (seek penalty=true) — keeping configured prefetch/GMA settings.");
        }
    } else {
        WriteStatusLog("Storage detected: SSD / NVMe (seek penalty=false).");
    }

    WriteStatusLog(std::string("winmm.dll optimizer+profiler v16.7 initialized (optimizer=") +
                   (g_cfg.enableOptimizer ? "ON" : "OFF") +
                   ", profiler=" + (g_cfg.profilerEnabled ? "ON" : "OFF") +
                   ", ram_profile=" + g_cfg.ramProfile + "->" + g_effectiveRamProfile +
                   ", sys_ram=" + std::to_string(g_sysTotalRamMb) + "MB" +
                   ", avail_ram=" + std::to_string(g_sysAvailRamMbAtStart) + "MB" +
                   ", storage=" + (g_storageIsHdd ? "HDD" : "SSD") +
                   ", ram_guard=" + (g_cfg.optRamGuard ? (std::to_string(g_cfg.ramGuardPct) + "%") : std::string("OFF")) +
                   ", caps=[vtf=" + std::to_string(g_cfg.vtfRamCacheMaxMb) +
                   "MB vpk=" + std::to_string(g_cfg.vpkBlockCacheMb) +
                   "MB vfs=" + std::to_string(g_cfg.vfsRamCacheMaxMb) +
                   "MB vfree=" + std::to_string(g_cfg.vfreeQuarantineMb) +
                   "MB bsp_slab=" + std::to_string(g_cfg.bspPakSlabMb) +
                   "MB prefetch=" + std::to_string(g_cfg.prefetchMaxMb) +
                   "MB gma_warm=" + (g_cfg.optStartupGmaWarm
                       ? (std::to_string(g_cfg.startupGmaMaxTotalMb) + "MB")
                       : std::string("OFF")) +
                   "]).");

    AcquireSRWLockExclusive(&g_modCacheLock);
    RefreshModuleCacheLocked();
    ReleaseSRWLockExclusive(&g_modCacheLock);

    if (g_cfg.profilerEnabled && g_cfg.sampleIntervalMs > 0) {
        g_samplerThread = CreateThread(nullptr, 0, SamplerThreadProc, nullptr, 0, nullptr);
    }

    bool tier0Checked = false;
    bool fsHooked = false;
    bool launcherIatHooked = false;
    bool engineIatHooked = false;
    bool resourcesIatHooked = false;
    bool datacacheIatHooked = false;
    bool matSysIatHooked = false;
    bool luaIatHooked = false;
    bool engineReRegistered = false;
    ULONGLONG lastRamCheckTick = 0;

    while (!g_shuttingDown.load(std::memory_order_relaxed)) {
        if (!tier0Checked) {
            HMODULE hTier0 = GetModuleHandleW(L"tier0.dll");
            if (hTier0) {
                tier0Checked = true;
                g_fnRegisterListener = reinterpret_cast<FnRegisterLoggingListener>(
                    GetProcAddress(hTier0, "LoggingSystem_RegisterLoggingListener"));
                g_fnUnregisterListener = reinterpret_cast<FnUnregisterLoggingListener>(
                    GetProcAddress(hTier0, "LoggingSystem_UnregisterLoggingListener"));
                if (g_fnRegisterListener) {
                    g_fnRegisterListener(&g_listenerObj);
                    g_listenerRegistered = true;
                    WriteStatusLog("tier0.dll LoggingSystem listener registered.");
                } else {
                    WriteStatusLog("tier0.dll LoggingSystem not exported (main branch); running without console listener.");
                }
                if (g_cfg.enableOptimizer && g_cfg.optVirtualFreeQuarantine) {
                    bool okVf = PatchModuleIat(hTier0, "KERNEL32.dll", "VirtualFree",
                                               reinterpret_cast<void*>(&HookedTier0VirtualFree),
                                               reinterpret_cast<void**>(&g_origTier0VirtualFree));
                    if (okVf) g_tier0ModForIat = hTier0;
                    WriteStatusLog(std::string("tier0.dll IAT hook VirtualFree=") + (okVf ? "OK" : "NOT_FOUND"));
                }
            }
        }

        if (g_listenerRegistered && !launcherIatHooked) {
            HMODULE hLauncher = GetModuleHandleW(L"launcher.dll");
            if (hLauncher) {
                bool okR = PatchModuleIat(hLauncher, "tier0.dll", "LoggingSystem_ResetCurrentLoggingState",
                                          reinterpret_cast<void*>(&HookedResetCurrentLoggingState),
                                          reinterpret_cast<void**>(&g_origResetLoggingState));
                launcherIatHooked = true;
                WriteStatusLog(std::string("launcher.dll IAT hook ResetCurrentLoggingState=") + (okR ? "OK" : "NOT_FOUND"));
            }
        }

        if (!engineIatHooked) {
            HMODULE hEngine = GetModuleHandleW(L"engine.dll");
            if (hEngine) {
                bool okR = false;
                bool okP = false;
                if (g_listenerRegistered) {
                    okR = PatchModuleIat(hEngine, "tier0.dll", "LoggingSystem_ResetCurrentLoggingState",
                                         reinterpret_cast<void*>(&HookedResetCurrentLoggingState),
                                         reinterpret_cast<void**>(&g_origResetLoggingState));
                    okP = PatchModuleIat(hEngine, "tier0.dll", "LoggingSystem_PushLoggingState",
                                         reinterpret_cast<void*>(&HookedPushLoggingState),
                                         reinterpret_cast<void**>(&g_origPushLoggingState));
                }
                int engFs = 0;
                if (g_cfg.enableOptimizer && g_cfg.optDirCache && g_cfg.optExtraDllDirCache) {
                    g_engineModForIat = hEngine;
                    engFs += PatchModuleIat(hEngine, "KERNEL32.dll", "CreateFileA", reinterpret_cast<void*>(&HookedEngineCreateFileA), reinterpret_cast<void**>(&g_origEngineCreateFileA)) ? 1 : 0;
                    engFs += PatchModuleIat(hEngine, "KERNEL32.dll", "CreateFileW", reinterpret_cast<void*>(&HookedEngineCreateFileW), reinterpret_cast<void**>(&g_origEngineCreateFileW)) ? 1 : 0;
                }
                engineIatHooked = true;
                WriteStatusLog(std::string("engine.dll IAT hooks: Reset=") + (okR ? "OK" : "N/A") +
                               ", Push=" + (okP ? "OK" : "N/A") +
                               ", DirCacheCreateFile=" + std::to_string(engFs));
                if (g_listenerRegistered && !engineReRegistered && g_fnRegisterListener &&
                    g_logMessagesReceived.load(std::memory_order_relaxed) == 0) {
                    g_fnRegisterListener(&g_listenerObj);
                    engineReRegistered = true;
                    WriteStatusLog("Re-registered LoggingSystem listener after engine.dll load.");
                }
            }
        }

        if (!fsHooked) {
            fsHooked = TryInstallFilesystemHook();
            if (fsHooked) {
                LaunchStartupGmaWarmup();
            }
        }

        if (!resourcesIatHooked && g_cfg.enableOptimizer && g_cfg.optDirCache && g_cfg.optExtraDllDirCache) {
            HMODULE hRes = GetModuleHandleW(L"resources.dll");
            if (hRes) {
                g_resourcesModForIat = hRes;
                bool okRes = PatchModuleIat(hRes, "KERNEL32.dll", "CreateFileW",
                                            reinterpret_cast<void*>(&HookedResourcesCreateFileW),
                                            reinterpret_cast<void**>(&g_origResourcesCreateFileW));
                resourcesIatHooked = true;
                WriteStatusLog(std::string("resources.dll IAT hook CreateFileW=") + (okRes ? "OK" : "NOT_FOUND"));
            }
        }

        if (!datacacheIatHooked && g_cfg.enableOptimizer && g_cfg.optDirCache && g_cfg.optExtraDllDirCache) {
            HMODULE hDc = GetModuleHandleW(L"datacache.dll");
            if (hDc) {
                g_datacacheModForIat = hDc;
                bool okDc = PatchModuleIat(hDc, "KERNEL32.dll", "CreateFileW",
                                           reinterpret_cast<void*>(&HookedDatacacheCreateFileW),
                                           reinterpret_cast<void**>(&g_origDatacacheCreateFileW));
                datacacheIatHooked = true;
                WriteStatusLog(std::string("datacache.dll IAT hook CreateFileW=") + (okDc ? "OK" : "NOT_FOUND"));
            }
        }

        if (!matSysIatHooked && g_cfg.enableOptimizer && g_cfg.optDirCache && g_cfg.optExtraDllDirCache) {
            HMODULE hMs = GetModuleHandleW(L"materialsystem.dll");
            if (hMs) {
                g_matSysModForIat = hMs;
                bool okMs = PatchModuleIat(hMs, "KERNEL32.dll", "CreateFileW",
                                           reinterpret_cast<void*>(&HookedMatSysCreateFileW),
                                           reinterpret_cast<void**>(&g_origMatSysCreateFileW));
                matSysIatHooked = true;
                WriteStatusLog(std::string("materialsystem.dll IAT hook CreateFileW=") + (okMs ? "OK" : "NOT_FOUND"));
            }
        }

        if (!luaIatHooked && g_cfg.enableOptimizer && g_cfg.optDirCache && g_cfg.optExtraDllDirCache) {
            HMODULE hLua = GetModuleHandleW(L"lua_shared.dll");
            if (hLua) {
                g_luaModForIat = hLua;
                bool okLua = PatchModuleIat(hLua, "KERNEL32.dll", "CreateFileW",
                                            reinterpret_cast<void*>(&HookedLuaCreateFileW),
                                            reinterpret_cast<void**>(&g_origLuaCreateFileW));
                bool okLuaVf = false;
                if (g_cfg.optVirtualFreeQuarantine) {
                    okLuaVf = PatchModuleIat(hLua, "KERNEL32.dll", "VirtualFree",
                                             reinterpret_cast<void*>(&HookedLuaVirtualFree),
                                             reinterpret_cast<void**>(&g_origLuaVirtualFree));
                }
                luaIatHooked = true;
                WriteStatusLog(std::string("lua_shared.dll IAT hook CreateFileW=") + (okLua ? "OK" : "NOT_FOUND") +
                               ", VirtualFree=" + (okLuaVf ? "OK" : "NOT_FOUND"));
            }
        }

        TryInstallSetCursorHooks();

        const char* flushReason = nullptr;
        AcquireSRWLockShared(&g_sessionLock);
        if (g_session.active) {
            const int64_t nowTk = g_timer.NowTick();
            if (g_session.spawnSeen) {
                double sinceSpawnMs = g_timer.DeltaUs(g_session.spawnTick, nowTk) / 1000.0;
                if (sinceSpawnMs >= static_cast<double>(g_cfg.postSpawnTailMs)) {
                    flushReason = "post_spawn_tail_complete";
                }
            } else if (g_session.lightmapsSeen && g_session.luaStartupSeen) {
                double sinceLmMs = g_timer.DeltaUs(g_session.lightmapsTick, nowTk) / 1000.0;
                if (sinceLmMs >= 6000.0) {
                    flushReason = "mp_lightmaps_tail_complete";
                }
            } else if (g_session.compactSeen && g_session.luaStartupSeen) {
                double sinceCompMs = g_timer.DeltaUs(g_session.compactTick, nowTk) / 1000.0;
                if (sinceCompMs >= 6000.0) {
                    flushReason = "mp_compact_tail_complete";
                }
            } else if (g_session.luaStartupSeen) {
                double sinceLuaMs = g_timer.DeltaUs(g_session.luaStartupTick, nowTk) / 1000.0;
                if (sinceLuaMs >= 25000.0) {
                    flushReason = "mp_lua_startup_timeout";
                }
            }
            if (!flushReason) {
                double sinceStartMs = g_timer.DeltaUs(g_session.startTick, nowTk) / 1000.0;
                if (!g_listenerRegistered && sinceStartMs >= 45000.0) {
                    flushReason = "no_listener_timeout";
                } else if (sinceStartMs >= 180000.0) {
                    flushReason = "session_hard_timeout";
                }
            }
        }
        ReleaseSRWLockShared(&g_sessionLock);

        if (flushReason) {
            FinalizeSessionAsyncOrSync(flushReason, false);
        }

        const ULONGLONG nowRamTick = ::GetTickCount64();
        if (nowRamTick - lastRamCheckTick >= 300ULL) {
            lastRamCheckTick = nowRamTick;
            CheckMemoryPressureAndRelieve();
        }

        Sleep(25);
    }
    return 0;
}

void CleanupOnDetach() {
    g_shuttingDown.store(true, std::memory_order_release);
    g_samplerActive.store(false, std::memory_order_release);
    CancelActivePrefetch();
    ApplyLoadPriorityBoost(false);

    RestoreSetCursorHooks();
    RestoreFsIatHooks();

    HMODULE hFsAlive = g_fsModuleHandle ? GetModuleHandleW(L"filesystem_stdio.dll") : nullptr;
    if (hFsAlive && hFsAlive == g_fsModuleHandle) {
        if (g_fsPrimaryVtable) {
            if (g_origOpenEx)        PatchVtableSlot(g_fsPrimaryVtable, g_vtableIdxOpenEx, reinterpret_cast<void*>(g_origOpenEx), nullptr);
            if (g_origReadEx)        PatchVtableSlot(g_fsPrimaryVtable, g_vtableIdxReadEx, reinterpret_cast<void*>(g_origReadEx), nullptr);
            if (g_origFindFirstEx)   PatchVtableSlot(g_fsPrimaryVtable, g_vtableIdxFindFirstEx, reinterpret_cast<void*>(g_origFindFirstEx), nullptr);
            if (g_origSetBufferSize) PatchVtableSlot(g_fsPrimaryVtable, g_vtableIdxSetBufferSize, reinterpret_cast<void*>(g_origSetBufferSize), nullptr);
            if (g_origIsOk)          PatchVtableSlot(g_fsPrimaryVtable, g_vtableIdxIsOk, reinterpret_cast<void*>(g_origIsOk), nullptr);
            if (g_origEndOfFile)     PatchVtableSlot(g_fsPrimaryVtable, g_vtableIdxEndOfFile, reinterpret_cast<void*>(g_origEndOfFile), nullptr);
            if (g_origReadLine)      PatchVtableSlot(g_fsPrimaryVtable, g_vtableIdxReadLine, reinterpret_cast<void*>(g_origReadLine), nullptr);
        }
        if (g_fsSubVtable) {
            if (g_origWrite)         PatchVtableSlot(g_fsSubVtable, kSubVtableIdxWrite, reinterpret_cast<void*>(g_origWrite), nullptr);
            if (g_origClose)         PatchVtableSlot(g_fsSubVtable, kSubVtableIdxClose, reinterpret_cast<void*>(g_origClose), nullptr);
            if (g_origSeek)          PatchVtableSlot(g_fsSubVtable, kSubVtableIdxSeek, reinterpret_cast<void*>(g_origSeek), nullptr);
            if (g_origTell)          PatchVtableSlot(g_fsSubVtable, kSubVtableIdxTell, reinterpret_cast<void*>(g_origTell), nullptr);
            if (g_origSizePath)      PatchVtableSlot(g_fsSubVtable, kSubVtableIdxSizePath, reinterpret_cast<void*>(g_origSizePath), nullptr);
            if (g_origSizeHandle)    PatchVtableSlot(g_fsSubVtable, kSubVtableIdxSizeHandle, reinterpret_cast<void*>(g_origSizeHandle), nullptr);
            if (g_origFlush)         PatchVtableSlot(g_fsSubVtable, kSubVtableIdxFlush, reinterpret_cast<void*>(g_origFlush), nullptr);
            if (g_origFileExists)    PatchVtableSlot(g_fsSubVtable, kSubVtableIdxExists, reinterpret_cast<void*>(g_origFileExists), nullptr);
            if (g_origGetFileTime)   PatchVtableSlot(g_fsSubVtable, kSubVtableIdxFileTime, reinterpret_cast<void*>(g_origGetFileTime), nullptr);
        }
    }
    if (g_listenerRegistered && g_fnUnregisterListener) {
        if (GetModuleHandleW(L"tier0.dll") != nullptr) {
            g_fnUnregisterListener(&g_listenerObj);
        }
        g_listenerRegistered = false;
    }

    FinalizeSessionAsyncOrSync("process_exit", true);
    ClearVpkBlockCache();

    if (g_samplerThread) {
        WaitForSingleObject(g_samplerThread, 500);
        CloseHandle(g_samplerThread);
        g_samplerThread = nullptr;
    }
    if (g_mainThreadHandle) {
        CloseHandle(g_mainThreadHandle);
        g_mainThreadHandle = nullptr;
    }
}

} // namespace

// ============================================================================
// 13. WinMM Proxy Exports (100% auto-generated from local System32\winmm.dll)
// ============================================================================
template <typename Fn>
inline Fn ResolveWinmmByOrdinal(WORD ordinal, std::atomic<void*>& cache) {
    void* p = cache.load(std::memory_order_acquire);
    if (!p) {
        HMODULE h = EnsureRealWinmm();
        if (h) {
            p = reinterpret_cast<void*>(GetProcAddress(h, MAKEINTRESOURCEA(ordinal)));
            cache.store(p, std::memory_order_release);
        }
    }
    return reinterpret_cast<Fn>(p);
}

#define WINMM_PROXY_STUB(name) \
    extern "C" uintptr_t WINAPI Proxy_##name( \
        uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, \
        uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8) { \
        static std::atomic<void*> s_fn{nullptr}; \
        using Fn = uintptr_t(WINAPI*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, \
                                      uintptr_t, uintptr_t, uintptr_t, uintptr_t); \
        auto fn = ResolveWinmm<Fn>(#name, s_fn); \
        return fn ? fn(a1, a2, a3, a4, a5, a6, a7, a8) : 0; \
    }

#define WINMM_PROXY_ORD_STUB(ord) \
    extern "C" uintptr_t WINAPI Proxy_Ord_##ord( \
        uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, \
        uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8) { \
        static std::atomic<void*> s_fn{nullptr}; \
        using Fn = uintptr_t(WINAPI*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, \
                                      uintptr_t, uintptr_t, uintptr_t, uintptr_t); \
        auto fn = ResolveWinmmByOrdinal<Fn>(static_cast<WORD>(ord), s_fn); \
        return fn ? fn(a1, a2, a3, a4, a5, a6, a7, a8) : 0; \
    }

#include "winmm_exports.inc"

// ============================================================================
// 14. DllMain
// ============================================================================
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_selfModule = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        g_timer.Init();
        g_watcherThread = CreateThread(nullptr, 0, WatcherThreadProc, nullptr, 0, nullptr);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        g_shuttingDown.store(true, std::memory_order_release);
        g_samplerActive.store(false, std::memory_order_release);
        CancelActivePrefetch();
        if (lpvReserved != nullptr) {
            // Process is terminating under Loader Lock: other DLLs (vguimatsurface, filesystem_stdio, tier0)
            // may already be unmapped, and WaitForSingleObject/SRW locks can deadlock or AV.
            // Let the OS reclaim process memory cleanly without touching other modules.
            return TRUE;
        }
        CleanupOnDetach();
        if (g_watcherThread) {
            WaitForSingleObject(g_watcherThread, 500);
            CloseHandle(g_watcherThread);
            g_watcherThread = nullptr;
        }
    }
    return TRUE;
}
