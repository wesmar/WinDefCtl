#pragma once

#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

#define UAC_REGISTRY_PATH L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"
#define UAC_BACKUP_KEY L"UACStatus"
#define KEY_NOT_EXISTED 0xFF

namespace StealthUtils {
    // Window management
    HWND FindAndCloakSecurityWindow(int maxRetries = 20);
    HWND FindSecurityWindowOnly(int maxRetries = 20); // Find without cloaking
    
    // UAC management
    bool BackupAndDisableUAC();
    bool RestoreUAC();
    bool RecoverUACIfNeeded();
    
    // Registry helpers (internal use)
    bool ReadRegistryDword(const wchar_t* valueName, DWORD& outValue, bool& existed);
    bool WriteRegistryDword(const wchar_t* valueName, DWORD value);
    bool DeleteRegistryValue(const wchar_t* valueName);
    
    // UAC encoding/decoding
    DWORD EncodeUACStatus(DWORD cpba, bool cpbaExisted, DWORD posd, bool posdExisted);
    void DecodeUACStatus(DWORD encoded, DWORD& cpba, bool& cpbaExisted, DWORD& posd, bool& posdExisted);
    
    // Session warm marker (volatile registry)
    bool CheckVolatileWarmMarker();  // Check if Defender was already warmed this session
    bool SetVolatileWarmMarker();    // Mark Defender as warmed (volatile key)
}
