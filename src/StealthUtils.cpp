#include "StealthUtils.h"
#include <iostream>

// ============================================================================
// Registry Helpers
// ============================================================================

bool StealthUtils::ReadRegistryDword(const wchar_t* valueName, DWORD& outValue, bool& existed) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, UAC_REGISTRY_PATH, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) { existed = false; return false; }

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

void StealthUtils::DecodeUACStatus(DWORD encoded, DWORD& cpba, bool& cpbaExisted,
                                   DWORD& posd, bool& posdExisted) {
    BYTE cpbaByte = encoded & 0xFF;
    BYTE posdByte = (encoded >> 8) & 0xFF;

    cpbaExisted = (cpbaByte != KEY_NOT_EXISTED);
    cpba = cpbaExisted ? cpbaByte : 0;

    posdExisted = (posdByte != KEY_NOT_EXISTED);
    posd = posdExisted ? posdByte : 0;
}

bool StealthUtils::BackupAndDisableUAC() {
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
    DWORD encoded = 0;
    bool backupExisted = false;
    if (!ReadRegistryDword(UAC_BACKUP_KEY, encoded, backupExisted) || !backupExisted)
        return false;

    DWORD cpba = 0, posd = 0;
    bool cpbaExisted = false, posdExisted = false;
    DecodeUACStatus(encoded, cpba, cpbaExisted, posd, posdExisted);

    if (cpbaExisted) WriteRegistryDword(L"ConsentPromptBehaviorAdmin", cpba);
    else             DeleteRegistryValue(L"ConsentPromptBehaviorAdmin");

    if (posdExisted) WriteRegistryDword(L"PromptOnSecureDesktop", posd);
    else             DeleteRegistryValue(L"PromptOnSecureDesktop");

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
// Volatile Registry Marker (Session Persistence)
// ============================================================================

bool StealthUtils::CheckVolatileWarmMarker() {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\WinDefCtl", 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) return false;

    DWORD value;
    DWORD size = sizeof(DWORD);
    result = RegQueryValueExW(hKey, L"WinDefCtl_Warmed", nullptr, nullptr, (LPBYTE)&value, &size);
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

bool StealthUtils::SetVolatileWarmMarker() {
    HKEY hKey;
    DWORD disposition;
    LONG result = RegCreateKeyExW(
        HKEY_CURRENT_USER, L"Software\\WinDefCtl",
        0, NULL, REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hKey, &disposition
    );
    if (result != ERROR_SUCCESS) return false;

    DWORD marker = 1;
    result = RegSetValueExW(hKey, L"WinDefCtl_Warmed", 0, REG_DWORD,
                            (LPBYTE)&marker, sizeof(DWORD));
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}
