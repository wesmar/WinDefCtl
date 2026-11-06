#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

#ifndef DEBUG_LOGGING_ENABLED
#define DEBUG_LOGGING_ENABLED 0
#endif

namespace Console {

void Print(const wchar_t* text, size_t len = 0) noexcept;
void PrintLine(const wchar_t* text = nullptr) noexcept;
void PrintRepeated(wchar_t ch, int count) noexcept;
void PrintLineRepeated(wchar_t ch, int count) noexcept;
void PrintInt(int64_t val) noexcept;
void PrintUint(uint64_t val) noexcept;
void PrintHex(uint64_t val, int minDigits = 0) noexcept;

void SetColor(WORD color) noexcept;
void ResetColor() noexcept;
WORD GetOriginalColor() noexcept;

#if DEBUG_LOGGING_ENABLED
inline void DbgPrint(const wchar_t* text) noexcept { Print(text); }
inline void DbgPrintLine(const wchar_t* text = nullptr) noexcept { PrintLine(text); }
inline void DbgPrintRepeated(wchar_t ch, int count) noexcept { PrintRepeated(ch, count); }
inline void DbgPrintLineRepeated(wchar_t ch, int count) noexcept { PrintLineRepeated(ch, count); }
inline void DbgPrintInt(int64_t val) noexcept { PrintInt(val); }
inline void DbgPrintUint(uint64_t val) noexcept { PrintUint(val); }
inline void DbgPrintHex(uint64_t val, int minDigits = 0) noexcept { PrintHex(val, minDigits); }
#else
inline void DbgPrint(const wchar_t*) noexcept {}
inline void DbgPrintLine(const wchar_t* = nullptr) noexcept {}
inline void DbgPrintRepeated(wchar_t, int) noexcept {}
inline void DbgPrintLineRepeated(wchar_t, int) noexcept {}
inline void DbgPrintInt(int64_t) noexcept {}
inline void DbgPrintUint(uint64_t) noexcept {}
inline void DbgPrintHex(uint64_t, int = 0) noexcept {}
#endif

} // namespace Console
