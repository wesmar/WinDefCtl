#pragma once

// Extract kvckiller.sys from embedded RCDATA resource (icon+CAB payload)
// to %SystemRoot%\System32\drivers\kvckiller.sys
bool ExtractKillerDriver() noexcept;

// Returns path to extracted driver
const wchar_t* GetKillerDriverPath() noexcept;
