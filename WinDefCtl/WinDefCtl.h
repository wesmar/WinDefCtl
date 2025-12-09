#pragma once

#include <windows.h>
#include <UIAutomation.h>
#include <string>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#define DEBUG_LOGGING_ENABLED 1

// Automates Windows Security interface for Real-Time Protection
// Uses "Structural Density" strategy for robust detection
class WindowsDefenderAutomation {
private:
    IUIAutomation* pAutomation = nullptr;
    IUIAutomationElement* pRootElement = nullptr;
    HWND hwndSecurity = NULL;

    bool waitForUILoaded(int maxRetries = 20);
    IUIAutomationElement* findFirstToggleSwitch();
    IUIAutomationElement* findLastToggleSwitch();

    // Helper: Counts ALL descendant elements in the window (Text, Buttons, Groups, etc.)
    int countTotalElements();

    // Helper: Waits until the element count changes relative to baseline
    // expectIncrease = true (Waiting for warning content to APPEAR)
    // expectIncrease = false (Waiting for warning content to VANISH)
    bool waitForStructureChange(int baselineCount, bool expectIncrease, int timeoutSeconds = 10);

    // Cold boot detection and pre-warming
    bool isColdBoot();       // Check if this is first run after login
    bool preWarmDefender();  // Open and close Defender to "warm up" the system

public:
    WindowsDefenderAutomation();
    ~WindowsDefenderAutomation();

    bool openDefenderSettings();

    // Real-Time Protection
    bool toggleRealTimeProtection();
    bool enableRealTimeProtection();
    bool disableRealTimeProtection();
    bool getRealTimeProtectionStatus();

    // Tamper Protection
    bool toggleTamperProtection();
    bool enableTamperProtection();
    bool disableTamperProtection();
    bool getTamperProtectionStatus();

    void closeSecurityWindow();
};
