#include "BannerUtils.h"
#include <iostream>
#include <iomanip>

void BannerUtils::PrintCentered(std::wstring_view text, HANDLE hConsole, WORD color) noexcept
{
    int textLen = static_cast<int>(text.length());
    int padding = (WIDTH - textLen) / 2;
    if (padding < 0) padding = 0;
    
    SetConsoleTextAttribute(hConsole, color);
    std::wcout << std::wstring(padding, L' ') << text << L"\n";
}

void BannerUtils::PrintBoxLine(std::wstring_view text, HANDLE hConsole, 
                                WORD borderColor, WORD textColor) noexcept
{
    int textLen = static_cast<int>(text.length());
    int innerWidth = WIDTH - 2;
    int padding = (innerWidth - textLen) / 2;
    if (padding < 0) padding = 0;
    
    SetConsoleTextAttribute(hConsole, borderColor);
    std::wcout << L"|";
    
    SetConsoleTextAttribute(hConsole, textColor);
    std::wcout << std::wstring(padding, L' ') << text
               << std::wstring(innerWidth - padding - textLen, L' ');
    
    SetConsoleTextAttribute(hConsole, borderColor);
    std::wcout << L"|\n";
}

void BannerUtils::PrintSectionHeader(const wchar_t* title) noexcept
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalColor = csbi.wAttributes;
    
    SetConsoleTextAttribute(hConsole, Colors::YELLOW_BRIGHT);
    std::wcout << L"=== " << title << L" ===\n";
    
    SetConsoleTextAttribute(hConsole, originalColor);
}

void BannerUtils::PrintCommandLine(const wchar_t* command, const wchar_t* description) noexcept
{
    std::wcout << L"  " << std::left << std::setw(38) 
               << command << L"- " << description << L"\n";
}

void BannerUtils::ShowBanner() noexcept
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalColor = csbi.wAttributes;

    SetConsoleTextAttribute(hConsole, Colors::BLUE_BRIGHT);
    std::wcout << L"\n" << BORDER_DOUBLE << L"\n";

    PrintCentered(L"Marek Wesolowski - WESMAR - 2025", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"WinDefCtl v1.0.0 https://kvc.pl", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"+48 607-440-283, marek@wesolowski.eu.org", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"WinDefCtl - Windows Defender Automation & Control Utility", hConsole, Colors::WHITE_BRIGHT);
    PrintCentered(L"Automated Real-Time Protection and Tamper Protection Management", hConsole, Colors::WHITE_BRIGHT);

    SetConsoleTextAttribute(hConsole, Colors::BLUE_BRIGHT);
    std::wcout << BORDER_DOUBLE << L"\n";

    SetConsoleTextAttribute(hConsole, originalColor);
}

void BannerUtils::PrintFooter() noexcept
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalColor = csbi.wAttributes;

    // Top border
    SetConsoleTextAttribute(hConsole, Colors::BLUE_BRIGHT);
    std::wcout << L"\n" << BORDER_SINGLE << L"\n";

    // Footer content lines
    PrintBoxLine(L"Support this project - a small donation is greatly appreciated", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);
    PrintBoxLine(L"and helps sustain private research builds.", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);
    PrintBoxLine(L"GitHub source code: https://github.com/wesmar/WinDefCtl/", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);
    PrintBoxLine(L"Professional services: marek@wesolowski.eu.org", 
                 hConsole, Colors::BLUE_BRIGHT, Colors::WHITE_BRIGHT);

    // Donation line with colored links
    SetConsoleTextAttribute(hConsole, Colors::BLUE_BRIGHT);
    std::wcout << L"|";
    
    std::wstring_view paypal = L"PayPal: ";
    std::wstring_view paypalLink = L"paypal.me/ext1";
    std::wstring_view middle = L"        ";
    std::wstring_view revolut = L"Revolut: ";
    std::wstring_view revolutLink = L"revolut.me/marekb92";
    
    int totalLen = static_cast<int>(paypal.length() + paypalLink.length() + 
                                   middle.length() + revolut.length() + revolutLink.length());
    int innerWidth = WIDTH - 2;
    int padding = (innerWidth - totalLen) / 2;
    if (padding < 0) padding = 0;
    
    SetConsoleTextAttribute(hConsole, Colors::WHITE_BRIGHT);
    std::wcout << std::wstring(padding, L' ') << paypal;
    SetConsoleTextAttribute(hConsole, Colors::GREEN_BRIGHT);
    std::wcout << paypalLink;
    SetConsoleTextAttribute(hConsole, Colors::WHITE_BRIGHT);
    std::wcout << middle << revolut;
    SetConsoleTextAttribute(hConsole, Colors::GREEN_BRIGHT);
    std::wcout << revolutLink;
    SetConsoleTextAttribute(hConsole, Colors::WHITE_BRIGHT);
    std::wcout << std::wstring(innerWidth - totalLen - padding, L' ');
    
    SetConsoleTextAttribute(hConsole, Colors::BLUE_BRIGHT);
    std::wcout << L"|\n";

    // Bottom border
    std::wcout << BORDER_SINGLE << L"\n\n";

    SetConsoleTextAttribute(hConsole, originalColor);
}

void BannerUtils::ShowUsage() noexcept
{
    ShowBanner();
    
    std::wcout << L"Usage: WinDefCtl <command> [arguments]\n\n";
    
    // Real-Time Protection commands
    PrintSectionHeader(L"Real-Time Protection Control");
    PrintCommandLine(L"rtp status", L"Check current RTP status");
    PrintCommandLine(L"rtp on", L"Enable Real-Time Protection");
    PrintCommandLine(L"rtp off", L"Disable Real-Time Protection");
    std::wcout << L"\n";
    
    // Tamper Protection commands
    PrintSectionHeader(L"Tamper Protection Control");
    PrintCommandLine(L"tp status", L"Check current Tamper Protection status");
    PrintCommandLine(L"tp on", L"Enable Tamper Protection");
    PrintCommandLine(L"tp off", L"Disable Tamper Protection");
    std::wcout << L"\n";
    
    // Usage examples
    PrintSectionHeader(L"Usage Examples");
    std::wcout << L"  WinDefCtl rtp status                  # Check if RTP is enabled\n";
    std::wcout << L"  WinDefCtl rtp off                     # Disable Real-Time Protection\n";
    std::wcout << L"  WinDefCtl rtp on                      # Re-enable Real-Time Protection\n";
    std::wcout << L"  WinDefCtl tp off                      # Disable Tamper Protection\n";
    std::wcout << L"  WinDefCtl tp on                       # Re-enable Tamper Protection\n";
    
    PrintFooter();
}