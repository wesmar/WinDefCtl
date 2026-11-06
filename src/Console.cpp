#include "Console.h"
#include "StrUtils.h"

namespace Console {

namespace {

struct ConsoleTarget {
    HANDLE handle     = nullptr;
    bool   isConsole  = false;
    WORD   origColor  = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    bool   inited     = false;
};

static ConsoleTarget g_target;

static void EnsureInit() noexcept {
    if (g_target.inited) return;
    g_target.handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!g_target.handle || g_target.handle == INVALID_HANDLE_VALUE) {
        g_target.handle = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, OPEN_EXISTING, 0, nullptr);
    }
    DWORD mode = 0;
    g_target.isConsole = (g_target.handle && GetConsoleMode(g_target.handle, &mode));

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (g_target.isConsole && GetConsoleScreenBufferInfo(g_target.handle, &csbi)) {
        g_target.origColor = csbi.wAttributes;
    }
    g_target.inited = true;
}

} // namespace

void Print(const wchar_t* text, size_t len) noexcept {
    if (!text) return;
    EnsureInit();
    if (!g_target.handle || g_target.handle == INVALID_HANDLE_VALUE) return;

    if (len == 0) {
        len = StrUtils::StrLen(text);
    }
    if (len == 0) return;

    DWORD written = 0;
    if (g_target.isConsole) {
        WriteConsoleW(g_target.handle, text, static_cast<DWORD>(len), &written, nullptr);
    } else {
        constexpr size_t kChunk = 512;
        char utf8[kChunk * 3];
        const wchar_t* p = text;
        size_t remaining = len;

        while (remaining > 0) {
            size_t count = (remaining < kChunk) ? remaining : kChunk;
            if (count < remaining && IS_HIGH_SURROGATE(p[count - 1])) {
                --count;
            }
            int bytes = WideCharToMultiByte(CP_UTF8, 0, p, static_cast<int>(count),
                                            utf8, static_cast<int>(sizeof(utf8)), nullptr, nullptr);
            if (bytes > 0) {
                WriteFile(g_target.handle, utf8, static_cast<DWORD>(bytes), &written, nullptr);
            }
            p += count;
            remaining -= count;
        }
    }
}

void PrintLine(const wchar_t* text) noexcept {
    if (text) Print(text);
    Print(L"\r\n", 2);
}

void PrintRepeated(wchar_t ch, int count) noexcept {
    if (count <= 0) return;
    wchar_t buf[64];
    int fill = (count < 64) ? count : 64;
    for (int i = 0; i < fill; ++i) {
        buf[i] = ch;
    }
    while (count > 0) {
        int chunk = (count < 64) ? count : 64;
        Print(buf, static_cast<size_t>(chunk));
        count -= chunk;
    }
}

void PrintLineRepeated(wchar_t ch, int count) noexcept {
    PrintRepeated(ch, count);
    Print(L"\r\n", 2);
}

void PrintInt(int64_t val) noexcept {
    wchar_t buf[32];
    size_t len = StrUtils::IntToStr(val, buf, sizeof(buf) / sizeof(buf[0]));
    Print(buf, len);
}

void PrintUint(uint64_t val) noexcept {
    wchar_t buf[32];
    size_t len = StrUtils::UintToStr(val, buf, sizeof(buf) / sizeof(buf[0]));
    Print(buf, len);
}

void PrintHex(uint64_t val, int minDigits) noexcept {
    wchar_t buf[32];
    size_t len = StrUtils::HexToStr(val, buf, sizeof(buf) / sizeof(buf[0]), minDigits);
    Print(buf, len);
}

void SetColor(WORD color) noexcept {
    EnsureInit();
    if (g_target.isConsole && g_target.handle) {
        SetConsoleTextAttribute(g_target.handle, color);
    }
}

void ResetColor() noexcept {
    EnsureInit();
    if (g_target.isConsole && g_target.handle) {
        SetConsoleTextAttribute(g_target.handle, g_target.origColor);
    }
}

WORD GetOriginalColor() noexcept {
    EnsureInit();
    return g_target.origColor;
}

} // namespace Console
