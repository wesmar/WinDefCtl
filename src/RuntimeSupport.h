#pragma once

#include <windows.h>
#include <cstddef>

extern "C" {
int __cdecl memcmp(const void* left, const void* right, size_t count);
void* __cdecl memcpy(void* dst, const void* src, size_t count);
void* __cdecl memmove(void* dst, const void* src, size_t count);
void* __cdecl memset(void* dst, int value, size_t count);
size_t __cdecl wcslen(const wchar_t* text);
int __cdecl _purecall();
}
