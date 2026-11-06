#pragma once

#include <windows.h>

namespace BannerUtils {
    inline constexpr int WIDTH = 80;
    
    namespace Colors {
        inline constexpr WORD BLUE_BRIGHT = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        inline constexpr WORD WHITE_BRIGHT = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        inline constexpr WORD YELLOW_BRIGHT = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        inline constexpr WORD GREEN_BRIGHT = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        inline constexpr WORD GRAY = FOREGROUND_INTENSITY;
    }
    
    void PrintBorder(wchar_t fillChar, wchar_t edgeChar = 0, int width = WIDTH) noexcept;
    void PrintDoubleBorder(int width = WIDTH) noexcept;
    void PrintSingleBorder(int width = WIDTH) noexcept;
    
    void ShowBanner() noexcept;
    void ShowUsage() noexcept;
    
    void PrintCentered(const wchar_t* text, HANDLE hConsole = nullptr, WORD color = Colors::WHITE_BRIGHT) noexcept;
    void PrintBoxLine(const wchar_t* text, HANDLE hConsole = nullptr, WORD borderColor = Colors::BLUE_BRIGHT, WORD textColor = Colors::WHITE_BRIGHT) noexcept;
    void PrintSectionHeader(const wchar_t* title) noexcept;
    void PrintCommandLine(const wchar_t* command, const wchar_t* description) noexcept;
    void PrintFooter() noexcept;
}
