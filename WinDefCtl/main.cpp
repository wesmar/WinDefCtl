#include "WinDefCtl.h"
#include "BannerUtils.h"
#include <iostream>

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        BannerUtils::ShowUsage();
        return 0;
    }

    std::wstring command = argv[1];

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

        if (action == L"on") {
            result = wda.enableRealTimeProtection();
        }
        else if (action == L"off") {
            result = wda.disableRealTimeProtection();
        }
        else if (action == L"status") {
            wda.getRealTimeProtectionStatus();
        }
        else {
            std::wcout << L"  [ERROR] Unknown action: " << action << L"\n";
            BannerUtils::ShowUsage();
        }

        wda.closeSecurityWindow();
        std::wcout << L"\n  [*] Operation completed.\n";
        return result ? 0 : 1;
    }
    else if (command == L"tp") {
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

        if (action == L"on") {
            result = wda.enableTamperProtection();
        }
        else if (action == L"off") {
            result = wda.disableTamperProtection();
        }
        else if (action == L"status") {
            wda.getTamperProtectionStatus();
        }
        else {
            std::wcout << L"  [ERROR] Unknown action: " << action << L"\n";
            BannerUtils::ShowUsage();
        }

        wda.closeSecurityWindow();
        std::wcout << L"\n  [*] Operation completed.\n";
        return result ? 0 : 1;
    }
    
    BannerUtils::ShowUsage();
    return 0;
}