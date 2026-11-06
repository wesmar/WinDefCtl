#include "DriverExtract.h"
#include "GenIconSize.h"
#include "Resource.h"
#include "Console.h"
#include "StrUtils.h"
#include "RuntimeSupport.h"

#include <windows.h>
#include <fdi.h>

#pragma comment(lib, "Cabinet.lib")

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

// ============================================================================
// FDI IN-MEMORY STATE (multi-file CAB support)
// ============================================================================

struct ExtractedFile {
    char    name[64];
    BYTE*   data;
    SIZE_T  size;
};

struct CabStream {
    bool   inUse;
    SIZE_T offset;
};

static constexpr int kMaxCabStreams = 8;
static constexpr int kMaxFiles      = 2;

static struct {
    const BYTE* cabPtr;
    SIZE_T      cabEnd;

    BYTE*       outPtr;
    SIZE_T      outUsed;
    SIZE_T      outCap;

    CabStream   streams[kMaxCabStreams];

    ExtractedFile files[kMaxFiles];
    int           fileCount;
    int           currentFile;
} g_fdi;

// ============================================================================
// FDI CALLBACKS
// ============================================================================

static LPVOID DIAMONDAPI fdi_alloc(ULONG cb)
{
    return HeapAlloc(GetProcessHeap(), 0, cb);
}

static void DIAMONDAPI fdi_free(LPVOID pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}

static INT_PTR DIAMONDAPI fdi_open(char*, int, int)
{
    for (int i = 0; i < kMaxCabStreams; ++i) {
        if (!g_fdi.streams[i].inUse) {
            g_fdi.streams[i].inUse = true;
            g_fdi.streams[i].offset = 0;
            return static_cast<INT_PTR>(i + 1);
        }
    }
    return -1;
}

static UINT DIAMONDAPI fdi_read(INT_PTR hf, void* pv, UINT cb)
{
    if (hf < 1 || hf > kMaxCabStreams) return 0;
    auto& s = g_fdi.streams[hf - 1];
    if (!s.inUse) return 0;

    SIZE_T avail = (s.offset < g_fdi.cabEnd) ? (g_fdi.cabEnd - s.offset) : 0;
    if (cb > static_cast<UINT>(avail))
        cb = static_cast<UINT>(avail);

    if (cb > 0) {
        memcpy(pv, g_fdi.cabPtr + s.offset, cb);
        s.offset += cb;
    }
    return cb;
}

static UINT DIAMONDAPI fdi_write(INT_PTR hf, void* pv, UINT cb)
{
    int idx = static_cast<int>(hf - 0x100);
    if (idx < 0 || idx >= kMaxFiles) return 0;

    if (g_fdi.outUsed + cb > g_fdi.outCap) {
        SIZE_T newCap = (g_fdi.outCap + cb) * 2;
        BYTE* newData = reinterpret_cast<BYTE*>(
            HeapReAlloc(GetProcessHeap(), 0, g_fdi.files[idx].data, newCap));
        if (!newData) return 0;
        g_fdi.files[idx].data = newData;
        g_fdi.outPtr = newData;
        g_fdi.outCap = newCap;
    }

    memcpy(g_fdi.outPtr + g_fdi.outUsed, pv, cb);
    g_fdi.outUsed += cb;
    return cb;
}

static int DIAMONDAPI fdi_close(INT_PTR hf)
{
    if (hf >= 1 && hf <= kMaxCabStreams) {
        g_fdi.streams[hf - 1].inUse = false;
        g_fdi.streams[hf - 1].offset = 0;
    }
    return 0;
}

static long DIAMONDAPI fdi_seek(INT_PTR hf, long dist, int seektype)
{
    if (hf >= 0x100 && hf < 0x100 + kMaxFiles) {
        long newPos = 0;
        if (seektype == SEEK_SET)
            newPos = dist;
        else if (seektype == SEEK_CUR)
            newPos = static_cast<long>(g_fdi.outUsed) + dist;
        else
            newPos = static_cast<long>(g_fdi.outCap) + dist;
        if (newPos < 0) newPos = 0;
        g_fdi.outUsed = static_cast<SIZE_T>(newPos);
        return newPos;
    }

    if (hf < 1 || hf > kMaxCabStreams) return -1;
    auto& s = g_fdi.streams[hf - 1];
    if (!s.inUse) return -1;

    long newPos = 0;
    if (seektype == SEEK_SET)
        newPos = dist;
    else if (seektype == SEEK_CUR)
        newPos = static_cast<long>(s.offset) + dist;
    else
        newPos = static_cast<long>(g_fdi.cabEnd) + dist;

    if (newPos < 0) newPos = 0;
    if (static_cast<SIZE_T>(newPos) > g_fdi.cabEnd)
        newPos = static_cast<long>(g_fdi.cabEnd);

    s.offset = static_cast<SIZE_T>(newPos);
    return newPos;
}

static INT_PTR DIAMONDAPI fdi_notify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin)
{
    if (fdint == fdintCOPY_FILE) {
        if (g_fdi.fileCount >= kMaxFiles) return 0;

        int idx = g_fdi.fileCount;
        SIZE_T allocSize = (pfdin->cb > 0) ? static_cast<SIZE_T>(pfdin->cb) : (256 * 1024);
        g_fdi.files[idx].data = reinterpret_cast<BYTE*>(
            HeapAlloc(GetProcessHeap(), 0, allocSize));
        if (!g_fdi.files[idx].data) return 0;

        size_t nameLen = StrUtils::StrLen(pfdin->psz1);
        if (nameLen >= sizeof(g_fdi.files[idx].name)) {
            nameLen = sizeof(g_fdi.files[idx].name) - 1;
        }
        memcpy(g_fdi.files[idx].name, pfdin->psz1, nameLen);
        g_fdi.files[idx].name[nameLen] = '\0';
        g_fdi.files[idx].size = 0;

        g_fdi.outPtr  = g_fdi.files[idx].data;
        g_fdi.outUsed = 0;
        g_fdi.outCap  = allocSize;
        g_fdi.currentFile = idx;

        return static_cast<INT_PTR>(0x100 + idx);
    }

    if (fdint == fdintCLOSE_FILE_INFO) {
        int idx = g_fdi.currentFile;
        if (idx >= 0 && idx < kMaxFiles) {
            g_fdi.files[idx].size = g_fdi.outUsed;
            g_fdi.fileCount++;
        }
        return TRUE;
    }

    if (fdint == fdintENUMERATE)
        return TRUE;

    return 0;
}

// ============================================================================
// DECOMPRESS FROM RESOURCE
// ============================================================================

static bool DecompressFromResource() noexcept
{
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_DRIVER), (LPCWSTR)RT_RCDATA);
    if (!hRes) {
        Console::PrintLine(L"[!] IDR_DRIVER resource not found");
        return false;
    }

    DWORD totalSize = SizeofResource(nullptr, hRes);
    if (totalSize <= ICON_HEADER_SIZE + 4) {
        Console::PrintLine(L"[!] Resource too small (no CAB data)");
        return false;
    }

    HGLOBAL hGlob = LoadResource(nullptr, hRes);
    if (!hGlob) return false;

    const BYTE* raw = reinterpret_cast<const BYTE*>(LockResource(hGlob));
    if (!raw) return false;

    // Verify CAB magic signature ("MSCF")
    if (memcmp(raw + ICON_HEADER_SIZE, "MSCF", 4) != 0) {
        Console::PrintLine(L"[!] Invalid CAB header signature (expected 'MSCF')");
        return false;
    }

    memset(&g_fdi, 0, sizeof(g_fdi));
    g_fdi.cabPtr      = raw + ICON_HEADER_SIZE;
    g_fdi.cabEnd      = totalSize - ICON_HEADER_SIZE;
    g_fdi.currentFile = -1;

    ERF erf{};
    HFDI hfdi = FDICreate(fdi_alloc, fdi_free, fdi_open, fdi_read,
                          fdi_write, fdi_close, fdi_seek, cpuUNKNOWN, &erf);
    if (!hfdi) return false;

    BOOL ok = FDICopy(hfdi,
                      const_cast<char*>("memory.cab"),
                      const_cast<char*>(""),
                      0, fdi_notify, nullptr, nullptr);

    FDIDestroy(hfdi);

    if (!ok || g_fdi.fileCount == 0) {
        for (int i = 0; i < kMaxFiles; ++i) {
            if (g_fdi.files[i].data) {
                HeapFree(GetProcessHeap(), 0, g_fdi.files[i].data);
                g_fdi.files[i].data = nullptr;
            }
        }
        Console::Print(L"[!] FDI decompression failed (erf=");
        Console::PrintInt(erf.erfOper);
        Console::PrintLine(L")");
        return false;
    }

    return true;
}

// ============================================================================
// WRITE DRIVER FILE TO System32\drivers
// ============================================================================

static bool WriteDriverFile(const BYTE* data, SIZE_T size,
                            const wchar_t* fileName, wchar_t* outPath) noexcept
{
    wchar_t winDir[MAX_PATH] = {};
    GetSystemWindowsDirectoryW(winDir, MAX_PATH);

    StrUtils::StrCopy(outPath, MAX_PATH, winDir);
    StrUtils::StrConcat(outPath, MAX_PATH, L"\\System32\\drivers\\");
    StrUtils::StrConcat(outPath, MAX_PATH, fileName);

    DWORD attr = GetFileAttributesW(outPath);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        bool contentMatches = false;
        HANDLE hCheck = CreateFileW(outPath, GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hCheck != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER existingSize{};
            if (GetFileSizeEx(hCheck, &existingSize) &&
                existingSize.QuadPart == static_cast<LONGLONG>(size)) {
                
                BYTE* diskBuf = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, size));
                if (diskBuf) {
                    DWORD bytesRead = 0;
                    if (ReadFile(hCheck, diskBuf, static_cast<DWORD>(size), &bytesRead, nullptr) &&
                        bytesRead == static_cast<DWORD>(size)) {
                        if (memcmp(diskBuf, data, size) == 0) {
                            contentMatches = true;
                        }
                    }
                    HeapFree(GetProcessHeap(), 0, diskBuf);
                }
            }
            CloseHandle(hCheck);
        }

        if (contentMatches) {
            Console::DbgPrint(L"[*] ");
            Console::DbgPrint(fileName);
            Console::DbgPrint(L" already present and verified in drivers directory (");
            Console::DbgPrintUint(size);
            Console::DbgPrintLine(L" B)");
            return true;
        }

        Console::DbgPrint(L"[*] Existing ");
        Console::DbgPrint(fileName);
        Console::DbgPrintLine(L" is corrupted or mismatched - replacing with clean copy...");
        SetFileAttributesW(outPath, FILE_ATTRIBUTE_NORMAL);
        if (!DeleteFileW(outPath)) {
            DWORD delErr = GetLastError();
            if (delErr == ERROR_SHARING_VIOLATION || delErr == ERROR_ACCESS_DENIED) {
                Console::DbgPrint(L"[-] ");
                Console::DbgPrint(fileName);
                Console::DbgPrint(L" locked by system (err=");
                Console::DbgPrintUint(delErr);
                Console::DbgPrintLine(L")");
            }
        }
    }

    HANDLE hFile = CreateFileW(outPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        Console::Print(L"[!] Cannot write ");
        Console::Print(outPath);
        Console::Print(L" (err=");
        Console::PrintUint(err);
        Console::PrintLine(L")");
        return false;
    }

    DWORD written = 0;
    BOOL writeOk = WriteFile(hFile, data, static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(hFile);

    if (!writeOk || written != static_cast<DWORD>(size)) {
        Console::Print(L"[!] WriteFile failed for ");
        Console::PrintLine(fileName);
        DeleteFileW(outPath);
        return false;
    }

    Console::DbgPrint(L"[+] ");
    Console::DbgPrint(fileName);
    Console::DbgPrint(L" extracted and verified (");
    Console::DbgPrintUint(written);
    Console::DbgPrintLine(L" B)");
    return true;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool ExtractDrivers(ExtractedDrivers& out) noexcept
{
    memset(&out, 0, sizeof(out));

    if (!DecompressFromResource()) return false;

    for (int i = 0; i < g_fdi.fileCount; ++i) {
        const auto& f = g_fdi.files[i];

        if (StrUtils::StrEqualNoCase(f.name, "1")) {
            out.drv1Ok = WriteDriverFile(f.data, f.size,
                                         L"1", out.drv1Path);
        } else if (StrUtils::StrEqualNoCase(f.name, "2")) {
            out.drv2Ok = WriteDriverFile(f.data, f.size,
                                         L"2", out.drv2Path);
        }

        HeapFree(GetProcessHeap(), 0, f.data);
    }

    return out.drv1Ok || out.drv2Ok;
}

void CleanupExtractedDrivers(const ExtractedDrivers& drivers) noexcept
{
    Console::DbgPrintLine(L"[*] Cleaning up driver files from System32\\drivers...");

    auto removeOne = [](const wchar_t* path, const wchar_t* name) {
        if (path && path[0] != L'\0') {
            DWORD attr = GetFileAttributesW(path);
            if (attr != INVALID_FILE_ATTRIBUTES) {
                SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
                if (DeleteFileW(path)) {
                    Console::DbgPrint(L"[+] Cleaned up ");
                    Console::DbgPrintLine(name);
                } else {
                    DWORD err = GetLastError();
                    if (err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED) {
                        MoveFileExW(path, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                        Console::DbgPrint(L"[-] ");
                        Console::DbgPrint(name);
                        Console::DbgPrintLine(L" locked; scheduled for delete on reboot");
                    } else {
                        Console::DbgPrint(L"[-] Failed to delete ");
                        Console::DbgPrint(name);
                        Console::DbgPrint(L" (err=");
                        Console::DbgPrintUint(err);
                        Console::DbgPrintLine(L")");
                    }
                }
            }
        }
    };

    removeOne(drivers.drv1Path, L"1");
    removeOne(drivers.drv2Path, L"2");
}
