#pragma once

#include <windows.h>

struct ExtractedDrivers {
    wchar_t drv1Path[MAX_PATH];
    wchar_t drv2Path[MAX_PATH];
    bool    drv1Ok;
    bool    drv2Ok;
};

bool ExtractDrivers(ExtractedDrivers& out) noexcept;
void CleanupExtractedDrivers(const ExtractedDrivers& drivers) noexcept;
