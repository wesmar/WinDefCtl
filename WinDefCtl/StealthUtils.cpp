#include "StealthUtils.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

#define DEBUG_LOGGING_ENABLED 1

#if DEBUG_LOGGING_ENABLED
    #define LOG(msg) std::wcout << msg
#else
    #define LOG(msg) ((void)0)
#endif

// ============================================================================
// EnumWindows Callback: Finds and Cloaks the window immediately
// ============================================================================

struct FindWindowData {
    HWND hWndFound;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;
    wchar_t className[256] = { 0 };

    if (GetClassNameW(hwnd, className, 256)) {
        if (wcscmp(className, L"ApplicationFrameWindow") == 0) {
            
            if (IsWindowVisible(hwnd)) {

                // --- GHOST MODE START ---

                // 1. Opacity Hack: Set alpha to 0 (Invisible)
                LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                if (!(exStyle & WS_EX_LAYERED)) {
                    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                }
                SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);

                // 2. DWM Cloak: Hides from window manager/taskbar
                int cloakValue = 1;
                DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloakValue, sizeof(cloakValue));

                // 3. Logical Teleport: Hijack the restore position
                WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
                if (GetWindowPlacement(hwnd, &wp)) {
                    wp.flags = WPF_ASYNCWINDOWPLACEMENT;
                    wp.showCmd = SW_SHOWNOACTIVATE;
                    wp.rcNormalPosition.left = -4000;
                    wp.rcNormalPosition.top = -4000;
                    wp.rcNormalPosition.right = -3200;
                    wp.rcNormalPosition.bottom = -3400;
                    SetWindowPlacement(hwnd, &wp);
                }

                // 4. Physical Teleport: Move off-screen immediately
                SetWindowPos(hwnd, NULL, -4000, -4000, 0, 0, 
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);

                // 5. Ensure window is active for automation but hidden
                ShowWindow(hwnd, SW_SHOWNOACTIVATE);

                // --- GHOST MODE END ---

                data->hWndFound = hwnd;
                return FALSE; // Stop enumeration
            }
        }
    }
    return TRUE;
}

// ============================================================================
// EnumWindows Callback: Find only (no cloaking) for pre-warm
// ============================================================================

BOOL CALLBACK EnumWindowsProcFindOnly(HWND hwnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;
    wchar_t className[256] = { 0 };

    if (GetClassNameW(hwnd, className, 256)) {
        if (wcscmp(className, L"ApplicationFrameWindow") == 0) {
            if (IsWindowVisible(hwnd)) {
                data->hWndFound = hwnd;
                return FALSE; // Stop enumeration
            }
        }
    }
    return TRUE;
}

// ============================================================================
// Registry Helpers
// ============================================================================

bool StealthUtils::ReadRegistryDword(const wchar_t* valueName, DWORD& outValue, bool& existed) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, UAC_REGISTRY_PATH, 0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        existed = false;
        return false;
    }

    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    result = RegQueryValueExW(hKey, valueName, nullptr, &type, (LPBYTE)&outValue, &size);
    RegCloseKey(hKey);
    
    existed = (result == ERROR_SUCCESS && type == REG_DWORD);
    return existed;
}

bool StealthUtils::WriteRegistryDword(const wchar_t* valueName, DWORD value) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, UAC_REGISTRY_PATH, 0, KEY_WRITE, &hKey);
    
    if (result != ERROR_SUCCESS) return false;

    result = RegSetValueExW(hKey, valueName, 0, REG_DWORD, (LPBYTE)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

bool StealthUtils::DeleteRegistryValue(const wchar_t* valueName) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, UAC_REGISTRY_PATH, 0, KEY_WRITE, &hKey);
    
    if (result != ERROR_SUCCESS) return false;

    result = RegDeleteValueW(hKey, valueName);
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

// ============================================================================
// UAC Logic
// ============================================================================

DWORD StealthUtils::EncodeUACStatus(DWORD cpba, bool cpbaExisted, DWORD posd, bool posdExisted) {
    DWORD encoded = 0;
    encoded |= (cpbaExisted ? (cpba & 0xFF) : KEY_NOT_EXISTED);
    encoded |= ((posdExisted ? (posd & 0xFF) : KEY_NOT_EXISTED) << 8);
    return encoded;
}

void StealthUtils::DecodeUACStatus(DWORD encoded, DWORD& cpba, bool& cpbaExisted, DWORD& posd, bool& posdExisted) {
    BYTE cpbaByte = encoded & 0xFF;
    BYTE posdByte = (encoded >> 8) & 0xFF;
    
    cpbaExisted = (cpbaByte != KEY_NOT_EXISTED);
    cpba = cpbaExisted ? cpbaByte : 0;
    
    posdExisted = (posdByte != KEY_NOT_EXISTED);
    posd = posdExisted ? posdByte : 0;
}

bool StealthUtils::BackupAndDisableUAC() {
    LOG(L"  [*] Backing up and disabling UAC prompts...\n");
    
    DWORD cpba = 0, posd = 0;
    bool cpbaExisted = false, posdExisted = false;
    
    ReadRegistryDword(L"ConsentPromptBehaviorAdmin", cpba, cpbaExisted);
    ReadRegistryDword(L"PromptOnSecureDesktop", posd, posdExisted);
    
    DWORD encoded = EncodeUACStatus(cpba, cpbaExisted, posd, posdExisted);
    if (!WriteRegistryDword(UAC_BACKUP_KEY, encoded)) return false;
    
    bool success = true;
    success &= WriteRegistryDword(L"ConsentPromptBehaviorAdmin", 0);
    success &= WriteRegistryDword(L"PromptOnSecureDesktop", 0);
    
    return success;
}

bool StealthUtils::RestoreUAC() {
    LOG(L"  [*] Restoring original UAC settings...\n");
    
    DWORD encoded = 0;
    bool backupExisted = false;
    
    if (!ReadRegistryDword(UAC_BACKUP_KEY, encoded, backupExisted) || !backupExisted) return false;
    
    DWORD cpba = 0, posd = 0;
    bool cpbaExisted = false, posdExisted = false;
    DecodeUACStatus(encoded, cpba, cpbaExisted, posd, posdExisted);
    
    if (cpbaExisted) WriteRegistryDword(L"ConsentPromptBehaviorAdmin", cpba);
    else DeleteRegistryValue(L"ConsentPromptBehaviorAdmin");
    
    if (posdExisted) WriteRegistryDword(L"PromptOnSecureDesktop", posd);
    else DeleteRegistryValue(L"PromptOnSecureDesktop");
    
    DeleteRegistryValue(UAC_BACKUP_KEY);
    return true;
}

bool StealthUtils::RecoverUACIfNeeded() {
    DWORD encoded = 0;
    bool backupExisted = false;
    if (ReadRegistryDword(UAC_BACKUP_KEY, encoded, backupExisted) && backupExisted) {
        std::wcout << L"  [RECOVERY] Found incomplete UAC backup, restoring...\n";
        return RestoreUAC();
    }
    return true;
}

// ============================================================================
// Window Finder Loop
// ============================================================================

HWND StealthUtils::FindAndCloakSecurityWindow(int maxRetries) {
    FindWindowData data = { 0 };

    for (int i = 0; i < maxRetries; ++i) {
        EnumWindows(EnumWindowsProc, (LPARAM)&data);

        if (data.hWndFound) {
            HWND hwnd = data.hWndFound;

            // --- DOUBLE TAP INSURANCE ---
            // Re-apply opacity in case EnumWindows missed the exact timing
            
            LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            if (!(exStyle & WS_EX_LAYERED)) {
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
            }

            // Re-apply teleport
            SetWindowPos(hwnd, NULL, -4000, -4000, 0, 0, 
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            
            return hwnd;
        }

        // DELAY FIX FOR SLOW HARDWARE
        // 250ms delay gives the OS time to create the window (Battery Saving Mode/Slow CPU)
        std::this_thread::sleep_for(250ms);
    }
    return NULL;
}

HWND StealthUtils::FindSecurityWindowOnly(int maxRetries) {
    FindWindowData data = { 0 };

    for (int i = 0; i < maxRetries; ++i) {
        EnumWindows(EnumWindowsProcFindOnly, (LPARAM)&data);

        if (data.hWndFound) {
            return data.hWndFound;
        }

        std::this_thread::sleep_for(250ms);
    }
    return NULL;
}

// ============================================================================
// Volatile Registry Marker (Session Persistence)
// ============================================================================

bool StealthUtils::CheckVolatileWarmMarker() {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\WinDefCtl", 0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return false; // Key doesn't exist = cold boot
    }

    // Check if WinDefCtl_Warmed value exists
    DWORD value;
    DWORD size = sizeof(DWORD);
    result = RegQueryValueExW(hKey, L"WinDefCtl_Warmed", nullptr, nullptr, (LPBYTE)&value, &size);
    RegCloseKey(hKey);
    
    return (result == ERROR_SUCCESS); // If exists = already warmed
}

bool StealthUtils::SetVolatileWarmMarker() {
    HKEY hKey;
    DWORD disposition;
    
    // Create volatile key (disappears on logout/reboot)
    LONG result = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\WinDefCtl",
        0,
        NULL,
        REG_OPTION_VOLATILE, // Key disappears on logout
        KEY_WRITE,
        NULL,
        &hKey,
        &disposition
    );
    
    if (result != ERROR_SUCCESS) {
        return false;
    }

    // Set marker value
    DWORD marker = 1;
    result = RegSetValueExW(hKey, L"WinDefCtl_Warmed", 0, REG_DWORD, (LPBYTE)&marker, sizeof(DWORD));
    RegCloseKey(hKey);
    
    return (result == ERROR_SUCCESS);
}
