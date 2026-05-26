#pragma once

#include <windows.h>
#include <string_view>

namespace BannerUtils {
    inline constexpr int WIDTH = 80;
    inline constexpr std::wstring_view BORDER_DOUBLE = 
        L"================================================================================";
    inline constexpr std::wstring_view BORDER_SINGLE =
        L"+------------------------------------------------------------------------------+";
    
    namespace Colors {
        inline constexpr WORD BLUE_BRIGHT = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        inline constexpr WORD WHITE_BRIGHT = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        inline constexpr WORD YELLOW_BRIGHT = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        inline constexpr WORD GREEN_BRIGHT = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        inline constexpr WORD GRAY = FOREGROUND_INTENSITY;
    }
    
    void ShowBanner() noexcept;
    void ShowUsage() noexcept;
    
    void PrintCentered(std::wstring_view text, HANDLE hConsole, WORD color) noexcept;
    void PrintBoxLine(std::wstring_view text, HANDLE hConsole, WORD borderColor, WORD textColor) noexcept;
    void PrintSectionHeader(const wchar_t* title) noexcept;
    void PrintCommandLine(const wchar_t* command, const wchar_t* description) noexcept;
    void PrintFooter() noexcept;
}
