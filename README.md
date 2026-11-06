# WinDefCtl — Windows Defender Automation & Control Utility v2.1

**Zero-CRT Native Architecture + Dual Kernel BYOVD Engine + RTP/TP Slider Control**

[![Platform](https://img.shields.io/badge/platform-Windows%2011-blue)]()
[![Build](https://img.shields.io/badge/build-Release%20x64%20(Zero--CRT)-brightgreen)]()
[![Size](https://img.shields.io/badge/binary%20size-80.5%20KB-success)]()
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

---

## ⚡ What's New in v2.1 (06.11.2026)

v2.1 brings a complete architectural overhaul focused on **Zero-CRT native implementation**, **sub-100KB binary size record**, **dual-driver kernel redundancy**, and **full MASM string obfuscation**.

<details>
<summary>🚀 <b>Spoiler: Detailed v2.1.0 Architecture & Release Notes</b></summary>

### ⚡ Sub-100KB Size Record & Zero-CRT (~80 KB)
- **100% Native OS APIs (`/NODEFAULTLIB`)** — eliminated all C/C++ runtime library dependencies (`libcmt.lib`, `vcruntime*.dll`, `msvcrt.dll`). Executable dropped from ~380 KB down to **80.5 KB** (>78% reduction). Automated `dumpbin /dependents` in CI/CD pipeline enforces 0 CRT imports.
- **Custom Native CRT Runtime** — handwritten implementations for memory (`memset`, `memcpy`, `memmove`), string algorithms (`StrLen`, `StrEqual`, `StrCopy`, `StrConcat`, `IntToStr`, `HexToStr`), and float/hex formatters running directly on Win32 without standard library overhead.
- **Micro-Icon PNG CAB Compression** — transformed embedded multi-res icon to optimized 48x48 PNG format (1,088 B header), slashing icon resource overhead and total binary size.

### 🛡️ Dual Kernel BYOVD Architecture & Extension Independence
- **Two-tier Driver Redundancy** — engine kill upgraded from a single driver to dual signed kernel drivers (`1` and `2`), packed into a single LZX compressed CAB stream within the icon.
- **Extension-Independent Driver Loading** — drivers extract and load directly without `.sys` extensions (`%SystemRoot%\System32\drivers\1` and `\2`). Windows SCM `CreateServiceW` and kernel loader resolve native PE headers directly.
- **Automated Driver Teardown** — automated cleanup purges both driver files from disk and stops services immediately after the kill cycle completes.

### 🔒 Low-Level String & Telemetry Obfuscation (x64 MASM)
- **Zero Plaintext Driver Identifiers** — driver service names and device identifiers completely purged from executable `.rdata` string tables.
- **Algorithmic Assembly Decoders** — implemented x64 MASM routines (`wdc1.asm` and `wdc2.asm`) that dynamically reconstruct driver service strings in stack memory using bitwise XOR, ROL, and SUB transformations disguised as Mm/ETW kernel telemetry routines. Verified clean via `dumpbin` and binary string inspection.

### ⏱️ UI Automation & Zero Cursor Delay (0 ms)
- **Instant Busy Cursor Cancellation (`IDC_WAIT` / `IDC_APPSTARTING`)** — integrated `MsgWaitForMultipleObjects(0, nullptr, FALSE, 0, QS_ALLINPUT)` at entry point, overlay creation, and post-`ShellExecuteW`. Satisfies `WaitForInputIdle` instantly (0 ms), eliminating the spinning wheel wait cursor on Windows 11.
- **Overlay Window Cursor Enforcement** — registered `IDC_ARROW` cursor and handled `WM_SETCURSOR` in the Direct2D fullscreen overlay (`WDCOverlay`), preventing shell wait cursor leakage over the "PLEASE WAIT" screen.

### 📐 Procedural Banner & Clean Systems Engineering
- **Dynamic Mathematical Banner Lines** — replaced hardcoded banner string literals (`====`, `+----+`, space paddings) in `.rdata` with procedural loop-fill console routines (`PrintRepeated`, `PrintLineRepeated`, `PrintDoubleBorder`, `PrintSingleBorder`), saving `.rdata` memory.
- **Codebase Sanitization** — completely purged informal/script-kiddie jargon ("payload", etc.) in favor of professional systems engineering nomenclature across comments, identifiers, and logs.
- **Synchronized Build System** — automated alignment between `WinDefCtl.vcxproj` and `WinDefCtl.vcxproj.filters` with integrated MASM toolchain.

</details>

---

## 📥 Download

**[Latest Release → v2.1.0](https://github.com/wesmar/WinDefCtl/releases/tag/v2.1.0)**

Single standalone native executable (**80.5 KB**) — no installer, no dependencies, zero CRT dependencies (`/NODEFAULTLIB`).  
Run as **Administrator**.

PowerShell edition: [`data/WinDefCtl-v2.ps1`](data/WinDefCtl-v2.ps1) (~65 KB, no compiler needed).

<details>
<summary>🔍 Spoiler: ARM64 edition (v1.1.2) — byproduct of the research</summary>

This build was never officially announced — it is a direct byproduct of the
[Analyze Defender](https://kvc.pl/research/analyze-defender) research into hooking
the SecHealthUI XAML tree. While implementing and verifying the injection technique
on an ARM64 machine, a fully functional native binary emerged as a side effect.

`WinDefCtl_arm64.exe` is attached to the release for anyone who needs it.
It covers all five Threat Settings sliders individually — something the x64 v2.0
line does not expose through a single command:

```
WinDefCtl rtp on|off   # Real-Time Protection
WinDefCtl tp  on|off   # Tamper Protection
WinDefCtl cdp on|off   # Cloud-delivered protection
WinDefCtl ass on|off   # Automatic sample submission
WinDefCtl ddp on|off   # Dev Drive protection
```

The binary must be compiled natively for ARM64 — it is injected into `SecHealthUI.exe`
which on ARM64 hardware is a native ARM64 process and cannot host x86-64 code.
Source code is not published for this build.

</details>

---

## 🚀 Usage

```cmd
WinDefCtl kill              # Kill Defender engine (no restart required)
WinDefCtl restore           # Re-enable Defender engine

WinDefCtl rtp status        # Check Real-Time Protection state
WinDefCtl rtp off           # Disable Real-Time Protection
WinDefCtl rtp on            # Enable Real-Time Protection

WinDefCtl tp status         # Check Tamper Protection state
WinDefCtl tp off            # Disable Tamper Protection
WinDefCtl tp on             # Enable Tamper Protection
```

### Typical Workflow

```cmd
# Kill engine completely (blocks MsMpEng restart via IFEO)
WinDefCtl kill

# ... do your thing ...

# Restore everything
WinDefCtl restore

# Or: just disable RTP for a moment
WinDefCtl rtp off
WinDefCtl rtp on
```

---

## ⚙️ How It Works

### `kill` — Engine Kill (v2.0)

Two-stage, no restart required:

**Stage 1 — IFEO offline hive bypass:**

Direct registry write to `Image File Execution Options` is blocked by Tamper Protection.  
WinDefCtl bypasses this using an offline hive cycle:

1. `RegSaveKeyEx` — dump live `IFEO` key to `%TEMP%\Ifeo.hiv`
2. `RegLoadKey(HKLM, TempIFEO, Ifeo.hiv)` — mount as `HKLM\TempIFEO`
3. Write `Debugger=systray.exe` to `TempIFEO\MsMpEng.exe` (and two secondary targets)
4. `RegUnLoadKey(TempIFEO)` — unmount
5. `RegRestoreKey(IFEO, Ifeo.hiv, REG_FORCE_RESTORE)` — kernel-level force-replace

`REG_FORCE_RESTORE` operates below the Tamper Protection filter. Requires `SE_BACKUP_NAME` + `SE_RESTORE_NAME`.

**Stage 2 — Ring-0 process kill via Dual Kernel Engine:**

Both kernel drivers carry valid **digital signatures**.  
Loads without DSE bypass, without HVCI restart, without any unsigned-driver prerequisites.

- Drivers extracted from embedded LZX CAB (appended to the `.ico` resource at `ICON_HEADER_SIZE` offset)
- Written to `System32\drivers\1` and `System32\drivers\2` (extension-independent SCM loading)
- Driver service names and device identifiers reconstructed dynamically via x64 MASM assembly routines (`wdc1.asm`, `wdc2.asm`) — 0 plaintext string occurrences in `.rdata`
- Terminates `MsMpEng.exe` + `SecurityHealthSystray.exe`; `SecurityHealthService` stopped via SCM
- Immediate atomic cleanup after use (stop services, delete services, remove driver files from disk)

### `rtp` / `tp` — UI Automation

Opens `windowsdefender://threatsettings` with `SW_SHOWMINNOACTIVE` (minimized, not activated).  
Full-screen **Direct2D overlay** covers the desktop immediately — hiding the minimized window from view.  
Toggles the switch via `IUIAutomationTogglePattern`.

**Overlay technical details:**
- `WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE` fullscreen popup, fully opaque black
- Direct2D + DirectWrite rendering: Consolas Bold 80pt, pulsing green, animated dots, CRT scanlines
- **0 ms cursor delay:** Instant release of Windows `WaitForInputIdle` via `MsgWaitForMultipleObjects(..., QS_ALLINPUT)` to cancel the spinning wait cursor (`IDC_WAIT`/`IDC_APPSTARTING`)
- `WM_SETCURSOR` handler and `IDC_ARROW` registration prevent shell wait cursor leakage
- Background thread — non-blocking, no window activation
- Released on `Hide()` via `PostMessage(WM_CLOSE)`

### UAC Bypass

Before UI automation, UAC prompts are suppressed:
- Backup `ConsentPromptBehaviorAdmin` + `PromptOnSecureDesktop` to `HKLM\...\UACStatus`
- Set both to 0 (no prompts)
- Automatically restored after operation (or on next run if interrupted)

### Cold Boot Pre-Warming

First run after login: Windows Security components are not yet loaded in memory.  
WinDefCtl detects this via a volatile registry marker at `HKCU\Software\WinDefCtl\WinDefCtl_Warmed`  
and pre-loads the Security window before automation begins (~5-7 seconds, first run only).

---

## 📜 PowerShell Edition (Addon)

[`data/WinDefCtl-v2.ps1`](data/WinDefCtl-v2.ps1) — same command palette as the C++ binary,
in a single self-contained PowerShell script (~65 KB). No compiler, no Visual Studio.
Drop the file anywhere, run from elevated PowerShell.

```powershell
.\WinDefCtl-v2.ps1                  # show help (also: /?, -?, -h, --help, "help")
.\WinDefCtl-v2.ps1 status           # read Defender state (read-only)
.\WinDefCtl-v2.ps1 kill             # IFEO block + BYOVD kernel kill
.\WinDefCtl-v2.ps1 restore          # remove IFEO + start WinDefend
.\WinDefCtl-v2.ps1 rtp off          # Real-Time Protection toggle off
.\WinDefCtl-v2.ps1 tp  on           # Tamper Protection toggle on
```

### How it works

| Layer | Implementation |
|-------|---------------|
| Driver embedding | `kvckiller.sys` packed with `makecab.exe` (LZX) → base64-chunked → inline in `$DriverCabB64` |
| Driver deployment | `expand.exe` decompresses CAB directly to `%SystemRoot%\System32\drivers\kvckiller.sys` |
| SCM lifecycle | `sc.exe create wsftprm type= kernel start= demand` → `sc.exe start` → `sc.exe stop`/`delete` |
| Kernel kill | P/Invoke `CreateFileW(\\.\Warsaw_PM)` + `DeviceIoControl(0x22201C, <PID buffer>)` |
| IFEO bypass | `reg.exe save` → `reg.exe load HKLM\TempIFEO` → `reg.exe add Debugger=systray.exe` → `reg.exe unload` → `reg.exe restore /f` (`REG_FORCE_RESTORE`) |
| UI automation | `System.Windows.Automation` — `TogglePattern` on Real-Time / Tamper / Dev Drive protection toggles |
| UAC bypass | Both `ConsentPromptBehaviorAdmin` and `PromptOnSecureDesktop` packed into a single `UACStatus` DWORD, restored after the toggle |
| Overlay | Fullscreen multi-monitor `System.Windows.Forms.Form`, pulsing "PLEASE WAIT" label (sine-wave grey-to-white, 25 FPS) |
| Cold boot detect | Volatile marker `HKCU\Software\Temp\WinDefCtl_Warmed` |

### Zero trace after `kill`

After the kill flow finishes, all of the following are gone:

- `wsftprm` service (stopped + `DeleteService`)
- `%SystemRoot%\System32\drivers\kvckiller.sys` (deleted)
- `%TEMP%\kk.cab` and `%TEMP%\Ifeo.hiv*` (deleted)

Only IFEO entries remain (intentional — that **is** the active block; `restore` clears them).

### Build the script

```powershell
# Source driver: data\kvckiller.sys
.\generator\build-ps.ps1
# Output: out\WinDefCtl-v2.ps1
```

The generator runs `makecab.exe`, base64-encodes the CAB into 76-char lines and
substitutes into `generator\template.ps1`. No external NuGet packages, no .NET
build tools — pure inbox Windows.

### Limits vs. C++ binary

- Slower startup (~1.5 s PowerShell + .NET cold load vs. ~200 ms native exe).
- Requires `-ExecutionPolicy Bypass` or a signed copy.
- Defender RTP **may** flag the script on disk (CAB-encoded signed driver inside a PS1).
  The compiled C++ binary embeds the same driver as an ICO resource so it sits below most heuristics.
- No D2D overlay — uses WinForms.

---

## 🛠️ Build from Source

**Requirements:** Visual Studio 2022 (v145 toolset), Windows SDK 10.0+

```powershell
# Full chain: embed driver → compile → stamp timestamps → clean artifacts
.\build.ps1

# Skip driver embedding (use existing ICON\WinDefCtl.ico)
.\build.ps1 -SkipDriverPack

# Custom timestamp (for reproducible builds)
.\build.ps1 -Timestamp "2030-01-01 00:00:00"
```

**Build chain:**
1. `Build-DriverIcon` — `makecab` LZX compresses drivers `1` and `2`, appends CAB to `IcoBuilder\WinDefCtl.ico` → `ICON\WinDefCtl.ico`
2. Regenerates `src\GenIconSize.h` with exact icon header byte count (1088 B)
3. MSBuild `Release|x64`: `/NODEFAULTLIB` (Zero-CRT), `/O1` / `/Os` (FavorSize), `/GF`, `/Gy`, `/Gw`, MASM assembly compilation
4. Dumpbin import check — automated verification of 0 CRT imports
5. Timestamp stamp → `bin\WinDefCtl.exe`
6. Cleans `obj\`

**Output:** `bin\WinDefCtl.exe` (**80.5 KB**, 100% native Windows OS APIs)

---

## 📋 Technical Requirements

- **OS:** Windows 11 (tested on 24H2 / 26H1)
- **Privileges:** Administrator
- **Architecture:** x64
- **Dependencies:** none (Zero-CRT, 100% native Win32/NT APIs)
- **Size:** ~80 KB

---

## ⚠️ Notes

- `kill` does NOT unload `WinDefend` service — IFEO prevents `MsMpEng.exe` from starting on service start
- `restore` removes the IFEO block then starts `WinDefend` + `SecurityHealthService`
- UI Automation (`rtp`/`tp`) requires an active user session
- Cannot run from Safe Mode or Windows PE
- kvckiller.sys is digitally signed — no HVCI workaround needed

---

## 📞 Contact & Support

| | |
|-|-|
| **Email** | marek@wesolowski.eu.org |
| **Phone** | +48 607-440-283 |
| **Website** | https://kvc.pl |
| **GitHub** | https://github.com/wesmar/WinDefCtl |

**Donations:**
- PayPal: paypal.me/ext1
- Revolut: revolut.me/marekb92

---

## ⚖️ Legal Notice

For **authorized security testing and system administration only**.  
User assumes full legal responsibility for all actions performed.  
Misuse may violate computer crime laws in your jurisdiction.

---

*© 2026 Marek Wesołowski — WESMAR*
