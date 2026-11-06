#include "BannerUtils.h"
#include "Console.h"
#include "StrUtils.h"

namespace {

inline void PrintSpaces(int count) noexcept {
    Console::PrintRepeated(L' ', count);
}

} // namespace

void BannerUtils::PrintCentered(const wchar_t* text, HANDLE hConsole, WORD color) noexcept
{
    (void)hConsole;
    int textLen = text ? static_cast<int>(StrUtils::StrLen(text)) : 0;
    int padding = (WIDTH - textLen) / 2;
    if (padding < 0) padding = 0;
    
    Console::SetColor(color);
    PrintSpaces(padding);
    Console::Print(text, textLen);
    Console::PrintLine();
}

void BannerUtils::PrintBoxLine(const wchar_t* text, HANDLE hConsole, 
                               WORD borderColor, WORD textColor) noexcept
{
    (void)hConsole;
    int textLen = text ? static_cast<int>(StrUtils::StrLen(text)) : 0;
    int innerWidth = WIDTH - 2;
    int padding = (innerWidth - textLen) / 2;
    if (padding < 0) padding = 0;
    
    Console::SetColor(borderColor);
    Console::Print(L"|");
    
    Console::SetColor(textColor);
    PrintSpaces(padding);
    Console::Print(text, textLen);
    PrintSpaces(innerWidth - padding - textLen);
    
    Console::SetColor(borderColor);
    Console::PrintLine(L"|");
}

void BannerUtils::PrintSectionHeader(const wchar_t* title) noexcept
{
    WORD originalColor = Console::GetOriginalColor();
    
    Console::SetColor(Colors::YELLOW_BRIGHT);
    Console::Print(L"=== ");
    Console::Print(title);
    Console::PrintLine(L" ===");
    
    Console::SetColor(originalColor);
}

void BannerUtils::PrintCommandLine(const wchar_t* command, const wchar_t* description) noexcept
{
    Console::Print(L"  ");
    Console::Print(command);
    int cmdLen = static_cast<int>(StrUtils::StrLen(command));
    int pad = 38 - cmdLen;
    if (pad < 1) pad = 1;
    PrintSpaces(pad);
    Console::Print(L"- ");
    Console::PrintLine(description);
}

void BannerUtils::PrintBorder(wchar_t fillChar, wchar_t edgeChar, int width) noexcept
{
    if (edgeChar != 0) {
        Console::Print(&edgeChar, 1);
        Console::PrintRepeated(fillChar, width - 2);
        Console::Print(&edgeChar, 1);
        Console::PrintLine();
    } else {
        Console::PrintLineRepeated(fillChar, width);
    }
}

void BannerUtils::PrintDoubleBorder(int width) noexcept
{
    PrintBorder(L'=', 0, width);
}

void BannerUtils::PrintSingleBorder(int width) noexcept
{
    PrintBorder(L'-', L'+', width);
}

void BannerUtils::ShowBanner() noexcept
{
    HANDLE hConsole = nullptr;
    WORD originalColor = Console::GetOriginalColor();

    Console::SetColor(Colors::BLUE_BRIGHT);
    Console::PrintLine();
    PrintDoubleBorder();

    PrintCentered(L"Marek Wesolowski - WESMAR - 2026", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"WinDefCtl v2.1.0 https://kvc.pl", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"+48 607-440-283, marek@wesolowski.eu.org", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"WinDefCtl - Windows Defender Automation & Control Utility", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"RTP/TP slider control + full engine kill via IFEO+kernel", hConsole, Colors::WHITE_BRIGHT);

    Console::SetColor(Colors::BLUE_BRIGHT);
    PrintDoubleBorder();

    Console::SetColor(originalColor);
}

void BannerUtils::PrintFooter() noexcept
{
    HANDLE hConsole = nullptr;
    WORD originalColor = Console::GetOriginalColor();

    Console::SetColor(Colors::BLUE_BRIGHT);
    Console::PrintLine();
    PrintSingleBorder();

    PrintBoxLine(L"Support this project - a small donation is greatly appreciated", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);
    PrintBoxLine(L"and helps sustain private research builds.", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);
    PrintBoxLine(L"GitHub source code: https://github.com/wesmar/WinDefCtl/", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);
    PrintBoxLine(L"Professional services: marek@wesolowski.eu.org", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);

    Console::SetColor(Colors::BLUE_BRIGHT);
    Console::Print(L"|");
    
    static const wchar_t paypal[] = L"PayPal: ";
    static const wchar_t paypalLink[] = L"paypal.me/ext1";
    static const wchar_t middle[] = L"        ";
    static const wchar_t revolut[] = L"Revolut: ";
    static const wchar_t revolutLink[] = L"revolut.me/marekb92";
    
    int totalLen = static_cast<int>(StrUtils::StrLen(paypal) + StrUtils::StrLen(paypalLink) + 
                                   StrUtils::StrLen(middle) + StrUtils::StrLen(revolut) + StrUtils::StrLen(revolutLink));
    int innerWidth = WIDTH - 2;
    int padding = (innerWidth - totalLen) / 2;
    if (padding < 0) padding = 0;
    
    Console::SetColor(Colors::WHITE_BRIGHT);
    PrintSpaces(padding);
    Console::Print(paypal);
    Console::SetColor(Colors::GREEN_BRIGHT);
    Console::Print(paypalLink);
    Console::SetColor(Colors::WHITE_BRIGHT);
    Console::Print(middle);
    Console::Print(revolut);
    Console::SetColor(Colors::GREEN_BRIGHT);
    Console::Print(revolutLink);
    Console::SetColor(Colors::WHITE_BRIGHT);
    PrintSpaces(innerWidth - totalLen - padding);
    
    Console::SetColor(Colors::BLUE_BRIGHT);
    Console::PrintLine(L"|");

    PrintSingleBorder();
    Console::PrintLine();

    Console::SetColor(originalColor);
}

void BannerUtils::ShowUsage() noexcept
{
    ShowBanner();
    
    Console::PrintLine(L"Usage: WinDefCtl <command> [arguments]\r\n");

    PrintSectionHeader(L"Engine Kill (IFEO + kernel driver)");
    PrintCommandLine(L"kill",    L"Terminate Defender engine (IFEO block + kernel kill)");
    PrintCommandLine(L"restore", L"Re-enable Defender (remove IFEO, start services)");
    Console::PrintLine();

    PrintSectionHeader(L"Real-Time Protection (UI automation + overlay)");
    PrintCommandLine(L"rtp status", L"Check current RTP status");
    PrintCommandLine(L"rtp on",     L"Enable Real-Time Protection");
    PrintCommandLine(L"rtp off",    L"Disable Real-Time Protection");
    Console::PrintLine();

    PrintSectionHeader(L"Tamper Protection (UI automation + overlay)");
    PrintCommandLine(L"tp status", L"Check current Tamper Protection status");
    PrintCommandLine(L"tp on",     L"Enable Tamper Protection");
    PrintCommandLine(L"tp off",    L"Disable Tamper Protection");
    Console::PrintLine();

    PrintSectionHeader(L"Usage Examples");
    Console::PrintLine(L"  WinDefCtl kill                        # Kill Defender engine completely");
    Console::PrintLine(L"  WinDefCtl restore                     # Restore Defender");
    Console::PrintLine(L"  WinDefCtl rtp off                     # Disable Real-Time Protection");
    Console::PrintLine(L"  WinDefCtl tp off                      # Disable Tamper Protection");
    Console::PrintLine(L"  WinDefCtl tp on                       # Re-enable Tamper Protection");
    
    PrintFooter();
}
