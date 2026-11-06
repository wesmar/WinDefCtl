#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace StrUtils {

inline size_t StrLen(const wchar_t* s) noexcept {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) ++len;
    return len;
}

inline size_t StrLen(const char* s) noexcept {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) ++len;
    return len;
}

inline bool StrEqual(const wchar_t* a, const wchar_t* b) noexcept {
    if (a == b) return true;
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline bool StrEqualNoCase(const wchar_t* a, const wchar_t* b) noexcept {
    if (a == b) return true;
    if (!a || !b) return false;
    while (*a && *b) {
        wchar_t ca = *a;
        wchar_t cb = *b;
        if (ca >= L'A' && ca <= L'Z') ca += (L'a' - L'A');
        if (cb >= L'A' && cb <= L'Z') cb += (L'a' - L'A');
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline bool StrEqualNoCase(const char* a, const char* b) noexcept {
    if (a == b) return true;
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += ('a' - 'A');
        if (cb >= 'A' && cb <= 'Z') cb += ('a' - 'A');
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline size_t StrCopy(wchar_t* dst, size_t dstCap, const wchar_t* src) noexcept {
    if (!dst || dstCap == 0) return 0;
    if (!src) { dst[0] = L'\0'; return 0; }
    size_t i = 0;
    while (i + 1 < dstCap && src[i] != L'\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = L'\0';
    return i;
}

inline size_t StrConcat(wchar_t* dst, size_t dstCap, const wchar_t* src) noexcept {
    if (!dst || dstCap == 0) return 0;
    size_t len = 0;
    while (len < dstCap && dst[len] != L'\0') ++len;
    if (len >= dstCap) return dstCap - 1;
    if (!src) return len;
    size_t i = 0;
    while (len + 1 < dstCap && src[i] != L'\0') {
        dst[len++] = src[i++];
    }
    dst[len] = L'\0';
    return len;
}

inline wchar_t* StrRChr(wchar_t* s, wchar_t ch) noexcept {
    if (!s) return nullptr;
    wchar_t* last = nullptr;
    while (*s) {
        if (*s == ch) last = s;
        ++s;
    }
    return last;
}

inline size_t UintToStr(uint64_t val, wchar_t* buf, size_t bufCap) noexcept {
    if (!buf || bufCap == 0) return 0;
    if (val == 0) {
        if (bufCap > 1) { buf[0] = L'0'; buf[1] = L'\0'; return 1; }
        buf[0] = L'\0'; return 0;
    }
    wchar_t temp[32];
    int pos = 0;
    while (val > 0) {
        temp[pos++] = static_cast<wchar_t>(L'0' + (val % 10));
        val /= 10;
    }
    size_t needed = static_cast<size_t>(pos);
    if (needed >= bufCap) needed = bufCap - 1;
    for (size_t i = 0; i < needed; ++i) {
        buf[i] = temp[pos - 1 - i];
    }
    buf[needed] = L'\0';
    return needed;
}

inline size_t IntToStr(int64_t val, wchar_t* buf, size_t bufCap) noexcept {
    if (!buf || bufCap == 0) return 0;
    if (val < 0) {
        buf[0] = L'-';
        size_t n = UintToStr(static_cast<uint64_t>(-val), buf + 1, bufCap - 1);
        return (n > 0) ? (n + 1) : 0;
    }
    return UintToStr(static_cast<uint64_t>(val), buf, bufCap);
}

inline size_t HexToStr(uint64_t val, wchar_t* buf, size_t bufCap, int minDigits = 0) noexcept {
    if (!buf || bufCap == 0) return 0;
    static const wchar_t hexDigits[] = L"0123456789ABCDEF";
    wchar_t temp[32];
    int pos = 0;
    if (val == 0) {
        temp[pos++] = L'0';
    } else {
        while (val > 0) {
            temp[pos++] = hexDigits[val & 0xF];
            val >>= 4;
        }
    }
    while (pos < minDigits && pos < 30) {
        temp[pos++] = L'0';
    }
    size_t needed = static_cast<size_t>(pos);
    if (needed >= bufCap) needed = bufCap - 1;
    for (size_t i = 0; i < needed; ++i) {
        buf[i] = temp[pos - 1 - i];
    }
    buf[needed] = L'\0';
    return needed;
}

} // namespace StrUtils
