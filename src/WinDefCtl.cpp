#include "WinDefCtl.h"
#include "StealthUtils.h"
#include "BannerUtils.h"
#include "OverlayWindow.h"
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
// Constructor / Destructor
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

// ============================================================================
// Window Finder - Simple EnumWindows approach
// ============================================================================

struct FindWindowData {
    HWND hWndFound;
};

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;
    wchar_t className[256] = { 0 };

    if (GetClassNameW(hwnd, className, 256)) {
        if (wcscmp(className, L"ApplicationFrameWindow") == 0 && IsWindowVisible(hwnd)) {
            data->hWndFound = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

HWND WindowsDefenderAutomation::findSecurityWindow(int maxRetries) {
    FindWindowData data = { 0 };

    for (int i = 0; i < maxRetries; ++i) {
        EnumWindows(EnumWindowsCallback, (LPARAM)&data);
        if (data.hWndFound) {
            return data.hWndFound;
        }
        std::this_thread::sleep_for(100ms);
    }
    return NULL;
}

// ============================================================================
// Cold Boot Detection and Pre-Warming
// ============================================================================

bool WindowsDefenderAutomation::isColdBoot() {
    return !StealthUtils::CheckVolatileWarmMarker();
}

bool WindowsDefenderAutomation::preWarmDefender() {
    LOG(L"  [*] Cold boot detected - pre-warming Windows Defender...\n");
    
    // Console shield is already active from openDefenderSettings
    ShellExecuteW(nullptr, L"open", L"windowsdefender://threatsettings", 
                  nullptr, nullptr, SW_SHOWMINNOACTIVE);
    
    std::this_thread::sleep_for(800ms);
    
    HWND hwnd = findSecurityWindow(10);
    
    if (hwnd) {
        LOG(L"  [*] Pre-warm window found, waiting for full initialization...\n");
        std::this_thread::sleep_for(800ms);
        
        SetForegroundWindow(hwnd);
        std::this_thread::sleep_for(100ms);
        
        LOG(L"  [*] Closing pre-warm window...\n");
        SendMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
        
        // Wait for window to close
        bool closed = false;
        for (int i = 0; i < 30; i++) {
            if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
                closed = true;
                break;
            }
            std::this_thread::sleep_for(100ms);
        }
        
        if (!closed) {
            LOG(L"  [*] Retry close with PostMessage...\n");
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            std::this_thread::sleep_for(1000ms);
        }
        
        StealthUtils::SetVolatileWarmMarker();
        LOG(L"  [*] Pre-warm complete\n");
        return true;
    }
    
    LOG(L"  [WARN] Pre-warm window not found, continuing anyway...\n");
    return false;
}

// ============================================================================
// Open Defender Settings
// ============================================================================

bool WindowsDefenderAutomation::openDefenderSettings() {
    LOG(L"\n  [*] Opening Windows Defender...\n");
    
    // Show overlay window FIRST - before anything else can flash on screen
    OverlayWindow::Show();

    if (isColdBoot()) {
        // Pre-warming now happens safely behind the console shield
        preWarmDefender();
        std::this_thread::sleep_for(800ms);
        
        // Re-raise overlay in case OS changed Z-order during pre-warm window close
        OverlayWindow::Show();
    }
    
    ShellExecuteW(nullptr, L"open", L"windowsdefender://threatsettings", nullptr, nullptr, SW_SHOWMINNOACTIVE);
    
    hwndSecurity = findSecurityWindow(10);

    if (!hwndSecurity || !waitForUILoaded(50)) { 
        std::wcout << L"  [ERROR] Failed to load UI (Timeout on slow system).\n";
        OverlayWindow::Hide();
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
    pAutomation->CreateTrueCondition(&pCondition);
    
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
            std::this_thread::sleep_for(200ms);
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
    OverlayWindow::Hide();
    BannerUtils::ShowBanner();
    std::wcout << L"\n  [*] Operation finished.\n";
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
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    IUIAutomationElement* pButton = findFirstToggleSwitch();
    if (!pButton) { 
        StealthUtils::RestoreUAC(); 
        return false; 
    }

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = true;
    if (pToggle) {
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        
        if (state == ToggleState_Off) {
            int baseline = countTotalElements();
            pToggle->Toggle();
            pToggle->Release();
            pButton->Release();
            result = waitForStructureChange(baseline, false);
        } else {
            std::wcout << L"  [*] RTP already enabled\n";
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
    if (!StealthUtils::BackupAndDisableUAC()) return false;
    
    IUIAutomationElement* pButton = findLastToggleSwitch();
    if (!pButton) { 
        StealthUtils::RestoreUAC(); 
        return false; 
    }

    IUIAutomationTogglePattern* pToggle = nullptr;
    pButton->GetCurrentPatternAs(UIA_TogglePatternId, IID_IUIAutomationTogglePattern, (void**)&pToggle);

    bool result = true;
    if (pToggle) {
        ToggleState state;
        pToggle->get_CurrentToggleState(&state);
        
        if (state == ToggleState_Off) {
            int baseline = countTotalElements();
            pToggle->Toggle();
            pToggle->Release();
            pButton->Release();
            result = waitForStructureChange(baseline, false);
        } else {
            std::wcout << L"  [*] Tamper Protection already enabled\n";
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