#include "RuntimeSupport.h"
#include <intrin.h>

#pragma function(memcmp, memcpy, memmove, memset, wcslen)

extern "C" {

int __cdecl memcmp(const void* left, const void* right, size_t count) {
    const auto* a = static_cast<const unsigned char*>(left);
    const auto* b = static_cast<const unsigned char*>(right);
    for (size_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) {
            return (a[i] < b[i]) ? -1 : 1;
        }
    }
    return 0;
}

void* __cdecl memcpy(void* dst, const void* src, size_t count) {
    __movsb(static_cast<unsigned char*>(dst), static_cast<const unsigned char*>(src), count);
    return dst;
}

void* __cdecl memmove(void* dst, const void* src, size_t count) {
    auto* d = static_cast<unsigned char*>(dst);
    const auto* s = static_cast<const unsigned char*>(src);
    if (d <= s || d >= s + count) {
        __movsb(d, s, count);
        return dst;
    }
    const size_t gap = static_cast<size_t>(d - s);
    size_t remaining = count;
    while (remaining > 0) {
        const size_t block = (remaining < gap) ? remaining : gap;
        remaining -= block;
        __movsb(d + remaining, s + remaining, block);
    }
    return dst;
}

void* __cdecl memset(void* dst, int value, size_t count) {
    __stosb(static_cast<unsigned char*>(dst), static_cast<unsigned char>(value), count);
    return dst;
}

size_t __cdecl wcslen(const wchar_t* text) {
    if (!text) return 0;
    const wchar_t* p = text;
    while (*p) {
        ++p;
    }
    return static_cast<size_t>(p - text);
}

int __cdecl _purecall() {
    ExitProcess(0xC0000025);
}

#if defined(_M_AMD64)
uintptr_t __security_cookie = 0x2B992DDFA23249D6;
void __fastcall __security_check_cookie(uintptr_t) {}
#endif

int _fltused = 0x9875;

} // extern "C"

void* operator new(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
}

void* operator new[](size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
}

void operator delete(void* p) noexcept {
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

void operator delete[](void* p) noexcept {
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

void operator delete(void* p, size_t) noexcept {
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

void operator delete[](void* p, size_t) noexcept {
    if (p) HeapFree(GetProcessHeap(), 0, p);
}
