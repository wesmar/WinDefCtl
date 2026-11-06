#pragma once

#include <windows.h>

class DefenderManager {
public:
    enum class SecurityState {
        ACTIVE,
        IFEO_BLOCKED,
        INACTIVE,
        NOT_INSTALLED,
        UNKNOWN
    };

    struct DefenderStatus {
        SecurityState state;
        bool          ifeoBlocked;
        bool          winDefendRunning;
        bool          msmpengRunning;
        wchar_t       ifeoDebugger[MAX_PATH];
    };

    static bool DisableSecurityEngine() noexcept;
    static bool EnableSecurityEngine()  noexcept;
    static DefenderStatus QueryStatus() noexcept;
    static SecurityState  GetSecurityEngineStatus() noexcept;

private:
    struct HiveContext {
        wchar_t tempPath[MAX_PATH];
        wchar_t hiveFile[MAX_PATH];

        HiveContext() noexcept {
            tempPath[0] = L'\0';
            hiveFile[0] = L'\0';
        }
        ~HiveContext() { Cleanup(); }
        HiveContext(const HiveContext&)            = delete;
        HiveContext& operator=(const HiveContext&) = delete;

        void Cleanup() noexcept;
    };

    static bool EnableRequiredPrivileges() noexcept;
    static bool CreateIFEOSnapshot(HiveContext& ctx) noexcept;
    static bool ModifyMsMpEngIFEO(const HiveContext& ctx, bool addBlock) noexcept;
    static bool RestoreIFEOSnapshot(const HiveContext& ctx) noexcept;
    static bool StartWinDefend() noexcept;
    static bool IsMsMpEngRunning() noexcept;
    static bool IsWinDefendRunning() noexcept;

    static constexpr const wchar_t* IFEO_KEY =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options";
    static constexpr const wchar_t* MSMPENG_SUBKEY =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\MsMpEng.exe";
    static constexpr const wchar_t* TEMP_HIVE_NAME   = L"TempIFEO";
    static constexpr const wchar_t* DEBUGGER_VALUE   = L"Debugger";
    static constexpr const wchar_t* DEBUGGER_TARGET  = L"systray.exe";
    static constexpr const wchar_t* WINDEFEND_SVC    = L"WinDefend";
};
